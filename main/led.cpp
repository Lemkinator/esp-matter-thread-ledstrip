#include "led.h"

static const char* TAG = "led";

led::led(const led_config_t* config) : config(*config) {
    // allocate and zero-initialize pixel buffer
    pixels.resize(this->config.led_count);
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
    cct_to_rgb((uint16_t)mired, &rgb_dest);
    return ESP_OK;
}

esp_err_t led::set_xy(uint16_t x, uint16_t y) {
    ESP_LOGI(TAG, "Set XY: x=%d, y=%d", x, y);
    xy_to_rgb(x, y, &rgb_dest);
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
        } else if ((instance->power || instance->power_dest != instance->power) && instance->mode != nullptr) {
            instance->handle_transitions();
            instance->mode->render(instance);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        maintain_fps(start_tick, 30);
    }
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
    Mode{20, "Relax", true, relax_render},
    Mode{30, "Bounce", true, bounce_render},
    Mode{35, "Pulse", true, pulse_render},
    Mode{40, "Rainbow", false, rainbow_render},
};

void solid_render(led* l) {
    CRGB rgb = l->rgb;
    led_strip_set_all(l->handle, l->config.led_count, rgb.nscale8_video(l->brightness));
}

void bounce_render(led* l) {
    float speed_factor = l->speed / 255.0f;
    float time = get_time_s() * speed_factor;
    float position = (sinf(time) + 1.0f) / 2.0f;
    int max_idx = l->config.led_count - 1;
    int index = std::clamp(static_cast<int>(position * l->config.led_count), 0, max_idx);
    int mod = static_cast<int>((l->mode_modification / 255.0f) * 60) - 30;  
    uint8_t fade = static_cast<uint8_t>(64 + mod - speed_factor * 56);

    for (int i = 0; i <= max_idx; i++) {
        l->pixels[i].fadeToBlackBy(fade);
        led_strip_set_pixel(l->handle, i, l->pixels[i].fadeToBlackBy(fade)); 
    }
    CRGB rgb = l->rgb;
    l->pixels[index] = rgb.nscale8_video(l->brightness);
    led_strip_set_pixel(l->handle, index, l->pixels[index]); 
    led_strip_refresh(l->handle);
}

void pulse_render(led* l) {
    float s = (sinf(get_time_s() * 5.0f * l->speed / 255.0f) + 1.0f) / 2.0f;
    uint8_t bri = static_cast<uint8_t>(s * l->brightness);
    CRGB rgb = l->rgb;
    led_strip_set_all(l->handle, l->config.led_count, rgb.nscale8_video(bri));
}

void rainbow_render(led* l) {
    uint8_t base_hue = static_cast<uint8_t>(get_time_s() * 100.0f * l->speed / 255.0f);
    uint8_t rainbow_width = (l->mode_modification * 8) / 255.0f;
    for (int i = 0; i < (int)l->config.led_count; i++) {
        CRGB rgb;
        hsv2rgb_rainbow(CHSV(base_hue + (i * rainbow_width), 255, l->brightness), rgb);
        led_strip_set_pixel(l->handle, i, rgb);
    }
    led_strip_refresh(l->handle);
}

void relax_render(led* l) {
    CRGB rgb = l->rgb;
    rgb.scale8(220);
    int dots = static_cast<int>((l->mode_modification / 255.0f) * 10);
    float speed_factor = l->speed / 255.0f;

    for (int i = 0; i < (int)l->config.led_count; i++) {
        fadeToColor(l->pixels[i], rgb, 2);
        CRGB pixel = l->pixels[i];
        led_strip_set_pixel(l->handle, i, pixel.nscale8_video(l->brightness));
    }
    for (int i = 0; i < dots; i++) {
        int pos = beatsin16(static_cast<int>((0.3f * i + speed_factor) * 256), 0, l->config.led_count - 1, 0, 32767 / (i + 1));
        l->pixels[pos] += CRGB(15, 8, 4);
        CRGB pixel = l->pixels[pos];
        led_strip_set_pixel(l->handle, pos, pixel.nscale8_video(l->brightness));
    }
    led_strip_refresh(l->handle);
}