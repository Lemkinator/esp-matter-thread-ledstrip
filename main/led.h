#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <math.h>

#include <algorithm>
#include <vector>

#include "color_format.h"
#include "led_strip_helper.h"

typedef struct
{
    int gpio;
    uint32_t led_count;
} led_config_t;

using mode_render_fn_t = void (*)(class led*);

struct Mode {
    uint8_t id;
    const char* name;
    bool supports_color;
    mode_render_fn_t render;
};

extern std::vector<Mode> modes;
void solid_render(led* l);
void demo_render(led* l);
void dynamic_demo_render(led* l);
void relax_render(led* l);
void fireplace_render(led* l);
void candle_render(led* l);
void lava_render(led* l);
void ocean_render(led* l);
void aurora_render(led* l);
void twinkle_render(led* l);
void breathing_render(led* l);
void comet_render(led* l);
void sunrise_render(led* l);
void neon_render(led* l);
void plasma_render(led* l);
void meteor_shower_render(led* l);
void forest_render(led* l);
void color_flow_render(led* l);
void bounce_render(led* l);
void pulse_render(led* l);
void theater_chase_render(led* l);
void rainbow_render(led* l);
void sparkle_render(led* l);
void strobe_render(led* l);
void lightning_render(led* l);

/**
 * @brief LED class managing hardware interface and Matter state.
 */
class led {
    friend void solid_render(led* l);
    friend void demo_render(led* l);
    friend void dynamic_demo_render(led* l);
    friend void relax_render(led* l);
    friend void fireplace_render(led* l);
    friend void candle_render(led* l);
    friend void lava_render(led* l);
    friend void ocean_render(led* l);
    friend void aurora_render(led* l);
    friend void twinkle_render(led* l);
    friend void breathing_render(led* l);
    friend void comet_render(led* l);
    friend void sunrise_render(led* l);
    friend void neon_render(led* l);
    friend void plasma_render(led* l);
    friend void meteor_shower_render(led* l);
    friend void forest_render(led* l);
    friend void color_flow_render(led* l);
    friend void bounce_render(led* l);
    friend void pulse_render(led* l);
    friend void theater_chase_render(led* l);
    friend void rainbow_render(led* l);
    friend void sparkle_render(led* l);
    friend void strobe_render(led* l);
    friend void lightning_render(led* l);

   public:
    /**
     * @brief Constructs the led instance.
     * @param config Pointer to the configuration structure.
     */
    led(const led_config_t* config);

    /**
     * @brief Initializes the RMT peripheral and LED driver.
     * @return ESP_OK on success, or specific error code.
     */
    esp_err_t init();

    /**
     * @brief Starts the identify effect (visual feedback).
     * @return ESP_OK on success.
     */
    esp_err_t identify_start();

    /**
     * @brief Stops the identify effect and restores state.
     * @return ESP_OK on success.
     */
    esp_err_t identify_stop();

    /**
     * @brief Sets the On/Off power state.
     * @param power True for ON, False for OFF.
     * @return ESP_OK on success.
     */
    esp_err_t set_power(bool power);

    /**
     * @brief Sets the brightness level.
     * @param brightness Brightness level (0-255, though usually mapped 0-100 in Matter).
     * @return ESP_OK on success.
     */
    esp_err_t set_brightness(uint8_t brightness);

    /**
     * @brief Gets the target brightness level.
     * @return Brightness level.
     */
    uint8_t get_brightness_dest();

    /**
     * @brief Sets the color temperature (CCT).
     * @param mired Color temperature in Mireds.
     * @return ESP_OK on success.
     */
    esp_err_t set_temperature(uint32_t mired);

    /**
     * @brief Sets the color using CIE xy coordinates.
     * @param x CIE x coordinate (scaled 0-65535).
     * @param y CIE y coordinate (scaled 0-65535).
     * @return ESP_OK on success.
     */
    esp_err_t set_xy(uint16_t x, uint16_t y);

    /**
     * @brief Sets the operation mode (e.g., solid, effect).
     * @param mode Mode identifier.
     * @return ESP_OK on success.
     */
    esp_err_t set_mode(uint8_t mode);

    /**
     * @brief Gets the current mode.
     * @return Pointer to the current mode structure.
     */
    Mode* get_mode();

    /**
     * @brief Sets the speed for effects that support it.
     * @param speed Speed level (0-255).
     * @return ESP_OK on success.
     */
    esp_err_t set_speed(uint8_t speed);

    /**
     * @brief Sets the mode modification for effects that support it.
     * @param mod Mode modification value (0-255).
     * @return ESP_OK on success.
     */
    esp_err_t set_mode_modification(uint8_t mod);

   private:
    /**
     * @brief Entry point for the LED effect task.
     * @param pvParameters Pointer to the LED instance.
     */
    static void effect_task_entry(void* pvParameters);

    /**
     * @brief Handles transitions for power, brightness, and color changes.
     * @param instance Pointer to the LED instance.
     *
     * This function is called on each iteration of the effect task to smoothly transition
     * power, brightness, and color towards their destination values.
     */
    void handle_transitions();

    led_config_t config;
    led_strip_handle_t handle = nullptr;

    CRGB rgb;
    CRGB rgb_dest;
    bool power = false;
    bool power_dest = false;
    uint8_t brightness = 0;
    uint8_t brightness_dest = 128;
    Mode* mode = nullptr;
    uint8_t speed = 128;
    uint8_t mode_modification = 128;
    bool identifying = false;
    std::vector<CRGB> pixels;
    TaskHandle_t effect_task_handle;
};
