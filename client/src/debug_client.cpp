#include "ae/core/log.h"
#include "ae/core/math.h"
#include "ae/core/time.h"
#include "ae/platform/window.h"
#include "ae/runtime/application.h"
#include "ae/runtime/metrics.h"
#include "ae/render/debug_renderer.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/client_config.h"
#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/local_play.h"
#include "ahamkara/client/window_input_provider.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <memory>
#include <sstream>
#include <string>

namespace {

constexpr ae::KeyCode kPerspectiveToggleKey = ae::KeyCode::V;
constexpr float kFirstPersonTargetDistance = 12.0F;
constexpr float kDebugFollowDistance = 4.5F;
constexpr float kDebugFollowLift = 1.8F;
constexpr float kDebugFollowShoulderOffset = 0.75F;
constexpr float kDebugFollowLeadDistance = 1.5F;

struct DebugViewState {
    ahamkara::client::CameraMode camera_mode {ahamkara::client::CameraMode::FirstPerson};
};

[[nodiscard]] ae::Vec3 camera_forward_from_anchor(const ahamkara::game::CameraAnchor& anchor) {
    const float yaw_radians = ae::to_radians(anchor.yaw);
    const float pitch_radians = ae::to_radians(anchor.pitch);
    const float cos_pitch = std::cos(pitch_radians);

    return {
        std::sin(yaw_radians) * cos_pitch,
        std::sin(pitch_radians),
        std::cos(yaw_radians) * cos_pitch,
    };
}

[[nodiscard]] ae::Vec3 horizontal_forward_from_anchor(const ahamkara::game::CameraAnchor& anchor) {
    ae::Vec3 forward = camera_forward_from_anchor(anchor);
    forward.y = 0.0F;

    if (forward.length_squared() <= ae::epsilon) {
        return {0.0F, 0.0F, 1.0F};
    }

    return forward.normalized();
}

[[nodiscard]] ae::render::Vec3 to_render_vec3(const ae::Vec3& value) {
    return {value.x, value.y, value.z};
}

[[nodiscard]] std::string build_debug_window_title(
    const std::string& base_title,
    ahamkara::client::CameraMode camera_mode,
    bool metrics_visible,
    const ae::RuntimeMetricsSnapshot& displayed_metrics) {
    std::ostringstream title;
    title << base_title << " | View " << ahamkara::client::camera_mode_name(camera_mode);

    if (metrics_visible) {
        title << " | FPS " << static_cast<int>(std::floor(displayed_metrics.fps))
              << " | Frame " << static_cast<int>(std::lround(displayed_metrics.frame_time_ms)) << "ms"
              << " | RSS " << static_cast<int>(std::lround(displayed_metrics.process_rss_mb)) << "MB"
              << " | CPU " << static_cast<int>(std::lround(displayed_metrics.process_cpu_percent)) << "%"
              << " | SYS " << static_cast<int>(std::lround(displayed_metrics.system_cpu_percent)) << "%"
              << " | GPU N/A";
    }

    return title.str();
}

[[nodiscard]] bool perspective_toggle_requested(
    const ae::PlatformWindow& window,
    const ahamkara::client::ControllerBindings& controller_bindings,
    DebugViewState& /*view_state*/) {
    const bool keyboard_toggle_requested = window.is_key_pressed(kPerspectiveToggleKey);
    const ae::GamepadDebugState& debug_state = window.gamepad_debug_state();
    const bool controller_toggle_requested = debug_state.is_code_pressed(controller_bindings.toggle_perspective);

    return keyboard_toggle_requested || controller_toggle_requested;
}

[[nodiscard]] ae::render::DebugScene build_debug_scene(
    const ahamkara::client::LocalPlaySimulation& simulation,
    ahamkara::client::CameraMode camera_mode,
    bool metrics_visible,
    bool always_day,
    bool menu_visible,
    int menu_tab,
    float gamma,
    const ae::RuntimeMetricsSnapshot& displayed_metrics) {
    // Camera smoothing state
    static ae::Vec3 smooth_eye_pos {0, 2, -5};
    static ae::Vec3 smooth_target_pos {0, 1, 0};
    static bool smooth_initialized = false;

    const auto& player_state = simulation.get_player_state();
    const auto& anchor = simulation.get_camera_anchor();
    const float player_height = simulation.get_player_visual_height();

    const ae::Vec3 player_position {player_state.position.x, player_state.position.y, player_state.position.z};
    const ae::Vec3 player_center {
        player_position.x,
        player_position.y + player_height * 0.5F,
        player_position.z,
    };
    const ae::Vec3 anchor_position {anchor.position.x, anchor.position.y, anchor.position.z};
    const ae::Vec3 world_up {0.0F, 1.0F, 0.0F};
    const ae::Vec3 forward = camera_forward_from_anchor(anchor).normalized();

    ae::render::DebugScene scene {};
    scene.player_position = to_render_vec3(player_position);
    scene.player_height = player_height;
    scene.player_yaw = ae::to_radians(anchor.yaw);
    scene.player_health = player_state.health;
    scene.player_max_health = 100.0F;
    scene.ammo_current = static_cast<float>(simulation.get_ammo_current());
    scene.ammo_max = static_cast<float>(simulation.get_ammo_max());
    scene.always_day = always_day;
    scene.menu_visible = menu_visible;
    scene.menu_tab = menu_tab;
    scene.gamma = gamma;

    // Populate projectile data
    const int pc = simulation.get_projectile_count();
    scene.projectile_count = pc;
    for (int i = 0; i < pc && i < 64; ++i) {
        const auto& p = simulation.get_projectiles()[i];
        if (p.alive) {
            scene.projectile_positions[i] = {p.position.x, p.position.y, p.position.z};
        }
    }

    scene.metrics_visible = metrics_visible;
    scene.fps = displayed_metrics.fps;
    scene.frame_time_ms = displayed_metrics.frame_time_ms;
    scene.fps_p1_low = displayed_metrics.fps_p1_low;
    scene.fps_p1_high = displayed_metrics.fps_p1_high;
    scene.process_cpu_percent = displayed_metrics.process_cpu_percent;
    scene.system_cpu_percent = displayed_metrics.system_cpu_percent;
    scene.process_rss_mb = displayed_metrics.process_rss_mb;
    scene.system_used_memory_mb = displayed_metrics.system_used_memory_mb;
    scene.system_total_memory_mb = displayed_metrics.system_total_memory_mb;
    scene.gpu_usage_available = displayed_metrics.gpu_usage_available;
    scene.gpu_usage_percent = displayed_metrics.gpu_usage_percent;
    scene.camera_mode_name = ahamkara::client::camera_mode_name(camera_mode);

    if (camera_mode == ahamkara::client::CameraMode::FirstPerson) {
        scene.camera_position = to_render_vec3(anchor_position);
        scene.camera_target = to_render_vec3(anchor_position + forward * kFirstPersonTargetDistance);
        scene.show_player_marker = false;
        scene.show_crosshair = true;
        return scene;
    }

    const ae::Vec3 horizontal_forward = horizontal_forward_from_anchor(anchor);
    const ae::Vec3 right = ae::cross(world_up, horizontal_forward).normalized();
    const ae::Vec3 eye = anchor_position
        - horizontal_forward * kDebugFollowDistance
        + world_up * kDebugFollowLift
        + right * kDebugFollowShoulderOffset;
    const ae::Vec3 target = player_center + horizontal_forward * kDebugFollowLeadDistance;

    const float lerp_factor = std::min(1.0F, 15.0F * 0.016F); // ~15/sec smoothing
    if (!smooth_initialized) {
        smooth_eye_pos = eye;
        smooth_target_pos = target;
        smooth_initialized = true;
    } else {
        smooth_eye_pos.x += (eye.x - smooth_eye_pos.x) * lerp_factor;
        smooth_eye_pos.y += (eye.y - smooth_eye_pos.y) * lerp_factor;
        smooth_eye_pos.z += (eye.z - smooth_eye_pos.z) * lerp_factor;
        smooth_target_pos.x += (target.x - smooth_target_pos.x) * lerp_factor;
        smooth_target_pos.y += (target.y - smooth_target_pos.y) * lerp_factor;
        smooth_target_pos.z += (target.z - smooth_target_pos.z) * lerp_factor;
    }
    scene.camera_position = to_render_vec3(smooth_eye_pos);
    scene.camera_target = to_render_vec3(smooth_target_pos);
    scene.show_player_marker = true;
    scene.show_crosshair = false;
    return scene;
}

}  // namespace

int run_local_client(
    const ahamkara::client::ClientConfig& client_config,
    const ahamkara::client::ControllerBindings& controller_bindings) {
    ae::WindowConfig window_config {};
    window_config.title = "Ahamkara — Debug View";
    window_config.width = client_config.window_width;
    window_config.height = client_config.window_height;
    window_config.fullscreen = client_config.fullscreen;
    window_config.create_opengl_context = true;

    std::unique_ptr<ae::PlatformWindow> window;
    try {
        window = ae::PlatformWindow::create(window_config);
    } catch (const std::exception& ex) {
        ae::log_error(ex.what());
        return 1;
    }

    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ae::render::DebugRenderer renderer;
    if (!renderer.initialize(*window)) {
        ae::log_error("Failed to initialize debug renderer.");
        return 1;
    }

    auto input_provider = std::make_unique<ahamkara::client::WindowInputProvider>(
        *window,
        client_config.mouse_sensitivity,
        controller_bindings);
    ahamkara::client::LocalPlaySimulation simulation(std::move(input_provider));
    ae::RuntimeMetricsCollector metrics_collector;
    ae::RuntimeMetricsSnapshot displayed_metrics {};
    bool metrics_visible = false;
    double metrics_update_accumulator = 0.0;
    DebugViewState view_state {};
    bool always_day = false;
    bool menu_visible = false;
    int menu_tab = 0;  // 0=Character, 1=Settings
    bool prev_lb = false;
    bool prev_rb = false;

    ae::log_info(
        "Debug view started. Keyboard: W/A/S/D move, Shift sprint, Space jump, C slide, Ctrl crouch, "
        "V toggle perspective. Controller: left stick move, right stick look, LB sprint, A jump, B crouch, "
        "X slide, Y reload, RB ability, L3+R3 toggle perspective, Back metrics, Start exit. "
        "Right trigger = fire, L toggles day/night, F3 toggles metrics.");

    window->set_title(build_debug_window_title(window_config.title, view_state.camera_mode, metrics_visible, displayed_metrics));

    // Fixed-timestep accumulator — physics runs at a steady 60 Hz regardless
    // of render frame rate, preventing variable-rate physics artifacts.
    constexpr float kFixedDt = 1.0F / 60.0F;
    double accumulator = 0.0;
    double last_time = ae::now_seconds();
    float smoothed_delta = 0.0F;

    while (application.is_running() && window->poll_events()) {
        const ae::GamepadState& gamepad = window->gamepad_state();

        // Menu toggle: Start opens/closes
        const ae::GamepadDebugState& debug_state = window->gamepad_debug_state();
        const bool start_pressed = window->is_key_pressed(ae::KeyCode::Escape)
            || debug_state.is_code_pressed(controller_bindings.menu);
        if (start_pressed) {
            menu_visible = !menu_visible;
            ae::log_info(menu_visible ? "Menu opened." : "Menu closed.");
        }

        if (menu_visible) {
            // Menu navigation: LB/RB switch tabs
            const bool lb = gamepad.is_button_down(ae::GamepadButton::LeftBumper)
                || window->is_key_down(ae::KeyCode::Q);
            const bool rb = gamepad.is_button_down(ae::GamepadButton::RightBumper)
                || window->is_key_down(ae::KeyCode::E);
            if (lb && !prev_lb) { menu_tab = (menu_tab + 1) % 2; }
            if (rb && !prev_rb) { menu_tab = (menu_tab + 1) % 2; }
            prev_lb = lb;
            prev_rb = rb;

            // Still render the scene (frozen) with menu overlay
            const ae::render::DebugScene scene =
                build_debug_scene(simulation, view_state.camera_mode, metrics_visible, always_day,
                                  menu_visible, menu_tab, client_config.gamma, displayed_metrics);
            auto menu_scene = scene;
            menu_scene.controller_buttons = 0;
            if (gamepad.connected) {
                for (int i = 0; i < static_cast<int>(ae::kGamepadButtonCount); ++i)
                    if (gamepad.is_button_down(static_cast<ae::GamepadButton>(i)))
                        menu_scene.controller_buttons |= (1u << i);
            }
            renderer.render(menu_scene);
            continue;
        }

        const bool exit_requested = false;  // Exit via window close button
        const bool metrics_toggle_requested =
            window->is_key_pressed(ae::KeyCode::F3) || debug_state.is_code_pressed(controller_bindings.metrics);
        const bool camera_toggle_requested = perspective_toggle_requested(*window, controller_bindings, view_state);

        if (exit_requested) {
            ae::log_info("Escape pressed, shutting down.");
            break;
        }
        if (metrics_toggle_requested) {
            metrics_visible = !metrics_visible;
            ae::log_info(metrics_visible ? "Metrics HUD enabled." : "Metrics HUD disabled.");
            window->set_title(build_debug_window_title(window_config.title, view_state.camera_mode, metrics_visible, displayed_metrics));
        }
        if (camera_toggle_requested) {
            view_state.camera_mode = ahamkara::client::next_camera_mode(view_state.camera_mode);
            ae::log_info(view_state.camera_mode == ahamkara::client::CameraMode::FirstPerson
                             ? "Perspective: first-person"
                             : "Perspective: debug third-person");
            window->set_title(build_debug_window_title(window_config.title, view_state.camera_mode, metrics_visible, displayed_metrics));
        }
        if (window->is_key_pressed(ae::KeyCode::L)) {
            always_day = !always_day;
            ae::log_info(always_day ? "Lighting: always day" : "Lighting: day/night cycle");
        }

        const double current_time = ae::now_seconds();
        float raw_delta = static_cast<float>(current_time - last_time);
        if (raw_delta > 0.1F) {
            raw_delta = 0.1F;
        }
        last_time = current_time;

        // EMA-smooth the delta time for stable frame pacing.
        if (smoothed_delta <= 0.0F) {
            smoothed_delta = raw_delta;
        } else {
            smoothed_delta += 0.18F * (raw_delta - smoothed_delta);
        }

        // Fixed-timestep accumulator: run physics at 60 Hz regardless of frame rate.
        accumulator += static_cast<double>(smoothed_delta);
        while (accumulator >= static_cast<double>(kFixedDt)) {
            simulation.tick(kFixedDt);
            const bool compute_percentiles = (metrics_update_accumulator >= 1.0 || displayed_metrics.fps <= 0.0);
            const ae::RuntimeMetricsSnapshot sampled_metrics =
                metrics_collector.sample(kFixedDt, compute_percentiles);
            metrics_update_accumulator += kFixedDt;
            if (metrics_update_accumulator >= 1.0 || displayed_metrics.fps <= 0.0) {
                displayed_metrics = sampled_metrics;
                metrics_update_accumulator = 0.0;
            }
            accumulator -= static_cast<double>(kFixedDt);
        }

        const ae::render::DebugScene scene =
            build_debug_scene(simulation, view_state.camera_mode, metrics_visible, always_day,
                              menu_visible, menu_tab, client_config.gamma, displayed_metrics);
        auto render_scene = scene;
        render_scene.controller_buttons = 0;
        if (gamepad.connected) {
            for (int i = 0; i < static_cast<int>(ae::kGamepadButtonCount); ++i)
                if (gamepad.is_button_down(static_cast<ae::GamepadButton>(i)))
                    render_scene.controller_buttons |= (1u << i);
        }
        renderer.render(render_scene);

        if (metrics_update_accumulator == 0.0) {
            window->set_title(
                build_debug_window_title(window_config.title, view_state.camera_mode, metrics_visible, displayed_metrics));
        }
    }

    renderer.shutdown();
    application.shutdown();
    return EXIT_SUCCESS;
}
