#include "led.h"

static const char* TAG = "led";

// Forward declarations — implementation detail of this file only
void solid_render(led_render_ctx& ctx);
void demo_render(led_render_ctx& ctx);
void dynamic_demo_render(led_render_ctx& ctx);
void relax_render(led_render_ctx& ctx);
void fireplace_render(led_render_ctx& ctx);
void candle_render(led_render_ctx& ctx);
void lava_render(led_render_ctx& ctx);
void ocean_render(led_render_ctx& ctx);
void aurora_render(led_render_ctx& ctx);
void twinkle_render(led_render_ctx& ctx);
void breathing_render(led_render_ctx& ctx);
void comet_render(led_render_ctx& ctx);
void sunrise_render(led_render_ctx& ctx);
void neon_render(led_render_ctx& ctx);
void plasma_render(led_render_ctx& ctx);
void meteor_shower_render(led_render_ctx& ctx);
void forest_render(led_render_ctx& ctx);
void color_flow_render(led_render_ctx& ctx);
void bounce_render(led_render_ctx& ctx);
void pulse_render(led_render_ctx& ctx);
void theater_chase_render(led_render_ctx& ctx);
void rainbow_render(led_render_ctx& ctx);
void sparkle_render(led_render_ctx& ctx);
void strobe_render(led_render_ctx& ctx);
void lightning_render(led_render_ctx& ctx);
uint16_t rand16seed = 1337;

led::led(const led_config_t* config) : config(*config) {
    pixels.resize(this->config.led_count);
    mode = &modes[0];
}

esp_err_t led::init() {
    ESP_LOGI(TAG, "Initializing LED strip on GPIO %d with %d LEDs", config.gpio, config.led_count);
    led_strip_config_t strip_config = {
        .strip_gpio_num = config.gpio,
        .max_leds = config.led_count,
        .led_model = LED_MODEL_WS2812,
        //.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .mem_block_symbols = 64,
        .flags = {.with_dma = false}};

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip device: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ret = xTaskCreate(
        led::effect_task_entry,  // Function pointer
        "led_effect_task",       // Name for debugging
        4096,                    // Stack size (4KB is usually plenty for LEDs)
        this,                    // Parameter (pointer to this instance)
        5,                       // Priority
        nullptr                  // Task handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t led::identify_start() {
    ESP_LOGI(TAG, "Identify start");
    identifying = true;
    return ESP_OK;
}

esp_err_t led::identify_stop() {
    ESP_LOGI(TAG, "Identify Stop");
    identifying = false;
    return ESP_OK;
}

esp_err_t led::set_power(bool pwr) {
    ESP_LOGI(TAG, "Set Power: %s", pwr ? "ON" : "OFF");
    power_dest = pwr;
    return ESP_OK;
}

esp_err_t led::set_brightness(uint8_t bri) {
    ESP_LOGI(TAG, "Set Brightness: %d", bri);
    if (bri < 1) {  // Matter is weird, why is brightness set to 0 first and then to the actual brightness on power on?
        return ESP_OK;
    }
    brightness_dest = bri;
    return ESP_OK;
}

uint8_t led::get_brightness_dest() {
    return brightness_dest;
}

esp_err_t led::set_temperature(uint32_t mired) {
    ESP_LOGI(TAG, "Set Temperature: %ld Mireds", mired);
    rgb_dest = cct_to_rgb(static_cast<uint16_t>(mired));
    return ESP_OK;
}

esp_err_t led::set_xy(uint16_t x, uint16_t y) {
    ESP_LOGI(TAG, "Set XY: x=%d, y=%d", x, y);
    rgb_dest = xy_to_rgb(x, y);
    return ESP_OK;
}

esp_err_t led::set_mode(uint8_t id) {
    ESP_LOGI(TAG, "Set Mode: %d", id);
    for (auto& m : modes) {
        if (m.id == id) {
            mode = &m;
            return ESP_OK;
        }
    }
    mode = &modes[0];
    ESP_LOGW(TAG, "Mode ID %d not found, setting to default mode: %s", id, mode->name);
    return ESP_OK;
}

Mode* led::get_mode() {
    return mode;
}

esp_err_t led::set_speed(uint8_t s) {
    ESP_LOGI(TAG, "Set Speed: %d", s);
    speed = s;
    return ESP_OK;
}

esp_err_t led::set_mode_modification(uint8_t mod) {
    ESP_LOGI(TAG, "Set ModeModification: %d", mod);
    mode_modification = mod;
    return ESP_OK;
}

void led::effect_task_entry(void* pvParameters) {
    led* instance = static_cast<led*>(pvParameters);

    while (1) {
        uint32_t start_tick = xTaskGetTickCount();

        if (instance->identifying) {
            static uint8_t count = 0;
            static bool state = false;
            if (++count >= 15) {  // Toggle every ~500ms at 30fps
                state = !state;
                count = 0;
            }
            // Identity is always full white or off, ignoring current color/brightness
            CRGB blink_color = state ? CRGB(255, 255, 255) : CRGB(0, 0, 0);
            led_strip_set_all(instance->handle, instance->config.led_count, blink_color);
        } else if (instance->power || instance->power_dest != instance->power) {
            instance->handle_transitions();
            led_render_ctx ctx = instance->make_render_ctx();
            instance->mode->render(ctx);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        maintain_fps(start_tick, 30);
    }
}

led_render_ctx led::make_render_ctx() {
    return {handle, pixels.data(), config.led_count, rgb, rgb_dest, brightness, speed, mode_modification};
}

void led::handle_transitions() {
    fadeToColor(rgb, rgb_dest, 1);
    if (power_dest != power) {
        fadeToU8(brightness, power_dest ? brightness_dest : 0, 1);
        if (power_dest && brightness == brightness_dest) {
            ESP_LOGI(TAG, "Power transition complete, power is now ON");
            power = true;
        } else if (!power_dest && brightness == 0) {
            ESP_LOGI(TAG, "Power transition complete, power is now OFF");
            power = false;
        }
    } else {
        fadeToU8(brightness, brightness_dest, 1);
    }
}

std::vector<Mode> modes = {
    Mode{0, "Solid", true, solid_render},
    Mode{10, "Demo", true, demo_render},
    Mode{11, "Dynamic Demo", true, dynamic_demo_render},
    Mode{20, "Relax", true, relax_render},
    Mode{21, "Fireplace", true, fireplace_render},
    Mode{22, "Candle", true, candle_render},
    Mode{23, "Lava Lamp", true, lava_render},
    Mode{24, "Ocean Waves", true, ocean_render},
    Mode{25, "Aurora", true, aurora_render},
    Mode{26, "Twinkle Stars", true, twinkle_render},
    Mode{27, "Breathing", true, breathing_render},
    Mode{28, "Comet", true, comet_render},
    Mode{29, "Sunrise", true, sunrise_render},
    Mode{30, "Neon Sign", true, neon_render},
    Mode{31, "Plasma", true, plasma_render},
    Mode{32, "Meteor Shower", true, meteor_shower_render},
    Mode{33, "Forest", true, forest_render},
    Mode{34, "Color Flow", true, color_flow_render},
    Mode{40, "Bounce", true, bounce_render},
    Mode{41, "Pulse", true, pulse_render},
    Mode{42, "Chase", true, theater_chase_render},
    Mode{50, "Rainbow", false, rainbow_render},
    Mode{70, "Sparkle", true, sparkle_render},
    Mode{71, "Strobe", true, strobe_render},
    Mode{72, "Lightning", true, lightning_render},
};

void solid_render(led_render_ctx& ctx) {
    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, rgb.nscale8_video(ctx.brightness));
}

void demo_render(led_render_ctx& ctx) {
    // Note: If MULTIPLE independent LED strips running at the same time,
    // these should be moved into the `led` class properties instead of being static here.
    static uint32_t last_switch_tick = 0;
    static size_t current_mode_idx = 0;
    uint32_t current_tick = xTaskGetTickCount();
    uint8_t delay_seconds = map8(ctx.speed, 1, 60);
    uint32_t delay_ticks = pdMS_TO_TICKS(delay_seconds * 1000);

    // Initialization or Timer expiration check
    if (last_switch_tick == 0 || (current_tick - last_switch_tick) >= delay_ticks) {
        last_switch_tick = current_tick;

        current_mode_idx++;
        if (current_mode_idx >= modes.size()) {
            current_mode_idx = 0;
        }

        while (modes[current_mode_idx].render == demo_render || modes[current_mode_idx].render == dynamic_demo_render) {
            current_mode_idx++;
            if (current_mode_idx >= modes.size()) {
                current_mode_idx = 0;
            }
        }

        ESP_LOGI("DEMO", "Switching to mode: %s", modes[current_mode_idx].name);
    }

    if (modes[current_mode_idx].render != nullptr) {
        modes[current_mode_idx].render(ctx);
    }
}

void dynamic_demo_render(led_render_ctx& ctx) {
    // Note: If MULTIPLE independent LED strips running at the same time,
    // these should be moved into the `led` class properties instead of being static here.
    static uint32_t last_switch_tick = 0;
    static size_t current_mode_idx = 0;
    uint32_t current_tick = xTaskGetTickCount();
    uint8_t delay_seconds = map8(ctx.speed, 1, 60);
    uint32_t delay_ticks = pdMS_TO_TICKS(delay_seconds * 1000);

    if (last_switch_tick == 0 || (current_tick - last_switch_tick) >= delay_ticks) {
        last_switch_tick = current_tick;

        current_mode_idx++;
        if (current_mode_idx >= modes.size()) {
            current_mode_idx = 0;
        }

        while (modes[current_mode_idx].render == demo_render ||
               modes[current_mode_idx].render == dynamic_demo_render) {
            current_mode_idx++;
            if (current_mode_idx >= modes.size()) {
                current_mode_idx = 0;
            }
        }

        ESP_LOGI("DEMO", "Switching to mode: %s", modes[current_mode_idx].name);

        // If the new mode supports color (the 'true' boolean in your struct),
        // give it a fresh, random destination color to transition to.
        if (modes[current_mode_idx].supports_color) {
            // Generate a random bright color (avoiding dim/black colors)
            uint8_t r = random8(50, 255);
            uint8_t g = random8(50, 255);
            uint8_t b = random8(50, 255);

            ctx.rgb_dest = CRGB(r, g, b);
        }
    }

    if (modes[current_mode_idx].render != nullptr) {
        modes[current_mode_idx].render(ctx);
    }
}

void relax_render(led_render_ctx& ctx) {
    CRGB dot_add = CRGB(16, 8, 4);
    CRGB bg = ctx.rgb;
    bg.nscale8_video(220);
    CRGB limit = bg + CRGB(50, 50, 50);
    uint8_t num_dots = map8(ctx.mode_modification, 1, 8);
    uint16_t base_bpm_88 = ctx.speed + 1;
    for (int i = 0; i < ctx.led_count; i++) {
        fadeToColor(ctx.pixels[i], bg, 1);
        CRGB pixel = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, pixel.nscale8_video(ctx.brightness));
    }
    for (int i = 0; i < num_dots; i++) {
        uint16_t dot_bpm = base_bpm_88 + (i * ctx.speed / 3);
        uint16_t pos = beatsin88(dot_bpm, 0, ctx.led_count - 1, 0, i * 65536 / num_dots);
        ctx.pixels[pos] += dot_add;
        if (ctx.pixels[pos].r > limit.r) ctx.pixels[pos].r = limit.r;
        if (ctx.pixels[pos].g > limit.g) ctx.pixels[pos].g = limit.g;
        if (ctx.pixels[pos].b > limit.b) ctx.pixels[pos].b = limit.b;
        CRGB pixel = ctx.pixels[pos];
        led_strip_set_pixel(ctx.handle, pos, pixel.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🔥 Fireplace
//   speed → sparking energy    50–200   (128 ≈ 125)
//   mod   → flame height       high mod = tall; low mod = low embers
// ─────────────────────────────────────────────────────────────────────────────
void fireplace_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t cooling = map8(ctx.mode_modification, 55, 23);  // low mod→more cooling→shorter flames
    uint8_t sparking = map8(ctx.speed, 50, 200);

    for (int i = 0; i < n; i++) {
        uint8_t cool = random8(0, ((cooling * 10) / n) + 2);
        ctx.pixels[i].r = qsub8(ctx.pixels[i].r, cool);
    }
    for (int i = n - 1; i >= 2; i--) {
        ctx.pixels[i].r = (static_cast<uint16_t>(ctx.pixels[i - 1].r) + static_cast<uint16_t>(ctx.pixels[i - 2].r) + static_cast<uint16_t>(ctx.pixels[i - 2].r)) / 3;
    }
    if (random8() < sparking) {
        int y = random8(0, 7);
        if (y < n) ctx.pixels[y].r = qadd8(ctx.pixels[y].r, random8(160, 255));
    }
    CRGB hot = ctx.rgb;
    for (int i = 0; i < n; i++) {
        uint8_t h = ctx.pixels[i].r;
        CRGB c;
        if (h < 128) {
            uint8_t t2 = h << 1;
            c.r = scale8(hot.r, t2);
            c.g = scale8(hot.g, t2);
            c.b = scale8(hot.b, t2);
        } else {
            uint8_t t2 = (h - 128) << 1;
            c.r = hot.r + scale8(255 - hot.r, t2);
            c.g = hot.g + scale8(255 - hot.g, t2);
            c.b = hot.b + scale8(255 - hot.b, t2);
        }
        c.nscale8_video(ctx.brightness);
        led_strip_set_pixel(ctx.handle, i, c);
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🕯️ Candle  — now uses three inharmonic beatsin8 oscillators
//   speed → flicker rate BPM    20–120     (128 ≈ 70 BPM)
//   mod   → glow corona radius  pinpoint ↔ full-strip soft fill
// ─────────────────────────────────────────────────────────────────────────────
void candle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;

    // Three BPMs with irrational ratios — ensures aperiodic, organic flicker
    uint16_t bpm_a = static_cast<uint16_t>(map8(ctx.speed, 20, 120)) * 256;  // base
    uint16_t bpm_b = static_cast<uint16_t>(map8(ctx.speed, 26, 154)) * 256;  // ×1.28
    uint16_t bpm_c = static_cast<uint16_t>(map8(ctx.speed, 13, 78)) * 256;   // ×0.65

    float f1 = beatsin8(bpm_a, 0, 255, 0, 0) / 255.0f;
    float f2 = beatsin8(bpm_b, 0, 255, 0, 85) / 255.0f;
    float f3 = beatsin8(bpm_c, 0, 255, 0, 170) / 255.0f;
    // Keep candle in upper 65–100 % brightness range (real candles are always lit)
    uint8_t bri = static_cast<uint8_t>((0.65f + (f1 * 0.5f + f2 * 0.3f + f3 * 0.2f) * 0.35f) * ctx.brightness);

    float sigma = 1.5f + (ctx.mode_modification / 255.0f) * (n * 0.45f);
    float inv_2sig2 = 1.0f / (2.0f * sigma * sigma);
    int center = n / 2;
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) {
        float dist = static_cast<float>(i - center);
        float falloff = expf(-dist * dist * inv_2sig2);
        CRGB c = rgb;
        led_strip_set_pixel(ctx.handle, i, c.nscale8_video(static_cast<uint8_t>(bri * falloff)));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🫧 Lava Lamp
//   speed → blob drift speed    0.08–1.0   (128 ≈ gentle viscous movement)
//   mod   → blob count          2–5        (128 ≈ 3)
// ─────────────────────────────────────────────────────────────────────────────
void lava_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 8, 100)) / 100.0f;  // 0.08–1.0 (always moving)
    int n = ctx.led_count;
    int num_blobs = map8(ctx.mode_modification, 2, 5);
    CRGB rgb = ctx.rgb;

    float blob_pos[5], blob_size[5];
    for (int b = 0; b < num_blobs; b++) {
        blob_pos[b]  = (sinf(t * (0.4f + b * 0.15f) * sf + b * 2.094f) + 1.0f) / 2.0f;
        blob_size[b] = 0.15f + sinf(t * 0.09f * sf + b * 1.732f) * 0.05f;
    }

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float total = 0.0f;
        for (int b = 0; b < num_blobs; b++) {
            float dist = fabsf(fi - blob_pos[b]);
            if (dist > 0.5f) dist = 1.0f - dist;  // wrap-around continuity
            float contrib = std::max(0.0f, 1.0f - dist / blob_size[b]);
            total += contrib * contrib * contrib;  // cubic: defined edges, soft center
        }
        CRGB c = rgb;
        led_strip_set_pixel(ctx.handle, i, c.nscale8_video(static_cast<uint8_t>(std::min(total, 1.0f) * ctx.brightness)));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌊 Ocean
//   speed → wave travel speed    0.5–4.0   (128 ≈ 2.25)
//   mod   → wave layers          1–3       (128 ≈ 2)
// ─────────────────────────────────────────────────────────────────────────────
void ocean_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float spd = static_cast<float>(map8(ctx.speed, 5, 40)) / 10.0f;  // 0.5–4.0
    int n = ctx.led_count;
    int num_waves = map8(ctx.mode_modification, 1, 3);
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float val = 0.0f;
        for (int w = 0; w < num_waves; w++) {
            float freq = 1.0f + w * 0.8f;
            float wspd = spd * (1.0f + w * 0.4f);
            float phase = static_cast<float>(w) * 2.094f;  // 120° apart — no dead-band nulls
            val += sinf(fi * 6.28318f * freq - t * wspd + phase);
        }
        val = (val / static_cast<float>(num_waves) + 1.0f) / 2.0f;
        val = val * val;  // accentuate bright crests, deepen troughs
        CRGB c = rgb;
        led_strip_set_pixel(ctx.handle, i, c.nscale8_video(static_cast<uint8_t>(val * ctx.brightness)));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌌 Aurora
//   speed → curtain drift speed    frozen shimmer ↔ active curtain  (128 ≈ gentle)
//   mod   → hue spread             10–147 hue units                 (128 ≈ 78)
// ─────────────────────────────────────────────────────────────────────────────
void aurora_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 5, 80)) / 100.0f;  // 0.05–0.80
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t spread = map8(ctx.mode_modification, 10, 147);

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float w1 = sinf(fi * 3.14159f + t * 0.40f * sf);
        float w2 = sinf(fi * 6.28318f - t * 0.65f * sf + 1.5f);
        float w3 = sinf(fi * 1.88495f + t * 0.22f * sf + 3.0f);

        float brightness = ((w1 + w2 * 0.5f + w3 * 0.3f) / 1.8f + 1.0f) / 2.0f;
        brightness = brightness * brightness;  // sparse dark gaps between curtains

        float hue_shift = (w1 * 0.6f + w2 * 0.4f) * static_cast<float>(spread);
        uint8_t hue = base_hue + static_cast<int8_t>(hue_shift);
        CRGB c;
        hsv2rgb_rainbow(CHSV(hue, 220, static_cast<uint8_t>(brightness * ctx.brightness)), c);
        led_strip_set_pixel(ctx.handle, i, c);
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// ✨ Twinkle Stars
//   speed → star lifetime    long-lived ↔ brief pops   (128 ≈ moderate)
//   mod   → star density     sparse ↔ dense field      (128 ≈ moderate)
// ─────────────────────────────────────────────────────────────────────────────
void twinkle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t fade_amount = map8(ctx.speed, 3, 21);
    uint8_t spawn_prob = map8(ctx.mode_modification, 2, 34);
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) {
        ctx.pixels[i].fadeToBlackBy(fade_amount);
        if (random8() < spawn_prob) ctx.pixels[i] = rgb;
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🫁 Breathing  — BPM-mapped, biological 4-phase pattern (inhale/hold/exhale/rest)
//   speed → breathing rate    3–20 BPM   (128 ≈ 11 BPM ≈ 5.5 s/breath)
//   mod   → hold + rest       5–30 %     (128 ≈ 17 % — adds meditative pause)
// ─────────────────────────────────────────────────────────────────────────────
void breathing_render(led_render_ctx& ctx) {
    float bpm = static_cast<float>(map8(ctx.speed, 3, 20));
    float hold_pct = static_cast<float>(map8(ctx.mode_modification, 5, 30)) / 100.0f;

    float cycle = fmodf(get_time_s() * bpm / 60.0f, 1.0f);

    float inhale_end = 0.40f - hold_pct * 0.30f;
    float hold_end = inhale_end + hold_pct;
    float exhale_end = hold_end + 0.40f - hold_pct * 0.30f;
    // [exhale_end → 1.0] = dark rest

    float bri_f;
    if (cycle < inhale_end) {
        float p = cycle / inhale_end;
        bri_f = p * p * (3.0f - 2.0f * p);  // smoothstep up
    } else if (cycle < hold_end) {
        bri_f = 1.0f;
    } else if (cycle < exhale_end) {
        float p = (cycle - hold_end) / (exhale_end - hold_end);
        bri_f = 1.0f - p * p * (3.0f - 2.0f * p);  // smoothstep down
    } else {
        bri_f = 0.0f;
    }

    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count,
                      rgb.nscale8_video(static_cast<uint8_t>(bri_f * ctx.brightness)));
}

// ─────────────────────────────────────────────────────────────────────────────
// ☄️ Comet
//   speed → travel speed    4–29 px/s    (128 ≈ 16 px/s → ~3 s to cross strip)
//   mod   → tail length     5–35 px      (128 ≈ 20 px)
// ─────────────────────────────────────────────────────────────────────────────
void comet_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    float travel = static_cast<float>(map8(ctx.speed, 4, 29));
    int tail = map8(ctx.mode_modification, 5, 35);
    int total = n + tail;
    int head = static_cast<int>(fmodf(get_time_s() * travel, static_cast<float>(total)));

    for (int i = 0; i < n; i++) ctx.pixels[i] = CRGB::Black;

    CRGB rgb = ctx.rgb;
    for (int j = 0; j <= tail; j++) {
        int pos = head - j;
        if (pos < 0 || pos >= n) continue;
        float intens = 1.0f - static_cast<float>(j) / static_cast<float>(tail + 1);
        intens = intens * intens;  // quadratic falloff
        ctx.pixels[pos].r = static_cast<uint8_t>(rgb.r * intens);
        ctx.pixels[pos].g = static_cast<uint8_t>(rgb.g * intens);
        ctx.pixels[pos].b = static_cast<uint8_t>(rgb.b * intens);
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌅 Sunrise
//   speed → glow cycle BPM      1–8       (128 ≈ 4 BPM → ~15 s full cycle)
//   mod   → hue journey width   0–60      (0 = single color pulse, 255 = warm→cool shift)
//   color → sunrise anchor hue  (warm amber/orange recommended)
//
//   Whole strip breathes with a gentle edge-to-center warmth gradient.
//   Hue shifts cooler as brightness rises, mimicking dawn light temperature.
// ─────────────────────────────────────────────────────────────────────────────
void sunrise_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint16_t bpm_88 = static_cast<uint16_t>(map8(ctx.speed, 1, 8)) * 256;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t hue_span = map8(ctx.mode_modification, 0, 60);

    uint8_t bri_raw = beatsin8(bpm_88, 5, ctx.brightness);

    // Hue is warmer (lower = more red) when dim, shifts cooler as it brightens
    uint8_t hue_shift = scale8(hue_span, 255 - bri_raw);
    uint8_t hue = base_hue + hue_shift;

    CRGB base_c;
    hsv2rgb_rainbow(CHSV(hue, 240, bri_raw), base_c);

    // Gentle spatial gradient — center slightly brighter than ends (like a horizon glow)
    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        uint8_t edge = static_cast<uint8_t>((0.80f + 0.20f * sinf(fi * 3.14159f)) * 255);
        CRGB c = base_c;
        led_strip_set_pixel(ctx.handle, i, c.nscale8_video(edge));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 💡 Neon Sign
//   speed → buzz BPM        30–200     (128 ≈ 115 BPM — audible-frequency buzz)
//   mod   → instability     subtle ↔ heavy hum   (128 ≈ faint steady hum)
//   color → tube color
//
//   Three inharmonic oscillators create an organic, non-repeating buzz.
//   The tube stays mostly bright — neon doesn't dim much, it just isn't perfect.
// ─────────────────────────────────────────────────────────────────────────────
void neon_render(led_render_ctx& ctx) {
    uint8_t buzz_depth = map8(ctx.mode_modification, 2, 60);

    uint16_t bpm_a = static_cast<uint16_t>(map8(ctx.speed, 30, 200)) * 256;
    uint16_t bpm_b = static_cast<uint16_t>(map8(ctx.speed, 38, 254)) * 256;  // ×1.27
    uint16_t bpm_c = static_cast<uint16_t>(map8(ctx.speed, 19, 128)) * 256;  // ×0.64

    uint8_t v1 = beatsin8(bpm_a, 0, buzz_depth, 0, 0);
    uint8_t v2 = beatsin8(bpm_b, 0, buzz_depth, 0, 85);
    uint8_t v3 = beatsin8(bpm_c, 0, buzz_depth, 0, 170);

    // Weighted sum — v1 dominates, v2/v3 add texture
    uint8_t dip = static_cast<uint8_t>((v1 * 50u + v2 * 33u + v3 * 17u) / 100u);
    uint8_t bri = qsub8(ctx.brightness, dip);

    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, rgb.nscale8_video(bri));
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌈 Plasma
//   speed → wave drift speed    static ↔ flowing    (128 ≈ moderate)
//   mod   → hue range           20–255 hue units    (128 ≈ wide colorful spread)
//   color → center/anchor hue
//
//   Three non-harmonic sine waves produce a continuously morphing color field.
// ─────────────────────────────────────────────────────────────────────────────
void plasma_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 5, 80)) / 100.0f;  // 0.05–0.80
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t hue_range = map8(ctx.mode_modification, 20, 255);

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n);
        float v = sinf(fi * 6.28318f + t * sf) + sinf(fi * 13.56637f - t * sf * 0.73f) + sinf((fi + t * sf * 0.41f) * 9.42477f);
        v = (v / 3.0f + 1.0f) / 2.0f;  // normalize 0–1

        CRGB c;
        hsv2rgb_rainbow(CHSV(base_hue + static_cast<uint8_t>(v * hue_range), 240, ctx.brightness), c);
        led_strip_set_pixel(ctx.handle, i, c);
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌠 Meteor Shower
//   speed → travel speed    5–35 px/s    (128 ≈ 20 px/s)
//   mod   → count           1–6          (128 ≈ 3–4 simultaneous meteors)
//   color → meteor color
//
//   Each meteor has a slightly different speed — they never perfectly align.
//   Additive blending where they cross looks realistic.
// ─────────────────────────────────────────────────────────────────────────────
void meteor_shower_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float base_travel = static_cast<float>(map8(ctx.speed, 5, 35));
    int n = ctx.led_count;
    int num_meteors = map8(ctx.mode_modification, 1, 6);
    const int TAIL = 8;
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(35);

    for (int m = 0; m < num_meteors; m++) {
        float speed_var = 0.70f + static_cast<float>(m) * 0.13f;
        float offset = static_cast<float>(m) / static_cast<float>(num_meteors);
        float total = static_cast<float>(n + TAIL);
        int head = static_cast<int>(fmodf(t * base_travel * speed_var + offset * total, total));

        for (int j = 0; j <= TAIL; j++) {
            int pos = head - j;
            if (pos < 0 || pos >= n) continue;
            float intens = 1.0f - static_cast<float>(j) / static_cast<float>(TAIL + 1);
            intens = intens * intens;
            CRGB c;
            c.r = static_cast<uint8_t>(rgb.r * intens);
            c.g = static_cast<uint8_t>(rgb.g * intens);
            c.b = static_cast<uint8_t>(rgb.b * intens);
            ctx.pixels[pos] += c;  // additive — crossing meteors blend naturally
        }
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌿 Forest
//   speed → breeze speed / dapple movement    (128 ≈ gentle rustle)
//   mod   → light-spot count   2–10           (128 ≈ 6 spots)
//   color → spot color  (warm green-gold recommended, e.g. CRGB(180, 220, 60))
//
//   Uses beatsin16 for positions (spot drifts with breeze) and beatsin8 for
//   per-spot brightness breathing — same approach as your relax_render.
// ─────────────────────────────────────────────────────────────────────────────
void forest_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint16_t bpm_base = static_cast<uint16_t>(map8(ctx.speed, 5, 40));  // 5–40 BPM
    int num_spots = map8(ctx.mode_modification, 2, 10);
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(20);

    for (int s = 0; s < num_spots; s++) {
        uint16_t spot_bpm = (bpm_base + static_cast<uint16_t>(s)) * 256;         // accum88
        uint8_t ph_pos = static_cast<uint8_t>(static_cast<uint16_t>(s) * 256 / num_spots);  // spread phases
        uint8_t ph_bri = ph_pos + 64;

        // Position oscillates with breeze, brightness breathes independently
        int pos = beatsin16(spot_bpm, 0, n - 1, 0, static_cast<uint16_t>(ph_pos) * 256);
        uint8_t bright = beatsin8(spot_bpm, 30, 210, 0, ph_bri);

        // Soft triangle falloff (±3 px) — quick, no expf needed
        const int W = 3;
        for (int i = pos - W; i <= pos + W; i++) {
            if (i < 0 || i >= n) continue;
            uint8_t falloff = 255 - static_cast<uint8_t>(static_cast<uint16_t>(abs(i - pos)) * 255 / (W + 1));
            CRGB c = rgb;
            c.nscale8_video(scale8(bright, falloff));
            ctx.pixels[i] += c;
        }
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌊 Color Flow
//   speed → gradient drift speed    still ↔ flowing    (128 ≈ gentle drift)
//   mod   → hue span across strip   20–255             (128 ≈ ~137 hue units wide)
//   color → anchor/base hue
//
//   A living color gradient — spatially wider at high mod, flowing at high speed.
// ─────────────────────────────────────────────────────────────────────────────
void color_flow_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 3, 60)) / 100.0f;  // 0.03–0.60 hue-rotations/s
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t span = map8(ctx.mode_modification, 20, 255);
    uint8_t t_hue = static_cast<uint8_t>(t * 30.0f * sf);  // hue shifts over time

    for (int i = 0; i < n; i++) {
        uint8_t hue = base_hue + t_hue + static_cast<uint8_t>(static_cast<float>(i) / static_cast<float>(n) * static_cast<float>(span));
        CRGB c;
        hsv2rgb_rainbow(CHSV(hue, 240, ctx.brightness), c);
        led_strip_set_pixel(ctx.handle, i, c);
    }
    led_strip_refresh(ctx.handle);
}

void bounce_render(led_render_ctx& ctx) {
    int led_count = ctx.led_count;
    uint8_t bpm = map8(ctx.speed, 1, 15);
    uint16_t index = beatsin16(bpm, 0, led_count - 1);
    int mod = (ctx.mode_modification * 60) / 255 - 30;
    int speed_deduction = (ctx.speed * 56) / 255;
    uint8_t fade = std::clamp(64 + mod - speed_deduction, 0, 255);
    for (int i = 0; i < led_count; i++) {
        led_strip_set_pixel(ctx.handle, i, ctx.pixels[i].fadeToBlackBy(fade));
    }
    ctx.pixels[index] = ctx.rgb;
    ctx.pixels[index].nscale8_video(ctx.brightness);
    led_strip_set_pixel(ctx.handle, index, ctx.pixels[index]);
    led_strip_refresh(ctx.handle);
}

void pulse_render(led_render_ctx& ctx) {
    uint8_t bpm = map8(ctx.speed, 1, 48);
    uint8_t bri = beatsin16(bpm, 0, ctx.brightness);
    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, rgb.nscale8_video(bri));
}

// ─────────────────────────────────────────────────────────────────────────────
// 🎭 Theater Chase
//   speed → chase speed    1–30 steps/s    (128 ≈ 15 steps/s)
//   mod   → gap width      2–8 px          (128 ≈ 5 px dark gap)
//   color → lit-segment color
// ─────────────────────────────────────────────────────────────────────────────
void theater_chase_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    int steps_per_s = map8(ctx.speed, 1, 30);
    int gap = map8(ctx.mode_modification, 2, 8);
    const int SEG = 2;
    int period = SEG + gap;
    int step = static_cast<int>(get_time_s() * static_cast<float>(steps_per_s)) % period;

    CRGB rgb = ctx.rgb;
    for (int i = 0; i < n; i++) {
        bool lit = ((i + step) % period) < SEG;
        CRGB c = rgb;
        led_strip_set_pixel(ctx.handle, i, c.nscale8_video(lit ? ctx.brightness : 0));
    }
    led_strip_refresh(ctx.handle);
}

void rainbow_render(led_render_ctx& ctx) {
    uint8_t rotation_speed = map8(ctx.speed, 0, 20);
    uint8_t base_hue = beat8(rotation_speed);
    uint8_t delta_hue = map8(ctx.mode_modification, 0, 12);
    for (int i = 0; i < static_cast<int>(ctx.led_count); i++) {
        uint8_t pixel_hue = base_hue + (i * delta_hue);
        CHSV hsv(pixel_hue, 255, ctx.brightness);
        CRGB rgb;
        hsv2rgb_rainbow(hsv, rgb);
        led_strip_set_pixel(ctx.handle, i, rgb);
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// ✨ Sparkle
//   speed → spark lifetime    long-lived ↔ instant pops    (128 ≈ crisp but brief)
//   mod   → sparks per frame  1–15                         (128 ≈ 8)
//   color → spark color (automatically pushed brighter/whiter for camera-flash pop)
// ─────────────────────────────────────────────────────────────────────────────
void sparkle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t fade_rate = map8(ctx.speed, 15, 80);
    int spawn_rate = map8(ctx.mode_modification, 1, 15);

    // Push each new spark toward white — the brief overexposure sells the flash
    CRGB spark;
    spark.r = qadd8(ctx.rgb.r, 80);
    spark.g = qadd8(ctx.rgb.g, 80);
    spark.b = qadd8(ctx.rgb.b, 80);

    // Spawn before fade+output so sparks appear at full brightness this frame
    for (int s = 0; s < spawn_rate; s++) ctx.pixels[random8(n)] = spark;

    for (int i = 0; i < n; i++) {
        ctx.pixels[i].fadeToBlackBy(fade_rate);
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// ⚡ Strobe
//   speed → flash rate   1–20 Hz    (128 ≈ 10 Hz)
//   mod   → on-duty      5–40 %     (128 ≈ 22 %)
// ─────────────────────────────────────────────────────────────────────────────
void strobe_render(led_render_ctx& ctx) {
    int rate_hz = map8(ctx.speed, 1, 20);
    int duty_pct = map8(ctx.mode_modification, 5, 40);
    float period = 1.0f / static_cast<float>(rate_hz);
    bool on = fmodf(get_time_s(), period) < (period * duty_pct / 100.0f);
    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, on ? rgb.nscale8_video(ctx.brightness) : CRGB::Black);
}

// ─────────────────────────────────────────────────────────────────────────────
// ⚡ Lightning  — reworked timing + flash-count distribution
//   speed → strike frequency    rare ↔ stormy   (128 ≈ 1 strike/2.4 s avg)
//   mod   → bolts per strike    1–3             (128 ≈ 2)
//   Flash distribution: 30 % single · 40 % double · 20 % triple · 10 % quad
// ─────────────────────────────────────────────────────────────────────────────
void lightning_render(led_render_ctx& ctx) {
    float t = get_time_s();
    int n = ctx.led_count;
    const float SLOT_S = 0.5f;    // 500 ms decision window
    const float FLASH_W = 0.06f;  // each sub-flash: 30 ms

    uint32_t slot = static_cast<uint32_t>(t / SLOT_S);
    float slot_phase = fmodf(t, SLOT_S) / SLOT_S;

    // Per-slot xorshift32 RNG
    uint32_t rng = slot * 2246822519u ^ 0x9E3779B9u;
    auto rnd = [&]() -> uint32_t {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    rnd();  // initial mix

    // Strike probability per slot: speed=0 → 2 %, 128 → 21 %, 255 → 40 %
    bool has_strike = ((rnd() % 100) < static_cast<uint32_t>(map8(ctx.speed, 2, 40)));

    // Flash-count: 30/40/20/10 distribution
    int flash_count = 1;
    if (has_strike) {
        uint32_t roll = rnd() % 100;
        if (roll < 30)
            flash_count = 1;
        else if (roll < 70)
            flash_count = 2;
        else if (roll < 90)
            flash_count = 3;
        else
            flash_count = 4;
    }

    // Sub-flash positions within the slot (as fractions, all in first 30 % of slot)
    static const float FLASH_T[4][4] = {
        {0.05f, -1.0f, -1.0f, -1.0f},  // 1 flash
        {0.05f, 0.13f, -1.0f, -1.0f},  // 2 flashes
        {0.05f, 0.11f, 0.18f, -1.0f},  // 3 flashes
        {0.05f, 0.10f, 0.16f, 0.23f},  // 4 flashes
    };

    bool flashing = false;
    if (has_strike) {
        for (int f = 0; f < flash_count; f++) {
            float ft = FLASH_T[flash_count - 1][f];
            if (ft < 0.0f) break;
            if (slot_phase >= ft && slot_phase < ft + FLASH_W) {
                flashing = true;
                break;
            }
        }
    }

    uint8_t fade_rate = map8(ctx.speed, 10, 55);
    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(fade_rate);

    if (flashing) {
        CRGB bolt;
        bolt.r = qadd8(ctx.rgb.r, 80);
        bolt.g = qadd8(ctx.rgb.g, 80);
        bolt.b = qadd8(ctx.rgb.b, 80);

        int num_bolts = map8(ctx.mode_modification, 1, 3);

        // Bolt geometry seeded separately — stable across all sub-flashes of this slot
        uint32_t bolt_rng = slot * 1664525u + 1013904223u;
        auto brnd = [&]() -> uint32_t {
            bolt_rng ^= bolt_rng << 13;
            bolt_rng ^= bolt_rng >> 17;
            bolt_rng ^= bolt_rng << 5;
            return bolt_rng;
        };
        for (int b = 0; b < num_bolts; b++) {
            int pos = static_cast<int>(brnd() % n);
            int len = 3 + static_cast<int>(brnd() % 12);
            for (int i = pos; i < std::min(pos + len, n); i++) ctx.pixels[i] = bolt;
        }
    }

    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        led_strip_set_pixel(ctx.handle, i, px.nscale8_video(ctx.brightness));
    }
    led_strip_refresh(ctx.handle);
}