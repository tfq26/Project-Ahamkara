#pragma once

#include "ae/core/types.h"
#include "ae/network/interpolation_types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ae {

/**
 * @brief Generic, type-erased interpolation buffer for versioned snapshots.
 *
 * Maintains a fixed-capacity circular buffer of timestamped samples and
 * interpolates between bracketing samples at a given render time.  The
 * caller provides a lerp callback that defines how to blend two samples.
 *
 * The interpolation delay causes the buffer to render "in the past" relative
 * to the current time, absorbing network jitter.  Extrapolation is bounded
 * so that a brief network outage does not cause the render position to jump
 * wildly.
 *
 * @tparam T  The snapshot data type.  Must be copyable and assignable.
 *            Must NOT import Flashback, Wish, or game-specific types.
 */
template <typename T>
class InterpolationBuffer {
  public:
    /// Signature: lerp(a, b, t) -> interpolated value at t in [0, 1].
    using LerpFn = T (*)(const T& a, const T& b, float t);

    /**
     * @brief Construct with a lerp callback and optional config.
     *
     * @param lerp  Function pointer for blending two samples (a→b at t).
     * @param cfg   Configuration (delay, extrapolation limits, capacity).
     */
    explicit InterpolationBuffer(LerpFn lerp, const InterpolationConfig& cfg = {})
        : lerp_(lerp), config_(cfg) {
    }

    /**
     * @brief Push a timestamped sample into the buffer.
     *
     * Behavior matrix:
     * - Out-of-order (timestamp ≤ newest timestamp)  → discarded.
     * - Duplicate timestamp                          → discarded.
     * - Gap > teleport_threshold                     → buffer flushed, only the newest sample kept.
     * - Buffer full (capacity reached)               → oldest sample evicted.
     */
    void push(double timestamp, const T& sample) {
        if (count_ == 0) {
            // First sample: just insert.
            head_ = 0;
            entries_[0] = {timestamp, sample};
            count_ = 1;
            newest_timestamp_ = timestamp;
            return;
        }

        // Out-of-order or duplicate: discard silently.
        if (timestamp <= newest_timestamp_) {
            return;
        }

        // Teleport / discontinuity: gap exceeds threshold.
        if (timestamp - newest_timestamp_ > config_.teleport_threshold_seconds) {
            // Flush everything, insert only the new sample.
            count_ = 1;
            head_ = 0;
            entries_[0] = {timestamp, sample};
            newest_timestamp_ = timestamp;
            return;
        }

        // Normal insertion. Advance head (circular).
        const ae::usize next = wrap(head_ + 1);
        entries_[next] = {timestamp, sample};
        head_ = next;
        newest_timestamp_ = timestamp;

        if (count_ < config_.capacity) {
            ++count_;
        }
    }

    /**
     * @brief Sample (interpolate) the buffer at a given local render time.
     *
     * @param render_time  The local render time, typically `now - delay`.
     * @param out          Receives the interpolated sample.
     * @return InterpolationResult describing what occurred.
     */
    [[nodiscard]] InterpolationResult sample(double render_time, T& out) const {
        InterpolationResult result;
        result.render_time = render_time;

        if (count_ == 0) {
            result.status = InterpolationResult::Status::Empty;
            return result;
        }

        // Find the two bracketing samples: newest sample <= render_time (older)
        // and the next sample > render_time (newer).
        const Slot* older = nullptr;
        const Slot* newer = nullptr;

        for (ae::usize i = 0; i < count_; ++i) {
            const auto& slot = entries_[wrap_index(i)];
            if (slot.timestamp <= render_time) {
                // Candidate for older: keep the one with the largest timestamp
                // that is still <= render_time.
                if (older == nullptr || slot.timestamp > older->timestamp) {
                    older = &slot;
                }
            }
            if (slot.timestamp >= render_time) {
                // Candidate for newer: keep the one with the smallest timestamp
                // that is still >= render_time.
                if (newer == nullptr || slot.timestamp < newer->timestamp) {
                    newer = &slot;
                }
            }
        }

        // ── Edge cases ──────────────────────────────────────────────────────

        // No older and no newer samples → shouldn't happen if count_ > 0, but be safe.
        if (older == nullptr && newer == nullptr) {
            result.status = InterpolationResult::Status::Empty;
            return result;
        }

        // Underrun: render_time is before the oldest sample.
        if (older == nullptr) {
            out = newer->data;
            result.status = InterpolationResult::Status::Underrun;
            result.render_time = newer->timestamp;
            return result;
        }

        // Exact timestamp match: render_time equals a sample exactly.
        // Return the exact value with Ok status.
        if (older == newer) {
            out = older->data;
            result.status = InterpolationResult::Status::Ok;
            result.render_time = render_time;
            return result;
        }

        // Extrapolation: render_time is past the newest sample.
        if (newer == nullptr) {
            // --- Bounded extrapolation ---
            const double max_extrapolation_end =
                newest_timestamp_ + config_.max_extrapolation_seconds;

            if (render_time > max_extrapolation_end) {
                // Beyond the extrapolation limit — clamp to the newest value.
                out = older->data;
                result.status = InterpolationResult::Status::Extrapolated;
                result.render_time = max_extrapolation_end;
                return result;
            }

            // We have only one entry, or we're in the extrapolation window.
            // Use the two newest samples to compute a velocity-like extrapolation.
            if (count_ == 1) {
                out = older->data;
                result.status = InterpolationResult::Status::Extrapolated;
                result.render_time = older->timestamp;
                return result;
            }

            // Extrapolate: find the second-newest sample.
            const Slot* newest = &entries_[head_];
            // Find the entry just before newest (second newest).
            const Slot* second_newest = nullptr;
            double second_newest_ts = -std::numeric_limits<double>::max();
            for (ae::usize i = 0; i < count_; ++i) {
                const auto& slot = entries_[wrap_index(i)];
                if (&slot != newest && slot.timestamp > second_newest_ts) {
                    second_newest = &slot;
                    second_newest_ts = slot.timestamp;
                }
            }

            if (second_newest == nullptr || second_newest == newest) {
                out = oldest_data();
                result.status = InterpolationResult::Status::Extrapolated;
                result.render_time = newest_timestamp_;
                return result;
            }

            const double extrap_range = newest_timestamp_ - second_newest->timestamp;
            const float t = (extrap_range > 0.0)
                                ? static_cast<float>((render_time - newest_timestamp_) / extrap_range) + 1.0F
                                : 1.0F;

            out = lerp_(second_newest->data, newest->data, t);
            result.status = InterpolationResult::Status::Extrapolated;
            result.render_time = render_time;
            return result;
        }

        // ── Normal interpolation between two bracketing samples ─────────────
        const double range = newer->timestamp - older->timestamp;
        const float t = (range > 0.0)
                            ? std::clamp(static_cast<float>(
                                             (render_time - older->timestamp) / range),
                                         0.0F, 1.0F)
                            : 0.0F;

        out = lerp_(older->data, newer->data, t);
        result.status = InterpolationResult::Status::Ok;
        return result;
    }

    /**
     * @brief Adjust interpolation latency smoothly without moving the render
     *        time backward.
     *
     * When the network clock detects a persistent clock drift, call this to
     * gradually shift the effective render-time offset.  The adjustment is
     * applied to future `sample()` queries by biasing the render time forward
     * or backward over a time window.
     */
    void apply_latency_adjustment(double new_latency) {
        // Store the target latency; sampling code will gradually converge.
        // The actual render-time adjustment is applied externally by the
        // caller (e.g. by smoothly lerping the delay).  If the new latency
        // is less than the current one, we DO allow it to decrease as long
        // as it never causes the effective render time to move backward
        // relative to the most recent sample() call.  The caller is
        // responsible for ensuring monotonic render-time progression.
        if (new_latency >= 0.0) {
            // Only positive (or zero) latency is valid.  Negative would mean
            // rendering into the future — not supported.
            config_.delay_seconds = new_latency;
        }
    }

    /** Discard all samples. */
    void reset() {
        count_ = 0;
        head_ = 0;
        newest_timestamp_ = 0.0;
    }

    /** Replace the config (affects subsequent push/sample calls). */
    void set_config(const InterpolationConfig& cfg) {
        config_ = cfg;
        // If capacity shrinks, we may have more entries than the new capacity.
        // Cap count_ to avoid overrunning the array.
        if (count_ > config_.capacity) {
            count_ = config_.capacity;
        }
    }

    [[nodiscard]] const InterpolationConfig& config() const {
        return config_;
    }
    [[nodiscard]] ae::usize size() const {
        return count_;
    }
    [[nodiscard]] double newest_timestamp() const {
        return newest_timestamp_;
    }

  private:
    struct Slot {
        double timestamp {0.0};
        T data {};
    };

    /// Map logical index (0 = oldest, count_-1 = newest) to circular buffer index.
    [[nodiscard]] ae::usize wrap_index(ae::usize logical) const {
        // head_ points to newest.  Logical 0 = oldest, logical count_-1 = newest.
        // Oldest = (head_ + 1 - count_ + capacity) % capacity.
        return (head_ + config_.capacity + 1 - count_ + logical) % config_.capacity;
    }

    /// Wrap a raw index into the circular buffer range.
    [[nodiscard]] ae::usize wrap(ae::usize raw) const {
        return raw % config_.capacity;
    }

    [[nodiscard]] T oldest_data() const {
        return entries_[wrap_index(0)].data;
    }

    LerpFn lerp_;
    InterpolationConfig config_;

    // Circular buffer. The real size is config_.capacity.
    // We use a fixed max capacity for compilation; the effective capacity
    // is governed by config_.capacity at runtime.
    static constexpr ae::usize kMaxCapacity = 256;

    // We need a dynamic-sized container.  Use a fixed array at the
    // maximum possible size and only use config_.capacity slots.
    // This keeps the template header-only and allocation-free.
    // kMaxCapacity is generous but bounded to avoid stack overflow.
    std::array<Slot, kMaxCapacity> entries_ {};

    ae::usize head_ {0};  ///< Index of the newest entry.
    ae::usize count_ {0}; ///< Number of valid entries.
    double newest_timestamp_ {0.0};
};

} // namespace ae
