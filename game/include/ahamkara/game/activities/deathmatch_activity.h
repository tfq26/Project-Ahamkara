#pragma once

#include "ae/network/packet_envelope.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/server_history.h"
#include "ahamkara/game/deathmatch_mode.h"
#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"
#include "wish/core/activity.h"
#include "wish/core/session_services.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ahamkara::game::activities {

/// Snapshot payload specific to deathmatch PvP.
struct RemotePlayerSnapshot {
    ae::u32 player_id {0};
    ae::u32 network_object_id {0};
    Vec3 position {};
    float yaw {0.0F};
    float health {0.0F};
};

struct DeathmatchSnapshot {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ReplicatedPlayerState local_player {};
    ae::u8 projectile_count {0};
    ProjectileState projectiles[8] {};
    ae::u8 dummy_count {0};
    TargetDummyState dummies[4] {};
    ae::u8 match_phase {0};
    float match_time {0.0F};
    ae::u32 team_score_red {0};
    ae::u32 team_score_blue {0};
    ae::u16 individual_score {0};
    ae::u8 remote_player_count {0};
    RemotePlayerSnapshot remote_players[4] {};
};

/// Result of a lag-compensated hit validation.
struct HitValidationResult {
    bool hit {false};
    int hit_dummy_idx {-1};
    Vec3 hit_position {};
    float damage {0};
    bool is_headshot {false};
};
/// Serialize a deathmatch snapshot payload into a ByteWriter.
inline bool write_deathmatch_snapshot(detail::ByteWriter& writer, const DeathmatchSnapshot& snap) {
    if (!writer.write(snap.server_tick)
        || !writer.write(snap.last_processed_input)
        || !write_player_state(writer, snap.local_player)
        || !writer.write(snap.projectile_count))
        return false;

    for (ae::u8 i = 0; i < snap.projectile_count && i < 8; ++i) {
        const auto& p = snap.projectiles[i];
        if (!write_vec3(writer, p.position)
            || !write_vec3(writer, p.velocity)
            || !writer.write(p.lifetime_seconds)
            || !writer.write_bool(p.alive)
            || !writer.write(p.client_tick))
            return false;
    }

    if (!writer.write(snap.dummy_count)) return false;
    for (ae::u8 i = 0; i < snap.dummy_count && i < 4; ++i) {
        const auto& d = snap.dummies[i];
        if (!writer.write(d.dummy_id)
            || !write_vec3(writer, d.position)
            || !writer.write(d.yaw)
            || !writer.write(d.health)
            || !writer.write_bool(d.alive))
            return false;
    }

    return writer.write(snap.match_phase)
        && writer.write(snap.match_time)
        && writer.write(snap.team_score_red)
        && writer.write(snap.team_score_blue)
        && writer.write(snap.individual_score);
}

/// Per-player session inside a deathmatch activity.
struct PlayerSlot {
    ae::NetAddress address {};
    wish::session::SessionId session_id {};
    ae::SequenceTracker sequence_tracker {};
    ae::u32 last_processed_input_sequence {0};
    ae::u32 last_received_input_sequence {0};
    bool connected {false};
    std::chrono::steady_clock::time_point last_seen {};
    ae::u32 client_tick {0};

    /// Index into World::players_ — which player this slot controls.
    ae::u32 player_index {0};
    /// Unique network object identity for this slot's player.
    ae::u32 network_object_id {kInvalidNetworkObjectId};

    // Last player state sent to this client, for delta compression.
    // Updated each time we broadcast a snapshot for this slot.
    ReplicatedPlayerState last_sent_player_state {};

    // Input buffering: the server tick() applies the most recent
    // buffered input per slot, not per-packet.  simulate_input()
    // stores input here, tick() drains it.
    PlayerInputCommand pending_input {};
    bool has_pending_input {false};

    // Per-player anti-cheat state
    Vec3 prev_position {};
    ae::u32 last_fire_tick {0};
};

/// Full deathmatch PvP activity.
/// Owns the World, game rules, match state, and per-player bookkeeping.
class DeathmatchActivity : public wish::core::IActivityBase {
public:
    DeathmatchActivity();
    ~DeathmatchActivity() override = default;

    bool initialize(const wish::core::ActivityConfig& cfg) override;
    void shutdown() override;

    bool admit_player(const wish::core::SessionAdmissionRequest& req) override;
    void remove_player(wish::session::SessionId sid) override;
    ae::u32 player_count() const override;

    void tick(float dt) override;

    void process_input(wish::session::SessionId sid,
                       const ae::PacketEnvelope& envelope,
                       ae::u32 command_sequence) override;

    /// Advance the authoritative simulation with a full input command.
    /// Called by the server after deserializing the PlayerInputCommand.
    void simulate_input(wish::session::SessionId sid, float delta_seconds,
                        const PlayerInputCommand& cmd);

    /// Lag-compensated hit validation.  Queries the server history buffer
    /// at the client's perceived tick and runs ray-vs-dummy hit detection
    /// against the historical dummy positions from that tick.
    [[nodiscard]] HitValidationResult validate_hit(
        wish::session::SessionId firing_player,
        ae::u32 client_tick,
        const Vec3& origin,
        const Vec3& forward,
        float base_damage,
        float headshot_multiplier);

    ae::usize build_snapshot_bytes(wish::session::SessionId sid,
                                   std::span<std::byte> buffer) override;

    bool is_complete() const override;

    wish::core::ActivityId activity_id() const override { return config_.id; }
    wish::core::ActivityCategory category() const override { return wish::core::ActivityCategory::PvP; }
    std::string_view activity_name() const override { return config_.name; }

    void for_each_connected_snapshot(
        void (*fn)(void* ctx, wish::session::SessionId sid,
                   const std::byte* data, ae::usize len),
        void* ctx) override;

    const World& world() const { return world_; }
    World& world() { return world_; }

    /// Load colliders and spawn points from a compiled .aelevel file.
    /// Overwrites any previously loaded map data.
    bool load_map(const std::string& path);

    /// Get the first slot (for single-player local test).
    PlayerSlot* first_slot();

private:
    PlayerSlot* find_slot(wish::session::SessionId sid);
    PlayerSlot* find_slot_by_address(const ae::NetAddress& addr);
    void build_snapshot_for_slot(PlayerSlot& slot);
    void apply_anti_cheat(PlayerSlot& slot, const PlayerInputCommand& cmd);

    wish::core::ActivityConfig config_;
    World world_;
    GameModeRules game_rules_;
    DeathmatchState dm_state_;
    ae::ServerHistoryBuffer<HistoricalState, 1024> history_buffer_;

    std::deque<PlayerSlot> slots_;
    wish::session::SessionId next_session_id_ {1};
    ae::u32 server_tick_ {0};
    static constexpr ae::u32 kFireTickCooldown {3};

    // Latest built snapshot for broadcast
    DeathmatchSnapshot current_snapshot_;

    // Buffer for per-client snapshot serialization
    std::array<std::byte, server_snapshot_packet_size()> snapshot_buffer_ {};
};

}  // namespace ahamkara::game::activities
