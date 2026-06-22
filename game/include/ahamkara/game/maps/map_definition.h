#pragma once

#include "ahamkara/game/debug_map.h"

#include <cstddef>

namespace ahamkara::game {

enum class MapCategory {
    World,
    Pvp,
    Social,
    Endgame,
    Sandbox
};

struct MapDefinition {
    const char* id {""};
    const char* display_name {""};
    MapCategory category {MapCategory::Sandbox};
    const ColliderBox* colliders {nullptr};
    std::size_t collider_count {0};
};

}  // namespace ahamkara::game
