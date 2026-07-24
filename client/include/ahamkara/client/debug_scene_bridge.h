#pragma once

#include "ae/core/frame_profiler.h"
#include "ae/core/types.h"
#include "ae/runtime/metrics.h"
#include "ae/render/compiled_level.h"
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
    bool is_reloading {false};
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

    // Ability state
    float grenade_cooldown {0.0F};
    int grenade_count {2};
    float special_cooldown {0.0F};
    float artifact_cooldown {0.0F};
    float ultimate_charge {0.0F};
    bool grenade_available {true};
    bool special_available {true};
    bool ultimate_ready {false};

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
    const ae::render::LevelAsset* level_asset {nullptr};

    // Per-section CPU profiling data
    const ae::FrameProfileSnapshot* profile_snapshot {nullptr};

    // Frame pacing and memory budget data
    double frame_budget_ms {16.7};
    double frame_p1_low_ms {0.0};
    double frame_rolling_avg_ms {0.0};
    double frame_budget_compliance {1.0};
    bool frame_pacing_healthy {true};
    bool frame_regression {false};
    std::uint8_t rss_pressure {0};
    double rss_bytes {0.0};
    double rss_peak_bytes {0.0};
    double rss_soft_budget {0.0};
    double rss_hard_budget {0.0};
    std::uint8_t frame_alloc_pressure {0};
    double frame_alloc_peak_bytes {0.0};
    double frame_alloc_capacity_bytes {0.0};
};

[[nodiscard]] ae::render::DebugScene build_debug_scene(
    const ClientSimulationSnapshot& previous_snapshot,
    const ClientSimulationSnapshot& current_snapshot,
    const DebugSceneBuildInputs& inputs);

}  // namespace ahamkara::client
