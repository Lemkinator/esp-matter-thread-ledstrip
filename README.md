# ESP Matter Thread LED Strip

Personal ESP-IDF firmware for an ESP32-C6 that controls an addressable WS2812 LED strip over Matter/Thread. The standard Matter color light device type is extended with a `ModeSelect` cluster to expose 24 animation modes, plus two piggy-backed attributes for per-mode speed and modification parameters.

## Hardware

| Component | Detail |
|---|---|
| MCU | ESP32-C6 |
| LED data pin | GPIO 2 |
| LED count | 50 × WS2812 |
| Status LED | Onboard RGB (GPIO 8, BSP) |
| Connectivity | Thread (IEEE 802.15.4) |

The status LED reflects commissioning state: red while booting/uncommissioned, green when Thread-connected or a fabric is committed, orange blink on incoming attribute updates.

## Matter Data Model

### Endpoint 1 — Extended Color Light

| Cluster | Notes |
|---|---|
| `OnOff` | Power on/off |
| `LevelControl` | Brightness (0–254) |
| `ColorControl` | Color temp (Mired) + CIE xy |
| `ModeSelect` | Animation mode selection |

Two extra parameters are piggy-backed on standard `LevelControl` attribute IDs:

| Attribute ID | Repurposed as | Scale |
|---|---|---|
| `OnTransitionTime` | Animation **speed** | GUI value ÷ 10 → `uint8_t` |
| `OffTransitionTime` | **Mode modification** | GUI value ÷ 10 → `uint8_t` |

Triggering *Identify* on endpoint 2 (temperature sensor) resets both to their defaults (128).

### Endpoint 2 — Temperature Sensor

Reads the ESP32-C6 internal temperature sensor every 30 s and reports it via Matter. The Identify callback on this endpoint is repurposed to reset speed/mode-mod to defaults.

## Animation Modes

Modes are published automatically via `DynamicSupportedModesManager`. The `supports_color` flag controls whether selecting a non-color mode while color temp/xy is active forces a fallback to Solid.

| ID | Name | Color |
|---|---|---|
| 0 | Solid | yes |
| 10 | Demo | yes |
| 11 | Dynamic Demo | yes |
| 20 | Relax | yes |
| 21 | Fireplace | yes |
| 22 | Candle | yes |
| 23 | Lava Lamp | yes |
| 24 | Ocean Waves | yes |
| 25 | Aurora | yes |
| 26 | Twinkle Stars | yes |
| 27 | Breathing | yes |
| 28 | Comet | yes |
| 29 | Sunrise | yes |
| 30 | Neon Sign | yes |
| 31 | Plasma | yes |
| 32 | Meteor Shower | yes |
| 33 | Forest | yes |
| 34 | Color Flow | yes |
| 40 | Bounce | yes |
| 41 | Pulse | yes |
| 42 | Chase | yes |
| 50 | Rainbow | no |
| 70 | Sparkle | yes |
| 71 | Strobe | yes |
| 72 | Lightning | yes |

## Building

Source the toolchain first:

```bash
. ~/esp/v5.5.1/esp-idf/export.sh
export ESP_MATTER_PATH=/home/leo/.espressif/esp-matter
. $ESP_MATTER_PATH/export.sh
```

Always pass the full `SDKCONFIG_DEFAULTS` chain — omitting it causes `set-target` to regenerate `sdkconfig` from scratch.

**Thread-only (default):**

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32c6;sdkconfig.defaults.c6_thread" \
  set-target esp32c6 build
```

**WiFi + Thread:**

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32c6;sdkconfig.defaults.c6_wifi_thread" \
  set-target esp32c6 build
```

**Flash and monitor** (port `/dev/ttyACM0`):

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

**Erase NVS** (clears Matter commissioning data):

```bash
idf.py -p /dev/ttyACM0 erase-flash
```

## Project Structure

```
main/
  app_main.cpp          # Matter endpoint + cluster setup
  app_driver.cpp        # Attribute write → led method dispatch
  led.h / led.cpp       # LED class, FreeRTOS effect task, all render functions
  led_strip_helper.h/cpp# esp_idf led_strip wrapper (CRGB helpers, fps limiter)
  color_format.h/cpp    # CCT→RGB (logarithmic) and CIE xy→sRGB (gamut-mapped)
  mode_select_driver.h  # DynamicSupportedModesManager — auto-publishes modes[]
  status_led.h/cpp      # Onboard RGB status indicator
  temp_driver.h/cpp     # Internal temperature sensor polling

components/fastled/     # Minimal FastLED port: CRGB/CHSV, lib8tion math, hsv2rgb
```

### LED effect task

`led` owns a FreeRTOS task running at 30 fps. Each tick:

1. `handle_transitions()` — smooth fades for power, brightness, and color
2. Current `Mode::render` function — writes pixels for the active animation

State changes from Matter attributes are non-blocking writes to destination fields (`power_dest`, `brightness_dest`, `rgb_dest`, …); the effect task picks them up on the next frame.

### Adding a new mode

1. Declare `void my_mode_render(led* l);` and `friend void my_mode_render(led* l);` in [main/led.h](main/led.h)
2. Implement the render function in [main/led.cpp](main/led.cpp)
3. Append `Mode{id, "Name", supports_color, my_mode_render}` to the `modes` vector in [main/led.cpp](main/led.cpp)
