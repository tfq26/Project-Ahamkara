#pragma once

#include "ae/core/packet_envelope.h"
#include "ae/core/types.h"

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

struct ServerSnapshot {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ReplicatedPlayerState local_player {};
};

/**
 * @brief A simple projectile fired by the player.
 */
struct ProjectileState {
    Vec3 position {};
    Vec3 velocity {};
    float lifetime_seconds {0.0F};
    bool alive {false};
    ae::u32 client_tick {0};
    bool first_tick {true};
};

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
    Vec3 normal {};  // surface normal for orientation
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
