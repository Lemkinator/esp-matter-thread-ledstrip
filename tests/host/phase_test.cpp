// Host-side regression test for the LED animation time base.
//
// Two independent bugs, tested in isolation:
//
//   A) get_time_s() casts milliseconds to a 32-bit float. Float has a
//      24-bit mantissa, so its resolution in seconds shrinks as uptime
//      grows. led_phase.h's integer phase accumulator must not do this.
//   B) fast_sinf() (fast_trig.h) quantizes its angle to 256 steps/cycle
//      (sin8). At the slow rotation rates the ambient modes use, that
//      quantizes motion to a stall-then-jump pattern from the moment the
//      device boots, independent of uptime. sinf16() (led_phase.h, sin16,
//      65536 steps/cycle) must not.
//
// Build: `make -C tests/host && ./tests/host/phase_test`

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "lib8tion.h"
#include "led_phase.h"  // production code under test (new model)

namespace {

// Historical reference only — this was main/led/fast_trig.h, deleted once
// led_phase.h replaced every call site. Kept here so this regression test
// can keep demonstrating the bug it was written to catch (sin8's 256
// steps/cycle vs sin16's 65536). Do not reintroduce into production.
float fast_sinf(float radians) {
    constexpr float RAD_TO_U8 = 256.0f / (2.0f * 3.14159265f);
    uint8_t theta = static_cast<uint8_t>(static_cast<int32_t>(radians * RAD_TO_U8));
    return (static_cast<int16_t>(sin8(theta)) - 128) / 127.0f;
}

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        g_failures++;
    } else {
        std::printf("ok:   %s\n", what);
    }
}

constexpr uint32_t FRAME_MS = 33;  // ~30fps
constexpr uint32_t MS_PER_DAY = 86400000u;

}  // namespace

// ---------------------------------------------------------------------
// A) Float time base loses precision as uptime grows; integer phase
//    accumulator does not.
// ---------------------------------------------------------------------
static void test_float_time_precision_degrades_with_uptime() {
    std::printf("\n-- A: float time precision vs uptime --\n");

    // Old model: exactly what get_time_s() computes (ms cast to float,
    // divided by 1000). Reproduced here since get_time_s() itself pulls in
    // FreeRTOS/esp_timer headers that don't build on host.
    auto old_frame_delta_s = [](uint32_t ms) -> float {
        float t0 = static_cast<float>(ms) / 1000.0f;
        float t1 = static_cast<float>(ms + FRAME_MS) / 1000.0f;
        return t1 - t0;
    };

    const double true_delta_s = FRAME_MS / 1000.0;

    float err_boot = std::fabs(old_frame_delta_s(0) - true_delta_s);
    float err_3day = std::fabs(old_frame_delta_s(3 * MS_PER_DAY) - true_delta_s);
    float err_near_wrap = std::fabs(old_frame_delta_s(4294000000u) - true_delta_s);

    std::printf("  old model frame-delta error: boot=%.6f  +3d=%.6f  near-wrap=%.6f (seconds)\n",
                err_boot, err_3day, err_near_wrap);

    check(err_boot < 1e-6, "old model: negligible error at boot");
    // This is the bug: error grows enormously with uptime.
    check(err_near_wrap > err_boot * 1000.0f, "old model: error balloons by uptime (demonstrates bug A)");

    // New model: integer phase accumulator. Same 1Hz-equivalent rate
    // (rate_q32 for 1 cycle/s), frame-delta phase step must be identical
    // bit-for-bit regardless of ms.
    uint32_t rate_q32 = phase_rate_from_hz(1.0f);
    uint16_t p_boot0 = phase16(0, rate_q32);
    uint16_t p_boot1 = phase16(FRAME_MS, rate_q32);
    uint16_t p_3day0 = phase16(3 * MS_PER_DAY, rate_q32);
    uint16_t p_3day1 = phase16(3 * MS_PER_DAY + FRAME_MS, rate_q32);
    uint16_t p_wrap0 = phase16(4294000000u, rate_q32);
    uint16_t p_wrap1 = phase16(4294000000u + FRAME_MS, rate_q32);

    uint16_t step_boot = static_cast<uint16_t>(p_boot1 - p_boot0);
    uint16_t step_3day = static_cast<uint16_t>(p_3day1 - p_3day0);
    uint16_t step_wrap = static_cast<uint16_t>(p_wrap1 - p_wrap0);

    std::printf("  new model phase16 step:      boot=%u  +3d=%u  near-wrap=%u (of 65536/cycle)\n",
                step_boot, step_3day, step_wrap);

    // +-1 unit (of 65536/cycle) is normal integer-truncation rounding at a
    // ms boundary that doesn't align exactly with the phase grid — it does
    // not grow with uptime, unlike the float model's error above.
    auto close = [](uint16_t a, uint16_t b) { return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= 1; };
    check(close(step_boot, step_3day), "new model: same phase step (+-1) at boot vs +3 days");
    check(close(step_boot, step_wrap), "new model: same phase step (+-1) at boot vs near millis-wrap");
}

// ---------------------------------------------------------------------
// A2) phase16() stays continuous across the actual uint32 millis-counter
//     wrap (get_millisecond_timer() wraps every ~49.7 days). Unsigned
//     overflow is well-defined (mod 2^32) in C++, so ms+FRAME_MS crossing
//     0xFFFFFFFF should produce the same phase step as any other frame —
//     no jump right at the wrap instant.
// ---------------------------------------------------------------------
static void test_phase_continuous_across_millis_wrap() {
    std::printf("\n-- A2: phase16 continuity across the uint32 millis wrap --\n");

    uint32_t rate_q32 = phase_rate_from_hz(1.0f);
    uint32_t ms_before_wrap = 0xFFFFFFF0u;         // 15ms before the counter overflows
    uint32_t ms_after_wrap = ms_before_wrap + FRAME_MS;  // overflows past 0xFFFFFFFF, wraps to 17

    check(ms_after_wrap < ms_before_wrap, "test setup: ms_after_wrap actually wrapped past 0");

    uint16_t p0 = phase16(ms_before_wrap, rate_q32);
    uint16_t p1 = phase16(ms_after_wrap, rate_q32);
    uint16_t step_at_wrap = static_cast<uint16_t>(p1 - p0);

    uint16_t p_ref0 = phase16(0, rate_q32);
    uint16_t p_ref1 = phase16(FRAME_MS, rate_q32);
    uint16_t step_steady_state = static_cast<uint16_t>(p_ref1 - p_ref0);

    std::printf("  phase16 step across wrap=%u  steady-state step=%u (of 65536/cycle)\n", step_at_wrap,
                step_steady_state);

    auto close = [](uint16_t a, uint16_t b) { return std::abs(static_cast<int>(a) - static_cast<int>(b)) <= 1; };
    check(close(step_at_wrap, step_steady_state), "phase16: no discontinuity crossing the millis wrap");
}

// ---------------------------------------------------------------------
// B) sin8-quantized fast_sinf() stalls between frames at slow rates, even
//    at boot; sin16-based sinf16() does not.
// ---------------------------------------------------------------------
static void test_sin8_quantization_stalls_slow_oscillators() {
    std::printf("\n-- B: sin8 vs sin16 step size at a slow oscillation rate --\n");

    // Mirrors lava_render's blob 0 at ctx.speed=8 (GUI speed 80): rate =
    // 0.4 rad/s, sf = map8(8,8,100)/100.0f.
    const float sf = static_cast<float>(map8(8, 8, 100)) / 100.0f;
    const float rate_rad_s = 0.4f * sf;

    int stalled_frames = 0;
    const int N = 60;  // 2 seconds at 30fps
    float prev_old = fast_sinf(0.0f);
    for (int f = 1; f <= N; f++) {
        float t = (f * FRAME_MS) / 1000.0f;
        float v = fast_sinf(t * rate_rad_s);
        if (v == prev_old) stalled_frames++;
        prev_old = v;
    }
    std::printf("  old model (fast_sinf/sin8): %d/%d frames with zero movement\n", stalled_frames, N);
    check(stalled_frames > N / 2, "old model: motion stalls most frames at this rate (demonstrates bug B)");

    uint32_t rate_q32 = phase_rate_from_rad_s(rate_rad_s);
    int stalled_new = 0;
    uint16_t prev_phase = phase16(0, rate_q32);
    float prev_new = sinf16(prev_phase);
    for (int f = 1; f <= N; f++) {
        uint32_t ms = f * FRAME_MS;
        uint16_t phase = phase16(ms, rate_q32);
        float v = sinf16(phase);
        if (v == prev_new) stalled_new++;
        prev_new = v;
    }
    std::printf("  new model (sinf16/sin16):   %d/%d frames with zero movement\n", stalled_new, N);
    check(stalled_new < stalled_frames / 3, "new model: far fewer stalled frames than sin8 (demonstrates fix)");
}

int main() {
    test_float_time_precision_degrades_with_uptime();
    test_phase_continuous_across_millis_wrap();
    test_sin8_quantization_stalls_slow_oscillators();

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
