#pragma once

#include "ahamkara/game/camera_anchor.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/movement.h"

#include <cmath>

namespace ahamkara::game {

inline bool has_move_input(const PlayerInputCommand& input) {
    return std::fabs(input.move_axis.x) > 0.001F || std::fabs(input.move_axis.y) > 0.001F;
}

void update_camera_state(
    CameraAnchor& anchor,
    const ReplicatedPlayerState& player_state,
    MovementSimState& sim_state,
    float delta_seconds,
    const PlayerInputCommand& input,
    bool crouch_active);

void resolve_movement_state(
    ReplicatedPlayerState& player_state,
    float slide_timer_seconds,
    const MovementSimState& sim_state,
    const PlayerInputCommand& input,
    bool on_ground);

void fill_movement_debug(
    MovementDebugState& debug,
    const ReplicatedPlayerState& player_state,
    const MovementSimState& sim_state,
    float slide_timer_seconds,
    float delta_seconds,
    const PlayerInputCommand& input,
    bool on_ground);

}  // namespace ahamkara::game
