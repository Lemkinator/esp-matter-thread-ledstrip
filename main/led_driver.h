#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "color_format.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        int gpio;
        int channel;
        bool output_invert;
    } led_driver_config_t;

    typedef void *led_driver_handle_t;

    led_driver_handle_t led_driver_init(led_driver_config_t *config);
    esp_err_t led_driver_set_power(led_driver_handle_t handle, bool power);
    esp_err_t led_driver_set_brightness(led_driver_handle_t handle, uint8_t brightness);
    esp_err_t led_driver_set_temperature(led_driver_handle_t handle, uint32_t temperature);
    esp_err_t led_driver_set_xy(led_driver_handle_t handle, uint16_t x, uint16_t y);
    esp_err_t led_driver_set_mode(led_driver_handle_t handle, uint8_t mode);
    esp_err_t led_driver_identify_start(led_driver_handle_t handle, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant);
    esp_err_t led_driver_identify_stop(led_driver_handle_t handle, uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant);


#ifdef __cplusplus
}
#endif
