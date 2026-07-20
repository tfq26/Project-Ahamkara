#pragma once

#include "ae/network/sequence_tracker.h"
#include "ae/network/udp_socket.h"
#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "wish/core/activity.h"

#include <chrono>
#include <deque>
#include <span>
#include <string>

namespace ahamkara::game::activities {

/// Lightweight enemy entity state for PvE horde mode.
struct EnemyState {
    ae::u32  enemy_id {0};
    Vec3     position {};
    Vec3     velocity {};
    float    yaw {0.0F};
    float    health {100.0F};
    bool     alive {true};
    ae::u32  enemy_type {0};  // 0=grunt, 1=brute, 2=flyer
};

/// Snapshot payload for PvE Horde mode.
struct HordeSnapshot {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ReplicatedPlayerState local_player {};
    ae::u8 enemy_count {0};
    EnemyState enemies[16] {};
    ae::u8 wave_number {1};
    float wave_timer {0.0F};
    float objective_health {100.0F};
    ae::u32 team_score {0};
};

inline bool write_horde_snapshot(detail::ByteWriter& writer, const HordeSnapshot& snap) {
    if (!writer.write(snap.server_tick)
        || !writer.write(snap.last_processed_input)
        || !write_player_state(writer, snap.local_player)
        || !writer.write(snap.enemy_count))
        return false;

    for (ae::u8 i = 0; i < snap.enemy_count && i < 16; ++i) {
        const auto& e = snap.enemies[i];
        if (!write_vec3(writer, e.position)
            || !write_vec3(writer, e.velocity)
            || !writer.write(e.yaw)
            || !writer.write(e.health)
            || !writer.write_bool(e.alive)
            || !writer.write(e.enemy_type)
            || !writer.write(e.enemy_id))
            return false;
    }

    return writer.write(snap.wave_number)
        && writer.write(snap.wave_timer)
        && writer.write(snap.objective_health)
        && writer.write(snap.team_score);
}

/// PvE Horde Mode — players cooperate against AI waves.
/// Defend an objective against increasing waves of enemies.
class HordeActivity : public wish::core::IActivityBase {
public:
    HordeActivity();
    ~HordeActivity() override = default;

    bool initialize(const wish::core::ActivityConfig& cfg) override;
    void shutdown() override;

    bool admit_player(const wish::core::SessionAdmissionRequest& req) override;
    void remove_player(wish::session::SessionId sid) override;
    ae::u32 player_count() const override;

    void tick(float dt) override;
    void process_input(wish::session::SessionId sid,
                       const wish::PacketEnvelope& envelope,
                       ae::u32 command_sequence) override;

    ae::usize build_snapshot_bytes(wish::session::SessionId sid,
                                   std::span<std::byte> buffer) override;

    bool is_complete() const override;

    wish::core::ActivityId activity_id() const override { return config_.id; }
    wish::core::ActivityCategory category() const override {
        return wish::core::ActivityCategory::PvE;
    }
    std::string_view activity_name() const override { return config_.name; }

    void for_each_connected_snapshot(
        void (*fn)(void* ctx, wish::session::SessionId sid,
                   const std::byte* data, ae::usize len),
        void* ctx) override;

private:
    struct Slot {
        ae::NetAddress address {};
        wish::session::SessionId session_id {};
        ae::SequenceTracker seq_tracker {};
        ae::u32 last_processed {0};
        ae::u32 last_received {0};
        bool connected {false};
        std::chrono::steady_clock::time_point last_seen {};
    };

    Slot* find_slot(wish::session::SessionId sid);

    void spawn_wave();
    void tick_enemies(float dt);
    void build_current_snapshot();

    wish::core::ActivityConfig config_;
    std::deque<Slot> slots_;
    wish::session::SessionId next_sid_ {1};
    ae::u32 server_tick_ {0};

    EnemyState enemies_[16] {};
    ae::u8 enemy_count_ {0};
    ae::u8 wave_number_ {1};
    float wave_timer_ {0.0F};
    float time_between_waves_ {30.0F};
    float objective_health_ {100.0F};
    float max_objective_health_ {100.0F};
    ae::u32 team_score_ {0};

    HordeSnapshot current_snapshot_;
    std::array<std::byte, 2048> snapshot_buffer_ {};
};

}  // namespace ahamkara::game::activities
