# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Development Environment

ESP-Matter requires Linux. This project is developed on **WSL** via the VSCode Remote extension.

The ESP32-C6 is connected over USB and forwarded into WSL using
`usbipd`. The device was bound once with `usbipd bind --busid=<busid>`
(elevated). On every replug or machine restart, re-attach it from the
Windows host:

```powershell
usbipd list                                            # find the busid
usbipd attach --wsl --busid=1-8 --host-ip=192.168.178.55
```

`--host-ip` is the Windows machine's LAN IP (physical Wi-Fi/Ethernet
adapter, not vEthernet (WSL)) — update it here if it changes.

The device appears as `/dev/ttyACM0` inside WSL.

## Environment Setup

Before building, source the toolchain and set required env vars:

```bash
. ~/esp/v5.5.1/esp-idf/export.sh
export ESP_MATTER_PATH=/home/leo/.espressif/esp-matter
. $ESP_MATTER_PATH/export.sh
```

## Build Commands

Always pass the full `SDKCONFIG_DEFAULTS` chain — without it, `set-target` regenerates `sdkconfig` from scratch and resets values not in the defaults files.

**Thread-only (default target):**

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

## Architecture

### Matter Data Model (`app_main.cpp`)

The firmware creates two Matter endpoints:

1. **Extended Color Light** (ep 1) — the main LED strip endpoint, with clusters:
   - `OnOff`, `LevelControl`, `ColorControl` (color temp + CIE xy)
   - `ModeSelect` — maps to LED animation modes (see `modes` vector in `led.cpp`)
   - Two custom attributes **piggy-backed on standard LevelControl IDs**:
     - `OnTransitionTime` → animation **speed** (value is 10ths-of-a-second in the GUI; divided by 10 before storing as `uint8_t`)
     - `OffTransitionTime` → **mode modification** parameter (same scaling)
     - Triggering *Identify* on the temperature endpoint resets both to default (1280)

2. **Temperature Sensor** (ep 2) — reads the ESP32-C6 internal temperature sensor every 30 s and reports via Matter. The identify callback on this endpoint is repurposed to reset speed/mode-mod to defaults.

### LED Driver (`led.h`, `led.cpp`)

`class led` owns the RMT LED strip handle and a FreeRTOS task (`led_effect_task`) that runs at 30 fps. Each tick calls `handle_transitions()` (smooth fade for power/brightness/color) then the current `Mode::render` function.

State changes from Matter attributes are non-blocking writes (`power_dest`, `brightness_dest`, `rgb_dest`, etc.); the effect task reads them on the next frame.

Hardware defaults: GPIO 2, 50 × WS2812 LEDs.

**Adding a new mode:** Implement a `mode_render_fn_t` render function in `led.cpp` (signature `void (led_render_ctx&)`) and append a `Mode{id, name, supports_color, fn}` entry to the `modes` vector — no `led.h` edit needed. The `DynamicSupportedModesManager` in `mode_select_driver.h` auto-publishes the `modes` vector over Matter.

The `supports_color` flag controls whether `app_driver_light_set_solid_mode_if_color_not_supported()` forces the device back to Solid mode when color temperature or XY is changed while a non-color mode is active.

### Color Pipeline (`color_format.h/cpp`, `led_strip_helper.h/cpp`)

- CCT (Mired) → RGB via logarithmic algorithm (`cct_to_rgb`)
- CIE xy → sRGB with iterative gamut mapping (`xy_to_rgb`)
- `led_strip_helper` wraps `esp_idf led_strip` with `CRGB`-typed helpers and `maintain_fps` / `fadeToColor` / `fadeToU8`

### FastLED Port (`components/fastled/`)

Minimal FastLED subset ported for ESP-IDF: `CRGB`/`CHSV` pixel types (`pixeltypes.h`), lib8tion math (`beatsin8/16/88`, `scale8`, `map8`, `qadd8`, `qsub8`, `random8`), and `hsv2rgb_rainbow`.

### Status LED (`status_led.h/cpp`)

Uses the ESP32-C6 onboard RGB LED (GPIO 8 via BSP):

- **Red** — booting / not commissioned
- **Green** — Thread connected or fabric committed
- **Orange blink** — incoming Matter attribute update

### Key Config Knobs

| sdkconfig defaults file | Purpose |
| --- | --- |
| `sdkconfig.defaults` | Common (BLE, partitions, OTA, button timing) |
| `sdkconfig.defaults.esp32c6` | Target selection |
| `sdkconfig.defaults.c6_thread` | Thread-only, no WiFi STA |
| `sdkconfig.defaults.c6_wifi_thread` | Thread + WiFi concurrent |

`partitions.csv` defines the custom partition layout (OTA-enabled, 4 MB flash).
