#pragma once

#include "ae/core/types.h"
#include "ae/network/jitter_buffer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ae {

/**
 * @brief Fixed-capacity snapshot buffer with interpolation between bracketing
 *        server snapshots.
 *
 * Clients push incoming `ServerSnapshot` packets with their arrival time.
 * To render, the client asks for the interpolated player state at
 * `render_time = now - delay`.  The interpolator finds the two snapshots
 * bracketing the render time and linearly interpolates between them.
 *
 * Edge cases:
 * - Render time ahead of all snapshots → hold newest (extrapolation).
 * - Render time behind all snapshots   → hold oldest.
 *
 * @tparam Snapshot  Must provide `.local_player` (ReplicatedPlayerState-like)
 *                   and a `.server_tick` field.
 * @tparam Capacity  Max snapshots to buffer (3 is typical).
 */
template <typename Snapshot, usize Capacity = 3>
class SnapshotInterpolator {
public:
    SnapshotInterpolator() = default;

    /**
     * @brief Push a new snapshot into the buffer.
     *
     * Out-of-order (older than newest) snapshots are silently discarded.
     */
    void push(const Snapshot& snapshot, double arrival_time) {
        if (count_ > 0 && snapshot.server_tick <= entries_[newest_].snapshot.server_tick) {
            return;  // Out-of-order or duplicate.
        }

        newest_ = (newest_ + 1) % Capacity;
        entries_[newest_] = {snapshot, arrival_time};

        if (count_ < Capacity) {
            ++count_;
        }
    }

    /**
     * @brief Interpolate the player state at a target local render time.
     *
     * @param target_time  Local monotonic time (typically `now - delay`).
     * @param out_player   Receives the interpolated player state.
     * @return true if we have at least one snapshot.
     */
    [[nodiscard]] bool interpolate(
        double target_time,
        typename std::remove_reference<decltype(Snapshot::local_player)>::type& out_player) const
    {
        using PlayerState = typename std::remove_reference<decltype(Snapshot::local_player)>::type;

        if (count_ == 0) {
            return false;
        }

        const Slot* older = nullptr;
        const Slot* newer = nullptr;

        for (usize i = 0; i < count_; ++i) {
            const auto& slot = entries_[wrap_nearest(newest_, i)];
            if (slot.arrival_time <= target_time) {
                if (older == nullptr || slot.arrival_time > older->arrival_time) {
                    older = &slot;
                }
            }
            if (slot.arrival_time >= target_time) {
                if (newer == nullptr || slot.arrival_time < newer->arrival_time) {
                    newer = &slot;
                }
            }
        }

        if (older == nullptr && newer == nullptr) {
            return false;
        }

        if (older == nullptr) {
            out_player = newer->snapshot.local_player;
            return true;
        }

        if (newer == nullptr || older == newer) {
            out_player = older->snapshot.local_player;
            return true;
        }

        const double range = newer->arrival_time - older->arrival_time;
        const float t = (range > 0.0)
            ? static_cast<float>((target_time - older->arrival_time) / range)
            : 0.0F;
        const float clamped_t = std::clamp(t, 0.0F, 1.0F);

        lerp_player(older->snapshot.local_player, newer->snapshot.local_player,
                    clamped_t, out_player);
        return true;
    }

    /**
     * @brief Also retrieve the interpolated snapshot metadata
     *        (server_tick, last_processed_input) from the "older" snapshot.
     */
    [[nodiscard]] bool get_bracketing_snapshots(
        double target_time,
        Snapshot* out_older_snap,
        Snapshot* out_newer_snap) const
    {
        if (count_ == 0) {
            return false;
        }

        const Slot* older = nullptr;
        const Slot* newer = nullptr;

        for (usize i = 0; i < count_; ++i) {
            const auto& slot = entries_[wrap_nearest(newest_, i)];
            if (slot.arrival_time <= target_time) {
                if (older == nullptr || slot.arrival_time > older->arrival_time) {
                    older = &slot;
                }
            }
            if (slot.arrival_time >= target_time) {
                if (newer == nullptr || slot.arrival_time < newer->arrival_time) {
                    newer = &slot;
                }
            }
        }

        if (older) *out_older_snap = older->snapshot;
        else      *out_older_snap = (newer ? newer->snapshot : Snapshot{});

        if (newer) *out_newer_snap = newer->snapshot;
        else      *out_newer_snap = (older ? older->snapshot : Snapshot{});

        return true;
    }

    /**
     * @brief Suggested interpolation delay based on snapshot arrival
     *        jitter.  Delegates to the internal JitterBuffer for
     *        EWMA-based adaptive delay calculation.
     */
    [[nodiscard]] float suggest_delay_seconds(float tick_rate_hz) const {
        return jitter_buffer_.suggest_delay(tick_rate_hz);
    }

    /// Access the underlying JitterBuffer for configuration.
    [[nodiscard]] JitterBuffer& jitter_buffer() { return jitter_buffer_; }
    [[nodiscard]] const JitterBuffer& jitter_buffer() const { return jitter_buffer_; }

    /** Discard all buffered snapshots. */
    void reset() {
        count_ = 0;
        newest_ = 0;
    }

    [[nodiscard]] usize size() const { return count_; }
    [[nodiscard]] static constexpr usize capacity() { return Capacity; }

private:
    struct Slot {
        Snapshot snapshot;
        double   arrival_time {0.0};
    };

    /// Walk back from `newest` by `offset` slots (0 = newest).
    usize wrap_nearest(usize base, usize offset) const {
        return (base + Capacity - offset) % Capacity;
    }

    template <typename PlayerState>
    static void lerp_player(const PlayerState& a, const PlayerState& b, float t, PlayerState& out) {
        out.position.x = a.position.x + (b.position.x - a.position.x) * t;
        out.position.y = a.position.y + (b.position.y - a.position.y) * t;
        out.position.z = a.position.z + (b.position.z - a.position.z) * t;

        out.velocity.x = a.velocity.x + (b.velocity.x - a.velocity.x) * t;
        out.velocity.y = a.velocity.y + (b.velocity.y - a.velocity.y) * t;
        out.velocity.z = a.velocity.z + (b.velocity.z - a.velocity.z) * t;

        float yaw_a = a.yaw;
        float yaw_b = b.yaw;
        float yaw_delta = yaw_b - yaw_a;
        if (yaw_delta > 180.0F)  yaw_delta -= 360.0F;
        if (yaw_delta < -180.0F) yaw_delta += 360.0F;
        out.yaw = yaw_a + yaw_delta * t;

        out.network_object_id = b.network_object_id;
        out.player_id         = b.player_id;
        out.movement_state    = b.movement_state;
        out.health            = a.health + (b.health - a.health) * t;
        out.shield            = a.shield + (b.shield - a.shield) * t;
    }

    /// Push a snapshot and feed its arrival interval into the jitter buffer.
    void push_with_jitter(const Snapshot& snapshot, double arrival_time) {
        if (count_ > 0) {
            const auto& prev = entries_[newest_];
            const float interval = static_cast<float>(arrival_time - prev.arrival_time);
            jitter_buffer_.record(interval);
        }
        push(snapshot, arrival_time);
    }

    std::array<Slot, Capacity> entries_ {};
    usize newest_ {0};
    usize count_  {0};
    mutable JitterBuffer jitter_buffer_ {};
};

}  // namespace ae
