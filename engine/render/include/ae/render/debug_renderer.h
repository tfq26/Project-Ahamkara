#pragma once

#include "ae/render/frustum.h"
#include "ae/platform/window.h"

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
    double gpu_time_entities_ms {0.0};
    double gpu_time_ui_ms {0.0};
    float player_health {100.0F};
    float player_max_health {100.0F};
    float ammo_current {24.0F};
    float ammo_max {30.0F};
    bool always_day {false};
    bool menu_visible {false};
    int menu_tab {0};
    unsigned int controller_buttons {0};  // bitmask of pressed buttons
    int projectile_count {0};
    Vec3 projectile_positions[64] {};
    bool projectile_hit[64] {};
    float hud_brightness {1.0F};  // day/night factor for HUD alpha
    float gamma {1.0F};           // user brightness adjustment

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

    void render(DebugScene& scene);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace ae::render
