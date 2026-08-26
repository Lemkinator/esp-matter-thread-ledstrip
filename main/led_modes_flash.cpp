#include "led_modes.h"

// ─────────────────────────────────────────────────────────────────────────────
// ✨ Twinkle Stars
//   speed → star lifetime    long-lived ↔ brief pops   (128 ≈ moderate)
//   mod   → star density     sparse ↔ dense field      (128 ≈ moderate)
// ─────────────────────────────────────────────────────────────────────────────
void twinkle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t fade_amount = map8(ctx.speed, 3, 21);
    uint8_t spawn_prob = map8(ctx.mode_modification, 2, 34);
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) {
        ctx.pixels[i].fadeToBlackBy(fade_amount);
        if (random8() < spawn_prob) ctx.pixels[i] = rgb;
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🫁 Breathing  — BPM-mapped, biological 4-phase pattern (inhale/hold/exhale/rest)
//   speed → breathing rate    3–20 BPM   (128 ≈ 11 BPM ≈ 5.5 s/breath)
//   mod   → hold + rest       5–30 %     (128 ≈ 17 % — adds meditative pause)
// ─────────────────────────────────────────────────────────────────────────────
void breathing_render(led_render_ctx& ctx) {
    float bpm = static_cast<float>(map8(ctx.speed, 3, 20));
    float hold_pct = static_cast<float>(map8(ctx.mode_modification, 5, 30)) / 100.0f;

    float cycle = fmodf(get_time_s() * bpm / 60.0f, 1.0f);

    float inhale_end = 0.40f - hold_pct * 0.30f;
    float hold_end = inhale_end + hold_pct;
    float exhale_end = hold_end + 0.40f - hold_pct * 0.30f;
    // [exhale_end → 1.0] = dark rest

    float bri_f;
    if (cycle < inhale_end) {
        float p = cycle / inhale_end;
        bri_f = p * p * (3.0f - 2.0f * p);  // smoothstep up
    } else if (cycle < hold_end) {
        bri_f = 1.0f;
    } else if (cycle < exhale_end) {
        float p = (cycle - hold_end) / (exhale_end - hold_end);
        bri_f = 1.0f - p * p * (3.0f - 2.0f * p);  // smoothstep down
    } else {
        bri_f = 0.0f;
    }

    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count,
                      rgb.nscale8_video(static_cast<uint8_t>(bri_f * ctx.brightness)));
}

// ─────────────────────────────────────────────────────────────────────────────
// 💡 Neon Sign
//   speed → buzz BPM        30–200     (128 ≈ 115 BPM — audible-frequency buzz)
//   mod   → instability     subtle ↔ heavy hum   (128 ≈ faint steady hum)
//   color → tube color
//
//   Three inharmonic oscillators create an organic, non-repeating buzz.
//   The tube stays mostly bright — neon doesn't dim much, it just isn't perfect.
// ─────────────────────────────────────────────────────────────────────────────
void neon_render(led_render_ctx& ctx) {
    uint8_t buzz_depth = map8(ctx.mode_modification, 2, 60);

    uint16_t bpm_a = static_cast<uint16_t>(map8(ctx.speed, 30, 200)) * 256;
    uint16_t bpm_b = static_cast<uint16_t>(map8(ctx.speed, 38, 254)) * 256;  // ×1.27
    uint16_t bpm_c = static_cast<uint16_t>(map8(ctx.speed, 19, 128)) * 256;  // ×0.64

    uint8_t v1 = beatsin8(bpm_a, 0, buzz_depth, 0, 0);
    uint8_t v2 = beatsin8(bpm_b, 0, buzz_depth, 0, 85);
    uint8_t v3 = beatsin8(bpm_c, 0, buzz_depth, 0, 170);

    // Weighted sum — v1 dominates, v2/v3 add texture
    uint8_t dip = static_cast<uint8_t>((v1 * 50u + v2 * 33u + v3 * 17u) / 100u);
    uint8_t bri = qsub8(ctx.brightness, dip);

    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, rgb.nscale8_video(bri));
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌈 Plasma
//   speed → wave drift speed    static ↔ flowing    (128 ≈ moderate)
//   mod   → hue range           20–255 hue units    (128 ≈ wide colorful spread)
//   color → center/anchor hue
//
//   Three non-harmonic sine waves produce a continuously morphing color field.
// ─────────────────────────────────────────────────────────────────────────────
void plasma_render(led_render_ctx& ctx) {
    float t = get_time_s();
    float sf = static_cast<float>(map8(ctx.speed, 5, 80)) / 100.0f;  // 0.05–0.80
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t hue_range = map8(ctx.mode_modification, 20, 255);

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n);
        float v = sinf(fi * 6.28318f + t * sf) + sinf(fi * 13.56637f - t * sf * 0.73f) + sinf((fi + t * sf * 0.41f) * 9.42477f);
        v = (v / 3.0f + 1.0f) / 2.0f;  // normalize 0–1

        CRGB c;
        hsv2rgb_rainbow(CHSV(base_hue + static_cast<uint8_t>(v * hue_range), 240, ctx.brightness), c);
        commit_pixel(ctx, i, c);
    }
    finish_frame(ctx);
}

void rainbow_render(led_render_ctx& ctx) {
    uint8_t rotation_speed = map8(ctx.speed, 0, 20);
    uint8_t base_hue = beat8(rotation_speed);
    uint8_t delta_hue = map8(ctx.mode_modification, 0, 12);
    for (int i = 0; i < static_cast<int>(ctx.led_count); i++) {
        uint8_t pixel_hue = base_hue + (i * delta_hue);
        CHSV hsv(pixel_hue, 255, ctx.brightness);
        CRGB rgb;
        hsv2rgb_rainbow(hsv, rgb);
        commit_pixel(ctx, i, rgb);
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// ✨ Sparkle
//   speed → spark lifetime    long-lived ↔ instant pops    (128 ≈ crisp but brief)
//   mod   → sparks per frame  1–15                         (128 ≈ 8)
//   color → spark color (automatically pushed brighter/whiter for camera-flash pop)
// ─────────────────────────────────────────────────────────────────────────────
void sparkle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t fade_rate = map8(ctx.speed, 15, 80);
    int spawn_rate = map8(ctx.mode_modification, 1, 15);

    // Push each new spark toward white — the brief overexposure sells the flash
    CRGB spark;
    spark.r = qadd8(ctx.rgb.r, 80);
    spark.g = qadd8(ctx.rgb.g, 80);
    spark.b = qadd8(ctx.rgb.b, 80);

    // Spawn before fade+output so sparks appear at full brightness this frame
    for (int s = 0; s < spawn_rate; s++) ctx.pixels[random8(n)] = spark;

    for (int i = 0; i < n; i++) {
        ctx.pixels[i].fadeToBlackBy(fade_rate);
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// ⚡ Strobe
//   speed → flash rate   1–20 Hz    (128 ≈ 10 Hz)
//   mod   → on-duty      5–40 %     (128 ≈ 22 %)
// ─────────────────────────────────────────────────────────────────────────────
void strobe_render(led_render_ctx& ctx) {
    int rate_hz = map8(ctx.speed, 1, 20);
    int duty_pct = map8(ctx.mode_modification, 5, 40);
    float period = 1.0f / static_cast<float>(rate_hz);
    bool on = fmodf(get_time_s(), period) < (period * duty_pct / 100.0f);
    CRGB rgb = ctx.rgb;
    led_strip_set_all(ctx.handle, ctx.led_count, on ? rgb.nscale8_video(ctx.brightness) : CRGB::Black);
}

// ─────────────────────────────────────────────────────────────────────────────
// ⚡ Lightning  — reworked timing + flash-count distribution
//   speed → strike frequency    rare ↔ stormy   (128 ≈ 1 strike/2.4 s avg)
//   mod   → bolts per strike    1–3             (128 ≈ 2)
//   Flash distribution: 30 % single · 40 % double · 20 % triple · 10 % quad
// ─────────────────────────────────────────────────────────────────────────────
void lightning_render(led_render_ctx& ctx) {
    float t = get_time_s();
    int n = ctx.led_count;
    const float SLOT_S = 0.5f;    // 500 ms decision window
    const float FLASH_W = 0.06f;  // each sub-flash: 30 ms

    uint32_t slot = static_cast<uint32_t>(t / SLOT_S);
    float slot_phase = fmodf(t, SLOT_S) / SLOT_S;

    // Per-slot xorshift32 RNG
    uint32_t rng = slot * 2246822519u ^ 0x9E3779B9u;
    auto rnd = [&]() -> uint32_t {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    rnd();  // initial mix

    // Strike probability per slot: speed=0 → 2 %, 128 → 21 %, 255 → 40 %
    bool has_strike = ((rnd() % 100) < static_cast<uint32_t>(map8(ctx.speed, 2, 40)));

    // Flash-count: 30/40/20/10 distribution
    int flash_count = 1;
    if (has_strike) {
        uint32_t roll = rnd() % 100;
        if (roll < 30)
            flash_count = 1;
        else if (roll < 70)
            flash_count = 2;
        else if (roll < 90)
            flash_count = 3;
        else
            flash_count = 4;
    }

    // Sub-flash positions within the slot (as fractions, all in first 30 % of slot)
    static const float FLASH_T[4][4] = {
        {0.05f, -1.0f, -1.0f, -1.0f},  // 1 flash
        {0.05f, 0.13f, -1.0f, -1.0f},  // 2 flashes
        {0.05f, 0.11f, 0.18f, -1.0f},  // 3 flashes
        {0.05f, 0.10f, 0.16f, 0.23f},  // 4 flashes
    };

    bool flashing = false;
    if (has_strike) {
        for (int f = 0; f < flash_count; f++) {
            float ft = FLASH_T[flash_count - 1][f];
            if (ft < 0.0f) break;
            if (slot_phase >= ft && slot_phase < ft + FLASH_W) {
                flashing = true;
                break;
            }
        }
    }

    uint8_t fade_rate = map8(ctx.speed, 10, 55);
    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(fade_rate);

    if (flashing) {
        CRGB bolt;
        bolt.r = qadd8(ctx.rgb.r, 80);
        bolt.g = qadd8(ctx.rgb.g, 80);
        bolt.b = qadd8(ctx.rgb.b, 80);

        int num_bolts = map8(ctx.mode_modification, 1, 3);

        // Bolt geometry seeded separately — stable across all sub-flashes of this slot
        uint32_t bolt_rng = slot * 1664525u + 1013904223u;
        auto brnd = [&]() -> uint32_t {
            bolt_rng ^= bolt_rng << 13;
            bolt_rng ^= bolt_rng >> 17;
            bolt_rng ^= bolt_rng << 5;
            return bolt_rng;
        };
        for (int b = 0; b < num_bolts; b++) {
            int pos = static_cast<int>(brnd() % n);
            int len = 3 + static_cast<int>(brnd() % 12);
            for (int i = pos; i < std::min(pos + len, n); i++) ctx.pixels[i] = bolt;
        }
    }

    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}
