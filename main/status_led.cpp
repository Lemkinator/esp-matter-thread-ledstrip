#include "status_led.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <led_strip.h>

namespace {

constexpr int      STATUS_LED_GPIO           = 8;
constexpr uint64_t ORANGE_BLINK_RESTORE_US   = 300 * 1000;  // 300 ms

struct StatusColor {
    uint8_t r, g, b;
};
constexpr StatusColor COLOR_RED{5, 0, 0};
constexpr StatusColor COLOR_GREEN{0, 5, 0};
constexpr StatusColor COLOR_ORANGE{5, 2, 0};

const char* TAG = "status_led";

struct State {
    led_strip_handle_t strip = nullptr;
    StatusColor prev_color = COLOR_RED;
    esp_timer_handle_t restore_timer = nullptr;
};
State s_state;

void set_color(StatusColor color) {
    if (!s_state.strip) return;
    s_state.prev_color = color;
    led_strip_set_pixel(s_state.strip, 0, color.r, color.g, color.b);
    led_strip_refresh(s_state.strip);
}

void restore_timer_cb(void* arg) {
    if (!s_state.strip) return;
    led_strip_set_pixel(s_state.strip, 0, s_state.prev_color.r, s_state.prev_color.g, s_state.prev_color.b);
    led_strip_refresh(s_state.strip);
}

}  // namespace

esp_err_t status_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = STATUS_LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
    };

    // --- SPI Configuration ---
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags = {
            .with_dma = true,  // DMA takes the load off the CPU
        },
    };

    // --- Initialize SPI device instead of RMT ---
    esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &s_state.strip);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to init led strip: %s", esp_err_to_name(err));
        s_state.strip = nullptr;
        return err;
    }

    // timer
    if (!s_state.restore_timer) {
        const esp_timer_create_args_t args = {
            .callback = restore_timer_cb,
            .name = "led_restore",
        };
        esp_timer_create(&args, &s_state.restore_timer);
    }

    set_color(COLOR_RED);
    return ESP_OK;
}

void status_led_set_red(void) {
    set_color(COLOR_RED);
}

void status_led_set_green(void) {
    set_color(COLOR_GREEN);
}

void status_led_blink_orange_once(void) {
    if (!s_state.strip) return;
    // orange = red + some green; transient, does not update prev_color
    led_strip_set_pixel(s_state.strip, 0, COLOR_ORANGE.r, COLOR_ORANGE.g, COLOR_ORANGE.b);
    led_strip_refresh(s_state.strip);
    if (s_state.restore_timer) {
        esp_timer_stop(s_state.restore_timer);
        esp_timer_start_once(s_state.restore_timer, ORANGE_BLINK_RESTORE_US);
    }
}
