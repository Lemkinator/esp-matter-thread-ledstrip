#include "led_modes.h"
#include "led_render_helpers.h"
#include "led_phase.h"

void relax_render(led_render_ctx& ctx) {
    CRGB dot_add = CRGB(16, 8, 4);
    CRGB bg = ctx.rgb;
    bg.nscale8_video(220);
    CRGB limit = bg + CRGB(50, 50, 50);
    uint8_t num_dots = map8(ctx.mode_modification, 1, 8);
    uint16_t base_bpm_88 = ctx.speed + 1;
    for (int i = 0; i < ctx.led_count; i++) {
        fadeToColor(ctx.pixels[i], bg, 1);
        CRGB pixel = ctx.pixels[i];
        commit_pixel(ctx, i, pixel.nscale8_video(ctx.brightness));
    }
    for (int i = 0; i < num_dots; i++) {
        uint16_t dot_bpm = base_bpm_88 + (i * ctx.speed / 3);
        uint16_t pos = beatsin88(dot_bpm, 0, ctx.led_count - 1, 0, i * 65536 / num_dots);
        ctx.pixels[pos] += dot_add;
        if (ctx.pixels[pos].r > limit.r) ctx.pixels[pos].r = limit.r;
        if (ctx.pixels[pos].g > limit.g) ctx.pixels[pos].g = limit.g;
        if (ctx.pixels[pos].b > limit.b) ctx.pixels[pos].b = limit.b;
        CRGB pixel = ctx.pixels[pos];
        commit_pixel(ctx, pos, pixel.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🔥 Fireplace
//   speed → sparking energy    50–200   (128 ≈ 125)
//   mod   → flame height       high mod = tall; low mod = low embers
// ─────────────────────────────────────────────────────────────────────────────
void fireplace_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint8_t cooling = map8(ctx.mode_modification, 55, 23);  // low mod→more cooling→shorter flames
    uint8_t sparking = map8(ctx.speed, 50, 200);

    for (int i = 0; i < n; i++) {
        uint8_t cool = random8(0, ((cooling * 10) / n) + 2);
        ctx.pixels[i].r = qsub8(ctx.pixels[i].r, cool);
    }
    for (int i = n - 1; i >= 2; i--) {
        ctx.pixels[i].r = (static_cast<uint16_t>(ctx.pixels[i - 1].r) + static_cast<uint16_t>(ctx.pixels[i - 2].r) + static_cast<uint16_t>(ctx.pixels[i - 2].r)) / 3;
    }
    if (random8() < sparking) {
        int y = random8(0, 7);
        if (y < n) ctx.pixels[y].r = qadd8(ctx.pixels[y].r, random8(160, 255));
    }
    CRGB hot = ctx.rgb;
    for (int i = 0; i < n; i++) {
        uint8_t h = ctx.pixels[i].r;
        CRGB c;
        if (h < 128) {
            uint8_t t2 = h << 1;
            c.r = scale8(hot.r, t2);
            c.g = scale8(hot.g, t2);
            c.b = scale8(hot.b, t2);
        } else {
            uint8_t t2 = (h - 128) << 1;
            c.r = hot.r + scale8(255 - hot.r, t2);
            c.g = hot.g + scale8(255 - hot.g, t2);
            c.b = hot.b + scale8(255 - hot.b, t2);
        }
        c.nscale8_video(ctx.brightness);
        commit_pixel(ctx, i, c);
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🕯️ Candle  — now uses three inharmonic beatsin8 oscillators
//   speed → flicker rate BPM    20–120     (128 ≈ 70 BPM)
//   mod   → glow corona radius  pinpoint ↔ full-strip soft fill
// ─────────────────────────────────────────────────────────────────────────────
void candle_render(led_render_ctx& ctx) {
    int n = ctx.led_count;

    // Three BPMs with irrational ratios — ensures aperiodic, organic flicker
    uint16_t bpm_a = static_cast<uint16_t>(map8(ctx.speed, 20, 120)) * 256;  // base
    uint16_t bpm_b = static_cast<uint16_t>(map8(ctx.speed, 26, 154)) * 256;  // ×1.28
    uint16_t bpm_c = static_cast<uint16_t>(map8(ctx.speed, 13, 78)) * 256;   // ×0.65

    float f1 = beatsin8(bpm_a, 0, 255, 0, 0) / 255.0f;
    float f2 = beatsin8(bpm_b, 0, 255, 0, 85) / 255.0f;
    float f3 = beatsin8(bpm_c, 0, 255, 0, 170) / 255.0f;
    // Keep candle in upper 65–100 % brightness range (real candles are always lit)
    uint8_t bri = static_cast<uint8_t>((0.65f + (f1 * 0.5f + f2 * 0.3f + f3 * 0.2f) * 0.35f) * ctx.brightness);

    float sigma = 1.5f + (ctx.mode_modification / 255.0f) * (n * 0.45f);
    float inv_2sig2 = 1.0f / (2.0f * sigma * sigma);
    int center = n / 2;
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) {
        float dist = static_cast<float>(i - center);
        float falloff = expf(-dist * dist * inv_2sig2);
        CRGB c = rgb;
        commit_pixel(ctx, i, c.nscale8_video(static_cast<uint8_t>(bri * falloff)));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🫧 Lava Lamp
//   speed → blob drift speed    0.08–1.0   (128 ≈ gentle viscous movement)
//   mod   → blob count          2–5        (128 ≈ 3)
// ─────────────────────────────────────────────────────────────────────────────
void lava_render(led_render_ctx& ctx) {
    float sf = static_cast<float>(map8(ctx.speed, 8, 100)) / 100.0f;  // 0.08–1.0 (always moving)
    int n = ctx.led_count;
    int num_blobs = map8(ctx.mode_modification, 2, 5);
    CRGB rgb = ctx.rgb;

    float blob_pos[5], blob_size[5];
    for (int b = 0; b < num_blobs; b++) {
        uint16_t pos_phase = phase16(ctx.ms, phase_rate_from_rad_s((0.4f + b * 0.15f) * sf)) + rad_to_phase16(b * 2.094f);
        blob_pos[b] = (sinf16(pos_phase) + 1.0f) / 2.0f;
        uint16_t size_phase = phase16(ctx.ms, phase_rate_from_rad_s(0.09f * sf)) + rad_to_phase16(b * 1.732f);
        blob_size[b] = 0.15f + sinf16(size_phase) * 0.05f;
    }

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float total = 0.0f;
        for (int b = 0; b < num_blobs; b++) {
            float dist = fabsf(fi - blob_pos[b]);
            if (dist > 0.5f) dist = 1.0f - dist;  // wrap-around continuity
            float contrib = std::max(0.0f, 1.0f - dist / blob_size[b]);
            total += contrib * contrib * contrib;  // cubic: defined edges, soft center
        }
        CRGB c = rgb;
        commit_pixel(ctx, i, c.nscale8_video(static_cast<uint8_t>(std::min(total, 1.0f) * ctx.brightness)));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌊 Ocean
//   speed → wave travel speed    0.5–4.0   (128 ≈ 2.25)
//   mod   → wave layers          1–3       (128 ≈ 2)
// ─────────────────────────────────────────────────────────────────────────────
void ocean_render(led_render_ctx& ctx) {
    float spd = static_cast<float>(map8(ctx.speed, 5, 40)) / 10.0f;  // 0.5–4.0
    int n = ctx.led_count;
    int num_waves = map8(ctx.mode_modification, 1, 3);
    CRGB rgb = ctx.rgb;

    float wave_freq[3];
    uint16_t wave_time_phase[3];
    for (int w = 0; w < num_waves; w++) {
        wave_freq[w] = 1.0f + w * 0.8f;
        float wspd = spd * (1.0f + w * 0.4f);
        wave_time_phase[w] = phase16(ctx.ms, phase_rate_from_rad_s(wspd));
    }

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float val = 0.0f;
        for (int w = 0; w < num_waves; w++) {
            uint16_t phase = rad_to_phase16(fi * 6.28318f * wave_freq[w] + w * 2.094f) - wave_time_phase[w];  // 120° apart — no dead-band nulls
            val += sinf16(phase);
        }
        val = (val / static_cast<float>(num_waves) + 1.0f) / 2.0f;
        val = val * val;  // accentuate bright crests, deepen troughs
        CRGB c = rgb;
        commit_pixel(ctx, i, c.nscale8_video(static_cast<uint8_t>(val * ctx.brightness)));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌌 Aurora
//   speed → curtain drift speed    frozen shimmer ↔ active curtain  (128 ≈ gentle)
//   mod   → hue spread             10–147 hue units                 (128 ≈ 78)
// ─────────────────────────────────────────────────────────────────────────────
void aurora_render(led_render_ctx& ctx) {
    float sf = static_cast<float>(map8(ctx.speed, 5, 80)) / 100.0f;  // 0.05–0.80
    int n = ctx.led_count;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t spread = map8(ctx.mode_modification, 10, 147);

    uint16_t t1_phase = phase16(ctx.ms, phase_rate_from_rad_s(0.40f * sf));
    uint16_t t2_phase = phase16(ctx.ms, phase_rate_from_rad_s(0.65f * sf));  // subtracted below (w2 runs backward)
    uint16_t t3_phase = phase16(ctx.ms, phase_rate_from_rad_s(0.22f * sf));

    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        float w1 = sinf16(rad_to_phase16(fi * 3.14159f) + t1_phase);
        float w2 = sinf16(rad_to_phase16(fi * 6.28318f + 1.5f) - t2_phase);
        float w3 = sinf16(rad_to_phase16(fi * 1.88495f + 3.0f) + t3_phase);

        float brightness = ((w1 + w2 * 0.5f + w3 * 0.3f) / 1.8f + 1.0f) / 2.0f;
        brightness = brightness * brightness;  // sparse dark gaps between curtains

        float hue_shift = (w1 * 0.6f + w2 * 0.4f) * static_cast<float>(spread);
        uint8_t hue = base_hue + static_cast<int8_t>(hue_shift);
        CRGB c;
        hsv2rgb_rainbow(CHSV(hue, 220, static_cast<uint8_t>(brightness * ctx.brightness)), c);
        commit_pixel(ctx, i, c);
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌿 Forest
//   speed → breeze speed / dapple movement    (128 ≈ gentle rustle)
//   mod   → light-spot count   2–10           (128 ≈ 6 spots)
//   color → spot color  (warm green-gold recommended, e.g. CRGB(180, 220, 60))
//
//   Uses beatsin16 for positions (spot drifts with breeze) and beatsin8 for
//   per-spot brightness breathing — same approach as your relax_render.
// ─────────────────────────────────────────────────────────────────────────────
void forest_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint16_t bpm_base = static_cast<uint16_t>(map8(ctx.speed, 5, 40));  // 5–40 BPM
    int num_spots = map8(ctx.mode_modification, 2, 10);
    CRGB rgb = ctx.rgb;

    for (int i = 0; i < n; i++) ctx.pixels[i].fadeToBlackBy(20);

    for (int s = 0; s < num_spots; s++) {
        uint16_t spot_bpm = (bpm_base + static_cast<uint16_t>(s)) * 256;         // accum88
        uint8_t ph_pos = static_cast<uint8_t>(static_cast<uint16_t>(s) * 256 / num_spots);  // spread phases
        uint8_t ph_bri = ph_pos + 64;

        // Position oscillates with breeze, brightness breathes independently
        int pos = beatsin16(spot_bpm, 0, n - 1, 0, static_cast<uint16_t>(ph_pos) * 256);
        uint8_t bright = beatsin8(spot_bpm, 30, 210, 0, ph_bri);

        // Soft triangle falloff (±3 px) — quick, no expf needed
        const int W = 3;
        for (int i = pos - W; i <= pos + W; i++) {
            if (i < 0 || i >= n) continue;
            uint8_t falloff = 255 - static_cast<uint8_t>(static_cast<uint16_t>(abs(i - pos)) * 255 / (W + 1));
            CRGB c = rgb;
            c.nscale8_video(scale8(bright, falloff));
            ctx.pixels[i] += c;
        }
    }
    for (int i = 0; i < n; i++) {
        CRGB px = ctx.pixels[i];
        commit_pixel(ctx, i, px.nscale8_video(ctx.brightness));
    }
    finish_frame(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// 🌅 Sunrise
//   speed → glow cycle BPM      1–8       (128 ≈ 4 BPM → ~15 s full cycle)
//   mod   → hue journey width   0–60      (0 = single color pulse, 255 = warm→cool shift)
//   color → sunrise anchor hue  (warm amber/orange recommended)
//
//   Whole strip breathes with a gentle edge-to-center warmth gradient.
//   Hue shifts cooler as brightness rises, mimicking dawn light temperature.
// ─────────────────────────────────────────────────────────────────────────────
void sunrise_render(led_render_ctx& ctx) {
    int n = ctx.led_count;
    uint16_t bpm_88 = static_cast<uint16_t>(map8(ctx.speed, 1, 8)) * 256;
    CHSV hsv_base = rgb2hsv_approximate(ctx.rgb);
    uint8_t base_hue = hsv_base.hue;
    uint8_t hue_span = map8(ctx.mode_modification, 0, 60);

    uint8_t bri_raw = beatsin8(bpm_88, 5, ctx.brightness);

    // Hue is warmer (lower = more red) when dim, shifts cooler as it brightens
    uint8_t hue_shift = scale8(hue_span, 255 - bri_raw);
    uint8_t hue = base_hue + hue_shift;

    CRGB base_c;
    hsv2rgb_rainbow(CHSV(hue, 240, bri_raw), base_c);

    // Gentle spatial gradient — center slightly brighter than ends (like a horizon glow)
    for (int i = 0; i < n; i++) {
        float fi = static_cast<float>(i) / static_cast<float>(n - 1);
        uint8_t edge = static_cast<uint8_t>((0.80f + 0.20f * sinf16(rad_to_phase16(fi * 3.14159f))) * 255);
        CRGB c = base_c;
        commit_pixel(ctx, i, c.nscale8_video(edge));
    }
    finish_frame(ctx);
}
