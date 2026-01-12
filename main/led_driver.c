#include <esp_log.h>
#include <led_strip.h>
#include "led_driver.h"

static const char *TAG = "led_driver";
static bool power = false;
static uint8_t brightness = 0;
static RGB_color_t rgb = {0, 0, 0};


led_driver_handle_t led_driver_init(led_driver_config_t *config)
{
    ESP_LOGI(TAG, "Initializing LED driver");
    esp_err_t err = ESP_OK;
    led_strip_handle_t strip;
    led_strip_config_t strip_config = {
        .strip_gpio_num = config->gpio,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED driver install failed");
        return NULL;
    }
    return (led_driver_handle_t)strip;
}

esp_err_t led_driver_update(led_driver_handle_t handle)
{
    esp_err_t err = ESP_OK;
    if (!handle) {
        ESP_LOGE(TAG, "LED driver handle cannot be NULL");
        return ESP_FAIL;
    } 
    led_strip_handle_t strip = (led_strip_handle_t)handle;
    if (!power || brightness == 0) {
        err = led_strip_set_pixel(strip, 0, 0, 0, 0);
    } else {
        err = led_strip_set_pixel(strip, 0, rgb.red * brightness / 100, rgb.green * brightness / 100, rgb.blue * brightness / 100);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip_set_pixel failed");
        return err;
    }
    ESP_LOGI(TAG, "LED set r:%d, g:%d, b:%d (%s, %d%%)", rgb.red, rgb.green, rgb.blue, power ? "on" : "off", brightness);
    err = led_strip_refresh(strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip_refresh failed");
    }
    return err;
}

esp_err_t led_driver_identify_start(led_driver_handle_t handle, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant)
{
    ESP_LOGI(TAG, "LED identify start: effect_id=%d, effect_variant=%d", effect_id, effect_variant);
    esp_err_t err = ESP_OK;
    if (!handle) {
        ESP_LOGE(TAG, "LED driver handle cannot be NULL");
        return ESP_FAIL;
    } 
    led_strip_handle_t strip = (led_strip_handle_t)handle;
    err = led_strip_set_pixel(strip, 0, 255, 255, 255);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip_set_pixel failed");
        return err;
    }
    err = led_strip_refresh(strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "strip_refresh failed");
    }
    return err;
}

esp_err_t led_driver_identify_stop(led_driver_handle_t handle, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant)
{
    ESP_LOGI(TAG, "LED identify stop: effect_id=%d, effect_variant=%d", effect_id, effect_variant);
    return led_driver_update(handle);
}

esp_err_t led_driver_set_power(led_driver_handle_t handle, bool newPower)
{
    ESP_LOGI(TAG, "LED set power: %d", newPower);
    power = newPower;
    return led_driver_update(handle);
}

esp_err_t led_driver_set_brightness(led_driver_handle_t handle, uint8_t newBrightness)
{
    ESP_LOGI(TAG, "LED set brightness: %d", newBrightness);
    if (newBrightness < 1) { //Matter is weird, why is brightness set to 0 first and then to the actual brightness on power on?
        return ESP_OK;
    }
    brightness = newBrightness;
    return led_driver_update(handle);
}

esp_err_t led_driver_set_temperature(led_driver_handle_t handle, uint32_t temperature)
{
    ESP_LOGI(TAG, "LED set temperature: %ld", temperature);
    uint16_t x, y;
    cct_to_xy(temperature, &x, &y);
    xy_to_rgb(x, y, &rgb);
    return led_driver_update(handle);
}

esp_err_t led_driver_set_xy(led_driver_handle_t handle, uint16_t x, uint16_t y)
{
    ESP_LOGI(TAG, "LED set x: %d, y: %d", x, y);
    xy_to_rgb(x, y, &rgb);
    return led_driver_update(handle);
}

esp_err_t led_driver_set_mode(led_driver_handle_t handle, uint8_t mode)
{
    ESP_LOGI(TAG, "LED set mode: %d", mode);
    // Switch functionality based on mode
    switch (mode) {
        case 0: // Solid
            // Stop animation tasks, set static color
            break;
        case 1: // ...
            // Start ... animation task
            //xTaskNotifyGive(..._task_handle); 
            break;
        // ... handle other cases
    }
    return ESP_OK;
}
