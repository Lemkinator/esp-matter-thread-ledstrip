#include "led_strip_helper.h"

#include <cstdint>

#include "esp_timer.h"

static const char* TAG = "led_strip_helper";

uint32_t get_millisecond_timer() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

float get_time_s() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS / 1000.0f;
}

void maintain_fps(uint32_t start_tick, uint8_t fps) {
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t elapsed_ticks = current_tick - start_tick;
    uint32_t target_ticks = pdMS_TO_TICKS((1000 / fps));

    if (elapsed_ticks < target_ticks) {
        vTaskDelay(target_ticks - elapsed_ticks);
    } else {
        vTaskDelay(1);  // We are lagging! Yield for 1 tick to keep the watchdog happy
    }
}

esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, CRGB rgb) {
    return led_strip_set_pixel(strip, index, rgb.r, rgb.g, rgb.b);
}

esp_err_t led_strip_set_all(led_strip_handle_t strip, uint32_t led_count, uint32_t red, uint32_t green, uint32_t blue) {
    esp_err_t err;
    for (uint32_t i = 0; i < led_count; i++) {
        err = led_strip_set_pixel(strip, i, red, green, blue);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
            return err;
        }
    }
    err = led_strip_refresh(strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to refresh strip: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t led_strip_set_all(led_strip_handle_t strip, uint32_t led_count, CRGB rgb) {
    return led_strip_set_all(strip, led_count, rgb.r, rgb.g, rgb.b);
}

// Helper function that blends one uint8_t toward another by a given amount
void fadeToU8(uint8_t& current, const uint8_t target, uint8_t amount) {
    if (current == target) return;

    if (current < target) {
        uint8_t delta = target - current;
        delta = scale8_video(delta, amount);
        current += delta;
    } else {
        uint8_t delta = current - target;
        delta = scale8_video(delta, amount);
        current -= delta;
    }
}

void fadeToColor(CRGB& current, const CRGB& target, uint8_t amount) {
    fadeToU8(current.red, target.red, amount);
    fadeToU8(current.green, target.green, amount);
    fadeToU8(current.blue, target.blue, amount);
}

void fadeToColor(CRGB* pixels, uint32_t count, const CRGB& target, uint8_t amount) {
    for (uint32_t i = 0; i < count; i++) {
        fadeToColor(pixels[i], target, amount);
    }
}