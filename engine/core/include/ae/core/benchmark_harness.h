#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ae {

// =============================================================================
// High-resolution timer for benchmark measurements.
// =============================================================================

class BenchmarkTimer {
public:
    using Clock = std::chrono::steady_clock;

    BenchmarkTimer() { reset(); }

    /// Start (or restart) the timer.
    void reset() { start_ = Clock::now(); }

    /// Elapsed time in seconds since the last reset().
    [[nodiscard]] double elapsed_seconds() const {
        return std::chrono::duration<double>(Clock::now() - start_).count();
    }

    /// Elapsed time in milliseconds since the last reset().
    [[nodiscard]] double elapsed_ms() const {
        return elapsed_seconds() * 1000.0;
    }

private:
    Clock::time_point start_;
};

// =============================================================================
// Statistics accumulator for a set of measurements.
// =============================================================================

class BenchmarkStats {
public:
    BenchmarkStats() = default;

    /// Add a single measurement value.
    void add_sample(double value) {
        samples_.push_back(value);
    }

    /// Number of samples collected.
    [[nodiscard]] std::size_t count() const { return samples_.size(); }

    /// Minimum value.
    [[nodiscard]] double min() const {
        if (samples_.empty()) return 0.0;
        return *std::min_element(samples_.begin(), samples_.end());
    }

    /// Maximum value.
    [[nodiscard]] double max() const {
        if (samples_.empty()) return 0.0;
        return *std::max_element(samples_.begin(), samples_.end());
    }

    /// Arithmetic mean.
    [[nodiscard]] double mean() const {
        if (samples_.empty()) return 0.0;
        return sum() / static_cast<double>(samples_.size());
    }

    /// Population standard deviation.
    [[nodiscard]] double stddev() const {
        if (samples_.size() < 2) return 0.0;
        const double m = mean();
        double sq_sum = 0.0;
        for (double v : samples_) {
            const double d = v - m;
            sq_sum += d * d;
        }
        return std::sqrt(sq_sum / static_cast<double>(samples_.size()));
    }

    /// Sum of all samples.
    [[nodiscard]] double sum() const {
        return std::accumulate(samples_.begin(), samples_.end(), 0.0);
    }

    /// Median value.
    [[nodiscard]] double median() const {
        if (samples_.empty()) return 0.0;
        return percentile(50.0);
    }

    /// P-th percentile (0-100). Uses linear interpolation.
    [[nodiscard]] double percentile(double p) const {
        if (samples_.empty()) return 0.0;
        if (samples_.size() == 1) return samples_[0];

        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        const double rank = (p / 100.0) * static_cast<double>(sorted.size() - 1);
        const std::size_t lower = static_cast<std::size_t>(rank);
        const std::size_t upper = std::min(lower + 1, sorted.size() - 1);
        const double frac = rank - static_cast<double>(lower);

        return sorted[lower] * (1.0 - frac) + sorted[upper] * frac;
    }

    /// 1% low (average of worst 1% of samples).
    [[nodiscard]] double percentile_01_low() const {
        if (samples_.empty()) return 0.0;
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        const std::size_t n_worst = std::max(std::size_t{1},
            static_cast<std::size_t>(0.01 * static_cast<double>(sorted.size())));
        double sum = 0.0;
        for (std::size_t i = sorted.size() - n_worst; i < sorted.size(); ++i) {
            sum += sorted[i];
        }
        return sum / static_cast<double>(n_worst);
    }

    /// Access raw samples (for serialization).
    [[nodiscard]] const std::vector<double>& samples() const { return samples_; }

    /// Clear all samples.
    void clear() { samples_.clear(); }

private:
    std::vector<double> samples_;
};

// =============================================================================
// BenchmarkResult — single benchmark outcome.
// =============================================================================

struct BenchmarkResult {
    std::string name;           ///< Benchmark identifier.
    std::string unit;           ///< Unit of measurement (e.g. "ms", "bytes", "iterations/sec").
    BenchmarkStats stats;       ///< Statistics of the measurements.
    std::int64_t iterations{0}; ///< Number of iterations run.
    double budget_warn{0.0};    ///< Warning threshold (0 = disabled).
    double budget_fail{0.0};    ///< Failure threshold (0 = disabled).
    bool passed{true};          ///< Whether the benchmark passed its budgets.
    std::string diagnostic;     ///< Diagnostic message if failed.

    /// Serialize this result to a JSON object string.
    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << std::setprecision(6) << std::fixed;
        oss << "  {\n";
        oss << "    \"name\": \"" << escape_json(name) << "\",\n";
        oss << "    \"unit\": \"" << escape_json(unit) << "\",\n";
        if (stats.count() > 0) {
            oss << "    \"mean\": " << stats.mean() << ",\n";
            oss << "    \"median\": " << stats.median() << ",\n";
            oss << "    \"min\": " << stats.min() << ",\n";
            oss << "    \"max\": " << stats.max() << ",\n";
            oss << "    \"stddev\": " << stats.stddev() << ",\n";
            oss << "    \"p01_low\": " << stats.percentile_01_low() << ",\n";
            oss << "    \"p50\": " << stats.percentile(50.0) << ",\n";
            oss << "    \"p95\": " << stats.percentile(95.0) << ",\n";
            oss << "    \"p99\": " << stats.percentile(99.0) << ",\n";
        }
        oss << "    \"iterations\": " << iterations << ",\n";
        oss << "    \"samples\": " << stats.count() << ",\n";
        oss << "    \"budget_warn\": " << budget_warn << ",\n";
        oss << "    \"budget_fail\": " << budget_fail << ",\n";
        oss << "    \"passed\": " << (passed ? "true" : "false") << ",\n";
        oss << "    \"diagnostic\": \"" << escape_json(diagnostic) << "\"\n";
        oss << "  }";
        return oss.str();
    }

private:
    static std::string escape_json(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    }
};

// =============================================================================
// BenchmarkReport — collection of results with machine information.
// =============================================================================

struct SystemMetadata {
    std::string hostname;
    std::string os_name;
    std::string cpu_brand;
    int cpu_core_count{0};
    std::uint64_t total_ram_bytes{0};
    std::string compiler;
    std::string build_config;
    std::string timestamp;

    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << std::setprecision(6) << std::fixed;
        oss << "  \"system\": {\n";
        oss << "    \"hostname\": \"" << escape_json(hostname) << "\",\n";
        oss << "    \"os_name\": \"" << escape_json(os_name) << "\",\n";
        oss << "    \"cpu_brand\": \"" << escape_json(cpu_brand) << "\",\n";
        oss << "    \"cpu_core_count\": " << cpu_core_count << ",\n";
        oss << "    \"total_ram_bytes\": " << total_ram_bytes << ",\n";
        oss << "    \"compiler\": \"" << escape_json(compiler) << "\",\n";
        oss << "    \"build_config\": \"" << escape_json(build_config) << "\",\n";
        oss << "    \"timestamp\": \"" << escape_json(timestamp) << "\"\n";
        oss << "  }";
        return oss.str();
    }

private:
    static std::string escape_json(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    }
};

struct BenchmarkReport {
    std::vector<BenchmarkResult> results;
    SystemMetadata system;
    bool all_passed{true};

    /// Serialize the entire report to a JSON string.
    [[nodiscard]] std::string to_json() const {
        std::ostringstream oss;
        oss << std::setprecision(6) << std::fixed;
        oss << "{\n";
        oss << system.to_json() << ",\n";
        oss << "  \"benchmarks\": [\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            oss << results[i].to_json();
            if (i + 1 < results.size()) oss << ",";
            oss << "\n";
        }
        oss << "  ],\n";
        oss << "  \"all_passed\": " << (all_passed ? "true" : "false") << "\n";
        oss << "}\n";
        return oss.str();
    }

    /// Check all results against their budgets and update all_passed.
    /// For "higher_is_better" metrics (throughput, allocs/sec, jobs/sec),
    /// the budget is a minimum: values below budget_fail are failures.
    /// For "lower_is_better" metrics (latency, bytes), values above budget_fail are failures.
    void evaluate_budgets() {
        all_passed = true;
        for (auto& r : results) {
            r.passed = true;
            r.diagnostic.clear();

            if (r.budget_fail <= 0.0 && r.budget_warn <= 0.0) continue;

            // Determine direction from the unit name
            const bool higher_is_better =
                r.unit.find("sec") != std::string::npos ||
                r.unit.find("throughput") != std::string::npos ||
                r.unit.find("FPS") != std::string::npos;

            const double mean_val = r.stats.mean();

            if (higher_is_better) {
                // Higher throughput is better: budget is a minimum threshold
                if (r.budget_fail > 0.0 && mean_val < r.budget_fail) {
                    r.passed = false;
                    all_passed = false;
                    std::ostringstream diag;
                    diag << "FAIL: " << r.name << " mean " << mean_val
                         << " " << r.unit << " below minimum budget " << r.budget_fail
                         << " " << r.unit;
                    r.diagnostic = diag.str();
                } else if (r.budget_warn > 0.0 && mean_val < r.budget_warn) {
                    std::ostringstream diag;
                    diag << "WARN: " << r.name << " mean " << mean_val
                         << " " << r.unit << " below warning threshold " << r.budget_warn
                         << " " << r.unit;
                    r.diagnostic = diag.str();
                }
            } else {
                // Lower is better: budget is a maximum threshold
                if (r.budget_fail > 0.0 && mean_val > r.budget_fail) {
                    r.passed = false;
                    all_passed = false;
                    std::ostringstream diag;
                    diag << "FAIL: " << r.name << " mean " << mean_val
                         << " " << r.unit << " exceeds hard budget " << r.budget_fail
                         << " " << r.unit;
                    r.diagnostic = diag.str();
                } else if (r.budget_warn > 0.0 && mean_val > r.budget_warn) {
                    std::ostringstream diag;
                    diag << "WARN: " << r.name << " mean " << mean_val
                         << " " << r.unit << " exceeds warning threshold " << r.budget_warn
                         << " " << r.unit;
                    r.diagnostic = diag.str();
                }
            }
        }
    }
};

// =============================================================================
// BenchmarkRunner — convenience class for running benchmarks.
// =============================================================================

class BenchmarkRunner {
public:
    /// Run a benchmark function `iterations` times and collect timing stats.
    /// `bench_fn` is called with a loop index; the timer is wrapped around it.
    static BenchmarkResult run_timing(const std::string& name,
                                       const std::string& unit,
                                       std::int64_t iterations,
                                       std::function<void(std::int64_t)> bench_fn,
                                       double budget_warn = 0.0,
                                       double budget_fail = 0.0,
                                       std::int64_t warmup_iterations = 5) {
        BenchmarkResult result;
        result.name = name;
        result.unit = unit;
        result.iterations = iterations;
        result.budget_warn = budget_warn;
        result.budget_fail = budget_fail;

        // Warmup
        for (std::int64_t i = 0; i < warmup_iterations; ++i) {
            bench_fn(i);
        }

        // Timed runs in batches for more statistical samples
        constexpr std::int64_t kBatchSize = 10;
        const std::int64_t batches = std::max(std::int64_t{1}, iterations / kBatchSize);

        BenchmarkTimer timer;
        for (std::int64_t b = 0; b < batches; ++b) {
            timer.reset();
            const std::int64_t start = b * kBatchSize;
            const std::int64_t end = std::min(start + kBatchSize, iterations);
            for (std::int64_t i = start; i < end; ++i) {
                bench_fn(i);
            }
            const double elapsed = timer.elapsed_ms();
            // Convert to per-iteration or throughput
            const double per_iter = elapsed / static_cast<double>(end - start);
            result.stats.add_sample(per_iter);
        }

        return result;
    }

    /// Run a benchmark that measures throughput (e.g. allocations per second).
    static BenchmarkResult run_throughput(const std::string& name,
                                           const std::string& unit,
                                           std::int64_t iterations,
                                           std::function<void(std::int64_t)> bench_fn,
                                           double budget_warn = 0.0,
                                           double budget_fail = 0.0,
                                           std::int64_t warmup_iterations = 5) {
        BenchmarkResult result;
        result.name = name;
        result.unit = unit;
        result.iterations = iterations;
        result.budget_warn = budget_warn;
        result.budget_fail = budget_fail;

        // Warmup
        for (std::int64_t i = 0; i < warmup_iterations; ++i) {
            bench_fn(i);
        }

        // Timed batches
        constexpr std::int64_t kBatchSize = 50;
        const std::int64_t batches = std::max(std::int64_t{1}, iterations / kBatchSize);

        BenchmarkTimer timer;
        for (std::int64_t b = 0; b < batches; ++b) {
            timer.reset();
            const std::int64_t start = b * kBatchSize;
            const std::int64_t end = std::min(start + kBatchSize, iterations);
            for (std::int64_t i = start; i < end; ++i) {
                bench_fn(i);
            }
            const double elapsed = timer.elapsed_seconds();
            // Throughput = iterations per second
            const double throughput = static_cast<double>(end - start) / elapsed;
            result.stats.add_sample(throughput);
        }

        return result;
    }

    /// Run a benchmark that measures allocation volume (bytes).
    static BenchmarkResult run_allocation(const std::string& name,
                                           std::int64_t iterations,
                                           std::function<std::size_t(std::int64_t)> bench_fn,
                                           double budget_warn = 0.0,
                                           double budget_fail = 0.0,
                                           std::int64_t warmup_iterations = 5) {
        BenchmarkResult result;
        result.name = name;
        result.unit = "bytes";
        result.iterations = iterations;
        result.budget_warn = budget_warn;
        result.budget_fail = budget_fail;

        // Warmup
        for (std::int64_t i = 0; i < warmup_iterations; ++i) {
            bench_fn(i);
        }

        for (std::int64_t i = 0; i < iterations; ++i) {
            const std::size_t bytes = bench_fn(i);
            result.stats.add_sample(static_cast<double>(bytes));
        }

        return result;
    }

    /// Run a deterministic counter benchmark (e.g. allocation count, job count).
    static BenchmarkResult run_counter(const std::string& name,
                                        const std::string& unit,
                                        std::int64_t expected,
                                        std::int64_t actual,
                                        double budget_warn = 0.0,
                                        double budget_fail = 0.0) {
        BenchmarkResult result;
        result.name = name;
        result.unit = unit;
        result.iterations = 1;
        result.budget_warn = budget_warn;
        result.budget_fail = budget_fail;

        result.stats.add_sample(static_cast<double>(actual));

        // Deterministic counter checks
        if (budget_fail > 0.0 && std::abs(actual - expected) > budget_fail) {
            result.passed = false;
            std::ostringstream diag;
            diag << "FAIL: " << name << " expected " << expected
                 << " " << unit << ", got " << actual
                 << " " << unit << " (delta " << std::abs(actual - expected)
                 << " exceeds hard budget " << budget_fail << ")";
            result.diagnostic = diag.str();
        } else if (budget_warn > 0.0 && std::abs(actual - expected) > budget_warn) {
            std::ostringstream diag;
            diag << "WARN: " << name << " expected " << expected
                 << " " << unit << ", got " << actual
                 << " " << unit << " (delta " << std::abs(actual - expected)
                 << " exceeds warning threshold " << budget_warn << ")";
            result.diagnostic = diag.str();
        }

        return result;
    }
};

} // namespace ae
