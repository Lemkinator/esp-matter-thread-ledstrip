#include <button_gpio.h>
#include <common_macros.h>
#include <device.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "led.h"
#include "temp_driver.h"

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char* TAG = "app_driver";
extern uint16_t light_endpoint_id;
extern uint16_t temp_endpoint_id;

// Global variables to store current XY color coordinates
static uint16_t current_x = 0;
static uint16_t current_y = 0;

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led* handle, esp_matter_attr_val_t* val) {
    return handle->set_power(val->val.b);
}

static esp_err_t app_driver_light_set_brightness(led* handle, esp_matter_attr_val_t* val) {
    return handle->set_brightness(val->val.u8);
}

static esp_err_t app_driver_light_set_mode(led* handle, esp_matter_attr_val_t* val) {
    return handle->set_mode(val->val.u8);
}

static esp_err_t app_driver_light_set_speed(led* handle, esp_matter_attr_val_t* val) {
    return handle->set_speed(val->val.u16 / 10);  // is shown in GUI as 10th of a second
}

static esp_err_t app_driver_light_set_mode_modification(led* handle, esp_matter_attr_val_t* val) {
    return handle->set_mode_modification(val->val.u16 / 10);  // is shown in GUI as 10th of a second
}

static void app_driver_light_set_solid_mode_if_color_not_supported(led* handle) {
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = ModeSelect::Id;
    uint32_t attribute_id = ModeSelect::Attributes::CurrentMode::Id;
    attribute_t* attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    Mode* mode = handle->get_mode();
    if (mode != nullptr && !mode->supports_color) {
        // Set to solid mode
        val.val.u8 = 0;
        attribute::update(light_endpoint_id, ModeSelect::Id, ModeSelect::Attributes::CurrentMode::Id, &val);
    }
}

static esp_err_t app_driver_light_set_temperature(led* handle, esp_matter_attr_val_t* val) {
    esp_err_t err = handle->set_temperature(val->val.u16);
    app_driver_light_set_solid_mode_if_color_not_supported(handle);
    return err;
}

static esp_err_t app_driver_light_set_xy(led* handle, uint16_t x, uint16_t y) {
    esp_err_t err = handle->set_xy(x, y);
    app_driver_light_set_solid_mode_if_color_not_supported(handle);
    return err;
}

esp_err_t app_driver_identify(app_driver_handle_t driver_handle, uint16_t endpoint_id, identification::callback_type_t type, uint8_t effect_id, uint8_t effect_variant) {
    ESP_LOGI(TAG, "Identify callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        led* handle = static_cast<led*>(driver_handle);
        if (type == identification::START) {
            err = handle->identify_start();
        } else if (type == identification::STOP) {
            err = handle->identify_stop();
        }
    } else if (endpoint_id == temp_endpoint_id) {
        // Use Temp sensor identify to reset speed and mode modification to default values.
        if (type == identification::START) {
            ESP_LOGI(TAG, "Resetting speed and mode modification to default values");
            esp_matter_attr_val_t val = esp_matter_nullable_uint16(nullable<uint16_t>(1280));
            attribute::update(light_endpoint_id, LevelControl::Id, LevelControl::Attributes::OnTransitionTime::Id, &val);
            attribute::update(light_endpoint_id, LevelControl::Id, LevelControl::Attributes::OffTransitionTime::Id, &val);
        }
    }
    return err;
}

static void app_driver_button_toggle_cb(void* arg, void* data) {
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t* attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t* val) {
    ESP_LOGI(TAG, "Attribute update callback: endpoint_id: %u, cluster_id: 0x%08x, attribute_id: 0x%08x", endpoint_id, cluster_id, attribute_id);
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        led* handle = static_cast<led*>(driver_handle);
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            } else if (attribute_id == LevelControl::Attributes::OnTransitionTime::Id) {
                // This attribute is used to control the speed for effects that support it.
                err = app_driver_light_set_speed(handle, val);
            } else if (attribute_id == LevelControl::Attributes::OffTransitionTime::Id) {
                // This attribute is used to control the mode modification for effects that support it.
                err = app_driver_light_set_mode_modification(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentX::Id) {
                current_x = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            } else if (attribute_id == ColorControl::Attributes::CurrentY::Id) {
                current_y = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            }
        } else if (cluster_id == ModeSelect::Id) {
            if (attribute_id == ModeSelect::Attributes::CurrentMode::Id) {
                err = app_driver_light_set_mode(handle, val);
            }
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id) {
    ESP_LOGI(TAG, "Setting light defaults for endpoint_id: %d", endpoint_id);
    esp_err_t err = ESP_OK;
    void* priv_data = endpoint::get_priv_data(endpoint_id);
    led* handle = static_cast<led*>(priv_data);
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    attribute_t* attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting mode */
    attribute = attribute::get(endpoint_id, ModeSelect::Id, ModeSelect::Attributes::CurrentMode::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_mode(handle, &val);
    Mode* mode = handle->get_mode();
    if (mode == nullptr || mode->supports_color) {
        /* Setting color */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id);
        attribute::get_val(attribute, &val);
        if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
            /* Setting temperature */
            attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
            attribute::get_val(attribute, &val);
            err |= app_driver_light_set_temperature(handle, &val);
        } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY) {
            /* Setting XY coordinates */
            attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id);
            attribute::get_val(attribute, &val);
            current_x = val.val.u16;
            attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id);
            attribute::get_val(attribute, &val);
            current_y = val.val.u16;
            err |= app_driver_light_set_xy(handle, current_x, current_y);
        } else {
            ESP_LOGE(TAG, "Color mode not supported");
        }
    }

    /* Setting power */
    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    /* Setting speed and mode modification */
    attribute = attribute::get(light_endpoint_id, LevelControl::Id, LevelControl::Attributes::OnTransitionTime::Id);
    if (attribute) {
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_speed(handle, &val);
    }
    attribute = attribute::get(light_endpoint_id, LevelControl::Id, LevelControl::Attributes::OffTransitionTime::Id);
    if (attribute) {
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_mode_modification(handle, &val);
    }

    return err;
}

// Application cluster specification, 7.18.2.11. Temperature
// represents a temperature on the Celsius scale with a resolution of 0.01°C.
// temp = (temperature in °C) x 100
static void temp_sensor_notification(uint16_t endpoint_id, float temp, void* user_data) {
    // schedule the attribute update so that we can report it from matter thread
    chip::DeviceLayer::SystemLayer().ScheduleLambda([endpoint_id, temp]() {
        attribute_t * attribute = attribute::get(endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id);

        esp_matter_attr_val_t val = esp_matter_invalid(NULL);
        attribute::get_val(attribute, &val);
        val.val.i16 = static_cast<int16_t>(temp * 100);

        attribute::update(endpoint_id, TemperatureMeasurement::Id,TemperatureMeasurement::Attributes::MeasuredValue::Id, &val); });
}

esp_err_t app_driver_temp_init() {
    temp_driver_config config = {
        .cb = temp_sensor_notification,
        .endpoint_id = temp_endpoint_id,
        .interval_ms = 30000};
    static temp_driver s_temp_sensor(&config);
    return s_temp_sensor.start();
}

app_driver_handle_t app_driver_light_init() {
    led_config_t config = {
        //.gpio = 8, // ESP32C6 onboard LED
        //.led_count = 1
        .gpio = 2,
        .led_count = 50};
    led* light = new led(&config);
    if (light->init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED");
        return NULL;
    }
    return static_cast<app_driver_handle_t>(light);
}

app_driver_handle_t app_driver_button_init() {
    /* Initialize Boot button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return static_cast<app_driver_handle_t>(handle);
}
