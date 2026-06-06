#pragma once

#include <array>
#include <cstddef>

namespace ae {

struct RuntimeMetricsSnapshot {
    double fps {0.0};
    double frame_time_ms {0.0};
    double fps_p1_low {0.0};   // 1% low FPS
    double fps_p1_high {0.0};  // 1% high FPS
    double process_cpu_percent {0.0};
    double system_cpu_percent {0.0};
    double process_rss_mb {0.0};
    double process_virtual_mb {0.0};
    double system_used_memory_mb {0.0};
    double system_total_memory_mb {0.0};
    bool gpu_usage_available {false};
    double gpu_usage_percent {0.0};
};

class RuntimeMetricsCollector {
public:
    RuntimeMetricsCollector();

    /**
     * @brief Sample metrics for the current frame.
     * @param frame_time_seconds Frame delta time in seconds.
     * @param compute_percentiles If true, sorts the history buffer to compute
     *        1% low/high FPS. Set to true periodically (e.g. once per second)
     *        to avoid per-frame sorting cost.
     */
    RuntimeMetricsSnapshot sample(double frame_time_seconds, bool compute_percentiles = false);

private:
    struct CpuTimes {
        double process_seconds {0.0};
        double wall_seconds {0.0};
        double system_total_ticks {0.0};
        double system_active_ticks {0.0};
    };

    CpuTimes read_cpu_times() const;
    RuntimeMetricsSnapshot read_memory_metrics() const;

    static constexpr std::size_t kHistorySize = 600; // ~10 sec at 60 fps
    std::array<double, kHistorySize> frame_time_history_ {};
    std::size_t history_index_ {0};
    std::size_t history_count_ {0};

    double logical_core_count_ {1.0};
    bool has_previous_sample_ {false};
    CpuTimes previous_cpu_times_ {};
};

}  // namespace ae
