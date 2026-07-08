#include "ae/core/frame_pacer.h"
#include "ae/core/frame_allocator.h"
#include "ae/core/memory_budget.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

namespace {

// ===================================================================
// FramePacer tests
// ===================================================================

void test_frame_pacer_default_budget() {
    ae::FramePacer pacer;
    assert(pacer.budget_ms() == 16.7);
    assert(pacer.warn_threshold_ms() == 2.0);
    assert(pacer.pacing_healthy());
    assert(!pacer.regression_detected());
    assert(pacer.frame_count() == 0);
    std::cout << "test_frame_pacer_default_budget passed.\n";
}

void test_frame_pacer_custom_budget() {
    ae::FramePacer pacer(33.3, 5.0);
    assert(pacer.budget_ms() == 33.3);
    assert(pacer.warn_threshold_ms() == 5.0);
    std::cout << "test_frame_pacer_custom_budget passed.\n";
}

void test_frame_pacer_single_frame() {
    ae::FramePacer pacer(16.7);
    pacer.end_frame(16.5);

    assert(pacer.frame_count() == 1);
    assert(std::abs(pacer.raw_frame_time_ms() - 16.5) < 0.001);
    assert(pacer.smooth_frame_time_ms() > 0.0);
    assert(pacer.rolling_avg_ms() > 0.0);
    assert(pacer.budget_compliance() == 1.0);
    assert(pacer.pacing_healthy());
    std::cout << "test_frame_pacer_single_frame passed.\n";
}

void test_frame_pacer_within_budget() {
    ae::FramePacer pacer(16.7);
    for (int i = 0; i < 60; ++i) {
        pacer.end_frame(15.0);  // consistently under 16.7 ms
    }

    assert(pacer.frame_count() == 60);
    assert(pacer.pacing_healthy());
    assert(!pacer.regression_detected());
    assert(pacer.budget_compliance() == 1.0);
    assert(pacer.smooth_frame_time_ms() < 16.0);
    std::cout << "test_frame_pacer_within_budget passed.\n";
}

void test_frame_pacer_exceeds_budget() {
    ae::FramePacer pacer(16.7, 2.0);
    for (int i = 0; i < 60; ++i) {
        pacer.end_frame(30.0);  // consistently over 16.7 ms
    }

    assert(pacer.frame_count() == 60);
    assert(!pacer.pacing_healthy());
    assert(pacer.regression_detected());
    assert(pacer.budget_compliance() == 0.0);
    assert(pacer.smooth_frame_time_ms() > 20.0);
    std::cout << "test_frame_pacer_exceeds_budget passed.\n";
}

void test_frame_pacer_mixed_timing() {
    ae::FramePacer pacer(16.7);
    // 80% within budget, 20% over
    for (int i = 0; i < 100; ++i) {
        if (i < 80) {
            pacer.end_frame(12.0);  // within budget
        } else {
            pacer.end_frame(25.0);  // over budget
        }
    }

    assert(pacer.frame_count() == 100);
    assert(pacer.budget_compliance() > 0.75);
    assert(pacer.budget_compliance() < 0.85);
    std::cout << "test_frame_pacer_mixed_timing passed.\n";
}

void test_frame_pacer_1p_low() {
    ae::FramePacer pacer(16.7);
    // Most frames are fast, some are very slow
    for (int i = 0; i < 200; ++i) {
        if (i >= 198) {
            pacer.end_frame(50.0);  // top 1% — very slow
        } else {
            pacer.end_frame(10.0);  // fast
        }
    }

    // 1% low should be high (worst 1% frames = the slow ones)
    assert(pacer.percentile_01_low_ms() >= 45.0);
    assert(pacer.rolling_avg_ms() < 20.0);  // average unaffected
    std::cout << "test_frame_pacer_1p_low passed.\n";
}

void test_frame_pacer_rolling_avg() {
    ae::FramePacer pacer(16.7);

    // Fill with 10.0 ms frames
    for (int i = 0; i < 50; ++i) {
        pacer.end_frame(10.0);
    }
    assert(std::abs(pacer.rolling_avg_ms() - 10.0) < 0.5);
    std::cout << "test_frame_pacer_rolling_avg passed.\n";
}

void test_frame_pacer_history_ring() {
    ae::FramePacer pacer(16.7);

    // Fill more than history size
    for (int i = 0; i < ae::FramePacer::kHistorySize + 50; ++i) {
        pacer.end_frame(static_cast<double>(i % 20));
    }

    assert(pacer.history_count() == ae::FramePacer::kHistorySize);
    std::cout << "test_frame_pacer_history_ring passed.\n";
}

void test_frame_pacer_reset() {
    ae::FramePacer pacer(16.7);
    for (int i = 0; i < 30; ++i) {
        pacer.end_frame(25.0);
    }
    assert(pacer.frame_count() == 30);
    assert(!pacer.pacing_healthy());

    pacer.reset();
    assert(pacer.frame_count() == 0);
    assert(pacer.pacing_healthy());
    assert(!pacer.regression_detected());
    assert(pacer.budget_compliance() == 1.0);
    std::cout << "test_frame_pacer_reset passed.\n";
}

void test_frame_pacer_set_budget_runtime() {
    ae::FramePacer pacer(16.7);
    assert(pacer.budget_ms() == 16.7);

    for (int i = 0; i < 30; ++i) {
        pacer.end_frame(20.0);  // over 16.7 budget
    }
    assert(!pacer.pacing_healthy());

    pacer.set_budget(25.0);  // now 20 < 25, should be healthy
    // Need a few more frames to pull the smooth average
    for (int i = 0; i < 10; ++i) {
        pacer.end_frame(20.0);
    }
    assert(pacer.pacing_healthy());
    std::cout << "test_frame_pacer_set_budget_runtime passed.\n";
}

// ===================================================================
// MemoryBudgetTracker tests
// ===================================================================

void test_memory_budget_defaults() {
    ae::MemoryBudgetTracker tracker;
    assert(tracker.rss_soft_bytes() == 512ULL * 1024ULL * 1024ULL);
    assert(tracker.rss_hard_bytes() == 768ULL * 1024ULL * 1024ULL);
    assert(tracker.rss_bytes() == 0);
    assert(tracker.rss_pressure() == ae::MemoryBudgetTracker::Pressure::Ok);
    std::cout << "test_memory_budget_defaults passed.\n";
}

void test_memory_budget_custom() {
    constexpr std::size_t kSoft = 100;
    constexpr std::size_t kHard = 200;
    ae::MemoryBudgetTracker tracker(kSoft, kHard);
    assert(tracker.rss_soft_bytes() == kSoft);
    assert(tracker.rss_hard_bytes() == kHard);
    std::cout << "test_memory_budget_custom passed.\n";
}

void test_memory_budget_pressure_ok() {
    ae::MemoryBudgetTracker tracker(1000, 2000);
    // Pressure is determined by compute_pressure which checks values >= threshold
    // Since RSS starts at 0 and we haven't called update_rss, it's Ok
    assert(tracker.rss_pressure() == ae::MemoryBudgetTracker::Pressure::Ok);
    std::cout << "test_memory_budget_pressure_ok passed.\n";
}

void test_memory_budget_frame_allocator_tracking() {
    ae::FrameAllocator alloc(1024, 2);
    alloc.end_frame();
    alloc.allocate(400);

    ae::MemoryBudgetTracker tracker(0, 0, 300, 500);  // frame alloc soft=300, hard=500
    tracker.track_frame_allocator(alloc);

    assert(tracker.alloc_peak_bytes() >= 400);
    assert(tracker.alloc_pressure() == ae::MemoryBudgetTracker::Pressure::Warning);  // soft exceeded
    std::cout << "test_memory_budget_frame_allocator_tracking passed.\n";
}

void test_memory_budget_allocator_hard() {
    ae::FrameAllocator alloc(1024, 2);
    alloc.end_frame();
    alloc.allocate(400);

    ae::MemoryBudgetTracker tracker(0, 0, 200, 350);  // hard=350, alloc 400 >= 350
    tracker.track_frame_allocator(alloc);

    assert(tracker.alloc_pressure() == ae::MemoryBudgetTracker::Pressure::Critical);  // hard exceeded
    std::cout << "test_memory_budget_allocator_hard passed.\n";
}

void test_memory_budget_allocator_ok() {
    ae::FrameAllocator alloc(1024, 2);
    alloc.end_frame();
    alloc.allocate(100);

    ae::MemoryBudgetTracker tracker(0, 0, 200, 500);  // soft=200, hard=500, alloc 100 < 200
    tracker.track_frame_allocator(alloc);

    assert(tracker.alloc_pressure() == ae::MemoryBudgetTracker::Pressure::Ok);
    std::cout << "test_memory_budget_allocator_ok passed.\n";
}

void test_memory_budget_reset() {
    ae::MemoryBudgetTracker tracker(100, 200);
    tracker.reset();
    assert(tracker.rss_bytes() == 0);
    assert(tracker.peak_rss_bytes() == 0);
    assert(tracker.rss_pressure() == ae::MemoryBudgetTracker::Pressure::Ok);
    std::cout << "test_memory_budget_reset passed.\n";
}

void test_memory_budget_set_budgets_runtime() {
    ae::MemoryBudgetTracker tracker(100, 200);
    assert(tracker.rss_soft_bytes() == 100);

    tracker.set_rss_soft_bytes(200);
    tracker.set_rss_hard_bytes(400);
    assert(tracker.rss_soft_bytes() == 200);
    assert(tracker.rss_hard_bytes() == 400);
    std::cout << "test_memory_budget_set_budgets_runtime passed.\n";
}

void test_memory_budget_disabled() {
    // 0 budgets = disabled
    ae::MemoryBudgetTracker tracker(0, 0);
    assert(tracker.rss_pressure() == ae::MemoryBudgetTracker::Pressure::Ok);
    std::cout << "test_memory_budget_disabled passed.\n";
}

}  // namespace

int main() {
    ae::set_log_level(ae::LogLevel::Error);

    std::cout << "--- FramePacer Tests ---\n";
    test_frame_pacer_default_budget();
    test_frame_pacer_custom_budget();
    test_frame_pacer_single_frame();
    test_frame_pacer_within_budget();
    test_frame_pacer_exceeds_budget();
    test_frame_pacer_mixed_timing();
    test_frame_pacer_1p_low();
    test_frame_pacer_rolling_avg();
    test_frame_pacer_history_ring();
    test_frame_pacer_reset();
    test_frame_pacer_set_budget_runtime();

    std::cout << "\n--- MemoryBudgetTracker Tests ---\n";
    test_memory_budget_defaults();
    test_memory_budget_custom();
    test_memory_budget_pressure_ok();
    test_memory_budget_frame_allocator_tracking();
    test_memory_budget_allocator_hard();
    test_memory_budget_allocator_ok();
    test_memory_budget_reset();
    test_memory_budget_set_budgets_runtime();
    test_memory_budget_disabled();

    std::cout << "\nAll profiling tests passed.\n";
    return 0;
}
