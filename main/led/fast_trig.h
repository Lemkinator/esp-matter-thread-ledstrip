#pragma once

#include <stdint.h>

#include "lib8tion.h"

// This chip has no hardware FPU, so libm sinf()/cosf() are software-emulated
// and comparatively expensive to call per-pixel at 30fps. sin8()/cos8()
// (lib8tion, integer table lookup) are much cheaper; these wrap them to take
// radians and return the same -1..1 range so call sites don't otherwise
// change. theta wraps mod 256 (== mod 2*pi) via the int32 cast, same idiom
// lib8tion's own beatsin8/88 use for phase accumulation.
inline float fast_sinf(float radians) {
    constexpr float RAD_TO_U8 = 256.0f / (2.0f * 3.14159265f);
    uint8_t theta = static_cast<uint8_t>(static_cast<int32_t>(radians * RAD_TO_U8));
    return (static_cast<int16_t>(sin8(theta)) - 128) / 127.0f;
}
inline float fast_cosf(float radians) {
    constexpr float RAD_TO_U8 = 256.0f / (2.0f * 3.14159265f);
    uint8_t theta = static_cast<uint8_t>(static_cast<int32_t>(radians * RAD_TO_U8));
    return (static_cast<int16_t>(cos8(theta)) - 128) / 127.0f;
}
