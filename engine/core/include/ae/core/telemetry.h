#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae {

/**
 * @brief A monotonically increasing counter metric.
 *
 * Thread-safe via std::atomic. Suitable for counting events,
 * packets, frames, allocs, etc.
 */
class TelemetryCounter {
public:
    explicit TelemetryCounter(std::string_view name);
    ~TelemetryCounter();

    /// Increment the counter by `delta` (default 1).
    void add(std::int64_t delta = 1) { value_.fetch_add(delta, std::memory_order_relaxed); }

    /// Read and reset the counter to zero (for periodic snapshot).
    [[nodiscard]] std::int64_t reset();

    /// Current raw value without resetting.
    [[nodiscard]] std::int64_t peek() const { return value_.load(std::memory_order_acquire); }

    [[nodiscard]] const std::string& name() const { return name_; }

private:
    std::string name_;
    std::atomic<std::int64_t> value_{0};
};

/**
 * @brief A point-in-time gauge metric.
 *
 * Represents a value that can go up and down, e.g. memory usage,
 * active connections, pool depth.
 */
class TelemetryGauge {
public:
    explicit TelemetryGauge(std::string_view name);
    ~TelemetryGauge();

    /// Set the gauge to an absolute value.
    void set(std::int64_t value) { value_.store(value, std::memory_order_relaxed); }

    /// Add delta (positive or negative).
    void add(std::int64_t delta) { value_.fetch_add(delta, std::memory_order_relaxed); }

    [[nodiscard]] std::int64_t get() const { return value_.load(std::memory_order_acquire); }

    /// Snapshot-style: return current and reset to zero.
    [[nodiscard]] std::int64_t reset();

    [[nodiscard]] const std::string& name() const { return name_; }

private:
    std::string name_;
    std::atomic<std::int64_t> value_{0};
};

/**
 * @brief A bucketed histogram metric.
 *
 * Records observations into pre-defined bucket boundaries + an overflow
 * bucket. Useful for frame times, allocation sizes, latency, etc.
 * Thread-safe via mutex.
 */
class TelemetryHistogram {
public:
    /**
     * @param name        Metric name.
     * @param boundaries  Upper bound for each bucket. The last bucket
     *                    catches everything >= the final boundary.
     */
    TelemetryHistogram(std::string_view name, std::vector<double> boundaries);
    ~TelemetryHistogram();

    /// Record an observation.
    void observe(double value);

    /// Snapshot: return current bucket counts and reset all buckets to zero.
    [[nodiscard]] std::vector<std::int64_t> reset();

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const std::vector<double>& boundaries() const { return boundaries_; }

private:
    std::string name_;
    std::vector<double> boundaries_;
    std::vector<std::int64_t> buckets_;
    mutable std::mutex mutex_;
};

// -----------------------------------------------------------------------
// TelemetrySystem — owns all metrics, periodic flush, env-driven config
// -----------------------------------------------------------------------

struct TelemetrySnapshot {
    struct CounterSample {
        std::string name;
        std::int64_t value;
    };
    struct GaugeSample {
        std::string name;
        std::int64_t value;
    };
    struct HistogramSample {
        std::string name;
        std::vector<double> boundaries;
        std::vector<std::int64_t> buckets;
    };

    std::vector<CounterSample> counters;
    std::vector<GaugeSample> gauges;
    std::vector<HistogramSample> histograms;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Central telemetry registry.
 *
 * Singleton. All metrics register themselves. Call `flush()` periodically
 * (e.g. once per second) to emit a CSV-formatted snapshot to the log.
 *
 * Enable with ConfigVar `telemetry.enabled` or env var `AE_TELEMETRY=1`.
 * Flush interval controlled by `telemetry.flush_interval_ms` (default 1000).
 */
class TelemetrySystem {
public:
    static TelemetrySystem& instance();

    /// Register a counter (called automatically by TelemetryCounter constructor).
    void register_counter(TelemetryCounter* counter);
    /// Register a gauge (called automatically by TelemetryGauge constructor).
    void register_gauge(TelemetryGauge* gauge);
    /// Register a histogram (called automatically by TelemetryHistogram constructor).
    void register_histogram(TelemetryHistogram* histogram);

    /// Take a snapshot of all registered metrics and reset counters/histograms.
    [[nodiscard]] TelemetrySnapshot snapshot();

    /// Write current snapshot to structured log lines (CSV format).
    void flush();

    /// Convenience: call snapshot() and write to a CSV file at `path`.
    void flush_to_csv(const std::string& path);

    /// Enable/disable telemetry collection.
    void set_enabled(bool enabled) { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const { return enabled_; }

    /// Set flush interval in milliseconds.
    void set_flush_interval_ms(int ms) { flush_interval_ms_ = ms; }
    [[nodiscard]] int flush_interval_ms() const { return flush_interval_ms_; }

    /// Remove all registered metrics (for testing).
    void clear();

    /// Deregister a previously registered counter.
    void deregister_counter(const TelemetryCounter* counter);
    /// Deregister a previously registered gauge.
    void deregister_gauge(const TelemetryGauge* gauge);
    /// Deregister a previously registered histogram.
    void deregister_histogram(const TelemetryHistogram* histogram);

private:
    TelemetrySystem() = default;
    ~TelemetrySystem() = default;
    TelemetrySystem(const TelemetrySystem&) = delete;
    TelemetrySystem& operator=(const TelemetrySystem&) = delete;

    mutable std::mutex mutex_;
    bool enabled_{false};
    int flush_interval_ms_{1000};

    std::vector<TelemetryCounter*> counters_;
    std::vector<TelemetryGauge*> gauges_;
    std::vector<TelemetryHistogram*> histograms_;
};

}  // namespace ae
