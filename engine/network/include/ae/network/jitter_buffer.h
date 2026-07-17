#pragma once

#include "ae/core/types.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace ae {

/**
 * @brief Adaptive jitter buffer that tracks arrival time variance and
 *        computes a dynamically-adjusted interpolation delay.
 *
 * Uses an EWMA on both the average arrival interval and the deviation
 * (jitter) from it.  The suggested delay is:
 *
 *   delay = base_delay + jitter_estimate * kJitterMultiplier
 *
 * where base_delay is configurable (default: 2 × tick interval) and
 * jitter_estimate is the EWMA-smoothed absolute deviation from the
 * average arrival interval.
 *
 * This replaces the ad-hoc formula in
 * SnapshotInterpolator::suggest_delay_seconds().
 */
class JitterBuffer {
public:
    explicit JitterBuffer(
        float base_delay_multiplier = 2.0F,
        float min_delay_seconds = 0.016F,
        float max_delay_seconds = 0.5F,
        float ewma_alpha = 0.125F)
        : base_delay_multiplier_(base_delay_multiplier)
        , min_delay_seconds_(min_delay_seconds)
        , max_delay_seconds_(max_delay_seconds)
        , ewma_alpha_(ewma_alpha) {}

    /**
     * @brief Record the arrival of a new packet.
     *
     * @param arrival_interval  Time since the previous packet (seconds).
     *        Pass 0.0F for the first packet.
     */
    void record(float arrival_interval) {
        if (!initialized_) {
            avg_interval_ = arrival_interval;
            jitter_estimate_ = 0.0F;
            initialized_ = true;
            return;
        }

        // EWMA on average arrival interval.
        avg_interval_ = avg_interval_ * (1.0F - ewma_alpha_)
                      + arrival_interval * ewma_alpha_;

        // EWMA on absolute deviation (jitter).
        const float deviation = std::fabs(arrival_interval - avg_interval_);
        jitter_estimate_ = jitter_estimate_ * (1.0F - ewma_alpha_)
                          + deviation * ewma_alpha_;
    }

    /**
     * @brief Get the suggested interpolation delay based on current
     *        jitter estimate and tick rate.
     *
     * @param tick_rate_hz  Expected packet rate (e.g. 60.0F).
     * @return Suggested delay in seconds.
     */
    [[nodiscard]] float suggest_delay(float tick_rate_hz) const {
        if (!initialized_) {
            return 1.0F / tick_rate_hz;
        }

        const float tick_interval = 1.0F / tick_rate_hz;
        const float base_delay = tick_interval * base_delay_multiplier_;
        const float jitter_buf = jitter_estimate_ * kJitterMultiplier;

        return std::clamp(base_delay + jitter_buf,
                          min_delay_seconds_,
                          max_delay_seconds_);
    }

    /**
     * @brief Current jitter estimate in seconds.
     */
    [[nodiscard]] float jitter_estimate() const { return jitter_estimate_; }

    /**
     * @brief Current average arrival interval.
     */
    [[nodiscard]] float avg_interval() const { return avg_interval_; }

    /// Reset to initial state.
    void reset() {
        avg_interval_ = 0.0F;
        jitter_estimate_ = 0.0F;
        initialized_ = false;
    }

    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ── Config ──────────────────────────────────────────────────────────

    void set_base_delay_multiplier(float m) { base_delay_multiplier_ = m; }
    void set_min_delay_seconds(float s) { min_delay_seconds_ = s; }
    void set_max_delay_seconds(float s) { max_delay_seconds_ = s; }
    void set_ewma_alpha(float a) { ewma_alpha_ = a; }

    [[nodiscard]] float base_delay_multiplier() const { return base_delay_multiplier_; }
    [[nodiscard]] float min_delay_seconds() const { return min_delay_seconds_; }
    [[nodiscard]] float max_delay_seconds() const { return max_delay_seconds_; }
    [[nodiscard]] float ewma_alpha() const { return ewma_alpha_; }

private:
    /// How many jitter estimates to add to the base delay.
    static constexpr float kJitterMultiplier = 4.0F;

    float avg_interval_ {0.0F};
    float jitter_estimate_ {0.0F};
    float base_delay_multiplier_;
    float min_delay_seconds_;
    float max_delay_seconds_;
    float ewma_alpha_;
    bool initialized_ {false};
};

}  // namespace ae
