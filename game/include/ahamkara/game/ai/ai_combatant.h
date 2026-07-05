#pragma once

// AI combatant types for Phase 8 PvE opponents.
//
// Builds on the existing NavAgent / NavGrid / PathFollower infrastructure.
// Defines combat archetypes, perception state, targeting, and a behavior
// state machine for AI combatants.
//
// Ownership: game layer. The World holds AI combatants via EnTT components
// and calls the tick_ai_combatant system function each fixed step.

#include "ahamkara/game/ai/nav_agent.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/debug_map.h"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ahamkara::game::ai {

/// Combat archetype that defines an AI opponent's role and capabilities.
enum class CombatArchetype : std::uint8_t {
    Grunt    = 0,  // Standard all-rounder: medium range, medium speed, burst fire
    Sniper   = 1,  // Long-range precision: slow, high damage, stays at distance
    Rusher   = 2,  // Close-quarters: fast, aggressive, shotguns
    Support  = 3,  // Defensive: stays near objectives, suppressive fire
    Scout    = 4,  // Mobile flanker: fast, light, hit-and-run
    Brute    = 5,  // Heavy: slow, high HP, high damage, intimidating
};

/// Configuration parameters for a combat archetype.
struct ArchetypeConfig {
    float health {100.0F};
    float armor {0.0F};
    float move_speed {3.0F};
    float engage_range_min {2.0F};
    float engage_range_max {40.0F};
    float ideal_distance {15.0F};
    float fire_interval {0.8F};
    int burst_count {3};
    float accuracy_deg {6.0F};
    float damage_per_shot {8.0F};
    float perception_range {50.0F};
    float perception_angle_deg {180.0F};
    float search_range {10.0F};
};

/// Retrieve the default config for a given archetype.
[[nodiscard]] inline ArchetypeConfig archetype_config(CombatArchetype archetype) {
    switch (archetype) {
        case CombatArchetype::Grunt:
            return {100.0F, 0.0F, 3.0F, 2.0F, 40.0F, 15.0F, 0.8F, 4, 6.0F, 8.0F, 50.0F, 180.0F, 10.0F};
        case CombatArchetype::Sniper:
            return {75.0F, 0.0F, 1.5F, 20.0F, 80.0F, 50.0F, 1.5F, 1, 1.0F, 25.0F, 80.0F, 30.0F, 5.0F};
        case CombatArchetype::Rusher:
            return {80.0F, 10.0F, 5.5F, 0.5F, 15.0F, 5.0F, 0.5F, 2, 12.0F, 15.0F, 35.0F, 120.0F, 15.0F};
        case CombatArchetype::Support:
            return {120.0F, 20.0F, 2.5F, 5.0F, 50.0F, 25.0F, 1.0F, 5, 8.0F, 10.0F, 55.0F, 140.0F, 8.0F};
        case CombatArchetype::Scout:
            return {60.0F, 0.0F, 6.0F, 1.0F, 25.0F, 10.0F, 0.6F, 3, 10.0F, 6.0F, 40.0F, 200.0F, 12.0F};
        case CombatArchetype::Brute:
            return {200.0F, 50.0F, 2.0F, 3.0F, 35.0F, 12.0F, 1.2F, 6, 5.0F, 20.0F, 45.0F, 160.0F, 8.0F};
    }
    return {};
}

/// Current behavior state of an AI combatant.
enum class BehaviorState : std::uint8_t {
    Idle       = 0,  // No target, standing by
    Patrol     = 1,  // Following a patrol path
    Alert      = 2,  // Heard/sensed something, searching
    Engage     = 3,  // Actively engaging a target
    Flank      = 4,  // Repositioning during combat
    Retreat    = 5,  // Falling back (low health)
    Investigate = 6, // Moving to last known position
};

/// Perception snapshot for an AI combatant.
struct PerceptionState {
    bool target_visible {false};      // Direct line of sight to target
    bool target_heard {false};        // Heard target (noise, shots)
    Vec3 last_known_position {};      // Last confirmed target position
    float last_seen_timer {0.0F};     // Seconds since target was visible
    float alertness {0.0F};           // 0..1 alert level
    float detection_range {50.0F};    // Max detection range
};

/// Full AI combatant component state.
/// Attached to an EnTT entity for each AI combatant in the world.
struct AICombatantComponent {
    // -- Identity --
    ae::u32 combatant_id {0};
    CombatArchetype archetype {CombatArchetype::Grunt};

    // -- Health --
    float health {100.0F};
    float max_health {100.0F};
    float armor {0.0F};

    // -- Movement --
    NavVec2 position_2d {};   // (world_x, world_z)
    float yaw {0.0F};

    // -- Perception --
    PerceptionState perception {};
    ae::u32 target_entity {entt::null};
    Vec3 target_world_pos {};

    // -- Behavior --
    BehaviorState behavior {BehaviorState::Idle};
    float state_timer {0.0F};          // Time in current behavior state
    NavVec2 investigate_point {};

    // -- Combat --
    float fire_timer {0.0F};
    float burst_timer {0.0F};
    int burst_remaining {0};
    bool alive {true};
    float respawn_timer {0.0F};

    // -- Config (derived from archetype at spawn) --
    ArchetypeConfig cfg {};

    // Construction helper
    void apply_archetype(CombatArchetype arch) {
        archetype = arch;
        cfg = archetype_config(arch);
        health = cfg.health;
        max_health = cfg.health;
        armor = cfg.armor;
        perception.detection_range = cfg.perception_range;
    }
};

// -- Helper functions for the AI system (defined in ai_combatant.cpp) --

/// Angle difference in degrees (signed, -180..180).
[[nodiscard]] float angle_diff_deg(float a, float b);

/// Check 2D line of sight between two world positions against collision boxes.
[[nodiscard]] bool los_clear_2d(Vec3 from, Vec3 to,
                                 const std::vector<ColliderBox>& colliders);

/// Update perception for one AI combatant: detect targets, update awareness.
void update_perception(AICombatantComponent& self,
                       const Vec3& self_world_pos,
                       const Vec3& player_pos,
                       float delta_seconds,
                       const std::vector<ColliderBox>& world_colliders);

/// Update targeting: select the best target from perception data.
void update_targeting(AICombatantComponent& self,
                      const Vec3& player_pos,
                      float delta_seconds);

/// Advance the combatant's behavior state machine one tick.
void tick_behavior(AICombatantComponent& self,
                   float delta_seconds,
                   const Vec3& self_world_pos);

/// Tick all AI combatants in a registry.
void tick_ai_combatants(entt::registry& registry,
                        float delta_seconds,
                        const Vec3& player_pos,
                        const std::vector<ColliderBox>& world_colliders,
                        bool is_server);

}  // namespace ahamkara::game::ai
