#pragma once

/// CLIENT-ONLY — depends on ae_animation which transitively includes
/// ae_render. Never include in server/headless targets.

#include "ae/animation/animation_driver.h"

#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// Placeholder anim movement state — mirrors what ae::animation::AnimMovementState
// provides. In a real integration the adapter would use the engine type
// directly; this local enum demonstrates the mapping boundary.
// ---------------------------------------------------------------------------
enum class AnimMovementState {
    Idle,
    Walking,
    Sprinting,
    Sliding,
    Jumping,
    OnLadder,
    LedgeGrab,
    Mantling,
    Crouching
};

/// Converts game-side MovementState into animation-driver consumable 
/// movement state.  CLIENT-ONLY — depends on ae_animation which transitively
/// includes ae_render.  Never include in server/headless targets.
[[nodiscard]] inline AnimMovementState game_to_anim_movement_state(
    MovementState state)
{
    switch (state) {
    case MovementState::Idle:      return AnimMovementState::Idle;
    case MovementState::Walking:   return AnimMovementState::Walking;
    case MovementState::Sprinting: return AnimMovementState::Sprinting;
    case MovementState::Sliding:   return AnimMovementState::Sliding;
    case MovementState::Jumping:   return AnimMovementState::Jumping;
    case MovementState::OnLadder:  return AnimMovementState::OnLadder;
    case MovementState::LedgeGrab: return AnimMovementState::LedgeGrab;
    case MovementState::Mantling:  return AnimMovementState::Mantling;
    }
    return AnimMovementState::Idle;
}

// ---------------------------------------------------------------------------
// Placeholder animation gameplay input struct.
// Demonstrates the boundary between game-side replicated/input state and
// what the animation driver consumes each frame.
// ---------------------------------------------------------------------------
struct AnimGameplayInput {
    Vec3  velocity        {};   // current world-space velocity
    bool  on_ground       {true};
    AnimMovementState movement_state {AnimMovementState::Idle};
    float speed           {0.0F}; // horizontal speed (m/s)
    float body_yaw        {0.0F}; // body facing direction (radians)
    float aim_yaw         {0.0F}; // look yaw (radians)
    float aim_pitch       {0.0F}; // look pitch (radians)
    bool  is_firing       {false};
    bool  is_reloading    {false};
};

/// Converts game-side state into animation-driver consumable input.
/// CLIENT-ONLY — depends on ae_animation which transitively includes
/// ae_render. Never include in server/headless targets.
[[nodiscard]] inline AnimGameplayInput build_anim_gameplay_input(
    const ReplicatedPlayerState& player,
    const PlayerInputCommand& cmd,
    const MovementSimState& sim)
{
    AnimGameplayInput input;
    input.velocity        = player.velocity;
    input.on_ground       = sim.was_on_ground;
    input.movement_state  = game_to_anim_movement_state(player.movement_state);
    input.speed           = player.velocity.x * player.velocity.x +
                            player.velocity.z * player.velocity.z;
    // speed is length² here — caller normalises if needed
    input.body_yaw        = player.yaw;
    input.aim_yaw         = player.yaw + cmd.look_delta.x;
    input.aim_pitch       = cmd.look_delta.y; // simplified
    input.is_firing       = cmd.fire_held;
    input.is_reloading    = cmd.reload_pressed;
    return input;
}

}  // namespace ahamkara::game::adapters
