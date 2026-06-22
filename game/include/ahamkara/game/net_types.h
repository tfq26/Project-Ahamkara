#pragma once

#include "ae/core/packet_envelope.h"
#include "ae/core/types.h"

#include <cmath>
#include <type_traits>

namespace ahamkara::game {

struct Vec2 {
    float x {0.0F};
    float y {0.0F};
};

struct Vec3 {
    float x {0.0F};
    float y {0.0F};
    float z {0.0F};
};

enum class MovementState : ae::u8 {
    Idle,
    Walking,
    Sprinting,
    Sliding,
    Jumping,
    OnLadder,
    LedgeGrab,
    Mantling
};

struct PlayerInputCommand {
    ae::u32 sequence {0};
    ae::u32 client_tick {0};
    float client_time {0.0F};
    Vec2 move_axis {};
    Vec2 look_delta {};
    bool jump_pressed {false};
    bool crouch_held {false};
    bool sprint_held {false};
    bool slide_pressed {false};
    bool fire_held {false};
    bool reload_pressed {false};
    bool ability_pressed {false};
    ae::u8 weapon_slot {0};
};

struct ReplicatedPlayerState {
    ae::u32 network_object_id {1};
    ae::u32 player_id {1};
    Vec3 position {};
    Vec3 velocity {};
    float yaw {0.0F};
    MovementState movement_state {MovementState::Idle};
    float health {100.0F};
    float shield {100.0F};
};

struct ProjectileState {
    Vec3 position {};
    Vec3 velocity {};
    float lifetime_seconds {0.0F};
    bool alive {false};
    ae::u32 client_tick {0};
    bool first_tick {true};
};

struct TargetDummyState {
    ae::u32 dummy_id {0};
    Vec3 position {};
    float yaw {0.0F};
    float health {100.0F};
    float armor {50.0F};
    float max_armor {50.0F};
    bool alive {true};
    Vec3 start_position {};
    Vec3 move_dir {};
    float move_timer {0.0F};
    float move_speed {0.0F};
    float move_distance {0.0F};
    float last_hit_timer {0.0F};
    bool was_hit_precision {false};
    float last_damage_dealt {0.0F};
    Vec3 last_hit_position {};
    float respawn_timer {0.0F};
};

struct ServerSnapshot {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ReplicatedPlayerState local_player {};

    // Projectile replication (up to 8)
    ae::u8 projectile_count {0};
    ProjectileState projectiles[8] {};

    // Dummy/entity replication (up to 4, matches World::kMaxDummies)
    ae::u8 dummy_count {0};
    TargetDummyState dummies[4] {};

    // Match state
    ae::u8 match_phase {0};
    float match_time {0.0F};
    ae::u32 team_score_red {0};
    ae::u32 team_score_blue {0};
    ae::u16 individual_score {0};

    // Remote players (for entity replication — other connected clients)
    ae::u8 remote_player_count {0};
    struct RemotePlayer {
        ae::u32 player_id {0};
        Vec3 position {};
        float yaw {0};
        float health {100};
    };
    RemotePlayer remote_players[4] {};
};

// Delta compression: bitmask indicates which player state fields changed.
// This allows sending only changed fields instead of the full 49 bytes.
struct SnapshotDelta {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ae::u16 dirty_mask {0xFFFF}; // bits 0-9 correspond to ReplicatedPlayerState fields
    // Only the flagged fields below are serialized:
    float pos_x {0}, pos_y {0}, pos_z {0};
    float vel_x {0}, vel_y {0}, vel_z {0};
    float yaw {0};
    ae::u8 movement {0};
    float health {0}, shield {0};
};

// Bit indices for dirty_mask
enum class PlayerDeltaBit : ae::u16 {
    PosX = 0, PosY = 1, PosZ = 2,
    VelX = 3, VelY = 4, VelZ = 5,
    Yaw = 6, Movement = 7,
    Health = 8, Shield = 9
};

inline SnapshotDelta compute_player_delta(const ReplicatedPlayerState& current, const ReplicatedPlayerState& last_ack) {
    SnapshotDelta d;
    d.dirty_mask = 0;

    auto set_if = [&](PlayerDeltaBit bit, float a, float b) {
        if (std::fabs(a - b) > 0.001F) d.dirty_mask |= (1 << static_cast<ae::u16>(bit));
    };

    if (current.network_object_id != last_ack.network_object_id) d.dirty_mask = 0xFFFF;
    if (current.player_id != last_ack.player_id) d.dirty_mask = 0xFFFF;

    set_if(PlayerDeltaBit::PosX, current.position.x, last_ack.position.x);
    set_if(PlayerDeltaBit::PosY, current.position.y, last_ack.position.y);
    set_if(PlayerDeltaBit::PosZ, current.position.z, last_ack.position.z);
    set_if(PlayerDeltaBit::VelX, current.velocity.x, last_ack.velocity.x);
    set_if(PlayerDeltaBit::VelY, current.velocity.y, last_ack.velocity.y);
    set_if(PlayerDeltaBit::VelZ, current.velocity.z, last_ack.velocity.z);
    set_if(PlayerDeltaBit::Yaw, current.yaw, last_ack.yaw);
    if (current.movement_state != last_ack.movement_state)
        d.dirty_mask |= (1 << static_cast<ae::u16>(PlayerDeltaBit::Movement));
    set_if(PlayerDeltaBit::Health, current.health, last_ack.health);
    set_if(PlayerDeltaBit::Shield, current.shield, last_ack.shield);

    if (d.dirty_mask == 0) return d; // nothing changed — skip full copy

    d.pos_x = current.position.x; d.pos_y = current.position.y; d.pos_z = current.position.z;
    d.vel_x = current.velocity.x; d.vel_y = current.velocity.y; d.vel_z = current.velocity.z;
    d.yaw = current.yaw;
    d.movement = static_cast<ae::u8>(current.movement_state);
    d.health = current.health; d.shield = current.shield;
    return d;
}

inline void apply_player_delta(ReplicatedPlayerState& state, const SnapshotDelta& d) {
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosX))) state.position.x = d.pos_x;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosY))) state.position.y = d.pos_y;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::PosZ))) state.position.z = d.pos_z;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelX))) state.velocity.x = d.vel_x;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelY))) state.velocity.y = d.vel_y;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::VelZ))) state.velocity.z = d.vel_z;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Yaw))) state.yaw = d.yaw;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Movement))) state.movement_state = static_cast<MovementState>(d.movement);
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Health))) state.health = d.health;
    if (d.dirty_mask & (1 << static_cast<ae::u16>(PlayerDeltaBit::Shield))) state.shield = d.shield;
}

struct ParticleState {
    Vec3 position {};
    Vec3 velocity {};
    float r {1.0F}, g {1.0F}, b {1.0F};
    float lifetime_seconds {0.0F};
    float max_lifetime {0.0F};
    float size {0.05F};
    bool alive {false};
};

struct DecalState {
    Vec3 position {};
    Vec3 normal {};
    float size {0.15F};
    float r {0.1F}, g {0.1F}, b {0.1F};
    float lifetime_seconds {5.0F};
    bool alive {false};
};

static_assert(std::is_trivially_copyable_v<PlayerInputCommand>);
static_assert(std::is_trivially_copyable_v<ReplicatedPlayerState>);
static_assert(std::is_trivially_copyable_v<ServerSnapshot>);

using PacketEnvelope = ae::PacketEnvelope;

}  // namespace ahamkara::game
