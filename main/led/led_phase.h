#pragma once

#include <stdint.h>

#include "lib8tion.h"

// Integer phase accumulator for time-based animation, replacing a
// float-seconds timebase. A float has a 24-bit mantissa, so its resolution
// in seconds shrinks as milliseconds-since-boot grows (2ms error at 4.7h,
// ~1 frame at 3 days) — every mode built on it drifts more the longer the
// device runs. This accumulator uses only integer multiply/shift on the raw
// millisecond counter, so its precision is constant for any uptime and
// wraps correctly across the uint32 millis rollover (~49.7 days), same as
// lib8tion's own beat8/beat16/beat88.
//
// A cycle is 65536 phase units. rate_q32 is cycles/ms scaled by 2^32
// (Q0.32 fixed point); phase16() truncates ms*rate_q32 to 32 bits (exact
// modular reduction of the true 64-bit product, since C's uint32
// multiplication already wraps mod 2^32) and takes the top 16 bits.
//
// Use sinf16()/sin16() instead of libm sinf() — this chip has no hardware
// FPU. sin16 (65536 steps/cycle) replaces the old fast_sinf()'s sin8 (256
// steps/cycle), which quantized slow oscillators (e.g. Lava Lamp at low
// speed) into a stall-then-jump pattern from the moment the device boots.

inline uint32_t phase_rate_from_hz(float cycles_per_s) {
    return static_cast<uint32_t>((cycles_per_s / 1000.0f) * 4294967296.0f);
}

// Old call sites read `t * rate_rad_s + offset` into fast_sinf(); this is
// the drop-in equivalent for `rate_rad_s` (offset becomes rad_to_phase16()
// added onto the phase16() result, see led_modes_*.cpp).
inline uint32_t phase_rate_from_rad_s(float radians_per_s) {
    constexpr float INV_TWO_PI = 1.0f / (2.0f * 3.14159265f);
    return phase_rate_from_hz(radians_per_s * INV_TWO_PI);
}

inline uint16_t phase16(uint32_t ms, uint32_t rate_q32) {
    return static_cast<uint16_t>((ms * rate_q32) >> 16);
}

// Converts a fixed radian offset (phase constants baked into render code,
// e.g. 2.094f for 120 degrees) into the same 0..65535 phase space, so it can
// be added directly to a phase16() result.
inline uint16_t rad_to_phase16(float radians) {
    constexpr float RAD_TO_PHASE16 = 65536.0f / (2.0f * 3.14159265f);
    return static_cast<uint16_t>(static_cast<int32_t>(radians * RAD_TO_PHASE16));
}

inline float sinf16(uint16_t phase) {
    return sin16(phase) / 32767.0f;
}

inline float cosf16(uint16_t phase) {
    return sin16(static_cast<uint16_t>(phase + 16384)) / 32767.0f;
}

// 0..1 sawtooth position within the cycle.
inline float frac16(uint16_t phase) {
    return phase / 65536.0f;
}

// Inverse of frac16(): a plain 0..1 fraction (not radians — use
// rad_to_phase16() for those) as a phase offset.
inline uint16_t frac_to_phase16(float frac) {
    return static_cast<uint16_t>(frac * 65536.0f);
}
