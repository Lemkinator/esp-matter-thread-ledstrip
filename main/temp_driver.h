#pragma once

#include <esp_err.h>
#include "driver/temperature_sensor.h"

/** @brief Callback for temperature updates: (endpoint_id, temperature, user_data) */
using temp_sensor_cb_t = void (*)(uint16_t, float, void *);

/** @brief Configuration for the temperature driver */
struct temp_driver_config {
    temp_sensor_cb_t cb;      ///< Function to call on update
    uint16_t endpoint_id;     ///< Matter endpoint ID
    void *user_data;          ///< Context passed to callback
    uint32_t interval_ms;     ///< Polling rate
};

/** @brief Class to manage ESP32 internal temperature sensor polling */
class temp_driver {
public:
    /** @param config Pointer to configuration (data will be copied) */
    temp_driver(temp_driver_config *config);

    /** @brief Initializes hardware and starts the timer. @return ESP_OK on success. */
    esp_err_t start();

    /** @brief Direct hardware read. @param value Output pointer. @return ESP_OK on success. */
    esp_err_t read_celsius(float *value);

private:
    static void timer_handler(void *arg);
    
    temp_driver_config config;
    temperature_sensor_handle_t sensor_handle = nullptr;
    esp_timer_handle_t timer_handle = nullptr;
};