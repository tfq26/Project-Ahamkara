#include "ae/core/benchmark_budget.h"
#include "ae/core/frame_allocator.h"
#include "ae/core/frame_pacer.h"
#include "ae/core/frame_profiler.h"
#include "ae/core/job_system.h"
#include "ae/core/log.h"
#include "ae/core/memory_budget.h"
#include "ae/core/residency_manager.h"
#include "ae/core/telemetry.h"
#include "ae/core/time.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

// ============================================================
// Machine Context
// ============================================================

static ae::BenchmarkContext collect_context() {
    ae::BenchmarkContext ctx;
    ctx.hostname = "unknown";
    ctx.os_name =
#if defined(_WIN32)
        "Windows"
#elif defined(__APPLE__)
        "macOS"
#elif defined(__linux__)
        "Linux"
#else
        "unknown"
#endif
        ;

    // CPU: try /proc/cpuinfo on Linux
    ctx.cpu_model = "unknown";
    ctx.cpu_cores = static_cast<int>(std::thread::hardware_concurrency());
    ctx.memory_bytes = 0;
#if defined(__linux__)
    {
        FILE* f = std::fopen("/proc/cpuinfo", "r");
        if (f) {
            char buf[512];
            while (std::fgets(buf, sizeof(buf), f)) {
                if (std::strncmp(buf, "model name", 10) == 0) {
                    const char* colon = std::strchr(buf, ':');
                    if (colon) {
                        ctx.cpu_model = std::string(colon + 2);
                        // Trim newline
                        while (!ctx.cpu_model.empty() &&
                               (ctx.cpu_model.back() == '\n' || ctx.cpu_model.back() == '\r'))
                            ctx.cpu_model.pop_back();
                    }
                    break;
                }
            }
            std::fclose(f);
        }
        FILE* fmem = std::fopen("/proc/meminfo", "r");
        if (fmem) {
            char buf[256];
            while (std::fgets(buf, sizeof(buf), fmem)) {
                if (std::strncmp(buf, "MemTotal:", 9) == 0) {
                    unsigned long long kb = 0;
                    std::sscanf(buf, "MemTotal: %llu kB", &kb);
                    ctx.memory_bytes = kb * 1024ULL;
                    break;
                }
            }
            std::fclose(fmem);
        }
    }
#endif
    ctx.compiler =
#if defined(__clang__)
        "clang"
#elif defined(__GNUC__) || defined(__GNUG__)
        "gcc"
#elif defined(_MSC_VER)
        "msvc"
#else
        "unknown"
#endif
        ;

    ctx.build_type =
#if defined(NDEBUG)
        "release"
#else
        "debug"
#endif
        ;

    return ctx;
}

// ============================================================
// Helper: high-resolution clock
// ============================================================
static double now_ms() {
    using clock = std::chrono::steady_clock;
    static const auto epoch = clock::now();
    auto d = clock::now() - epoch;
    return std::chrono::duration<double, std::milli>(d).count();
}

static double elapsed_ms(double start) {
    return now_ms() - start;
}

// ============================================================
// Helper: busy-work loop (deterministic CPU work)
// ============================================================
static void busy_work(int units) {
    volatile double sum = 0.0;
    for (int i = 0; i < units; ++i) {
        sum += std::sin(static_cast<double>(i) * 0.001);
    }
    (void)sum;
}

// ============================================================
// Benchmark 1: FrameAllocator throughput
// ============================================================
static ae::BenchmarkStats benchmark_frame_allocator(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "frame_allocator",
        .deterministic_min = 1000.0,     // at least 1K allocs/sec
        .deterministic_max = 1e12,       // sanity cap
        .statistical_warn_ms = 20.0,
        .statistical_fail_ms = 100.0,
    };

    return ae::run_benchmark(name, 30, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;

        ae::FrameAllocator alloc(4 * 1024 * 1024, 3);

        double start = now_ms();
        constexpr int kAllocs = 1000;
        uint64_t total_bytes = 0;

        for (int i = 0; i < kAllocs; ++i) {
            // Mix of small, medium, and large allocations
            std::size_t size = 16;
            if (i % 10 == 0) size = 4096;
            if (i % 50 == 0) size = 65536;
            void* ptr = alloc.allocate(size);
            if (ptr) {
                total_bytes += size;
                // Touch to force commit
                std::memset(ptr, 0xAB, std::min(size, std::size_t{64}));
            }
        }

        // Allocate arrays
        int* arr = alloc.allocate_array<int>(256);
        if (arr) {
            total_bytes += 256 * sizeof(int);
            arr[0] = 42;
        }

        double elapsed = elapsed_ms(start);
        alloc.end_frame();

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = total_bytes,
            .peak_bytes = alloc.peak_used(),
            .operation_count = static_cast<uint64_t>(kAllocs) + 1,
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 2: JobSystem dispatch throughput
// ============================================================
static ae::BenchmarkStats benchmark_job_system(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "job_system",
        .deterministic_min = 10.0,       // at least 10 job dispatches/sec
        .deterministic_max = 1e12,
        .statistical_warn_ms = 100.0,
        .statistical_fail_ms = 500.0,
    };

    return ae::run_benchmark(name, 20, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;
        ae::JobSystem jobs;
        jobs.init();

        double start = now_ms();
        constexpr int kJobs = 500;

        auto handles = jobs.dispatch(kJobs, [](int idx) {
            volatile double sum = 0.0;
            for (int i = 0; i < 100; ++i) {
                sum += static_cast<double>(idx * i) * 0.001;
            }
            (void)sum;
        });

        jobs.wait_all();
        double elapsed = elapsed_ms(start);

        jobs.shutdown();

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kJobs),
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 3: JobSystem with dependencies (DAG)
// ============================================================
static ae::BenchmarkStats benchmark_job_dag(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "job_dag",
        .deterministic_min = 5.0,
        .deterministic_max = 1e12,
        .statistical_warn_ms = 150.0,
        .statistical_fail_ms = 800.0,
    };

    return ae::run_benchmark(name, 15, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;
        ae::JobSystem jobs;
        jobs.init();

        double start = now_ms();
        constexpr int kWidth = 10;
        constexpr int kDepth = 5;

        // Build a diamond DAG: submit kWidth jobs at each depth level
        // Each level depends on all jobs from the previous level.
        std::vector<ae::JobSystem::JobHandle> prev;
        for (int d = 0; d < kDepth; ++d) {
            std::vector<ae::JobSystem::JobHandle> current;
            for (int w = 0; w < kWidth; ++w) {
                auto fn = [d, w]() {
                    volatile double sum = 0.0;
                    for (int i = 0; i < 50; ++i) {
                        sum += static_cast<double>(d * kWidth + w + i) * 0.001;
                    }
                    (void)sum;
                };

                ae::JobSystem::JobHandle h;
                if (prev.empty()) {
                    h = jobs.submit(fn);
                } else {
                    h = jobs.submit_after_all(prev, fn);
                }
                current.push_back(h);
            }
            prev = std::move(current);
        }

        jobs.wait_all();
        double elapsed = elapsed_ms(start);
        jobs.shutdown();

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kWidth * kDepth),
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 4: SnapshotInterpolator throughput
// ============================================================

// Minimal player state for snapshot interpolation
struct BenchPlayerState {
    double position[3]{};
    double velocity[3]{};
    float yaw{};
    float health{100.0f};
    float shield{100.0f};
    int network_object_id{};
    int player_id{};
    int movement_state{};
};

struct BenchSnapshot {
    BenchPlayerState local_player{};
    uint64_t server_tick{};
};

// We can't use the template directly in a simple way without the full types,
// so we'll implement a standalone snapshot processing benchmark.
static ae::BenchmarkStats benchmark_snapshot_processing(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "snapshot_processing",
        .deterministic_min = 100.0,
        .deterministic_max = 1e12,
        .statistical_warn_ms = 20.0,
        .statistical_fail_ms = 100.0,
    };

    return ae::run_benchmark(name, 30, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;

        // Simulate snapshot processing: encode/decode sequence and interpolation
        // This is a standalone benchmark that mirrors the network hot path.
        constexpr int kSnapshots = 500;
        std::vector<BenchSnapshot> snapshots;
        snapshots.reserve(kSnapshots);

        for (int i = 0; i < kSnapshots; ++i) {
            BenchSnapshot snap;
            snap.server_tick = static_cast<uint64_t>(i);
            snap.local_player.position[0] = static_cast<double>(i) * 1.5;
            snap.local_player.position[1] = 0.0;
            snap.local_player.position[2] = static_cast<double>(i) * 1.2;
            snap.local_player.health = 100.0f - static_cast<float>(i) * 0.1f;
            snapshots.push_back(snap);
        }

        double start = now_ms();

        // Simulate interpolation: for each pair, compute interpolated state
        for (int i = 0; i < kSnapshots - 1; ++i) {
            const auto& a = snapshots[i];
            const auto& b = snapshots[i + 1];
            float t = 0.5f;

            BenchPlayerState out{};
            out.position[0] = a.local_player.position[0] + (b.local_player.position[0] - a.local_player.position[0]) * t;
            out.position[1] = a.local_player.position[1] + (b.local_player.position[1] - a.local_player.position[1]) * t;
            out.position[2] = a.local_player.position[2] + (b.local_player.position[2] - a.local_player.position[2]) * t;
            out.health = a.local_player.health + (b.local_player.health - a.local_player.health) * t;

            volatile float check = out.health + static_cast<float>(out.position[0]);
            (void)check;
        }

        // Simulate sequence tracking (packet ordering / dedup)
        uint64_t last_tick = 0;
        int accepted = 0;
        int rejected = 0;
        for (const auto& snap : snapshots) {
            if (snap.server_tick > last_tick) {
                last_tick = snap.server_tick;
                ++accepted;
            } else {
                ++rejected;
            }
        }

        double elapsed = elapsed_ms(start);

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kSnapshots) + static_cast<uint64_t>(kSnapshots - 1),
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 5: ResidencyManager planning
// ============================================================
static ae::BenchmarkStats benchmark_residency_planning(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "residency_planning",
        .deterministic_min = 10.0,
        .deterministic_max = 1e12,
        .statistical_warn_ms = 15.0,
        .statistical_fail_ms = 80.0,
    };

    return ae::run_benchmark(name, 30, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;
        ae::core::ResidencyManager mgr;
        mgr.init(100, 100, 3);  // 100x100 grid, radius 3

        double start = now_ms();
        constexpr int kUpdates = 200;

        for (int i = 0; i < kUpdates; ++i) {
            float x = static_cast<float>(i) * 2.5f;
            float z = static_cast<float>(i) * 1.8f;
            mgr.update(x, z, 10.0f, 0.0f, 0.0f);
            auto pending = mgr.consume_pending();
            volatile size_t pc = pending.size();
            (void)pc;
        }

        double elapsed = elapsed_ms(start);

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kUpdates),
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 6: Headless frame simulation (FramePacer + FrameProfiler)
// ============================================================
static ae::BenchmarkStats benchmark_headless_frame(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "headless_frame",
        .deterministic_min = 10.0,
        .deterministic_max = 1e12,
        .statistical_warn_ms = 30.0,
        .statistical_fail_ms = 150.0,
    };

    return ae::run_benchmark(name, 20, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;
        ae::FramePacer pacer(16.7);
        ae::FrameProfiler profiler;

        double start = now_ms();
        constexpr int kFrames = 120;  // ~2 seconds at 60fps

        for (int frame = 0; frame < kFrames; ++frame) {
            pacer.start_frame();

            // Simulate a headless tick with profiled sections
            {
                ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Simulation);
                busy_work(500);
            }
            {
                ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Physics);
                busy_work(200);
            }
            {
                ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Network);
                busy_work(100);
            }

            pacer.end_frame();
            auto _snap = profiler.end_frame(static_cast<uint64_t>(frame));
            (void)_snap;
        }

        double elapsed = elapsed_ms(start);

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kFrames),
        });
    }, &kBudget);
}

// ============================================================
// Benchmark 7: Memory budget tracking
// ============================================================
static ae::BenchmarkStats benchmark_memory_budget(const char* name) {
    static constexpr ae::BenchmarkBudget kBudget{
        .name = "memory_budget",
        .deterministic_min = 100.0,
        .deterministic_max = 1e12,
        .statistical_warn_ms = 10.0,
        .statistical_fail_ms = 50.0,
    };

    return ae::run_benchmark(name, 30, [](int iteration, std::vector<ae::BenchmarkSample>& samples) {
        (void)iteration;
        ae::MemoryBudgetTracker tracker(1024 * 1024, 2 * 1024 * 1024);

        double start = now_ms();
        constexpr int kUpdates = 500;

        for (int i = 0; i < kUpdates; ++i) {
            // Simulate tracking
            volatile auto pressure = tracker.rss_pressure();
            (void)pressure;
            tracker.set_rss_soft_bytes(static_cast<std::size_t>(1024 * 1024 + i * 100));
            tracker.set_rss_hard_bytes(static_cast<std::size_t>(2 * 1024 * 1024 + i * 100));
        }

        double elapsed = elapsed_ms(start);

        samples.push_back(ae::BenchmarkSample{
            .elapsed_ms = elapsed,
            .bytes_allocated = 0,
            .peak_bytes = 0,
            .operation_count = static_cast<uint64_t>(kUpdates),
        });
    }, &kBudget);
}

// ============================================================
// Deliberate Regression Fixture
// ============================================================
// This fixture deliberately fails its budget to prove the gate works.
// It sets an impossibly low budget and then exceeds it.
static int run_deliberate_regression() {
    std::cout << "\n=== Deliberate Regression Fixture ===\n";
    std::cout << "This fixture intentionally fails to prove the budget gate works.\n\n";

    static constexpr ae::BenchmarkBudget kTightBudget{
        .name = "deliberate_regression",
        .deterministic_min = 1e12,   // impossibly high — always fails
        .deterministic_max = 1e12,   // only one value allowed
        .statistical_warn_ms = 0.001,
        .statistical_fail_ms = 0.002,
    };

    // Measure something trivially fast — but the budget demands 1e12 ops/sec
    double start = now_ms();
    volatile int x = 0;
    for (int i = 0; i < 1000; ++i) {
        x += i;
    }
    double elapsed = elapsed_ms(start);

    // Check deterministic: must get 1e12 ops/sec — impossible
    double measured_ops = 1000.0 / (elapsed / 1000.0);
    auto dr = kTightBudget.check_deterministic(measured_ops);
    std::cout << "Deterministic check: measured_ops=" << measured_ops
              << " (threshold_min=" << kTightBudget.deterministic_min << ")\n";
    std::cout << "  -> passed=" << (dr.passed ? "true" : "false") << "\n";

    // Check statistical: median ~elapsed ms, must be < 0.002ms — trivially fails
    auto sr = kTightBudget.check_statistical(elapsed);
    std::cout << "Statistical check: measured_ms=" << elapsed
              << " (fail_threshold=" << kTightBudget.statistical_fail_ms << ")\n";
    std::cout << "  -> passed=" << (sr.passed ? "true" : "false") << "\n";

    // Both should fail
    int failures = 0;
    if (!dr.passed) {
        std::cerr << "[EXPECTED FAIL] " << dr.name << "\n";
        ++failures;
    }
    if (!sr.passed) {
        std::cerr << "[EXPECTED FAIL] " << sr.name << "\n";
        ++failures;
    }

    std::cout << "Deliberate regression: " << failures << " expected failures detected.\n";
    if (failures == 0) {
        std::cerr << "[UNEXPECTED] Deliberate regression did NOT fail — budget gate may be broken!\n";
        return 1;
    }

    // Return non-zero: the fixture demonstrates a real regression.
    // CTest WILL_FAIL=TRUE expects this test to return non-zero.
    std::cout << "Deliberate regression fixture: " << failures
              << " budget failure(s) detected (expected).\n";
    return failures > 0 ? 1 : 0;
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    bool run_regression = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--regression") == 0) {
            run_regression = true;
        }
    }

    // If running deliberate regression in isolation
    if (run_regression) {
        return run_deliberate_regression();
    }

    ae::set_log_level(ae::LogLevel::Error);

    // Collect context
    auto ctx = collect_context();
    ae::emit_context_json(ctx);

    bool first = true;
    auto emit_separator = [&first]() {
        if (!first) std::cout << ",\n";
        first = false;
    };

    // Run benchmarks
    int total_failures = 0;

    {
        auto stats = benchmark_frame_allocator("frame_allocator");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_job_system("job_system");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_job_dag("job_dag");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_snapshot_processing("snapshot_processing");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_residency_planning("residency_planning");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_headless_frame("headless_frame");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    {
        auto stats = benchmark_memory_budget("memory_budget");
        emit_separator();
        ae::emit_benchmark_json(stats);
        total_failures += ae::report_budget_results(stats);
    }

    ae::emit_json_footer();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Budget failures: " << total_failures << "\n";

    // Run deliberate regression as validation (not part of normal pass/fail)
    // The regression fixture proves the gate *can* fail; we don't want it to
    // fail in CI unless explicitly requested.
    if (total_failures > 0) {
        std::cerr << total_failures << " budget failure(s) detected.\n";
        return 1;
    }

    std::cout << "All benchmarks passed within budgets.\n";
    return 0;
}
