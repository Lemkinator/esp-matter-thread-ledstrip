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
adapter, not vEthernet (WSL)); update it here if it changes.

The device appears as `/dev/ttyACM0` inside WSL.

The LED strip is not near the dev PC. To verify LED modes: flash as usual, human unpluggs and
checks at LED strip, then replug to the PC. Claude Code can build, flash,
and read serial logs (crashes, Matter/Thread state) but can't see LEDs.

## Environment Setup

Before building, source the toolchain and set required env vars:

```bash
. ~/esp/v5.5.1/esp-idf/export.sh
export ESP_MATTER_PATH=/home/leo/.espressif/esp-matter
. $ESP_MATTER_PATH/export.sh
```

## Build Commands

Always pass the full `SDKCONFIG_DEFAULTS` chain: without it, `set-target` regenerates `sdkconfig` from scratch and resets values not in the defaults files.

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

## Post-Flash Self-Check

After flashing, verify the boot before claiming success. `idf.py monitor`
needs a real TTY and fails non-interactively; capture serial output
headlessly instead:

```bash
python3 - <<'EOF'
import serial, time
ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
end = time.time() + 30
data = b""
while time.time() < end:
    chunk = ser.read(4096)
    if chunk: data += chunk
ser.close()
open("/tmp/boot.log", "wb").write(data)
EOF
```

Then check the capture for these signatures, generic to ESP-IDF/FreeRTOS
and Matter/Thread, so they hold even as app code changes:

- No `abort()`, `Guru Meditation`, `Backtrace`, or assert failure anywhere.
- No repeated boot banner within the window; that means a boot-time crash
  loop (e.g. an endpoint-creation failure right after the previous one
  succeeded, or missing/invalid init preventing steady state).
- No `E (` (ESP_LOGE) lines during startup, especially around Matter's
  data model / endpoint / cluster init.
- Thread/Matter reaches a steady state within ~15-20s: either a
  commissioning invite gets printed (not yet commissioned) or the device
  attaches/reconnects on an existing fabric (already commissioned).

This is everything Claude Code can verify unattended. It cannot see LED
output, so mode/color changes always need a human at the strip (see
above).

## Architecture

### Matter Data Model (`app_main.cpp`)

Two endpoints:

1. **Extended Color Light** (ep 1): `OnOff`, `LevelControl`, `ColorControl`
   (color temp + CIE xy), `ModeSelect` (LED animation modes, see `modes` in
   `led.cpp`). Two custom attrs piggy-backed on LevelControl IDs:
   `OnTransitionTime` → animation **speed**, `OffTransitionTime` → **mode
   modification** (both 10ths-of-a-second in the GUI, ÷10 before storing as
   `uint8_t`). Identify on the temp endpoint resets both to default (1280).
2. **Temperature Sensor** (ep 2): internal temp sensor, reported every 30s.

### LED Driver (`led.h`, `led.cpp`)

`class led` owns the RMT LED strip handle + a 30fps FreeRTOS task
(`led_effect_task`): `handle_transitions()` (fade power/brightness/color)
then the current `Mode::render`. Matter attribute writes
(`power_dest`/`brightness_dest`/`rgb_dest`/...) are non-blocking; the
effect task picks them up next frame. Defaults: GPIO 2, 50× WS2812.

**Adding a mode:** write a `mode_render_fn_t` (`void (led_render_ctx&)`) in
`led.cpp`, append `Mode{id, name, supports_color, fn}` to `modes`, no
`led.h` edit needed. `mode_select_driver.h`'s `DynamicSupportedModesManager`
auto-publishes `modes` over Matter. `supports_color` gates whether
`app_driver_light_set_solid_mode_if_color_not_supported()` forces Solid
mode back on when color temp/XY changes during a non-color mode.

### Color Pipeline (`color_format.h/cpp`, `led_strip_helper.h/cpp`)

- CCT (Mired) → RGB: logarithmic algorithm (`cct_to_rgb`)
- CIE xy → sRGB: iterative gamut mapping (`xy_to_rgb`)
- `led_strip_helper`: `esp_idf led_strip` wrapped with `CRGB` helpers +
  `maintain_fps` / `fadeToColor` / `fadeToU8`

### FastLED Port (`components/fastled/`)

Minimal FastLED subset for ESP-IDF: `CRGB`/`CHSV` (`pixeltypes.h`), lib8tion
math (`beatsin8/16/88`, `scale8`, `map8`, `qadd8`, `qsub8`, `random8`),
`hsv2rgb_rainbow`.

### Status LED (`status_led.h/cpp`)

ESP32-C6 onboard RGB LED (GPIO 8, BSP): **Red** booting/not commissioned,
**Green** Thread connected or fabric committed, **Orange blink** incoming
Matter attribute update.

### Key Config Knobs

| sdkconfig defaults file | Purpose |
| --- | --- |
| `sdkconfig.defaults` | Common (BLE, partitions, OTA, button timing) |
| `sdkconfig.defaults.esp32c6` | Target selection |
| `sdkconfig.defaults.c6_thread` | Thread-only, no WiFi STA |
| `sdkconfig.defaults.c6_wifi_thread` | Thread + WiFi concurrent |

`partitions.csv` defines the custom partition layout (OTA-enabled, 4 MB flash).

`CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT` counts the root endpoint (ep 0) too, not just app-level ones; undercounting aborts at boot. Thread-only: root + light + temp = 3. WiFi+Thread: + secondary network interface = 4.
