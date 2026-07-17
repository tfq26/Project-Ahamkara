#include "ae/core/types.h"
#include "ae/network/interpolation_buffer.h"
#include "ae/network/interpolation_types.h"

#include <cmath>
#include <cstdio>

namespace {

// ── Test snapshot type (no Flashback, no game types) ────────────────────────

struct TestSnap {
    double value {0.0};
};

struct TestVec2 {
    float x {0.0F};
    float y {0.0F};
};

// ── Lerp callbacks ──────────────────────────────────────────────────────────

TestSnap lerp_scalar(const TestSnap& a, const TestSnap& b, float t) {
    return {a.value + (b.value - a.value) * static_cast<double>(t)};
}

TestVec2 lerp_vec2(const TestVec2& a, const TestVec2& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t};
}

// ── Assert helpers ──────────────────────────────────────────────────────────

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

#define CHECK_CLOSE(a, b, eps)                                                  \
    do {                                                                        \
        const double _a = static_cast<double>(a);                               \
        const double _b = static_cast<double>(b);                               \
        if (std::fabs(_a - _b) > static_cast<double>(eps)) {                    \
            std::fprintf(stderr, "FAIL [%s:%d]: %f != %f (eps=%f)\n",           \
                         __FILE__, __LINE__, _a, _b, static_cast<double>(eps)); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

int g_failures = 0;

// ── Test: empty buffer ──────────────────────────────────────────────────────

void test_empty_buffer() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    CHECK(buf.size() == 0);

    TestSnap out {};
    auto result = buf.sample(0.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Empty);
}

// ── Test: single sample always returned ─────────────────────────────────────

void test_single_sample() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    buf.push(1.0, {10.0});
    CHECK(buf.size() == 1);

    TestSnap out {};
    // Before the sample.
    auto result = buf.sample(0.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Underrun);
    CHECK_CLOSE(out.value, 10.0, 1e-6);

    // Exactly at the sample.
    result = buf.sample(1.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6);

    // After the sample.
    result = buf.sample(2.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Extrapolated);
    CHECK_CLOSE(out.value, 10.0, 1e-6);
}

// ── Test: basic interpolation between two samples ───────────────────────────

void test_basic_interpolation() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(1.0, {10.0});
    CHECK(buf.size() == 2);

    TestSnap out {};
    auto result = buf.sample(0.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 5.0, 1e-6);

    // At older bound.
    result = buf.sample(0.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 0.0, 1e-6);

    // At newer bound.
    result = buf.sample(1.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6);
}

// ── Test: out-of-order push discarding ──────────────────────────────────────

void test_out_of_order_discard() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(10.0, {100.0});
    CHECK(buf.size() == 1);

    // Older timestamp — should be discarded.
    buf.push(5.0, {50.0});
    CHECK(buf.size() == 1);

    // Equal timestamp — should be discarded.
    buf.push(10.0, {999.0});
    CHECK(buf.size() == 1);

    // Newer timestamp — accepted.
    buf.push(15.0, {150.0});
    CHECK(buf.size() == 2);
}

// ── Test: interpolation delay config ────────────────────────────────────────

void test_interpolation_delay() {
    ae::InterpolationConfig cfg;
    cfg.delay_seconds = 0.1;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(1.0, {10.0});

    // Render time with delay applied: at "now"=1.1, render_time = 1.1 - 0.1 = 1.0
    TestSnap out {};
    auto result = buf.sample(1.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6);
}

// ── Test: bounded extrapolation ─────────────────────────────────────────────

void test_bounded_extrapolation() {
    ae::InterpolationConfig cfg;
    cfg.max_extrapolation_seconds = 0.2;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(1.0, {10.0});

    // At render_time = 1.1 (within extrapolation window: between newest and +0.2).
    TestSnap out {};
    auto result = buf.sample(1.1, out);
    CHECK(result.status == ae::InterpolationResult::Status::Extrapolated);
    CHECK_CLOSE(out.value, 11.0, 1e-6); // extrapolated = 10 + 1*1.0

    // At render_time = 1.2 (at the extrapolation bound).
    result = buf.sample(1.2, out);
    CHECK(result.status == ae::InterpolationResult::Status::Extrapolated);
    CHECK_CLOSE(out.value, 12.0, 1e-6); // extrapolated = 10 + 2*1.0

    // Beyond extrapolation bound: clamp to newest.
    result = buf.sample(2.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Extrapolated);
    CHECK_CLOSE(out.value, 10.0, 1e-6); // clamped to newest
    CHECK_CLOSE(result.render_time, 1.2, 1e-6);
}

// ── Test: buffer capacity overflow (oldest evicted) ─────────────────────────

void test_capacity_overflow() {
    ae::InterpolationConfig cfg;
    cfg.capacity = 4;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(1.0, {1.0});
    buf.push(2.0, {2.0});
    buf.push(3.0, {3.0});
    buf.push(4.0, {4.0});
    CHECK(buf.size() == 4);

    // Push a 5th entry — oldest (1.0) should be evicted.
    buf.push(5.0, {5.0});
    CHECK(buf.size() == 4);

    // Can no longer sample at 1.0 — it was evicted.
    TestSnap out {};
    auto result = buf.sample(1.5, out);
    // 1.5 is before the oldest (2.0) — underrun.
    CHECK(result.status == ae::InterpolationResult::Status::Underrun);
    CHECK_CLOSE(out.value, 2.0, 1e-6);

    // But 2.5 should interpolate between 2.0 and 3.0.
    result = buf.sample(2.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 2.5, 1e-6);
}

// ── Test: teleport/discontinuity detection ──────────────────────────────────

void test_teleport_discontinuity() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 1.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(0.5, {5.0});
    buf.push(0.9, {9.0});
    CHECK(buf.size() == 3);

    // Push with a gap of 2.0 > threshold of 1.0 → teleport detected.
    buf.push(3.0, {100.0});

    // Buffer should have been flushed — only the newest sample remains.
    CHECK(buf.size() == 1);

    TestSnap out {};
    auto result = buf.sample(3.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 100.0, 1e-6);
}

// ── Test: teleport detection with exact threshold boundary ──────────────────

void test_teleport_threshold_boundary() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 1.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    // Gap of exactly 1.0 — NOT a teleport (gap must be > threshold).
    buf.push(1.0, {10.0});
    CHECK(buf.size() == 2);

    // Gap of 1.001 > 1.0 — teleport.
    buf.push(2.001, {20.0});
    CHECK(buf.size() == 1); // Flushed and only newest kept.
}

// ── Test: buffer underrun (render_time before oldest) ───────────────────────

void test_underrun() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 100.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(10.0, {100.0});
    buf.push(20.0, {200.0});
    CHECK(buf.size() == 2);

    TestSnap out {};
    auto result = buf.sample(5.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Underrun);
    CHECK_CLOSE(out.value, 100.0, 1e-6);
}

// ── Test: reset ─────────────────────────────────────────────────────────────

void test_reset() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    buf.push(0.0, {0.0});
    buf.push(1.0, {10.0});
    CHECK(buf.size() == 2);

    buf.reset();
    CHECK(buf.size() == 0);

    TestSnap out {};
    auto result = buf.sample(0.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Empty);
}

// ── Test: clock drift correction via latency adjustment ─────────────────────

void test_latency_adjustment() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    CHECK_CLOSE(buf.config().delay_seconds, 0.1, 1e-6);

    // Increase latency.
    buf.apply_latency_adjustment(0.2);
    CHECK_CLOSE(buf.config().delay_seconds, 0.2, 1e-6);

    // Decrease latency (allowed, caller must ensure monotonic render time).
    buf.apply_latency_adjustment(0.15);
    CHECK_CLOSE(buf.config().delay_seconds, 0.15, 1e-6);

    // Negative latency — ignored.
    buf.apply_latency_adjustment(-0.1);
    CHECK_CLOSE(buf.config().delay_seconds, 0.15, 1e-6);
}

// ── Test: set_config ────────────────────────────────────────────────────────

void test_set_config() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    ae::InterpolationConfig new_cfg;
    new_cfg.capacity = 8;
    new_cfg.delay_seconds = 0.2;
    buf.set_config(new_cfg);

    CHECK(buf.config().capacity == 8);
    CHECK_CLOSE(buf.config().delay_seconds, 0.2, 1e-6);
}

// ── Test: jitter tolerance (irregular timestamps) ───────────────────────────

void test_jitter_tolerance() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    // Simulate snapshots arriving at irregular intervals.
    buf.push(0.0, {0.0});
    buf.push(0.15, {15.0}); // 150ms gap
    buf.push(0.22, {22.0}); // 70ms gap
    buf.push(0.40, {40.0}); // 180ms gap
    buf.push(0.55, {55.0}); // 150ms gap

    // Sample at various points — all should produce valid interpolation.
    TestSnap out {};
    auto result = buf.sample(0.1, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6); // between 0.0→0.15 at t=0.67

    result = buf.sample(0.18, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 18.0, 1e-6);

    result = buf.sample(0.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    // between 0.40→0.55: (0.5-0.4)/(0.55-0.4) = 0.1/0.15 = 0.6667
    // lerp: 40 + (55-40)*0.6667 = 40 + 10 = 50
    CHECK_CLOSE(out.value, 50.0, 1e-6);
}

// ── Test: many-samples stress test with loss ────────────────────────────────

void test_stress_with_loss() {
    ae::InterpolationBuffer<TestVec2> buf(lerp_vec2);

    // Simulate 200 snapshots at 60 Hz (period = 1/60 s) with 10% random loss.
    // Use simple deterministic "loss" pattern: every 10th sample omitted.
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 200; ++i) {
        // Simulate 10% loss: skip every 10th sample.
        if (i % 10 == 0)
            continue;

        const double t = static_cast<double>(i) * dt;
        buf.push(t, {static_cast<float>(i), static_cast<float>(i * 2)});
    }

    CHECK(buf.size() > 0);

    // Verify we can still successfully interpolate.
    // Sample near the end of the buffer where data is still available.
    // The buffer holds 64 samples. After pushing ~180 samples at 1/60s,
    // the oldest retained is ~(180-64+1)*dt = 117*dt ≈ 1.95s.
    // Sample at the newest timestamp to verify correctness.
    const double newest_t = buf.newest_timestamp();
    TestVec2 out {};
    auto result = buf.sample(newest_t, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);

    // Also sample slightly before newest.
    auto result_before = buf.sample(newest_t - 10.0 * dt, out);
    CHECK(result_before.status == ae::InterpolationResult::Status::Ok ||
          result_before.status == ae::InterpolationResult::Status::Extrapolated);
}

// ── Test: recovery after teleport ───────────────────────────────────────────

void test_recovery_after_teleport() {
    ae::InterpolationConfig cfg;
    cfg.teleport_threshold_seconds = 0.5;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(0.2, {2.0});
    CHECK(buf.size() == 2);

    // Teleport.
    buf.push(5.0, {100.0});
    CHECK(buf.size() == 1);

    // Now push regular samples after the teleport.
    buf.push(5.1, {101.0});
    buf.push(5.2, {102.0});
    CHECK(buf.size() == 3);

    // Sample should now interpolate normally.
    TestSnap out {};
    auto result = buf.sample(5.15, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 101.5, 1e-6);
}

// ── Test: Vec2 interpolation ────────────────────────────────────────────────

void test_vec2_interpolation() {
    ae::InterpolationBuffer<TestVec2> buf(lerp_vec2);

    buf.push(0.0, {0.0F, 0.0F});
    buf.push(1.0, {10.0F, 20.0F});

    TestVec2 out {};
    auto result = buf.sample(0.5, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.x, 5.0F, 1e-6F);
    CHECK_CLOSE(out.y, 10.0F, 1e-6F);
}

// ── Test: newest_timestamp getter ───────────────────────────────────────────

void test_newest_timestamp() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    CHECK_CLOSE(buf.newest_timestamp(), 0.0, 1e-6);

    buf.push(3.0, {30.0});
    CHECK_CLOSE(buf.newest_timestamp(), 3.0, 1e-6);

    buf.push(5.0, {50.0});
    CHECK_CLOSE(buf.newest_timestamp(), 5.0, 1e-6);
}

// ── Test: multiple interpolations from same buffer ──────────────────────────

void test_multiple_interpolations() {
    ae::InterpolationBuffer<TestVec2> buf(lerp_vec2);

    buf.push(0.0, {0.0F, 0.0F});
    buf.push(0.5, {5.0F, 10.0F});
    buf.push(1.0, {10.0F, 20.0F});

    // Interpolate at multiple different render times.
    TestVec2 out {};

    auto r1 = buf.sample(0.25, out);
    CHECK(r1.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.x, 2.5F, 1e-6F);
    CHECK_CLOSE(out.y, 5.0F, 1e-6F);

    auto r2 = buf.sample(0.75, out);
    CHECK(r2.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.x, 7.5F, 1e-6F);
    CHECK_CLOSE(out.y, 15.0F, 1e-6F);

    auto r3 = buf.sample(1.0, out);
    CHECK(r3.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.x, 10.0F, 1e-6F);
    CHECK_CLOSE(out.y, 20.0F, 1e-6F);

    // Buffer unchanged after reading.
    CHECK(buf.size() == 3);
}

// ── Test: duplicate timestamp handling ──────────────────────────────────────

void test_duplicate_timestamp() {
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar);

    buf.push(1.0, {10.0});
    buf.push(1.0, {20.0}); // Duplicate — should be discarded.
    CHECK(buf.size() == 1);

    TestSnap out {};
    auto result = buf.sample(1.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6); // Original value retained.
}

// ── Test: zero extrapolation (no extrapolation allowed) ─────────────────────

void test_zero_extrapolation() {
    ae::InterpolationConfig cfg;
    cfg.max_extrapolation_seconds = 0.0;
    ae::InterpolationBuffer<TestSnap> buf(lerp_scalar, cfg);

    buf.push(0.0, {0.0});
    buf.push(1.0, {10.0});

    // Render time at 1.0 — fine.
    TestSnap out {};
    auto result = buf.sample(1.0, out);
    CHECK(result.status == ae::InterpolationResult::Status::Ok);
    CHECK_CLOSE(out.value, 10.0, 1e-6);

    // Render time at 1.001 > newest + 0 — clamp to newest.
    result = buf.sample(1.001, out);
    CHECK(result.status == ae::InterpolationResult::Status::Extrapolated);
    CHECK_CLOSE(out.value, 10.0, 1e-6);
}

} // namespace

int main() {
    test_empty_buffer();
    test_single_sample();
    test_basic_interpolation();
    test_out_of_order_discard();
    test_interpolation_delay();
    test_bounded_extrapolation();
    test_capacity_overflow();
    test_teleport_discontinuity();
    test_teleport_threshold_boundary();
    test_underrun();
    test_reset();
    test_latency_adjustment();
    test_set_config();
    test_jitter_tolerance();
    test_stress_with_loss();
    test_recovery_after_teleport();
    test_vec2_interpolation();
    test_newest_timestamp();
    test_multiple_interpolations();
    test_duplicate_timestamp();
    test_zero_extrapolation();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) FAILED\n", g_failures);
        return 1;
    }

    std::printf("All interpolation buffer tests passed.\n");
    return 0;
}
