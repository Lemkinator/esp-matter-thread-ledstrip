#include <esp_log.h>
#include <esp_timer.h>
#include "temp_driver.h"

static const char *TAG = "temp_driver";

temp_driver::temp_driver(temp_driver_config *cfg) {
    if (cfg) {
        config = *cfg;
    }
}

void temp_driver::timer_handler(void *arg) {
    auto *instance = static_cast<temp_driver *>(arg);
    float temp;
    
    if (instance->read_celsius(&temp) == ESP_OK) {
        if (instance->config.cb) {
            instance->config.cb(instance->config.endpoint_id, temp, instance->config.user_data);
        }
    }
}

esp_err_t temp_driver::read_celsius(float *value) {
    esp_err_t err = temperature_sensor_get_celsius(sensor_handle, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t temp_driver::start() {
    // 1. Install Hardware
    temperature_sensor_config_t ts_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    esp_err_t err = temperature_sensor_install(&ts_cfg, &sensor_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Install failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = temperature_sensor_enable(sensor_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enable failed: %s", esp_err_to_name(err));
        return err;
    }

    // 2. Setup Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &temp_driver::timer_handler,
        .arg = this,
        .name = "temp_tmr"
    };

    err = esp_timer_create(&timer_args, &timer_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Timer creation failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_timer_start_periodic(timer_handle, config.interval_ms * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Timer start failed: %s", esp_err_to_name(err));
    }

    return err;
}