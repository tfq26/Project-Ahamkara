#pragma once

#include "ae/render/frustum.h"
#include "ae/render/render_backend.h"
#include "ae/platform/window.h"

#include <functional>
#include <memory>

namespace ae::render {

struct Vec3 {
    float x {0.0F};
    float y {0.0F};
    float z {0.0F};
};

struct DebugBox {
    Vec3 min {};
    Vec3 max {};
    float red {0.25F};
    float green {0.30F};
    float blue {0.36F};
};

struct DebugScene {
    Vec3 player_position {};
    float player_height {0.65F};
    float player_yaw {0.0F};
    Vec3 camera_position {};
    Vec3 camera_target {0.0F, 0.0F, 1.0F};
    // First-person rig anchor. When the game is in first-person, these are
    // populated from the active camera anchor so the viewmodel can follow the
    // same source of truth instead of re-deriving it from general camera state.
    Vec3 viewmodel_position {};
    Vec3 viewmodel_forward {0.0F, 0.0F, 1.0F};
    Vec3 viewmodel_right {1.0F, 0.0F, 0.0F};
    Vec3 viewmodel_up {0.0F, 1.0F, 0.0F};
    bool show_player_marker {true};
    bool show_crosshair {false};
    bool draw_default_map {true};
    bool hud_visible {true};
    const char* camera_mode_name {"3P"};
    const char* overlay_title {nullptr};
    const char* overlay_body {nullptr};
    const char* overlay_hint {nullptr};
    const char* objective_text {nullptr};
    bool metrics_visible {false};
    double fps {0.0};
    double frame_time_ms {0.0};
    double fps_p1_low {0.0};
    double fps_p1_high {0.0};
    double process_cpu_percent {0.0};
    double system_cpu_percent {0.0};
    double process_rss_mb {0.0};
    double system_used_memory_mb {0.0};
    double system_total_memory_mb {0.0};
    bool gpu_usage_available {false};
    double gpu_usage_percent {0.0};
    double gpu_time_total_ms {0.0};
    double gpu_time_depth_ms {0.0};
    double gpu_time_map_ms {0.0};
    double gpu_time_ui_ms {0.0};
    double gpu_time_entities_ms {0.0};  // placeholder — not separately measured
    float player_health {100.0F};
    float player_max_health {100.0F};
    float ammo_current {24.0F};
    float ammo_max {30.0F};
    int weapon_index {0};
    int reserve_ammo {150};
    const char* weapon_name {"AR-15"};
    const GpuModel* weapon_model {nullptr};
    bool weapon_animation_override {false};
    float weapon_animation_transform[16] {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
    // GPU skinning joint matrices for the current viewmodel.
    // Flat array of 8 * 16 = 128 floats (column-major 4x4 each).
    float weapon_joint_matrices[128] {};
    int weapon_joint_count {0};
    float enemy_health[16] {};
    float enemy_max_health[16] {};
    int enemy_count {0};
    bool always_day {false};
    bool menu_visible {false};
    int menu_tab {0};
    unsigned int controller_buttons {0};  // bitmask of pressed buttons
    int projectile_count {0};
    Vec3 projectile_positions[64] {};
    bool projectile_hit[64] {};
    float hud_brightness {1.0F};  // day/night factor for HUD alpha
    float gamma {1.0F};           // user brightness adjustment
    float match_time {0.0F};      // match timer
    ae::u8 match_phase {0};       // MatchPhase enum
    ae::u32 team_score_red {0};
    ae::u32 team_score_blue {0};

    // Sample/game-provided debug geometry.
    int level_box_count {0};
    DebugBox level_boxes[64] {};

    // Sensory feedback
    float hitmarker_time {0.0F};
    bool hitmarker_is_critical {false};
    float muzzle_flash_time {0.0F};
    
    // Floating damage numbers
    int hit_number_count {0};
    Vec3 hit_number_positions[16] {};
    float hit_number_values[16] {};
    bool hit_number_is_critical[16] {};
    float hit_number_lifetimes[16] {};
    
    // Target dummies
    int dummy_count {0};
    Vec3 dummy_positions[16] {};
    float dummy_yaws[16] {};
    bool dummy_alive[16] {};
    bool dummy_recently_hit[16] {}; // for visual flash

    // Particles
    int particle_count {0};
    Vec3 particle_positions[256] {};
    float particle_sizes[256] {};
    float particle_colors_r[256] {};
    float particle_colors_g[256] {};
    float particle_colors_b[256] {};
    float particle_alphas[256] {}; // derived from lifetime/max_lifetime

    // Decals
    int decal_count {0};
    Vec3 decal_positions[64] {};
    Vec3 decal_normals[64] {};
    float decal_sizes[64] {};

    // Render stats (populated each frame by the renderer)
    RenderStats render_stats {};

    // GPU profiler overlay (separate from main metrics)
    bool gpu_profiler_visible {false};

    // Character animation joint matrices for third-person character rendering.
    // Flat array of 256 * 16 = 4096 floats (column-major 4x4 each).
    float character_joint_matrices[4096] {};
    int character_joint_count {0};

    // Camera FOV override for ADS zoom effect. When > 0, the renderer uses
    // this value instead of the default 60° world FOV. When 0, the default
    // FOV is used. Set per-frame from the client layer's ADS blend.
    float camera_fov_override_deg {0.0F};

    // Viewmodel offset tuning — populated per-frame from the client layer's
    // per-weapon data (kWeaponViewmodelTransforms).  These are additive to the
    // base viewmodel position/rotation computed from the camera anchor.
    // Position offset: {right, up, forward} in meters.
    Vec3 viewmodel_position_offset {};
    float viewmodel_fov_scale {1.0F};
    float viewmodel_pitch_deg {0.0F};
    float viewmodel_yaw_deg {0.0F};
    float viewmodel_roll_deg {0.0F};

    // Screen shake state
    float screen_shake_intensity {0.0F};
    float screen_shake_angle {0.0F};
    float screen_shake_frequency {0.0F};

    // Damage flash for HUD red vignette
    float damage_flash_intensity {0.0F};

    // Melee active state for UI feedback
    bool melee_active {false};

    // Viewmodel IK debug visualization
    bool show_ik_target {false};
    Vec3 ik_target_position {};  // world-space IK target (grip socket)
    bool show_arm_chain {false}; // draw shoulder→elbow→hand lines
    Vec3 arm_shoulder_pos {};
    Vec3 arm_elbow_pos {};
    Vec3 arm_hand_pos {};
};

class DebugRenderer {
public:
    DebugRenderer();
    ~DebugRenderer();

    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;
    DebugRenderer(DebugRenderer&&) = delete;
    DebugRenderer& operator=(DebugRenderer&&) = delete;

    bool initialize(ae::PlatformWindow& window);
    void shutdown();

    /// Draw this frame's world (plus any `draw_world_extra` world-phase
    /// geometry) and screen-space overlays. This does NOT swap/display the
    /// frame by itself unless legacy auto-present is enabled (see
    /// set_auto_present). Staged frame loops should call present() explicitly.
    void render(DebugScene& scene, const std::function<void()>& draw_world_extra = {});

    /// Present (swap/display) the frame previously drawn by render(). Calls the
    /// backend's end_frame(). Call once per frame, after render(). Does not draw.
    void present();

    /// Legacy convenience: when enabled (the default), render() calls present()
    /// itself at the end. Staged frame loops set this false and call present()
    /// in their own present stage, keeping rendering and presentation separate.
    void set_auto_present(bool enabled);

    RenderBackend* backend();

    /// Drive sky/clear/fog color and ambient lighting from a loaded level's
    /// environment settings. Call once after loading a level. Without it, the
    /// renderer uses its built-in day/night palette.
    void set_level_environment(float sky_r, float sky_g, float sky_b,
                               float ambient_r, float ambient_g, float ambient_b);
    /// Revert to the built-in day/night environment.
    void clear_level_environment();

    // Camera matrices used by the most recent render() call. Column-major,
    // 16 floats. Valid after render() has run at least once; lets external
    // passes (e.g. the PBR level pass) draw aligned with the debug world.
    [[nodiscard]] const float* view_matrix() const;
    [[nodiscard]] const float* projection_matrix() const;
    [[nodiscard]] const float* camera_position() const;  // 3 floats

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace ae::render
