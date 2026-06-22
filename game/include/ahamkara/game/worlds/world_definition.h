#pragma once

#include "ahamkara/game/maps/map_definition.h"
#include "ahamkara/game/net_types.h"

#include <cstddef>

namespace ahamkara::game {

struct PlayerSpawnDefinition {
    Vec3 position {};
    float yaw {0.0F};
};

struct WorldDefinition {
    const char* id {""};
    const char* display_name {""};
    const MapDefinition* map {nullptr};
    PlayerSpawnDefinition player_spawn {};
    const TargetDummyState* target_dummies {nullptr};
    std::size_t target_dummy_count {0};
};

}  // namespace ahamkara::game
