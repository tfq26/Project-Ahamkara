#pragma once

#include "ae/core/types.h"

namespace ae {

/**
 * @brief Lightweight clock-synchronization state for client/server netcode.
 *
 * Tracks the estimated offset between local (client) monotonic time and
 * the server's authoritative tick timeline.  Used by the prediction and
 * interpolation systems to reconcile render time with server state.
 *
 * This is a simple linear model:  the client measures the local wall-clock
 * time at which each snapshot arrives, records the snapshot's server tick,
 * and maintains an exponentially-weighted moving average (EWMA) of the
 * observed offset.
 */
struct NetworkClock {
    /**
     * @brief Feed a received server snapshot tick into the estimator.
     *
     * @param server_tick   The server tick contained in the snapshot.
     * @param tick_rate_hz  Server tick rate (e.g. 60.0F).
     * @param local_time_s  Current local monotonic time in seconds.
     */
    void record_snapshot(u32 server_tick, float tick_rate_hz, double local_time_s);

    /**
     * @brief Estimate the server tick corresponding to a given local time.
     *
     * Uses the smoothed offset to project the server tick forward from the
     * most recent snapshot.
     */
    [[nodiscard]] float estimate_server_time(double local_time_s, float tick_rate_hz) const;

    /**
     * @brief The raw offset (in seconds) of the most recent snapshot.
     *        Positive means the server is ahead of the client clock.
     */
    [[nodiscard]] float smoothed_offset_seconds() const {
        return offset_seconds_;
    }

    /**
     * @brief Estimated round-trip time in seconds (EWMA).
     */
    [[nodiscard]] float estimated_rtt_seconds() const {
        return rtt_seconds_;
    }

    /**
     * @brief Update the RTT estimate directly (e.g. from ping measurement).
     */
    void record_rtt(float rtt_seconds);

    /** Reset all estimates to initial state. */
    void reset();

private:
    static constexpr float kSmoothingAlpha = 0.1F;   // EWMA blend factor.
    static constexpr float kRttSmoothingAlpha = 0.2F;

    float offset_seconds_ {0.0F};
    float rtt_seconds_ {0.0F};
    bool initialized_ {false};
};

}  // namespace ae
