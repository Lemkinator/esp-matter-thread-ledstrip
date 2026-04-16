#include "status_led.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <led_strip.h>

static const char* TAG = "status_led";

static led_strip_handle_t s_strip = NULL;
static int s_prev_color_r = 0;
static int s_prev_color_g = 0;
static int s_prev_color_b = 0;
static esp_timer_handle_t s_restore_timer = NULL;

static void restore_timer_cb(void* arg) {
    if (!s_strip) return;
    led_strip_set_pixel(s_strip, 0, s_prev_color_r, s_prev_color_g, s_prev_color_b);
    led_strip_refresh(s_strip);
}

esp_err_t status_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = 8,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
    };
    
    // --- SPI Configuration ---
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags = {
            .with_dma = true, // DMA takes the load off the CPU
        }
    };
    
    // --- Initialize SPI device instead of RMT ---
    esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &s_strip);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to init led strip: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }

    // timer
    if (!s_restore_timer) {
        const esp_timer_create_args_t args = {
            .callback = restore_timer_cb,
            .name = "led_restore",
        };
        esp_timer_create(&args, &s_restore_timer);
    }

    // set default red
    s_prev_color_r = 5; s_prev_color_g = 0; s_prev_color_b = 0;
    led_strip_set_pixel(s_strip, 0, s_prev_color_r, s_prev_color_g, s_prev_color_b);
    led_strip_refresh(s_strip);
    return ESP_OK;
}

void status_led_set_red(void) {
    if (!s_strip) return;
    s_prev_color_r = 5; s_prev_color_g = 0; s_prev_color_b = 0;
    led_strip_set_pixel(s_strip, 0, s_prev_color_r, s_prev_color_g, s_prev_color_b);
    led_strip_refresh(s_strip);
}

void status_led_set_green(void) {
    if (!s_strip) return;
    s_prev_color_r = 0; s_prev_color_g = 5; s_prev_color_b = 0;
    led_strip_set_pixel(s_strip, 0, s_prev_color_r, s_prev_color_g, s_prev_color_b);
    led_strip_refresh(s_strip);
}

void status_led_blink_orange_once(void) {
    if (!s_strip) return;
    // orange = red + some green
    led_strip_set_pixel(s_strip, 0, 5, 2, 0);
    led_strip_refresh(s_strip);
    if (s_restore_timer) {
        esp_timer_stop(s_restore_timer);
        esp_timer_start_once(s_restore_timer, 300 * 1000); // 300 ms
    }
}