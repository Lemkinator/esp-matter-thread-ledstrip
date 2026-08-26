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
