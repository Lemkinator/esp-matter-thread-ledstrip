#pragma once

#include "led.h"

// Forward declarations — implementation detail of the led_modes_*.cpp files
void solid_render(led_render_ctx& ctx);
void demo_render(led_render_ctx& ctx);
void dynamic_demo_render(led_render_ctx& ctx);
void relax_render(led_render_ctx& ctx);
void fireplace_render(led_render_ctx& ctx);
void candle_render(led_render_ctx& ctx);
void lava_render(led_render_ctx& ctx);
void ocean_render(led_render_ctx& ctx);
void aurora_render(led_render_ctx& ctx);
void twinkle_render(led_render_ctx& ctx);
void breathing_render(led_render_ctx& ctx);
void comet_render(led_render_ctx& ctx);
void sunrise_render(led_render_ctx& ctx);
void neon_render(led_render_ctx& ctx);
void plasma_render(led_render_ctx& ctx);
void meteor_shower_render(led_render_ctx& ctx);
void forest_render(led_render_ctx& ctx);
void color_flow_render(led_render_ctx& ctx);
void bounce_render(led_render_ctx& ctx);
void pulse_render(led_render_ctx& ctx);
void theater_chase_render(led_render_ctx& ctx);
void rainbow_render(led_render_ctx& ctx);
void sparkle_render(led_render_ctx& ctx);
void strobe_render(led_render_ctx& ctx);
void lightning_render(led_render_ctx& ctx);

// Shared by every render function to cut the ctx.handle repetition below.
inline void commit_pixel(led_render_ctx& ctx, uint32_t index, CRGB color) {
    led_strip_set_pixel(ctx.handle, index, color);
}
inline void finish_frame(led_render_ctx& ctx) {
    led_strip_refresh(ctx.handle);
}

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
