#pragma once

/**
 * @brief Budget configuration for performance benchmarks.
 *
 * Separates deterministic correctness budgets from statistical wall-clock
 * trend thresholds.  Deterministic budgets check invariant counters such as
 * allocation peak, object counts, or throughput minima — exceeding a hard
 * budget is a test FAILURE.  Statistical budgets track noisy wall-clock
 * timing and warn (or trend-report) without failing the build.
 *
 * Usage (in benchmark code):
 * @code
 *   static constexpr ae::BenchmarkBudget kFooBudget{
 *       .name = "foo_throughput",
 *       .deterministic_min = 1000,     // at least 1000 ops/sec
 *       .deterministic_max = 1000000,  // at most 1M ops/sec (sanity cap)
 *       .statistical_warn_ms = 50.0,   // warn if median > 50 ms
 *       .statistical_fail_ms = 100.0,  // fail if median > 100 ms
 *   };
 * @endcode
 *
 * When disabled (via AE_BENCHMARKS_ENABLED=0), benchmark targets compile to
 * a no-op main() so they do not affect release runtime behavior.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace ae {

/// Type of budget check.
enum class BudgetKind : uint8_t {
    /// Hard deterministic counter (always fails on violation).
    Deterministic,
    /// Statistical timing threshold (warn/trend on violation, no hard fail).
    Statistical,
};

/// Severity when a budget is exceeded.
enum class BudgetSeverity : uint8_t {
    /// Exceeded budget produces a diagnostic warning but test exits 0.
    Warning,
    /// Exceeded budget causes test FAILURE (non-zero exit).
    Failure,
};

/// Result of checking one budget against a measurement.
struct BudgetResult {
    std::string name;
    BudgetKind kind;
    BudgetSeverity severity;
    double measured;
    double threshold_min;
    double threshold_max;
    bool passed;
};

/**
 * @brief Budget description for one benchmark metric.
 *
 * Metrics fall into two categories:
 *
 * **Deterministic** (`deterministic_min` / `deterministic_max`):
 *   Invariant counters that must stay within bounds.  Examples:
 *   allocated bytes, object count, throughput (operations/second),
 *   cache misses.  Exceeding a hard threshold is always a test failure.
 *
 * **Statistical** (`statistical_warn_ms` / `statistical_fail_ms`):
 *   Wall-clock timing metrics that vary across hardware.  These use
 *   statistically defensible thresholds or trend reporting rather than
 *   hard pass/fail.  Crossing `statistical_fail_ms` produces a failure;
 *   crossing `statistical_warn_ms` produces a warning only.
 */
struct BenchmarkBudget {
    /// Human-readable metric name (snake_case).
    const char* name;

    // ── Deterministic (hard) budgets ──────────────────────────────────
    /// Minimum acceptable deterministic value (e.g. min throughput).
    double deterministic_min;
    /// Maximum acceptable deterministic value (e.g. max allocation).
    double deterministic_max;

    // ── Statistical (noisy timing) budgets ────────────────────────────
    /// Warning threshold in ms — exceeded median produces a warning.
    double statistical_warn_ms;
    /// Failure threshold in ms — exceeded median is a test failure.
    double statistical_fail_ms;

    /// Check a measurement against both budget types.
    [[nodiscard]] BudgetResult check_deterministic(double measured) const noexcept {
        bool ok = (measured >= deterministic_min) && (measured <= deterministic_max);
        BudgetSeverity sev = ok ? BudgetSeverity::Warning : BudgetSeverity::Failure;
        return BudgetResult{
            .name = std::string(name) + ".deterministic",
            .kind = BudgetKind::Deterministic,
            .severity = sev,
            .measured = measured,
            .threshold_min = deterministic_min,
            .threshold_max = deterministic_max,
            .passed = ok,
        };
    }

    [[nodiscard]] BudgetResult check_statistical(double median_ms) const noexcept {
        if (median_ms > statistical_fail_ms) {
            return BudgetResult{
                .name = std::string(name) + ".statistical",
                .kind = BudgetKind::Statistical,
                .severity = BudgetSeverity::Failure,
                .measured = median_ms,
                .threshold_min = 0.0,
                .threshold_max = statistical_fail_ms,
                .passed = false,
            };
        }
        bool warned = median_ms > statistical_warn_ms;
        return BudgetResult{
            .name = std::string(name) + ".statistical",
            .kind = BudgetKind::Statistical,
            .severity = warned ? BudgetSeverity::Warning : BudgetSeverity::Warning,
            .measured = median_ms,
            .threshold_min = 0.0,
            .threshold_max = statistical_warn_ms,
            .passed = !warned,
        };
    }
};

/**
 * @brief Machine-readable context metadata for a benchmark run.
 *
 * Emitted as JSON at the start of a benchmark run so CI can correlate
 * results to the host environment.
 */
struct BenchmarkContext {
    std::string hostname;
    std::string os_name;
    std::string cpu_model;
    int cpu_cores;
    uint64_t memory_bytes;
    std::string compiler;
    std::string build_type;
};

/**
 * @brief Record a single benchmark sample (time + allocation stats).
 */
struct BenchmarkSample {
    double elapsed_ms;
    std::size_t bytes_allocated;
    std::size_t peak_bytes;
    uint64_t operation_count;
};

/**
 * @brief Aggregated statistics for one benchmark.
 */
struct BenchmarkStats {
    std::string name;
    int sample_count;

    // Timing
    double min_ms;
    double max_ms;
    double median_ms;
    double mean_ms;
    double p01_ms;   // 1% low (99th percentile — worst frames)

    // Allocation
    std::size_t min_bytes;
    std::size_t max_bytes;
    std::size_t mean_bytes;

    // Throughput
    double ops_per_sec;

    // Budget check results
    std::vector<BudgetResult> budget_results;
};

/**
 * @brief Run a benchmark function N times, collecting samples.
 *
 * The benchmark function receives an iteration index [0, N) and should
 * perform its work, returning a BenchmarkSample.
 *
 * @tparam Fn  void(int iteration, std::vector<BenchmarkSample>& samples)
 */
template <typename Fn>
BenchmarkStats run_benchmark(
    const char* name,
    int num_samples,
    Fn&& fn,
    const BenchmarkBudget* budget = nullptr)
{
    std::vector<BenchmarkSample> samples;
    samples.reserve(static_cast<std::size_t>(num_samples));

    // Warmup (2 iterations)
    for (int i = 0; i < 2; ++i) {
        fn(i, samples);
    }

    samples.clear();

    // Measured iterations
    for (int i = 0; i < num_samples; ++i) {
        fn(i, samples);
    }

    // Compute stats
    BenchmarkStats stats{};
    stats.name = name;
    stats.sample_count = num_samples;

    if (samples.empty()) return stats;

    // Sort by time for percentiles
    std::vector<double> times;
    std::vector<std::size_t> bytes;
    times.reserve(samples.size());
    bytes.reserve(samples.size());

    double total_ms = 0.0;
    uint64_t total_ops = 0;
    stats.min_ms = samples[0].elapsed_ms;
    stats.max_ms = samples[0].elapsed_ms;
    stats.min_bytes = samples[0].bytes_allocated;
    stats.max_bytes = samples[0].bytes_allocated;

    for (const auto& s : samples) {
        times.push_back(s.elapsed_ms);
        bytes.push_back(s.bytes_allocated);
        total_ms += s.elapsed_ms;
        total_ops += s.operation_count;
        if (s.elapsed_ms < stats.min_ms) stats.min_ms = s.elapsed_ms;
        if (s.elapsed_ms > stats.max_ms) stats.max_ms = s.elapsed_ms;
        if (s.bytes_allocated < stats.min_bytes) stats.min_bytes = s.bytes_allocated;
        if (s.bytes_allocated > stats.max_bytes) stats.max_bytes = s.bytes_allocated;
    }
    stats.mean_ms = total_ms / static_cast<double>(samples.size());

    std::sort(times.begin(), times.end());
    std::sort(bytes.begin(), bytes.end());

    stats.median_ms = times[times.size() / 2];
    stats.mean_bytes = bytes[bytes.size() / 2];

    // 1% low: worst 1% of frames (99th percentile of slowness)
    if (!times.empty()) {
        int p01_idx = static_cast<int>(times.size() * 0.99);
        if (p01_idx >= static_cast<int>(times.size())) p01_idx = static_cast<int>(times.size()) - 1;
        stats.p01_ms = times[p01_idx];
    }

    // Throughput (ops/sec)
    double total_sec = total_ms / 1000.0;
    stats.ops_per_sec = total_sec > 0.0
        ? static_cast<double>(total_ops) / total_sec
        : 0.0;

    // Budget checks
    if (budget) {
        stats.budget_results.push_back(budget->check_deterministic(stats.ops_per_sec));
        stats.budget_results.push_back(budget->check_statistical(stats.median_ms));
    }

    return stats;
}

/**
 * @brief Emit benchmark results as JSON to stdout.
 */
inline void emit_benchmark_json(const BenchmarkStats& stats) {
    // Header omitted — caller wraps in array if needed.
    // Each stat blob is self-describing.
    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"" << stats.name << "\",\n";
    std::cout << "  \"samples\": " << stats.sample_count << ",\n";
    std::cout << "  \"min_ms\": " << stats.min_ms << ",\n";
    std::cout << "  \"max_ms\": " << stats.max_ms << ",\n";
    std::cout << "  \"median_ms\": " << stats.median_ms << ",\n";
    std::cout << "  \"mean_ms\": " << stats.mean_ms << ",\n";
    std::cout << "  \"p01_ms\": " << stats.p01_ms << ",\n";
    std::cout << "  \"ops_per_sec\": " << stats.ops_per_sec << ",\n";
    std::cout << "  \"budgets\": [\n";
    for (std::size_t i = 0; i < stats.budget_results.size(); ++i) {
        const auto& b = stats.budget_results[i];
        std::cout << "    {\n";
        std::cout << "      \"name\": \"" << b.name << "\",\n";
        std::cout << "      \"kind\": \"" << (b.kind == BudgetKind::Deterministic ? "deterministic" : "statistical") << "\",\n";
        std::cout << "      \"severity\": \"" << (b.severity == BudgetSeverity::Failure ? "failure" : "warning") << "\",\n";
        std::cout << "      \"measured\": " << b.measured << ",\n";
        std::cout << "      \"threshold_min\": " << b.threshold_min << ",\n";
        std::cout << "      \"threshold_max\": " << b.threshold_max << ",\n";
        std::cout << "      \"passed\": " << (b.passed ? "true" : "false") << "\n";
        std::cout << "    }";
        if (i < stats.budget_results.size() - 1) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}";
}

/**
 * @brief Emit machine context as JSON preamble.
 */
inline void emit_context_json(const BenchmarkContext& ctx) {
    std::cout << "{\n";
    std::cout << "  \"context\": {\n";
    std::cout << "    \"hostname\": \"" << ctx.hostname << "\",\n";
    std::cout << "    \"os\": \"" << ctx.os_name << "\",\n";
    std::cout << "    \"cpu\": \"" << ctx.cpu_model << "\",\n";
    std::cout << "    \"cores\": " << ctx.cpu_cores << ",\n";
    std::cout << "    \"memory_bytes\": " << ctx.memory_bytes << ",\n";
    std::cout << "    \"compiler\": \"" << ctx.compiler << "\",\n";
    std::cout << "    \"build_type\": \"" << ctx.build_type << "\"\n";
    std::cout << "  },\n";
    std::cout << "  \"benchmarks\": [\n";
}

/**
 * @brief Emit the closing JSON bracket.
 */
inline void emit_json_footer() {
    std::cout << "\n  ]\n}\n";
}

/// Report budget failures. Returns non-zero count of hard failures.
inline int report_budget_results(const BenchmarkStats& stats) {
    int failures = 0;
    for (const auto& r : stats.budget_results) {
        if (!r.passed) {
            if (r.severity == BudgetSeverity::Failure) {
                std::cerr << "[FAIL] " << r.name
                          << ": measured=" << r.measured
                          << " (threshold_max=" << r.threshold_max
                          << ", threshold_min=" << r.threshold_min << ")\n";
                ++failures;
            } else {
                std::cout << "[WARN] " << r.name
                          << ": measured=" << r.measured
                          << " (threshold_max=" << r.threshold_max << ")\n";
            }
        }
    }
    return failures;
}

}  // namespace ae
