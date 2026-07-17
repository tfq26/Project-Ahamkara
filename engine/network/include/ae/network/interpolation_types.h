#pragma once

#include "ae/core/types.h"

#include <cstddef>

namespace ae {

/**
 * @brief Timestamped snapshot value.
 *
 * A lightweight wrapper that pairs a timestamp with a sample value.
 * Users of InterpolationBuffer push these; the buffer sorts by timestamp.
 *
 * @tparam T The snapshot data type (e.g. a trivial struct holding position).
 */
template <typename T>
struct TimestampedSnapshot {
    double timestamp {0.0}; ///< Server or monotonic timestamp in seconds.
    T data {};              ///< The snapshot payload.
};

/**
 * @brief Configuration for InterpolationBuffer.
 */
struct InterpolationConfig {
    /// How far behind "now" the render time is lagged (seconds).
    /// Larger values smooth more jitter at the cost of added latency.
    double delay_seconds {0.1};

    /// Maximum time beyond the newest sample we are willing to extrapolate
    /// before clamping to the newest value (seconds).
    double max_extrapolation_seconds {0.05};

    /// If the gap between consecutive sample timestamps exceeds this value,
    /// a discontinuity / teleport is assumed and the buffer is flushed.
    double teleport_threshold_seconds {1.0};

    /// Maximum number of samples retained in the circular buffer.
    /// Oldest samples are silently evicted when capacity is exceeded.
    ae::usize capacity {64};
};

/**
 * @brief Result of an interpolation query.
 */
struct InterpolationResult {
    enum class Status {
        Ok,           ///< Normal interpolation between two samples.
        Extrapolated, ///< Closest available: extrapolated past newest.
        Underrun,     ///< Render time older than any sample; returned oldest.
        Empty,        ///< Buffer has no samples.
    };

    Status status {Status::Empty};
    double render_time {0.0}; ///< The actual render time used (may be clamped).
};

} // namespace ae
