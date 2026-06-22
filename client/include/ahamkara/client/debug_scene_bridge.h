#pragma once

#include "ae/core/types.h"
#include "ae/runtime/metrics.h"
#include "ae/render/debug_renderer.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/game/world.h"

namespace ahamkara::client {

struct ClientSimulationSnapshot {
    ahamkara::game::ReplicatedPlayerState player_state;
    float player_height {0.0F};
    ahamkara::game::Vec3 player_position {};
    ahamkara::game::CameraAnchor camera_anchor {};
    float ammo_current {0.0F};
    float ammo_max {0.0F};
    int weapon_index {0};
    int reserve_ammo {150};
    float hitmarker_time {0.0F};
    bool hitmarker_is_critical {false};
    float muzzle_flash_time {0.0F};
    float enemy_health[4] {150.0F, 150.0F, 150.0F, 0.0F};
    float enemy_max_health[4] {150.0F, 150.0F, 150.0F, 0.0F};

    static constexpr int kMaxDamageNumbers = 16;
    ahamkara::game::FloatingDamageNumber damage_numbers[kMaxDamageNumbers] {};
    int damage_number_count {0};

    static constexpr int kMaxDummies = 4;
    ahamkara::game::TargetDummyState dummies[kMaxDummies] {};
    int dummy_count {0};

    static constexpr int kMaxProjectiles = 64;
    ahamkara::game::ProjectileState projectiles[kMaxProjectiles] {};
    int projectile_count {0};

    static constexpr int kMaxParticles = 256;
    ahamkara::game::ParticleState particles[kMaxParticles] {};
    int particle_count {0};

    static constexpr int kMaxDecals = 64;
    ahamkara::game::DecalState decals[kMaxDecals] {};
    int decal_count {0};

    float match_time {0.0F};
    ae::u8 match_phase {0};
    ae::u32 team_score_red {0};
    ae::u32 team_score_blue {0};
    ae::u32 player_kills {0};
    ae::u32 player_deaths {0};
    bool player_alive {true};
    bool match_over {false};
    float damage_feedback_timer {0.0F};
};

struct DebugSceneBuildInputs {
    CameraMode camera_mode {CameraMode::FirstPerson};
    bool metrics_visible {false};
    bool gpu_profiler_visible {false};
    bool always_day {false};
    bool menu_visible {false};
    int menu_tab {0};
    float gamma {1.0F};
    const ae::RuntimeMetricsSnapshot* displayed_metrics {nullptr};
    float alpha {1.0F};
};

[[nodiscard]] ae::render::DebugScene build_debug_scene(
    const ClientSimulationSnapshot& previous_snapshot,
    const ClientSimulationSnapshot& current_snapshot,
    const DebugSceneBuildInputs& inputs);

}  // namespace ahamkara::client
