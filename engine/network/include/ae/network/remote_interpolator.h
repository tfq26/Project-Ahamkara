#pragma once

#include "ae/core/math.h"
#include "ae/core/types.h"
#include "ae/network/snapshot_interpolator.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace ae {

/**
 * @brief Wraps SnapshotInterpolator and provides explicit per-remote-player
 *        interpolation across server snapshots.
 *
 * For each server snapshot pushed, the local player state is interpolated
 * via the inner SnapshotInterpolator.  Remote players (other clients)
 * are tracked individually in per-player ring buffers.  Callers can then
 * query `interpolate_remote_player()` at a target render time to obtain
 * a smoothed (lerped) position and yaw for a specific remote player.
 *
 * @tparam Snapshot  Must provide `.local_player`, `.remote_players[]`,
 *                   `.remote_player_count`, and `.server_tick`.
 * @tparam Capacity  Max snapshots to buffer for the local player (3 is typical).
 */
template <typename Snapshot, usize Capacity = 3>
class RemoteInterpolator {
public:
    using PlayerState = typename std::remove_reference<
        decltype(Snapshot::local_player)>::type;

    RemoteInterpolator() = default;

    /// Push a new server snapshot.  Delegates local-player interpolation to the
    /// inner interpolator and also extracts remote-player entries into per-ID
    /// ring buffers.
    void push(const Snapshot& snapshot, double arrival_time) {
        interpolator_.push(snapshot, arrival_time);

        for (ae::u8 i = 0; i < snapshot.remote_player_count; ++i) {
            const auto& rp = snapshot.remote_players[i];

            RemoteSnapshot rs;
            rs.player_id   = rp.player_id;
            rs.position    = rp.position;
            rs.yaw         = rp.yaw;
            rs.health      = rp.health;
            rs.server_tick = snapshot.server_tick;
            rs.arrival_time = arrival_time;

            auto& ring = remote_histories_[rp.player_id];
            ring.push_back(rs);
            if (ring.size() > kMaxRemoteHistory) {
                ring.pop_front();
            }
        }
    }

    /**
     * @brief Interpolate a remote player's state at a target render time.
     *
     * @param player_id    Remote player to interpolate.
     * @param target_time  Local monotonic render time (typically `now - delay`).
     * @param out_position Receives lerped position.
     * @param out_yaw      Receives lerped yaw (handles ±360 wrap).
     * @return true if data was available for this player.
     */
    [[nodiscard]] bool interpolate_remote_player(
        ae::u32 player_id,
        double target_time,
        Vec3& out_position,
        float& out_yaw) const
    {
        auto it = remote_histories_.find(player_id);
        if (it == remote_histories_.end() || it->second.empty()) {
            return false;
        }

        const auto& history = it->second;

        const RemoteSnapshot* older = nullptr;
        const RemoteSnapshot* newer = nullptr;

        for (const auto& rs : history) {
            if (rs.arrival_time <= target_time) {
                if (!older || rs.arrival_time > older->arrival_time) {
                    older = &rs;
                }
            }
            if (rs.arrival_time >= target_time) {
                if (!newer || rs.arrival_time < newer->arrival_time) {
                    newer = &rs;
                }
            }
        }

        if (!older && !newer) return false;
        if (!older) older = newer;
        if (!newer) newer = older;

        // Single point — no interpolation possible.
        if (older == newer) {
            out_position = older->position;
            out_yaw      = older->yaw;
            return true;
        }

        const double range = newer->arrival_time - older->arrival_time;
        const float t = (range > 0.0)
            ? std::clamp(static_cast<float>(
                  (target_time - older->arrival_time) / range), 0.0F, 1.0F)
            : 0.0F;

        // Linear position interpolation.
        out_position.x = older->position.x +
                         (newer->position.x - older->position.x) * t;
        out_position.y = older->position.y +
                         (newer->position.y - older->position.y) * t;
        out_position.z = older->position.z +
                         (newer->position.z - older->position.z) * t;

        // Yaw with ±360° wrap handling.
        float yaw_delta = newer->yaw - older->yaw;
        if (yaw_delta > 180.0F)  yaw_delta -= 360.0F;
        if (yaw_delta < -180.0F) yaw_delta += 360.0F;
        out_yaw = older->yaw + yaw_delta * t;

        return true;
    }

    /// Discard all buffered snapshots and remote-player state.
    void reset() {
        interpolator_.reset();
        remote_histories_.clear();
    }

    /// Delegate local-player interpolation to the inner interpolator.
    [[nodiscard]] bool interpolate(
        double target_time,
        PlayerState& out_player) const
    {
        return interpolator_.interpolate(target_time, out_player);
    }

    /// Number of local-player snapshots buffered.
    [[nodiscard]] usize size() const   { return interpolator_.size(); }
    static constexpr usize capacity()  { return Capacity; }

    /// Number of distinct remote players being tracked.
    [[nodiscard]] usize remote_player_count() const {
        return remote_histories_.size();
    }

    /// Suggested interpolation delay based on snapshot arrival jitter.
    [[nodiscard]] float suggest_delay_seconds(float tick_rate_hz) const {
        return interpolator_.suggest_delay_seconds(tick_rate_hz);
    }

private:
    struct RemoteSnapshot {
        ae::u32 player_id   {0};
        Vec3    position    {};
        float   yaw         {0};
        float   health      {100};
        ae::u32 server_tick {0};
        double  arrival_time {0};
    };

    /// Keep up to 6 entries per remote player — enough to cover typical
    /// interpolation windows even under significant jitter.
    static constexpr usize kMaxRemoteHistory = 6;

    SnapshotInterpolator<Snapshot, Capacity> interpolator_;
    std::unordered_map<ae::u32, std::deque<RemoteSnapshot>> remote_histories_;
};

}  // namespace ae
