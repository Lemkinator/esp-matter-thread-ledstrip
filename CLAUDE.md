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

Multiple ESP-IDF versions live side-by-side under `~/esp/<version>/esp-idf`
(`ls ~/esp/` to see what's installed). esp-matter and ESP-IDF are
version-paired — see "SDK versions and local patches" below — so source the
ESP-IDF version that matches the esp-matter branch currently checked out at
`~/.espressif/esp-matter`, not just whichever is newest:

```bash
. ~/esp/<version>/esp-idf/export.sh
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
headlessly with pyserial instead (plain `cat`/`stty` on the port reads
nothing here, this CDC-ACM device needs the line-coding handshake
pyserial's `Serial()` sends on open):

```bash
python3 -c "
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=1); t = time.time() + 30; d = b''
while time.time() < t: d += s.read(4096)
open('/tmp/boot.log', 'wb').write(d)
"
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
   `led_core.cpp`). Two custom attrs piggy-backed on LevelControl IDs:
   `OnTransitionTime` → animation **speed**, `OffTransitionTime` → **mode
   modification** (both 10ths-of-a-second in the GUI, ÷10 before storing as
   `uint8_t`). Identify on the temp endpoint resets both to default (1280).
2. **Temperature Sensor** (ep 2): internal temp sensor, reported every 30s.

### LED Driver (`main/led/`)

`class led` (in `led_core.cpp`) owns the RMT LED strip handle + a 30fps
FreeRTOS task (`led_effect_task`): `handle_transitions()` (fade
power/brightness/color) then the current `led_mode_t::render`. Matter
attribute writes (`power_dest`/`brightness_dest`/`rgb_dest`/...) are
non-blocking; the effect task picks them up next frame. Defaults: GPIO 2,
50× WS2812.

The `std::vector<led_mode_t> modes` registry lives at the top of
`led_core.cpp`. Render functions themselves are split by category into
`led_modes_ambient.cpp`, `led_modes_flash.cpp`, `led_modes_motion.cpp`;
`led_modes.h` forward-declares them, `led_render_helpers.h` holds the
shared `commit_pixel`/`finish_frame` helpers, and `fast_trig.h` holds
`fast_sinf`/`fast_cosf` — ESP32-C6 has no hardware FPU, so prefer these
(lib8tion `sin8`/`cos8` table lookups) over libm `sinf`/`cosf` in any
per-pixel math; drop-in, same -1..1 range.

**Adding a mode:** write a `mode_render_fn_t` (`void (led_render_ctx&)`) in
the appropriate `led_modes_*.cpp`, forward-declare it in `led_modes.h`,
append `led_mode_t{id, name, supports_color, fn}` to `modes` in
`led_core.cpp`, no `led.h` edit needed. `mode_select_driver.h`'s
`DynamicSupportedModesManager` auto-publishes `modes` over Matter.
`supports_color` gates whether
`app_driver_light_set_solid_mode_if_color_not_supported()` forces Solid
mode back on when color temp/XY changes during a non-color mode.

### Color Pipeline (`main/led/`)

- CCT (Mired) → RGB: logarithmic algorithm (`cct_to_rgb`)
- CIE xy → sRGB: iterative gamut mapping (`xy_to_rgb`)
- `led_strip_helper`: `esp_idf led_strip` wrapped with `CRGB` helpers +
  `maintain_fps` / `fadeToColor` / `fadeToU8`

### FastLED Port (`components/fastled/`)

Minimal FastLED subset for ESP-IDF: `CRGB`/`CHSV` (`pixeltypes.h`), lib8tion
math (`beatsin8/16/88`, `scale8`, `map8`, `qadd8`, `qsub8`, `random8`),
`hsv2rgb_rainbow`.

### Status LED (`main/driver/status_led.h/cpp`)

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

`partitions.csv` defines the custom partition layout (OTA-enabled, 16 MB
flash — the board's actual chip size). `nvs` (commissioning/fabric data)
stays at a fixed offset/size across flash-size or partition-table changes
so re-flashing never wipes pairing; `ota_0`/`ota_1` are 4 MB each, leaving
the upper ~8 MB of the chip unpartitioned for future growth.

Compiler optimization level is per-variant Kconfig, not a `CMakeLists.txt`
override: `c6_thread` uses `CONFIG_COMPILER_OPTIMIZATION_PERF=y` (`-O2`,
speed over size — now has flash headroom to afford it); `c6_wifi_thread`
keeps `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (`-Os`). `-O2` alone fails to
compile: GCC's inliner false-positives `-Werror=stringop-truncation` on
connectedhomeip's `CopyString()` (`CHIPMemString.h`) — the exact-size-bound
`strncpy()` there always gets null-terminated on the next line, but not
every inlined call site lets GCC see that (a known GCC limitation; a
source-level `#pragma GCC diagnostic ignored` there fixes some call sites
but not all). Fixed via `CMakeLists.txt`'s own
`-Wno-error=stringop-truncation`, which downgrades it to a non-fatal
warning without touching vendored code — same pattern IDF's own
`project_include.cmake` already uses for a few other warnings.

`CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT` counts the root endpoint
(ep 0) too, not just app-level ones; undercounting aborts at boot.
Thread-only: root + light + temp = 3. WiFi+Thread: + secondary network
interface = 4.

### SDK versions and local patches

esp-matter (`~/.espressif/esp-matter`) and ESP-IDF are version-paired: each
esp-matter branch's README states the ESP-IDF version it expects, and
mismatches are prone to build or runtime breakage. esp-matter ships no
tags, only branches (`release/v1.N`, `main`); prefer the highest numbered
`release/v1.N` branch over `main`, which tracks an in-progress spec
migration and isn't a stable target.

`~/.espressif/esp-matter` carries one local modification on top of
upstream: `patches/esp-matter-kconfig-sec-cert-rename.patch` in this repo,
which must be re-applied after any esp-matter checkout/update. It renames a
Kconfig symbol that collides with an identically-named one in
connectedhomeip's own esp32 Kconfig; see the patch file header for details.
Check first whether upstream has fixed the collision before re-applying —
if `SEC_CERT_DAC_PROVIDER` no longer appears twice across
`components/esp_matter/Kconfig` and
`connectedhomeip/connectedhomeip/config/esp32/components/chip/Kconfig`, the
patch is obsolete and can be dropped.

connectedhomeip's submodules are numerous (~80) and mostly irrelevant to
this ESP32-C6 target — vendor SDKs for other chip families (Infineon, NXP,
TI, ASR, Bouffalo, STM32, Silabs, Qorvo) and host-tool-only libraries
(jsoncpp, libwebsockets, perfetto, editline). Only initialize what the
build actually references: `git grep third_party/` across
`connectedhomeip/connectedhomeip/config/esp32` and
`~/.espressif/esp-matter/components` names them (as of the current
`release/v1.N`: `nlassert`, `nlio`, `nanopb`, a `pigweed` subset,
`uriparser`, and `jsoncpp` transitively via the gn build graph). A plain
`git submodule update --init --depth 1 --recursive` can land the wrong
commit on some submodules if the pinned SHA isn't the shallow-fetchable
tip; verify with `git ls-tree HEAD <path>` vs `git -C <path> rev-parse
HEAD` and re-run `git submodule update --init <path>` (no `--depth`) for
any mismatch.
