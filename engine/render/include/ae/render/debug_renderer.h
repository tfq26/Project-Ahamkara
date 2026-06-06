#pragma once

#include "ae/platform/window.h"

#include <memory>

namespace ae::render {

struct Vec3 {
    float x {0.0F};
    float y {0.0F};
    float z {0.0F};
};

struct DebugScene {
    Vec3 player_position {};
    float player_height {0.65F};
    float player_yaw {0.0F};
    Vec3 camera_position {};
    Vec3 camera_target {0.0F, 0.0F, 1.0F};
    bool show_player_marker {true};
    bool show_crosshair {false};
    const char* camera_mode_name {"3P"};
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

    void render(const DebugScene& scene);

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace ae::render
