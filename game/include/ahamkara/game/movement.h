#pragma once

#include "ahamkara/game/net_types.h"

namespace ahamkara::game {

MovementState resolve_movement_state(const PlayerInputCommand& command);
void simulate_player_movement(
    ReplicatedPlayerState& player_state,
    const PlayerInputCommand& command,
    float delta_seconds);

}  // namespace ahamkara::game

