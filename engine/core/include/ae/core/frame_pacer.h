#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace ae {

/**
 * @brief Frame timing tracker with variable budget targets.
 *
 * Maintains a sliding window of frame deltas and computes:
 *   - Exponential moving average frame time and FPS
 *   - 1% low (99th percentile) frame time over the window
 *   - Budget compliance fraction
 *   - Regression detection: flags when smoothed avg drifts above budget
 *
 * Thread-safe: NO. Owned by the main loop.
 *
 * Usage:
 * @code
 *   FramePacer pacer(16.7);  // 60 fps budget
 *   pacer.start_frame();
 *   // ... do work ...
 *   pacer.end_frame();       // records delta
 *
 *   if (pacer.regression_detected()) { warn }
 * @endcode
 */
class FramePacer {
public:
    static constexpr int kHistorySize = 200;

    /**
     * @param budget_ms  Frame budget in milliseconds (default 16.7 ≈ 60 fps).
     * @param warn_threshold_ms  If smoothed avg exceeds budget by this much,
     *                           regression_detected() returns true. Default 2 ms.
     */
    explicit FramePacer(double budget_ms = 16.7,
                        double warn_threshold_ms = 2.0);

    /// Call at the very start of each frame (before any work).
    void start_frame();

    /// Call at the very end of each frame (after present).
    /// Records the elapsed frame delta.
    void end_frame();

    /// Convenience: call end_frame(dt) when the delta is known externally.
    void end_frame(double frame_delta_ms);

    // --- Accessors ---

    /// Smoothed frame time in ms (exponential moving average, α = 0.1).
    [[nodiscard]] double smooth_frame_time_ms() const { return smooth_ms_; }

    /// Smoothed FPS.
    [[nodiscard]] double smooth_fps() const {
        return smooth_ms_ > 0.0 ? 1000.0 / smooth_ms_ : 0.0;
    }

    /// Current raw frame time in ms (most recent sample).
    [[nodiscard]] double raw_frame_time_ms() const { return raw_ms_; }

    /// 1% low frame time in ms over the current window (99th percentile).
    [[nodiscard]] double percentile_01_low_ms() const { return p01_low_ms_; }

    /// Rolling average frame time over the window.
    [[nodiscard]] double rolling_avg_ms() const { return rolling_avg_ms_; }

    /// Fraction of frames within budget over the window [0, 1].
    [[nodiscard]] double budget_compliance() const { return compliance_; }

    /// Total frames recorded.
    [[nodiscard]] std::uint64_t frame_count() const { return frame_count_; }

    /// True if the smoothed moving average exceeds budget + warn_threshold.
    [[nodiscard]] bool regression_detected() const { return regression_; }

    /// True when the recent average is within budget.
    [[nodiscard]] bool pacing_healthy() const { return smooth_ms_ < budget_ms_; }

    /// Current frame budget in ms.
    [[nodiscard]] double budget_ms() const { return budget_ms_; }

    /// Change budget at runtime.
    void set_budget(double ms) { budget_ms_ = ms; }

    /// Current warn threshold in ms.
    [[nodiscard]] double warn_threshold_ms() const { return warn_threshold_ms_; }

    /// Change warn threshold.
    void set_warn_threshold(double ms) { warn_threshold_ms_ = ms; }

    /// Expose the raw history ring buffer (read-only, for debug rendering).
    [[nodiscard]] const std::array<double, kHistorySize>& history() const { return history_; }
    [[nodiscard]] int history_count() const { return history_count_; }

    /// Reset all accumulated state.
    void reset();

private:
    void recompute_percentiles();
    void update_compliance();

    double budget_ms_;
    double warn_threshold_ms_;

    // Ring buffer
    std::array<double, kHistorySize> history_{};
    int history_index_{0};
    int history_count_{0};

    // Accumulators
    std::uint64_t frame_count_{0};
    double raw_ms_{0.0};
    double smooth_ms_{0.0};
    double rolling_avg_ms_{0.0};
    double p01_low_ms_{0.0};
    double compliance_{1.0};
    bool regression_{false};

    // Internal frame timing
    double frame_start_time_{0.0};
    bool frame_active_{false};
};

}  // namespace ae
