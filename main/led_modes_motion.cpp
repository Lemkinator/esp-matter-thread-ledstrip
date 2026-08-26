#include "led_modes.h"

// ─────────────────────────────────────────────────────────────────────────────
// ☄️ Comet
//   speed → travel speed    4–29 px/s    (128 ≈ 16 px/s → ~3 s to cross strip)
//   mod   → tail length     5–35 px      (128 ≈ 20 px)
// ─────────────────────────────────────────────────────────────────────────────
void comet_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    float travel = static_cast<float>(map8(ctx.speed, 4, 29));
    int tail = map8(ctx.mode_modification, 5, 35);
    int total = n + tail;
    int head = static_cast<int>(fmodf(get_time_s() * travel, static_cast<float>(total)));

    for (int i = 0; i < n; i++) ctx.pixels[i] = CRGB::Black;

    CRGB rgb = ctx.rgb;
    for (int j = 0; j <= tail; j++) {
        int pos = head - j;
        if (pos < 0 || pos >= n) continue;
        float intens = 1.0f - static_cast<float>(j) / static_cast<float>(tail + 1);
        intens = intens * intens;  // quadratic falloff
        ctx.pixels[pos].r = static_cast<uint8_t>(rgb.r * intens);
        ctx.pixels[pos].g = static_cast<uint8_t>(rgb.g * intens);
        ctx.pixels[pos].b = static_cast<uint8_t>(rgb.b * intens);
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

void bounce_render(led_render_ctx& ctx) {
    int led_count = ctx.led_count;
    uint8_t bpm = map8(ctx.speed, 1, 15);
    uint16_t index = beatsin16(bpm, 0, led_count - 1);
    int mod = (ctx.mode_modification * 60) / 255 - 30;
    int speed_deduction = (ctx.speed * 56) / 255;
    uint8_t fade = std::clamp(64 + mod - speed_deduction, 0, 255);
    for (int i = 0; i < led_count; i++) {
        commit_pixel(ctx, i, ctx.pixels[i].fadeToBlackBy(fade));
    }
    ctx.pixels[index] = ctx.rgb;
    ctx.pixels[index].nscale8_video(ctx.brightness);
    commit_pixel(ctx, index, ctx.pixels[index]);
    finish_frame(ctx);
}

void pulse_render(led_render_ctx& ctx) {
    uint8_t bpm = map8(ctx.speed, 1, 48);
    uint8_t bri = beatsin16(bpm, 0, ctx.brightness);
    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, rgb.nscale8_video(bri));
}

// ─────────────────────────────────────────────────────────────────────────────
// 🎭 Theater Chase
//   speed → chase speed    1–30 steps/s    (128 ≈ 15 steps/s)
//   mod   → gap width      2–8 px          (128 ≈ 5 px dark gap)
//   color → lit-segment color
// ─────────────────────────────────────────────────────────────────────────────
void theater_chase_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    int steps_per_s = map8(ctx.speed, 1, 30);
    int gap = map8(ctx.mode_modification, 2, 8);
    const int SEG = 2;
    int period = SEG + gap;
    int step = static_cast<int>(get_time_s() * static_cast<float>(steps_per_s)) % period;

    CRGB rgb = ctx.rgb;
    for (int i = 0; i < n; i++) {
        bool lit = ((i + step) % period) < SEG;
        CRGB c = rgb;
        commit_pixel(ctx, i, c.nscale8_video(lit ? ctx.brightness : 0));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌠 Meteor Shower
//   speed → travel speed    5–35 px/s    (128 ≈ 20 px/s)
//   mod   → count           1–6          (128 ≈ 3–4 simultaneous meteors)
//   color → meteor color
//
//   Each meteor has a slightly different speed — they never perfectly align.
//   Additive blending where they cross looks realistic.
// ─────────────────────────────────────────────────────────────────────────────
void meteor_shower_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float base_travel = static_cast<float>(map8(ctx.speed, 5, 35));
    int n = ctx.led_count;
    int num_meteors = map8(ctx.mode_modification, 1, 6);
    const int TAIL = 8;
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(35);

    for (int m = 0; m < num_meteors; m++) {
        float speed_var = 0.70f + static_cast<float>(m) * 0.13f;
        float offset = static_cast<float>(m) / static_cast<float>(num_meteors);
        float total = static_cast<float>(n + TAIL);
        int head = static_cast<int>(fmodf(t * base_travel * speed_var + offset * total, total));

        for (int j = 0; j <= TAIL; j++) {
            int pos = head - j;
            if (pos < 0 || pos >= n) continue;
            float intens = 1.0f - static_cast<float>(j) / static_cast<float>(TAIL + 1);
            intens = intens * intens;
            CRGB c;
            c.r = static_cast<uint8_t>(rgb.r * intens);
            c.g = static_cast<uint8_t>(rgb.g * intens);
            c.b = static_cast<uint8_t>(rgb.b * intens);
            ctx.pixels[pos] += c;  // additive — crossing meteors blend naturally
        }
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌊 Color Flow
//   speed → gradient drift speed    still ↔ flowing    (128 ≈ gentle drift)
//   mod   → hue span across strip   20–255             (128 ≈ ~137 hue units wide)
//   color → anchor/base hue
//
//   A living color gradient — spatially wider at high mod, flowing at high speed.
// ─────────────────────────────────────────────────────────────────────────────
void color_flow_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 3, 60)) / 100.0f;  // 0.03–0.60 hue-rotations/s
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t span = map8(ctx.mode_modification, 20, 255);
    uint8_t t_hue = static_cast<uint8_t>(t * 30.0f * sf);  // hue shifts over time

    for (int i = 0; i < n; i++) {
        uint8_t hue = base_hue + t_hue + static_cast<uint8_t>(static_cast<float>(i) / static_cast<float>(n) * static_cast<float>(span));
        CRGB c;
        hsv2rgb_rainbow(CHSV(hue, 240, ctx.brightness), c);
        commit_pixel(ctx, i, c);
    }
    finish_frame(ctx);
}
