#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <led_strip.h>
#include <math.h>

#include "color_format.h"

#define TARGET_FPS 30
#define FRAME_DELAY_MS (1000 / TARGET_FPS)

/**
 * @brief Delays execution for specified milliseconds. (Non-blocking)
 * @param ms Milliseconds to delay.
 */
void delay_ms(uint32_t ms);

/**
 * @brief Set RGB for a specific pixel using CRGB structure
 *
 * @param strip: LED strip
 * @param index: index of pixel to set
 * @param rgb: CRGB color structure
 *
 * @return
 *      - ESP_OK: Set RGB for a specific pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set RGB for a specific pixel failed because of invalid parameters
 *      - ESP_FAIL: Set RGB for a specific pixel failed because other error occurred
 */
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, CRGB rgb);

/**
 * @brief Set RGB for a specific pixel
 *
 * @param strip: LED strip
 * @param led_count: number of LEDs in the strip
 * @param red: red part of color
 * @param green: green part of color
 * @param blue: blue part of color
 *
 * @return
 *      - ESP_OK: Set RGB for a all pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set RGB for a all pixel failed because of invalid parameters
 *      - ESP_FAIL: Set RGB for a all pixel failed because other error occurred
 */
esp_err_t led_strip_set_all(led_strip_handle_t strip, uint32_t led_count, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Set RGB for all pixels using CRGB structure
 *
 * @param strip: LED strip
 * @param led_count: number of LEDs in the strip
 * @param rgb: CRGB color structure
 *
 * @return
 *      - ESP_OK: Set RGB for all pixel successfully
 *      - ESP_ERR_INVALID_ARG: Set RGB for all pixel failed because of invalid parameters
 *      - ESP_FAIL: Set RGB for all pixel failed because other error occurred
 */
esp_err_t led_strip_set_all(led_strip_handle_t strip, uint32_t led_count, CRGB rgb);

/**
 * @brief Get current time in seconds (with millisecond precision)
 *
 * @return Current time in seconds
 */
float get_time_s();

/**
 * @brief Maintain a consistent frame rate.
 * @param start_tick The tick count when the frame started.
 */
void maintain_fps(uint32_t start_tick);
