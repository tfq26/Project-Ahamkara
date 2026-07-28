#pragma once

// Destination metadata for authoring large world-scale spaces.
//
// Phase 7 world scale.  Complements the SpatialGrid (1400) and
// ResidencyManager (1410) foundation by providing explicit descriptors
// for destinations, their regions, landing zones, and ambient population.
//
// Pure-data types: no runtime logic, platform deps, or rendering deps.
// Designed to be embedded in or referenced from WorldDefinition.
//
// All string fields are const char* pointers to static/compiled data
// (never owned or freed by these structs).

#include <cstddef>

namespace ahamkara::game {

/// Category of a region within a destination.
/// Controls spawn rules, music state, and HUD affordances at runtime.
enum class RegionType {
    Combat,       ///< PvE combat zone (enemies spawn, active combat music)
    Social,       ///< Safe area (no enemy spawns, vendors/social)
    Exploration,  ///< Open exploration (ambient enemies, no combat music)
    Boss,         ///< Boss encounter area (arena, locked until resolved)
    Dungeon,      ///< Instanced dungeon entrance
    Hub,          ///< Central hub (landing zone, service access)
    Transition,   ///< Corridor / connective tissue between regions
    Count         ///< Sentinel — number of region types
};

/// Axis-aligned bounding volume for a region, expressed in world-space
/// coordinates.
struct RegionBounds {
    float min_x {0.0F};
    float min_z {0.0F};
    float max_x {0.0F};
    float max_z {0.0F};
    float min_y {-100.0F};  ///< Vertical floor (default: generous)
    float max_y {100.0F};   ///< Vertical ceiling
};

/// A point where players can spawn into a destination.
struct LandingZoneDefinition {
    const char* id {""};             ///< Unique identifier, e.g. "landing_crossroads"
    const char* display_name {""};   ///< UI label, e.g. "Crossroads Landing Pad"
    float pos_x {0.0F};
    float pos_y {0.0F};
    float pos_z {0.0F};
    float yaw {0.0F};               ///< Facing direction on spawn
    bool is_primary {false};        ///< Default spawn for the destination
    bool requires_unlock {false};   ///< Hidden until a condition is met
    const char* unlock_condition {""}; ///< Condition ID, e.g. "complete_quest_01"
};

/// Spawn parameters for ambient (non-quest) NPCs or creatures in a region.
struct AmbientPopulationSpawn {
    const char* npc_id {""};         ///< NPC template ID, e.g. "npc_dreg"
    int min_count {0};               ///< Minimum count that should be present
    int max_count {3};               ///< Maximum cap
    float respawn_time_seconds {30.0F}; ///< Seconds before dead NPCs respawn
    float spawn_radius {10.0F};      ///< World-space radius from region center
    const char* spawn_group {""};    ///< Group key for co-spawn coordination
};

/// Descriptor for a single region within a destination.
///
/// Each region maps to one or more spatial-partition cells and carries
/// metadata for gameplay systems (encounter spawner, HUD, navigation).
struct RegionDescriptor {
    const char* id {""};              ///< Unique identifier, e.g. "region_temple_entrance"
    const char* display_name {""};    ///< UI label, e.g. "Temple Entrance"
    RegionType type {RegionType::Exploration};
    RegionBounds bounds {};
    const char* description {""};     ///< Flavour / loading-screen text
    int recommended_min_level {1};    ///< Suggested player level for this area
    int recommended_max_level {10};

    /// Whether enemies can spawn in this region (overrides type defaults).
    bool is_combat_zone {true};

    /// Whether players can freely land or spawn here.
    bool is_landing_allowed {true};

    /// Ambient population spawn rules for this region.
    /// When null or count == 0, the region inherits the destination-wide
    /// default population (if any).
    const AmbientPopulationSpawn* ambient_population {nullptr};
    std::size_t ambient_population_count {0};

    /// Landing zones within this region (subset of destination landing zones).
    const LandingZoneDefinition* landing_zones {nullptr};
    std::size_t landing_zone_count {0};
};

/// Top-level metadata for a world destination (large authored space).
///
/// A destination is a named, bounded area of the world that can be
/// independently streamed, scouted, and populated.  It aligns with the
/// ResidencyManager / SpatialGrid grid layout.
struct DestinationMetadata {
    const char* id {""};              ///< Unique identifier, e.g. "destination_ancient_forest"
    const char* display_name {""};    ///< UI label, e.g. "Ancient Forest"
    const char* description {""};     ///< Flavour / loading-screen text

    /// World-space extents of the destination.
    float world_min_x {0.0F};
    float world_min_z {0.0F};
    float world_max_x {0.0F};
    float world_max_z {0.0F};

    /// Spatial grid configuration (aligns with ResidencyManager init params).
    int grid_cols {1};
    int grid_rows {1};
    float cell_size {100.0F};

    /// Regions that make up this destination.
    const RegionDescriptor* regions {nullptr};
    std::size_t region_count {0};

    /// Landing zones accessible from orbit / fast-travel.
    const LandingZoneDefinition* landing_zones {nullptr};
    std::size_t landing_zone_count {0};

    /// Default ambient population for regions without explicit population.
    const AmbientPopulationSpawn* default_population {nullptr};
    std::size_t default_population_count {0};
};

/// Compile-time helper to check that a region type value is valid.
inline bool is_valid_region_type(RegionType t) noexcept {
    return t >= RegionType::Combat && t < RegionType::Count;
}

/// Find the region that contains a given world-space point.
/// Returns nullptr when the destination has no regions or when the point
/// falls outside all region bounds.
inline const RegionDescriptor* find_region_at(
    const DestinationMetadata& destination,
    float world_x,
    float world_z) noexcept {
    if (destination.regions == nullptr || destination.region_count == 0) {
        return nullptr;
    }
    for (std::size_t i = 0; i < destination.region_count; ++i) {
        const auto& r = destination.regions[i];
        if (world_x >= r.bounds.min_x && world_x <= r.bounds.max_x &&
            world_z >= r.bounds.min_z && world_z <= r.bounds.max_z) {
            return &r;
        }
    }
    return nullptr;
}

/// Determine the grid cell coordinate for a world-space position.
/// Returns the cell (cx, cy) that contains the position, clamped to grid bounds.
inline void world_to_grid(
    const DestinationMetadata& destination,
    float world_x,
    float world_z,
    int& out_cx,
    int& out_cy) noexcept {
    const float cs = (destination.cell_size > 0.0F) ? destination.cell_size : 1.0F;
    int cx = static_cast<int>((world_x - destination.world_min_x) / cs);
    int cy = static_cast<int>((world_z - destination.world_min_z) / cs);
    out_cx = (cx >= 0) ? (cx < destination.grid_cols ? cx : destination.grid_cols - 1) : 0;
    out_cy = (cy >= 0) ? (cy < destination.grid_rows ? cy : destination.grid_rows - 1) : 0;
}

}  // namespace ahamkara::game
