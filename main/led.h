#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <math.h>

#include <vector>

#include "color_format.h"
#include "led_strip_helper.h"

using mode_render_fn_t = void (*)(class led*);

struct Mode {
    uint8_t id;
    const char* name;
    bool supports_color;
    bool supports_speed;
    mode_render_fn_t render;
};

// Global list of available modes
extern std::vector<Mode> modes;

typedef struct
{
    int gpio;
    uint32_t led_count;
} led_config_t;

/**
 * @brief LED class managing hardware interface and Matter state.
 */
class led {
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
     * @brief Get a mode by its ID.
     * @param id The mode ID.
     * @return Pointer to the mode, or nullptr if not found.
     */
    Mode* get_mode_by_id(uint8_t id);

    /**
     * @brief Initialize the LED modes.
     * @return ESP_OK on success.
     */
    esp_err_t init_modes();

    /**
     * @brief Sets the operation mode (e.g., solid, effect).
     * @param mode Mode identifier.
     * @return ESP_OK on success.
     */
    esp_err_t set_mode(uint8_t mode);

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

   private:
    /**
     * @brief Entry point for the LED effect task.
     * @param pvParameters Pointer to the LED instance.
     */
    static void effect_task_entry(void* pvParameters);

    /**
     * @brief Updates the physical LED strip based on stored state.
     * @return ESP_OK on success.
     */
    esp_err_t update();

    led_config_t config;
    led_strip_handle_t handle = nullptr;

    // Internal State
    bool power = false;
    uint8_t brightness = 128;
    uint8_t speed = 128; 
    uint8_t mode_modification = 128;
    CRGB rgb;
    bool identifying = false;

    // Animation control
    const Mode* mode = nullptr;
    std::vector<CRGB> pixels;
    TaskHandle_t effect_task_handle;
    friend void bounce_render(class led*);
    friend void pulse_render(class led*);
    friend void rainbow_render(class led*);
    friend void relax_render(class led*);
};
