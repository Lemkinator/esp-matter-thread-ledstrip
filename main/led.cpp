#include "led.h"

static const char* TAG = "led";

// Global list of available modes (populated by init_modes)
std::vector<Mode> modes;

// Render implementations
void bounce_render(led* l) {
    float speed_factor = l->speed / 255.0f;
    float time = get_time_s() * speed_factor;
    float position = (sinf(time) + 1.0f) / 2.0f;
    int index = static_cast<int>(position * (l->config.led_count));
    if (index < 0)
        index = 0;
    if (index >= (int)l->config.led_count)
        index = (int)l->config.led_count - 1;

    int mod = (l->mode_modification / 255.0f) * 60 - 30;  // -30 to +30
    uint8_t fade = 64 + mod - speed_factor * 56;
    for (int i = 0; i < (int)l->config.led_count; i++) {
        l->pixels[i].fadeToBlackBy(fade);
    }

    l->pixels[index] += l->rgb.scale8(l->brightness);

    for (int i = 0; i < (int)l->config.led_count; i++) {
        led_strip_set_pixel(l->handle, i, l->pixels[i].scale8(l->brightness));
    }
    led_strip_refresh(l->handle);
}

void pulse_render(led* l) {
    float s = (sinf(get_time_s() * 5.0f * l->speed / 255.0f) + 1.0f) / 2.0f;
    uint8_t bri = (uint8_t)(s * l->brightness);
    led_strip_set_all(l->handle, l->config.led_count, l->rgb.scale8(bri));
}

void rainbow_render(led* l) {
    uint8_t base_hue = (uint8_t)(get_time_s() * 100.0f * l->speed / 255.0f);
    uint8_t rainbow_width = l->mode_modification / 255.0f * 8;
    for (int i = 0; i < (int)l->config.led_count; i++) {
        CRGB rgb;
        hsv2rgb_rainbow(CHSV(base_hue + (i * rainbow_width), 255, l->brightness), rgb);
        led_strip_set_pixel(l->handle, i, rgb);
    }
    led_strip_refresh(l->handle);
}

// Relax mode: gentle fade towards an colored background with a few moving dots
void relax_render(led* l) {
    CRGB target = l->rgb;
    target.nscale8_video(220);
     
    int dots = l->mode_modification / 255.0f * 10;
    float speed_factor = l->speed / 255.0f;

    for (int i = 0; i < (int)l->config.led_count; i++) {
        fadeToColor(l->pixels[i], target, 2);
    }

    for (int i = 0; i < dots; i++) {
        int pos = beatsin16((0.3 * i + speed_factor) * 256, 0, l->config.led_count, 0, 32767 / (i + 1));
        l->pixels[pos] += CRGB(15, 8, 4);
    }

    for (int i = 0; i < (int)l->config.led_count; i++) {
        led_strip_set_pixel(l->handle, i, l->pixels[i].scale8(l->brightness));
    }
    led_strip_refresh(l->handle);
}

esp_err_t led::init_modes() {
    // Clear and populate the static list of modes. Solid is represented by null.
    modes.clear();
    modes.push_back(Mode{20, "Relax", true, true, relax_render});
    modes.push_back(Mode{30, "Bounce", true, true, bounce_render});
    modes.push_back(Mode{35, "Pulse", true, true, pulse_render});
    modes.push_back(Mode{40, "Rainbow", false, true, rainbow_render});

    return ESP_OK;
}

Mode* led::get_mode_by_id(uint8_t id) {
    for (auto& m : modes) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

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

esp_err_t led::update() {
    ESP_LOGI(TAG, "Update: R=%d, G=%d, B=%d, Brightness=%d, Power=%s", rgb.r, rgb.g, rgb.b, brightness, power ? "ON" : "OFF");
    if (!handle)
        return ESP_ERR_INVALID_STATE;

    uint8_t r = 0, g = 0, b = 0;
    if (power && brightness > 0) {
        r = (rgb.r * brightness) / 255;
        g = (rgb.g * brightness) / 255;
        b = (rgb.b * brightness) / 255;
    }

    // Loop through all LEDs to set them to the same color
    for (int i = 0; i < config.led_count; i++) {
        led_strip_set_pixel(handle, i, r, g, b);
    }

    esp_err_t err = led_strip_refresh(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh strip: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t led::set_power(bool pwr) {
    ESP_LOGI(TAG, "Set Power: %s", pwr ? "ON" : "OFF");
    power = pwr;
    if (!pwr) {
        return led_strip_clear(handle);
    } else if (this->mode == nullptr) {
        return update();
    }
    return ESP_OK;
}

esp_err_t led::set_brightness(uint8_t bri) {
    ESP_LOGI(TAG, "Set Brightness: %d", bri);
    if (bri < 1) {  // Matter is weird, why is brightness set to 0 first and then to the actual brightness on power on?
        return ESP_OK;
    }
    brightness = bri;
    // If static, update now. If in a mode, the task will pick this up next frame.
    if (this->mode == nullptr) {
        return update();
    }
    return ESP_OK;
}

esp_err_t led::set_speed(uint8_t s) {
    ESP_LOGI(TAG, "Set Speed: %d", s);
    this->speed = s;
    return ESP_OK;
}

esp_err_t led::set_mode_modification(uint8_t mod) {
    ESP_LOGI(TAG, "Set ModeModification: %d", mod);
    this->mode_modification = mod;
    return ESP_OK;
}

esp_err_t led::set_temperature(uint32_t mired) {
    ESP_LOGI(TAG, "Set Temperature: %ld Mireds", mired);
    cct_to_rgb((uint16_t)mired, &rgb);
    if (this->mode == nullptr) {
        return update();
    }
    return ESP_OK;
}

esp_err_t led::set_xy(uint16_t x, uint16_t y) {
    ESP_LOGI(TAG, "Set XY: x=%d, y=%d", x, y);
    xy_to_rgb(x, y, &rgb);
    if (this->mode == nullptr) {
        return update();
    }
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
    mode = nullptr;
    return update();  // Refresh to current static color
}

esp_err_t led::identify_start() {
    ESP_LOGI(TAG, "Identify start");
    identifying = true;
    return ESP_OK;
}

esp_err_t led::identify_stop() {
    ESP_LOGI(TAG, "Identify Stop");
    identifying = false;
    if (this->mode == nullptr) {
        return update();
    }
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
        } else if (instance->power && instance->mode) {
            instance->mode->render(instance);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        maintain_fps(start_tick);
    }
}