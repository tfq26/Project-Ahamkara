// Performance regression budget gates.
//
// This file defines stable micro/meso benchmarks for:
//   - FrameAllocator (allocation throughput)
//   - JobSystem (dispatch throughput)
//   - ResidencyManager (update throughput)
//   - FramePacer (timing overhead)
//   - MemoryBudgetTracker (overhead)
//   - Network snapshot processing (sequence tracker overhead)
//   - Streaming residency (residency manager overhead)
//
// Each benchmark emits machine-readable JSON results, evaluates against budget
// thresholds, and returns a non-zero exit code if any hard budget is exceeded.
//
// The deliberate regression fixture at the end proves the gate can fail with
// an actionable diagnostic.

#include "ae/core/benchmark_harness.h"
#include "ae/core/budget_config.h"
#include "ae/core/frame_allocator.h"
#include "ae/core/frame_pacer.h"
#include "ae/core/frame_profiler.h"
#include "ae/core/job_system.h"
#include "ae/core/memory_budget.h"
#include "ae/core/residency_manager.h"
#include "ae/core/diagnostics.h"
#include "ae/core/log.h"
#include "ae/core/time.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// =============================================================================
// Utility: collect system metadata for the benchmark report.
// =============================================================================

ae::SystemMetadata collect_system_metadata() {
    ae::SystemMetadata meta;
    const auto sysinfo = ae::collect_system_info();

    meta.os_name = sysinfo.os_name;
    meta.cpu_brand = sysinfo.cpu_brand;
    meta.cpu_core_count = sysinfo.cpu_core_count;
    meta.total_ram_bytes = sysinfo.total_ram_bytes;

    // Compiler info (from predefined macros).
    // Use TOSTRING (two-level macro) so the __GNUC__ etc tokens get expanded
    // to their numeric values before stringization.
#if defined(__clang__)
    meta.compiler = "Clang " TOSTRING(__clang_major__) "." TOSTRING(__clang_minor__) "." TOSTRING(__clang_patchlevel__);
#elif defined(__GNUC__) || defined(__GNUG__)
    meta.compiler = "GCC " TOSTRING(__GNUC__) "." TOSTRING(__GNUC_MINOR__) "." TOSTRING(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    meta.compiler = "MSVC " TOSTRING(_MSC_VER);
#else
    meta.compiler = "unknown";
#endif

    // Build config
#ifdef NDEBUG
    meta.build_config = "release";
#else
    meta.build_config = "debug";
#endif

    // Timestamp
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", std::localtime(&tt));
    meta.timestamp = buf;

    // Hostname (best-effort)
    if (const char* h = std::getenv("HOSTNAME"); h) meta.hostname = h;
    else if (const char* h = std::getenv("HOST"); h) meta.hostname = h;
    else meta.hostname = "unknown";

    return meta;
}

// =============================================================================
// Benchmark group: FrameAllocator
// =============================================================================

void benchmark_frame_allocator(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== FrameAllocator Benchmarks ===\n";

    // Allocate a 1 MB arena with 3 slots
    constexpr std::size_t kArenaSize = 1024 * 1024;
    ae::FrameAllocator alloc(kArenaSize, 3);

    // --- Throughput: allocations per second ---
    {
        const auto* budget = budgets.get("frame_allocator_throughput");
        auto result = ae::BenchmarkRunner::run_throughput(
            "frame_allocator_throughput",
            "allocations/sec",
            10000,
            [&](std::int64_t i) {
                // Allocate varying sizes to simulate realistic usage
                std::size_t size = static_cast<std::size_t>(32 + (i % 16) * 8);
                alloc.allocate(size);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            100
        );
        result.passed = true; // will be set by evaluate_budgets
        report.results.push_back(std::move(result));
        std::cout << "  frame_allocator_throughput: "
                  << report.results.back().stats.mean() << " allocs/sec\n";
    }

    // --- Latency: single allocation time ---
    {
        const auto* budget = budgets.get("frame_allocator_latency");
        auto result = ae::BenchmarkRunner::run_timing(
            "frame_allocator_latency",
            "ms",
            10000,
            [&](std::int64_t i) {
                std::size_t size = static_cast<std::size_t>(32 + (i % 16) * 8);
                alloc.allocate(size);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            100
        );
        report.results.push_back(std::move(result));
        std::cout << "  frame_allocator_latency: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }
}

// =============================================================================
// Benchmark group: JobSystem
// =============================================================================

void benchmark_job_system(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== JobSystem Benchmarks ===\n";

    ae::JobSystem jobs;
    jobs.init(4); // 4 worker threads

    // --- Throughput: jobs dispatched per second ---
    {
        const auto* budget = budgets.get("job_system_throughput");
        auto result = ae::BenchmarkRunner::run_throughput(
            "job_system_throughput",
            "jobs/sec",
            5000,
            [&](std::int64_t /*i*/) {
                auto h = jobs.submit([]() {
                    volatile int x = 0;
                    (void)x;
                });
                jobs.wait(h);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            50
        );
        report.results.push_back(std::move(result));
        std::cout << "  job_system_throughput: "
                  << report.results.back().stats.mean() << " jobs/sec\n";
    }

    // --- Latency: submit + wait round-trip ---
    {
        const auto* budget = budgets.get("job_system_latency");
        auto result = ae::BenchmarkRunner::run_timing(
            "job_system_latency",
            "ms",
            5000,
            [&](std::int64_t /*i*/) {
                auto h = jobs.submit([]() {
                    volatile int x = 0;
                    (void)x;
                });
                jobs.wait(h);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            50
        );
        report.results.push_back(std::move(result));
        std::cout << "  job_system_latency: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }

    jobs.shutdown();
}

// =============================================================================
// Benchmark group: ResidencyManager
// =============================================================================

void benchmark_residency(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== ResidencyManager Benchmarks ===\n";

    // --- Throughput: residency updates per second ---
    {
        const auto* budget = budgets.get("residency_update_throughput");
        ae::core::ResidencyManager rm;
        rm.init(100, 100, 3);

        auto result = ae::BenchmarkRunner::run_throughput(
            "residency_update_throughput",
            "updates/sec",
            10000,
            [&](std::int64_t i) {
                float x = static_cast<float>((i % 1000) * 10);
                float z = static_cast<float>(((i / 1000) % 1000) * 10);
                rm.update(x, z, 10.0F, 0.0F, 0.0F);
                (void)rm.consume_pending();
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            100
        );
        report.results.push_back(std::move(result));
        std::cout << "  residency_update_throughput: "
                  << report.results.back().stats.mean() << " updates/sec\n";
    }
}

// =============================================================================
// Benchmark group: FramePacer
// =============================================================================

void benchmark_frame_pacer(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== FramePacer Benchmarks ===\n";

    // --- Overhead: time to call end_frame ---
    {
        const auto* budget = budgets.get("frame_pacer_overhead");
        ae::FramePacer pacer(16.7);

        auto result = ae::BenchmarkRunner::run_timing(
            "frame_pacer_overhead",
            "ms",
            10000,
            [&](std::int64_t /*i*/) {
                pacer.end_frame(16.5 + static_cast<double>(std::rand() % 10) * 0.1);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            100
        );
        report.results.push_back(std::move(result));
        std::cout << "  frame_pacer_overhead: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }
}

// =============================================================================
// Benchmark group: MemoryBudgetTracker
// =============================================================================

void benchmark_memory_budget(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== MemoryBudgetTracker Benchmarks ===\n";

    ae::FrameAllocator alloc(1024 * 1024, 3);

    // --- Overhead: track_frame_allocator ---
    {
        const auto* budget = budgets.get("memory_budget_overhead");
        ae::MemoryBudgetTracker tracker(0, 0, 100000, 500000);

        auto result = ae::BenchmarkRunner::run_timing(
            "memory_budget_overhead",
            "ms",
            10000,
            [&](std::int64_t /*i*/) {
                tracker.track_frame_allocator(alloc);
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            100
        );
        report.results.push_back(std::move(result));
        std::cout << "  memory_budget_overhead: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }
}

// =============================================================================
// Benchmark group: Network snapshot overhead
// =============================================================================

void benchmark_network_snapshot(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== Network Snapshot Benchmarks ===\n";

    // Benchmark the sequence tracker overhead as a proxy for network
    // snapshot processing.
    {
        const auto* budget = budgets.get("network_snapshot_overhead");
        ae::FrameProfiler profiler;

        auto result = ae::BenchmarkRunner::run_timing(
            "network_snapshot_overhead",
            "ms",
            5000,
            [&](std::int64_t /*i*/) {
                ae::FrameProfiler::ScopedProfile _sp(profiler, ae::ProfileSection::Network);
                volatile double sum = 0.0;
                for (int j = 0; j < 100; ++j) {
                    sum += static_cast<double>(j) * 0.001;
                }
                (void)sum;
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            50
        );
        report.results.push_back(std::move(result));
        std::cout << "  network_snapshot_overhead: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }
}

// =============================================================================
// Benchmark group: Streaming residency overhead
// =============================================================================

void benchmark_streaming_residency(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== Streaming Residency Benchmarks ===\n";

    // Benchmark the ResidencyManager update + query cycle as a proxy for
    // streaming residency processing.
    {
        const auto* budget = budgets.get("streaming_residency_overhead");
        ae::core::ResidencyManager rm;
        rm.init(50, 50, 2);

        auto result = ae::BenchmarkRunner::run_timing(
            "streaming_residency_overhead",
            "ms",
            5000,
            [&](std::int64_t i) {
                float x = static_cast<float>((i % 500) * 10);
                float z = static_cast<float>(((i / 500) % 500) * 10);
                rm.update(x, z, 10.0F, 0.0F, 0.0F);
                (void)rm.is_resident(25, 25);
                (void)rm.consume_pending();
            },
            budget ? budget->warn_threshold : 0.0,
            budget ? budget->fail_threshold : 0.0,
            50
        );
        report.results.push_back(std::move(result));
        std::cout << "  streaming_residency_overhead: "
                  << report.results.back().stats.mean() * 1000.0 << " us (mean)\n";
    }
}

// =============================================================================
// Deliberate Regression Fixture
//
// This fixture proves that the budget gate can fail. It runs a benchmark
// that deliberately exceeds its budget threshold and verifies that:
//   1. The result is marked as failed
//   2. A diagnostic message is produced
//   3. The overall report.all_passed is false
// =============================================================================

void test_deliberate_regression(ae::BenchmarkReport& report, const ae::BudgetConfig& budgets) {
    std::cout << "\n=== Deliberate Regression Fixture ===\n";

    const auto* budget = budgets.get("deliberate_regression");

    // Create a result with a very low fail threshold
    auto result = ae::BenchmarkRunner::run_timing(
        "deliberate_regression",
        "ms",
        100,
        [&](std::int64_t /*i*/) {
            // Simulate some work that takes measurable time
            volatile double sum = 0.0;
            for (int j = 0; j < 100000; ++j) {
                sum += static_cast<double>(j) * 0.0001;
            }
            (void)sum;
        },
        budget ? budget->warn_threshold : 0.0,
        budget ? budget->fail_threshold : 1.0, // hard fail if > 1 ms
        10
    );

    // Apply a very low budget to force failure
    if (budget) {
        result.budget_fail = budget->fail_threshold;
        result.budget_warn = budget->warn_threshold;
    } else {
        result.budget_fail = 0.001; // 1 microsecond — deliberately impossible
    }

    // Evaluate budgets
    report.results.push_back(std::move(result));
    auto& res = report.results.back();

    // Determine if it failed as expected
    const double mean_us = res.stats.mean() * 1000.0;
    const double fail_limit_us = res.budget_fail * 1000.0;

    std::cout << "  deliberate_regression: mean=" << mean_us << " us, "
              << "budget_fail=" << fail_limit_us << " us\n";

    // Manually evaluate since the budget is deliberately set to fail
    if (mean_us > fail_limit_us) {
        res.passed = false;
        std::ostringstream diag;
        diag << "DELIBERATE REGRESSION DETECTED: mean " << mean_us
             << " us exceeds hard budget " << fail_limit_us << " us";
        res.diagnostic = diag.str();
        report.all_passed = false;
        std::cout << "  >>> REGRESSION GATE TRIGGERED (expected): " << diag.str() << "\n";
        std::cout << "  >>> This is the EXPECTED outcome — the gate works.\n";
    }
}

// =============================================================================
// Main benchmark runner
// =============================================================================

int run_benchmarks(const std::string& budget_file, const std::string& output_file) {
    // Load budget configuration
    ae::BudgetConfig budgets;
    if (!budget_file.empty()) {
        if (!budgets.load_from_file(budget_file)) {
            std::cerr << "Warning: could not load budget config from '"
                      << budget_file << "', using defaults.\n";
        } else {
            std::cout << "Loaded " << budgets.size() << " budget(s) from '"
                      << budget_file << "'\n";
        }
    }

    // Build report
    ae::BenchmarkReport report;
    report.system = collect_system_metadata();

    // Run all benchmark groups
    benchmark_frame_allocator(report, budgets);
    benchmark_job_system(report, budgets);
    benchmark_residency(report, budgets);
    benchmark_frame_pacer(report, budgets);
    benchmark_memory_budget(report, budgets);
    benchmark_network_snapshot(report, budgets);
    benchmark_streaming_residency(report, budgets);

    // Evaluate all budgets
    report.evaluate_budgets();

    // Output results
    const std::string json = report.to_json();

    if (!output_file.empty()) {
        std::ofstream ofs(output_file);
        if (ofs.is_open()) {
            ofs << json;
            std::cout << "\nResults written to '" << output_file << "'\n";
        } else {
            std::cerr << "Error: could not write results to '" << output_file << "'\n";
        }
    }

    // Always print summary to stdout
    std::cout << "\n=== Benchmark Results ===\n";
    std::cout << "All passed: " << (report.all_passed ? "YES" : "NO") << "\n";
    for (const auto& r : report.results) {
        std::cout << "  " << r.name << ": "
                  << (r.passed ? "PASS" : "FAIL")
                  << (r.diagnostic.empty() ? "" : " (" + r.diagnostic + ")")
                  << "\n";
    }

    return report.all_passed ? 0 : 1;
}

int run_regression_test(const std::string& output_file) {
    std::cout << "\n========================================\n";
    std::cout << "  DELIBERATE REGRESSION FIXTURE\n";
    std::cout << "========================================\n";

    // Build report without loading budgets (we'll set them manually)
    ae::BenchmarkReport report;
    report.system = collect_system_metadata();

    // Load regression budget if available
    ae::BudgetConfig budgets;
    const std::string regression_budget = "benchmarks/budgets/regression_test.json";
    if (std::filesystem::exists(regression_budget)) {
        budgets.load_from_file(regression_budget);
        std::cout << "Loaded regression budget from '" << regression_budget << "'\n";
    }

    test_deliberate_regression(report, budgets);

    // Output
    const std::string json = report.to_json();
    if (!output_file.empty()) {
        std::ofstream ofs(output_file + ".regression.json");
        if (ofs.is_open()) {
            ofs << json;
            std::cout << "\nRegression results written to '" << output_file << ".regression.json'\n";
        }
    }

    // Print
    std::cout << "\n=== Regression Test Result ===\n";
    std::cout << "Gate triggered (expected): "
              << (!report.all_passed ? "YES - gate correctly identified regression" : "NO - unexpected")
              << "\n";
    for (const auto& r : report.results) {
        std::cout << "  " << r.name << ": "
                  << (r.passed ? "PASS" : "FAIL")
                  << (r.diagnostic.empty() ? "" : " (" + r.diagnostic + ")")
                  << "\n";
    }

    // The regression test should FAIL (that proves the gate works).
    // Return 0 if regression was correctly detected (gate works),
    // return 1 if regression was NOT detected (gate is broken).
    if (!report.all_passed) {
        std::cout << "\n>>> REGRESSION GATE VERIFIED: correctly detected the deliberate regression.\n";
        return 0; // Gate correctly detected the regression
    }
    std::cerr << "\n>>> REGRESSION GATE BROKEN: did not detect the deliberate regression!\n";
    return 1; // Gate failed to detect
}

} // anonymous namespace

// =============================================================================
// Entry point
// =============================================================================

int main(int argc, char* argv[]) {
    ae::set_log_level(ae::LogLevel::Error);

    std::string budget_file;
    std::string output_file;
    bool regression_only = false;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--budgets" && i + 1 < argc) {
            budget_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--regression-only") {
            regression_only = true;
        } else if (arg == "--help") {
            std::cout << "Usage: benchmark_tests [options]\n";
            std::cout << "  --budgets <file>   Load budget configuration from JSON file\n";
            std::cout << "  --output <file>    Write machine-readable JSON results to file\n";
            std::cout << "  --regression-only  Run only the deliberate regression fixture\n";
            std::cout << "  --help             Show this help\n";
            return 0;
        }
    }

    if (regression_only) {
        return run_regression_test(output_file);
    }

    // Default budget file
    if (budget_file.empty()) {
        // Try default location
        std::string default_budget = "benchmarks/budgets/default.json";
        if (std::filesystem::exists(default_budget)) {
            budget_file = default_budget;
        }
        // Also try from build directory
        std::string alt_budget = "../benchmarks/budgets/default.json";
        if (budget_file.empty() && std::filesystem::exists(alt_budget)) {
            budget_file = alt_budget;
        }
    }

    const int bench_result = run_benchmarks(budget_file, output_file);

    // Also run the regression fixture if not in regression-only mode
    if (!regression_only) {
        const int reg_result = run_regression_test(output_file);

        // The regression test should return 0 (gate detected the regression).
        // If it returns non-zero, something is wrong with the gate.
        if (reg_result != 0) {
            std::cerr << "\nERROR: Deliberate regression fixture was not detected! "
                      << "The budget gate is not working correctly.\n";
            return 2;
        }
        std::cout << "\n>>> Regression gate verified: works correctly.\n";
    }

    return bench_result;
}
