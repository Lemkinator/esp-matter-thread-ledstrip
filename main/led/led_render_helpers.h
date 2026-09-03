#pragma once

#include "led.h"

// Shared by every render function to cut the ctx.handle repetition below.
inline void commit_pixel(led_render_ctx& ctx, uint32_t index, CRGB color) {
    led_strip_set_pixel(ctx.handle, index, color);
}
inline void finish_frame(led_render_ctx& ctx) {
    led_strip_refresh(ctx.handle);
}
