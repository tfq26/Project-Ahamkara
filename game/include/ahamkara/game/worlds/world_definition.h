#pragma once

#include "ahamkara/game/destination_metadata.h"
#include "ahamkara/game/maps/map_definition.h"
#include "ahamkara/game/net_types.h"

#include <cstddef>

namespace ahamkara::game {

struct PlayerSpawnDefinition {
    Vec3 position {};
    float yaw {0.0F};
};

struct InteractionTargetDefinition {
    ae::u32 interaction_id {0};
    Vec3 position {};
    float radius {1.0F};
    bool one_shot {true};
    const char* label {""};
};

struct WorldDefinition {
    const char* id {""};
    const char* display_name {""};
    const MapDefinition* map {nullptr};
    PlayerSpawnDefinition player_spawn {};
    const TargetDummyState* target_dummies {nullptr};
    std::size_t target_dummy_count {0};
    const InteractionTargetDefinition* interaction_targets {nullptr};
    std::size_t interaction_target_count {0};

    /// Optional destination metadata for world-scale authored spaces.
    /// When null (default), the world behaves as a single-map level.
    /// When set, the destination metadata carries region definitions,
    /// landing zones, and ambient population markers used by streaming
    /// and gameplay systems.
    const DestinationMetadata* destination {nullptr};
};

}  // namespace ahamkara::game
