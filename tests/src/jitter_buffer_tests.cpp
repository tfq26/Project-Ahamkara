#include "ae/network/jitter_buffer.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {

int fail(const std::string& msg) {
    std::cerr << "jitter_buffer_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)
#define EXPECT_NEAR(a, b, eps)                                                               \
    do {                                                                                     \
        if (std::fabs((a) - (b)) > (eps)) {                                                  \
            std::cerr << "  expected " << (a) << " ≈ " << (b) << " (eps=" << (eps) << ")\n"; \
            return fail("tolerance exceeded");                                               \
        }                                                                                    \
    } while (0)

// ── Tests ────────────────────────────────────────────────────────────────────

int test_initial_state() {
    ae::JitterBuffer jb;
    EXPECT(!jb.is_initialized(), "not initialized initially");
    EXPECT_NEAR(jb.suggest_delay(60.0F), 1.0F / 60.0F, 0.001F);
    EXPECT_NEAR(jb.jitter_estimate(), 0.0F, 0.001F);
    EXPECT_NEAR(jb.avg_interval(), 0.0F, 0.001F);
    return 0;
}

int test_perfect_interval_no_jitter() {
    ae::JitterBuffer jb;
    const float tick_interval = 1.0F / 60.0F;

    // Perfect 60 Hz arrival.
    for (int i = 0; i < 100; ++i) {
        jb.record(tick_interval);
    }

    EXPECT(jb.is_initialized(), "should be initialized");
    EXPECT_NEAR(jb.avg_interval(), tick_interval, 0.002F);
    EXPECT_NEAR(jb.jitter_estimate(), 0.0F, 0.005F);

    // Delay should be approximately 2 * tick_interval (base) + small jitter.
    const float delay = jb.suggest_delay(60.0F);
    EXPECT(delay >= jb.min_delay_seconds(), "delay should be >= min");
    EXPECT(delay <= jb.max_delay_seconds(), "delay should be <= max");
    // With zero jitter, delay ≈ 2 * tick_interval.
    EXPECT_NEAR(delay, 2.0F * tick_interval, 0.005F);
    return 0;
}

int test_jitter_increases_delay() {
    ae::JitterBuffer jb;
    const float tick_interval = 1.0F / 60.0F;

    // Simulate bursty arrival: alternating fast and slow.
    for (int i = 0; i < 50; ++i) {
        jb.record(tick_interval * 0.5F); // fast
        jb.record(tick_interval * 1.5F); // slow
    }

    EXPECT(jb.jitter_estimate() > 0.001F, "jitter estimate should be > 0");

    // Delay should be larger than the zero-jitter case.
    const float delay_with_jitter = jb.suggest_delay(60.0F);
    EXPECT(delay_with_jitter > 2.0F * tick_interval,
           "delay should increase with jitter");
    return 0;
}

int test_reset_clears_state() {
    ae::JitterBuffer jb;

    jb.record(1.0F / 60.0F);
    jb.record(1.0F / 60.0F);
    EXPECT(jb.is_initialized(), "should be initialized");

    jb.reset();
    EXPECT(!jb.is_initialized(), "should be uninitialized after reset");
    EXPECT_NEAR(jb.jitter_estimate(), 0.0F, 0.001F);
    EXPECT_NEAR(jb.avg_interval(), 0.0F, 0.001F);

    // After reset, suggest_delay returns default.
    EXPECT_NEAR(jb.suggest_delay(60.0F), 1.0F / 60.0F, 0.001F);
    return 0;
}

int test_single_packet_initializes() {
    ae::JitterBuffer jb;

    jb.record(1.0F / 60.0F);
    EXPECT(jb.is_initialized(), "should be initialized after one record");
    EXPECT_NEAR(jb.avg_interval(), 1.0F / 60.0F, 0.001F);
    EXPECT_NEAR(jb.jitter_estimate(), 0.0F, 0.001F);
    return 0;
}

int test_first_packet_with_zero_interval() {
    ae::JitterBuffer jb;

    // First packet with 0 interval sets initial avg_interval.
    jb.record(0.0F);
    EXPECT(jb.is_initialized(), "should initialize");
    EXPECT_NEAR(jb.avg_interval(), 0.0F, 0.001F);

    // Second packet starts EWMA.
    jb.record(1.0F / 60.0F);
    EXPECT(jb.avg_interval() > 0.0F, "avg should now be > 0");
    return 0;
}

int test_configurable_bounds() {
    ae::JitterBuffer jb(2.0F, 0.05F, 0.2F, 0.125F);

    EXPECT_NEAR(jb.min_delay_seconds(), 0.05F, 0.001F);
    EXPECT_NEAR(jb.max_delay_seconds(), 0.2F, 0.001F);
    EXPECT_NEAR(jb.base_delay_multiplier(), 2.0F, 0.001F);
    EXPECT_NEAR(jb.ewma_alpha(), 0.125F, 0.001F);

    // With zero jitter, should be clamped between min and max.
    jb.record(1.0F / 60.0F);
    jb.record(1.0F / 60.0F);

    const float delay = jb.suggest_delay(60.0F);
    EXPECT(delay >= 0.05F, "delay should respect min bound");
    EXPECT(delay <= 0.2F, "delay should respect max bound");
    return 0;
}

int test_ewma_smoothing() {
    ae::JitterBuffer jb(2.0F, 0.0F, 1.0F, 0.5F); // High alpha for faster test.

    const float tick_interval = 1.0F / 60.0F;

    // Feed steady state.
    for (int i = 0; i < 10; ++i) {
        jb.record(tick_interval);
    }

    // Sudden spike.
    jb.record(tick_interval * 5.0F);

    // Jitter estimate should increase but not fully to the spike value due to EWMA.
    EXPECT(jb.jitter_estimate() > 0.0F, "jitter estimate should increase");
    EXPECT(jb.jitter_estimate() < tick_interval * 5.0F,
           "EWMA should smooth the spike");

    // Feed more steady-state packets: jitter should converge back toward 0.
    for (int i = 0; i < 20; ++i) {
        jb.record(tick_interval);
    }
    EXPECT_NEAR(jb.jitter_estimate(), 0.0F, tick_interval * 0.5F);
    return 0;
}

} // namespace

int main() {
    if (int rc = test_initial_state(); rc != 0)
        return rc;
    if (int rc = test_perfect_interval_no_jitter(); rc != 0)
        return rc;
    if (int rc = test_jitter_increases_delay(); rc != 0)
        return rc;
    if (int rc = test_reset_clears_state(); rc != 0)
        return rc;
    if (int rc = test_single_packet_initializes(); rc != 0)
        return rc;
    if (int rc = test_first_packet_with_zero_interval(); rc != 0)
        return rc;
    if (int rc = test_configurable_bounds(); rc != 0)
        return rc;
    if (int rc = test_ewma_smoothing(); rc != 0)
        return rc;
    std::cout << "jitter_buffer_tests passed\n";
    return 0;
}
