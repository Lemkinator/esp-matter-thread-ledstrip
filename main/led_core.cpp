#include "led_modes.h"

static const char* TAG = "led";

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
    return led_render_ctx(handle, pixels.data(), config.led_count, rgb, rgb_dest, brightness, speed, mode_modification);
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

            ctx.request_color(CRGB(r, g, b));
        }
    }

    if (modes[current_mode_idx].render != nullptr) {
        modes[current_mode_idx].render(ctx);
    }
}
