#pragma once

#include <esp_err.h>

esp_err_t status_led_init(void);
void status_led_set_red(void);
void status_led_set_green(void);
void status_led_blink_orange_once(void);


