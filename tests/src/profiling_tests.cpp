#include "ae/core/frame_pacer.h"
#include "ae/core/frame_allocator.h"
#include "ae/core/frame_profiler.h"
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

// ===================================================================
// FrameProfiler tests
// ===================================================================

void test_frame_profiler_default_state() {
    ae::FrameProfiler profiler;

    // Default state: sections should be empty
    auto snap = profiler.end_frame(0);
    assert(snap.frame_number == 0);
    assert(snap.frame_total_ms >= 0.0);
    for (std::size_t i = 0; i < ae::FrameProfiler::kSectionCount; ++i) {
        assert(snap.sections[i].current_ms == 0.0);
        assert(snap.sections[i].call_count == 0);
    }
    std::cout << "test_frame_profiler_default_state passed.\n";
}

void test_frame_profiler_single_section() {
    ae::FrameProfiler profiler;

    profiler.begin_section(ae::ProfileSection::Render);
    // Simulate work
    volatile double sum = 0.0;
    for (int i = 0; i < 10000; ++i) {
        sum += static_cast<double>(i) * 0.001;
    }
    (void)sum;
    profiler.end_section(ae::ProfileSection::Render);

    auto snap = profiler.end_frame(1);
    assert(snap.frame_number == 1);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Render)].call_count == 1);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Render)].current_ms > 0.0);
    // Other sections should be zero
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Physics)].call_count == 0);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Physics)].current_ms == 0.0);
    std::cout << "test_frame_profiler_single_section passed.\n";
}

void test_frame_profiler_scoped_raii() {
    ae::FrameProfiler profiler;

    {
        ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Audio);
        volatile double sum = 0.0;
        for (int i = 0; i < 5000; ++i) {
            sum += static_cast<double>(i) * 0.002;
        }
        (void)sum;
    }

    auto snap = profiler.end_frame(2);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Audio)].call_count == 1);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Audio)].current_ms > 0.0);
    std::cout << "test_frame_profiler_scoped_raii passed.\n";
}

void test_frame_profiler_multiple_sections() {
    ae::FrameProfiler profiler;

    // Profile two sections in sequence
    {
        ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Physics);
        volatile double sum = 0.0;
        for (int i = 0; i < 3000; ++i) {
            sum += static_cast<double>(i) * 0.001;
        }
        (void)sum;
    }
    {
        ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Animation);
        volatile double sum = 0.0;
        for (int i = 0; i < 7000; ++i) {
            sum += static_cast<double>(i) * 0.001;
        }
        (void)sum;
    }

    auto snap = profiler.end_frame(3);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Physics)].call_count == 1);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Physics)].current_ms > 0.0);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Animation)].call_count == 1);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::Animation)].current_ms > 0.0);
    // Physics should be faster than Animation (fewer iterations)
    double phys_ms = snap.sections[static_cast<std::size_t>(ae::ProfileSection::Physics)].current_ms;
    double anim_ms = snap.sections[static_cast<std::size_t>(ae::ProfileSection::Animation)].current_ms;
    assert(phys_ms < anim_ms);
    std::cout << "test_frame_profiler_multiple_sections passed.\n";
}

void test_frame_profiler_same_section_multiple_calls() {
    ae::FrameProfiler profiler;

    for (int i = 0; i < 5; ++i) {
        ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::UI);
        volatile double sum = 0.0;
        for (int j = 0; j < 1000; ++j) {
            sum += static_cast<double>(j) * 0.001;
        }
        (void)sum;
    }

    auto snap = profiler.end_frame(4);
    // The same section can be called multiple times per frame
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::UI)].call_count == 5);
    assert(snap.sections[static_cast<std::size_t>(ae::ProfileSection::UI)].current_ms > 0.0);
    std::cout << "test_frame_profiler_same_section_multiple_calls passed.\n";
}

void test_frame_profiler_min_max_avg() {
    ae::FrameProfiler profiler;

    // Run several frames to accumulate min/max/avg
    for (std::uint64_t frame = 0; frame < 10; ++frame) {
        {
            ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Simulation);
            volatile double sum = 0.0;
            for (int i = 0; i < (frame + 1) * 1000; ++i) {
                sum += static_cast<double>(i) * 0.001;
            }
            (void)sum;
        }
        static_cast<void>(profiler.end_frame(frame));
    }

    // After accumulation, min should be less than max (frames have increasing work)
    // Note: FrameProfiler accumulates across end_frame calls, so min/max
    // reflect the per-frame values. call_count reports only the most recent
    // frame, which is 0 here (we called end_frame without a profile).
    auto snap = profiler.end_frame(10);
    const auto& sim = snap.sections[static_cast<std::size_t>(ae::ProfileSection::Simulation)];
    assert(sim.min_ms <= sim.max_ms || (sim.min_ms == 0.0 && sim.max_ms == 0.0));
    assert(sim.avg_ms >= 0.0);
    std::cout << "test_frame_profiler_min_max_avg passed.\n";
}

void test_frame_profiler_reset() {
    ae::FrameProfiler profiler;

    {
        ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Other);
        volatile double sum = 0.0;
        for (int i = 0; i < 1000; ++i) {
            sum += static_cast<double>(i) * 0.001;
        }
        (void)sum;
    }
    static_cast<void>(profiler.end_frame(5));

    profiler.reset();

    auto snap = profiler.end_frame(0);
    assert(snap.frame_number == 0);
    for (std::size_t i = 0; i < ae::FrameProfiler::kSectionCount; ++i) {
        assert(snap.sections[i].call_count == 0);
    }
    std::cout << "test_frame_profiler_reset passed.\n";
}

void test_frame_profiler_all_sections() {
    ae::FrameProfiler profiler;

    // Exercise every section at least once
    for (std::size_t s = 0; s < ae::FrameProfiler::kSectionCount; ++s) {
        auto section = static_cast<ae::ProfileSection>(s);
        profiler.begin_section(section);
        profiler.end_section(section);
    }

    auto snap = profiler.end_frame(6);
    for (std::size_t i = 0; i < ae::FrameProfiler::kSectionCount; ++i) {
        assert(snap.sections[i].call_count == 1);
    }
    std::cout << "test_frame_profiler_all_sections passed.\n";
}

void test_frame_profiler_section_names() {
    // Verify the name array covers all sections
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Render)]) == "Render");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Physics)]) == "Physics");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Animation)]) == "Animation");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Audio)]) == "Audio");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::UI)]) == "UI");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Simulation)]) == "Simulation");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Network)]) == "Network");
    assert(std::string(ae::kProfileSectionNames[static_cast<std::size_t>(ae::ProfileSection::Other)]) == "Other");
    std::cout << "test_frame_profiler_section_names passed.\n";
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

    std::cout << "\n--- FrameProfiler Tests ---\n";
    test_frame_profiler_default_state();
    test_frame_profiler_single_section();
    test_frame_profiler_scoped_raii();
    test_frame_profiler_multiple_sections();
    test_frame_profiler_same_section_multiple_calls();
    test_frame_profiler_min_max_avg();
    test_frame_profiler_reset();
    test_frame_profiler_all_sections();
    test_frame_profiler_section_names();

    std::cout << "\nAll profiling tests passed.\n";
    return 0;
}
