#pragma once

// Encounter scripting types for Phase 8 PvE gameplay.
//
// Defines the data shapes and runtime state for authored encounters:
// objectives, triggers, spawn waves, and encounter progression.
//
// Ownership: game layer. The EncounterManager lives in the game's World
// and is ticked each fixed step. Activity types (Horde, Deathmatch) may
// also use these types for their own encounter logic.

#include "ahamkara/game/ai/ai_combatant.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ahamkara::game {

// -- Objective Types ----------------------------------------------------------

/// Types of objectives that can appear in an encounter.
enum class ObjectiveType : std::uint8_t {
    KillAll        = 0,  // Eliminate all spawned enemies
    Survive        = 1,  // Survive for a duration
    Defend         = 2,  // Protect a point/entity
    Collect        = 3,  // Gather items from the field
    Reach          = 4,  // Reach a location
    Boss           = 5,  // Defeat a named boss enemy
};

/// Condition that triggers objective completion.
enum class ObjectiveCondition : std::uint8_t {
    AllComplete    = 0,  // All sub-objectives done
    AnyComplete    = 1,  // Any one sub-objective done
    Sequential     = 2,  // Must complete in order
};

/// An objective within an encounter.
struct ObjectiveDef {
    std::string id {};
    std::string label {};
    ObjectiveType type {ObjectiveType::KillAll};
    ObjectiveCondition condition {ObjectiveCondition::AllComplete};

    /// For KillAll/Boss: number of enemies that must die.
    int kill_target {0};
    /// For Survive: duration in seconds.
    float survive_duration {0.0F};
    /// For Defend: entity/point ID to protect.
    std::string defend_target_id {};
    /// For Reach/Collect: position in world space.
    float pos_x {0.0F};
    float pos_y {0.0F};
    float pos_z {0.0F};
    /// For Collect: number of items to collect.
    int collect_target {0};
    /// For Boss: the archetype of the boss.
    ai::CombatArchetype boss_archetype {ai::CombatArchetype::Brute};

    /// Optional hint text shown to the player.
    std::string hint {};
};

// -- Trigger Types ------------------------------------------------------------

/// What activates a trigger.
enum class TriggerCondition : std::uint8_t {
    PlayerEnter   = 0,  // Player enters a volume
    EnemyDeath    = 1,  // Specific enemies die
    Timer         = 2,  // Time elapsed since encounter start
    PreviousDone  = 3,  // Previous wave/phase completed
    AllEnemiesDead = 4, // All current wave enemies dead
};

/// A trigger that activates something in an encounter.
struct TriggerDef {
    std::string id {};
    TriggerCondition condition {TriggerCondition::PlayerEnter};

    /// For PlayerEnter: trigger volume in world space.
    float vol_min_x {0.0F};
    float vol_min_z {0.0F};
    float vol_max_x {0.0F};
    float vol_max_z {0.0F};
    float vol_min_y {0.0F};
    float vol_max_y {3.0F};

    /// For EnemyDeath: which enemy IDs to watch.
    std::vector<std::string> watch_enemy_ids {};

    /// For Timer: delay in seconds.
    float delay_seconds {0.0F};

    /// For PreviousDone: which wave/phase ID must be complete.
    std::string depends_on_id {};

    /// Events triggered when this activates:
    /// Spawn wave ID to start.
    std::string spawn_wave_id {};
    /// Objective ID to activate.
    std::string objective_id {};
    /// Optional debug label.
    std::string label {};
};

// -- Spawn Wave Types ---------------------------------------------------------

/// Definition of a single spawn wave — one or more enemy groups.
struct SpawnGroupDef {
    ai::CombatArchetype archetype {ai::CombatArchetype::Grunt};
    int count {1};
    float spread {5.0F};
};

struct SpawnWaveDef {
    std::string id {};
    std::vector<SpawnGroupDef> groups {};
    float spawn_interval {0.5F};    // Seconds between spawning each enemy
    float delay_before {0.0F};      // Delay before wave starts spawning
    bool simultaneous {false};      // All enemies spawn at once (vs staggered)
    std::string label {};
};

// -- Encounter Definition -----------------------------------------------------

/// Complete definition of an authored encounter.
struct EncounterDef {
    std::string id {};
    std::string label {};
    bool enabled {true};

    /// Spawn point for the encounter's enemies (center position).
    float origin_x {0.0F};
    float origin_z {0.0F};

    /// Waves to spawn.
    std::vector<SpawnWaveDef> waves {};

    /// Triggers that drive encounter progression.
    std::vector<TriggerDef> triggers {};

    /// Objectives the player must complete.
    std::vector<ObjectiveDef> objectives {};
};

// -- Runtime Encounter State --------------------------------------------------

enum class EncounterPhase : std::uint8_t {
    Inactive   = 0,
    Active     = 1,
    BetweenWaves = 2,
    Complete   = 3,
    Failed     = 4,
};

struct ObjectiveState {
    std::string id {};
    bool complete {false};
    bool failed {false};
    /// For kill objectives: how many have been killed.
    int kill_count {0};
    /// For survive/defend: elapsed time protecting.
    float elapsed {0.0F};
};

struct SpawnWaveState {
    std::string id {};
    bool active {false};
    bool complete {false};
    float delay_timer {0.0F};
    float spawn_timer {0.0F};
    int current_group {0};
    int enemies_spawned {0};
    int total_to_spawn {0};
    int enemies_alive {0};
};

struct TriggerState {
    std::string id {};
    bool fired {false};
    float timer {0.0F};
};

/// Runtime encounter state managed by EncounterManager.
struct EncounterState {
    std::string id {};
    EncounterPhase phase {EncounterPhase::Inactive};

    std::vector<ObjectiveState> objectives {};
    std::vector<SpawnWaveState> waves {};
    std::vector<TriggerState> triggers {};

    float phase_timer {0.0F};
    int current_wave_index {0};
    bool started {false};
};

// -- Encounter Manager --------------------------------------------------------

/// Manages multiple encounters and their progression.
class EncounterManager {
public:
    EncounterManager() = default;

    // -- Lifecycle --

    /// Add an encounter definition.
    void add_encounter(const EncounterDef& def);

    /// Remove all encounters.
    void clear();

    /// Start a specific encounter by ID.
    bool start_encounter(const std::string& id);

    /// Start all encounters.
    void start_all();

    /// Tick all active encounters.
    void tick(float delta_seconds);

    /// Notify the manager that an enemy of a given type was killed.
    void notify_enemy_killed(const std::string& wave_id,
                             ai::CombatArchetype archetype);

    /// Check if the player is within a trigger volume.
    void check_player_volume(const std::string& encounter_id,
                             float px, float py, float pz);

    // -- Accessors --

    [[nodiscard]] const EncounterState* get_state(const std::string& id) const;

    [[nodiscard]] const std::vector<EncounterState>& all_states() const {
        return states_;
    }

    [[nodiscard]] int encounter_count() const {
        return static_cast<int>(defs_.size());
    }

    /// Returns the active spawn wave definition for a given encounter.
    /// Null if no active wave.
    [[nodiscard]] const SpawnWaveDef* current_wave_def(const std::string& id) const;

    /// Returns the world-space spawn origin for an encounter.
    [[nodiscard]] bool encounter_origin(const std::string& id,
                                        float& out_x, float& out_z) const;

    /// Get all incomplete objective IDs for an encounter.
    [[nodiscard]] std::vector<std::string> pending_objectives(const std::string& id) const;

private:
    void start_wave(EncounterState& state, int wave_index);
    void tick_wave(EncounterState& state, SpawnWaveState& ws,
                   const SpawnWaveDef& wd, float dt);
    void check_triggers(EncounterState& state, float dt);
    void check_objectives(EncounterState& state);

    std::vector<EncounterDef> defs_;
    std::vector<EncounterState> states_;
    std::vector<SpawnWaveDef> active_waves_; // waves being spawned right now
};

}  // namespace ahamkara::game
