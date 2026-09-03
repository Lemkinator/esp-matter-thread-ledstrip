#pragma once

#include <esp_err.h>
#include <esp_log.h>

#include <algorithm>
#include <vector>

#include "color_format.h"
#include "led_strip_helper.h"

typedef struct
{
    int gpio;
    uint32_t led_count;
} led_config_t;

struct led_render_ctx {
    led_render_ctx(led_strip_handle_t handle_, CRGB* pixels_, uint32_t led_count_, CRGB rgb_, CRGB& rgb_dest_,
                   uint8_t brightness_, uint8_t speed_, uint8_t mode_modification_)
        : handle(handle_),
          pixels(pixels_),
          led_count(led_count_),
          rgb(rgb_),
          brightness(brightness_),
          speed(speed_),
          mode_modification(mode_modification_),
          rgb_dest_ref(rgb_dest_) {}

    // Requests a new destination color for handle_transitions() to fade toward
    // on the next frame — only dynamic_demo_render uses this.
    void request_color(CRGB c) { rgb_dest_ref = c; }

    led_strip_handle_t handle;
    CRGB*               pixels;       // writable pixel scratch buffer (len = led_count)
    uint32_t            led_count;
    CRGB                rgb;          // current interpolated color (snapshot)
    uint8_t             brightness;
    uint8_t             speed;
    uint8_t             mode_modification;

   private:
    CRGB& rgb_dest_ref;
};

using mode_render_fn_t = void (*)(led_render_ctx&);

struct led_mode_t {
    uint8_t id;
    const char* name;
    bool supports_color;
    mode_render_fn_t render;
};

extern std::vector<led_mode_t> modes;

// Thread safety: designed for a single-core FreeRTOS target (ESP32-C6).
// Matter callbacks and the effect task run on the same core and do not interleave.
// Add a mutex if porting to a multi-core target.
class led {
   public:
    led(const led_config_t* config);

    esp_err_t init();

    esp_err_t identify_start();
    esp_err_t identify_stop();

    esp_err_t   set_power(bool power);
    esp_err_t   set_brightness(uint8_t brightness);
    uint8_t     get_brightness_dest();
    esp_err_t   set_temperature(uint32_t mired);
    esp_err_t   set_xy(uint16_t x, uint16_t y);
    esp_err_t   set_mode(uint8_t mode);
    led_mode_t* get_mode();
    esp_err_t   set_speed(uint8_t speed);
    esp_err_t   set_mode_modification(uint8_t mod);

   private:
    static void effect_task_entry(void* pvParameters);
    void handle_transitions();
    led_render_ctx make_render_ctx();

    led_config_t config;
    led_strip_handle_t handle = nullptr;

    CRGB rgb;
    CRGB rgb_dest;
    bool power = false;
    bool power_dest = false;
    uint8_t brightness = 0;
    uint8_t brightness_dest = 128;
    led_mode_t* mode = nullptr;
    uint8_t speed = 128;
    uint8_t mode_modification = 128;
    bool identifying = false;
    std::vector<CRGB> pixels;
};
