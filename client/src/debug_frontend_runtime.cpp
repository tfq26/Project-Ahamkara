#include "ahamkara/client/debug_frontend_runtime.h"

#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/platform/window.h"

#include <cmath>
#include <sstream>

namespace {

constexpr ae::KeyCode kPerspectiveToggleKey = ae::KeyCode::V;

[[nodiscard]] bool perspective_toggle_requested(
    const ae::PlatformWindow& window,
    const ahamkara::client::ControllerBindings& controller_bindings) {
    const bool keyboard_toggle_requested = window.is_key_pressed(kPerspectiveToggleKey);
    const ae::GamepadDebugState& debug_state = window.gamepad_debug_state();
    const bool controller_toggle_requested = debug_state.is_code_pressed(controller_bindings.toggle_perspective);
    return keyboard_toggle_requested || controller_toggle_requested;
}

}  // namespace

namespace ahamkara::client {

DebugFrontendState make_debug_frontend_state() {
    DebugFrontendState state;
    state.last_time_seconds = ae::now_seconds();
    state.perf_logger = std::make_unique<ae::PerformanceLogger>();
    state.perf_logger->open("ahamkara_perf");
    return state;
}

std::string build_debug_window_title(
    const std::string& base_title,
    CameraMode camera_mode,
    bool metrics_visible,
    const ae::RuntimeMetricsSnapshot& displayed_metrics) {
    std::ostringstream title;
    title << base_title << " | View " << camera_mode_name(camera_mode);

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

void process_debug_hotkeys(
    const ae::PlatformWindow& window,
    const ControllerBindings& controller_bindings,
    const std::string& base_title,
    DebugFrontendState& state,
    std::string& in_out_window_title) {
    const ae::GamepadDebugState& debug_state = window.gamepad_debug_state();
    const bool metrics_toggle_requested =
        window.is_key_pressed(ae::KeyCode::F3) || debug_state.is_code_pressed(controller_bindings.metrics);
    const bool camera_toggle = perspective_toggle_requested(window, controller_bindings);

    if (metrics_toggle_requested) {
        state.metrics_visible = !state.metrics_visible;
        ae::log_info(state.metrics_visible ? "Metrics HUD enabled." : "Metrics HUD disabled.");
        in_out_window_title =
            build_debug_window_title(base_title, state.camera_mode, state.metrics_visible, state.displayed_metrics);
    }

    if (camera_toggle) {
        state.camera_mode = next_camera_mode(state.camera_mode);
        ae::log_info(state.camera_mode == CameraMode::FirstPerson
                         ? "Perspective: first-person"
                         : "Perspective: debug third-person");
        in_out_window_title =
            build_debug_window_title(base_title, state.camera_mode, state.metrics_visible, state.displayed_metrics);
    }

    if (window.is_key_pressed(ae::KeyCode::L)) {
        state.always_day = !state.always_day;
        ae::log_info(state.always_day ? "Lighting: always day" : "Lighting: day/night cycle");
    }

    if (window.is_key_pressed(ae::KeyCode::F4)) {
        state.gpu_profiler_visible = !state.gpu_profiler_visible;
        ae::log_info(state.gpu_profiler_visible ? "GPU profiler enabled." : "GPU profiler disabled.");
    }
}

float update_debug_frame_timing(DebugFrontendState& state) {
    state.frame_pacer.end_frame();
    state.frame_pacer.start_frame();

    const double current_time = ae::now_seconds();
    float raw_delta = static_cast<float>(current_time - state.last_time_seconds);
    if (raw_delta > 0.1F) {
        raw_delta = 0.1F;
    }
    state.last_time_seconds = current_time;

    if (state.smoothed_delta_seconds <= 0.0F) {
        state.smoothed_delta_seconds = raw_delta;
    } else {
        state.smoothed_delta_seconds += 0.18F * (raw_delta - state.smoothed_delta_seconds);
    }

    return state.smoothed_delta_seconds;
}

void update_debug_metrics(DebugFrontendState& state, float frame_delta_seconds) {
    state.budget_tracker.update_rss();

    state.metrics_update_accumulator += frame_delta_seconds;
    const bool compute_percentiles =
        (state.metrics_update_accumulator >= 1.0 || state.displayed_metrics.fps <= 0.0);
    const ae::RuntimeMetricsSnapshot sampled_metrics =
        state.metrics_collector.sample(frame_delta_seconds, compute_percentiles);
    if (state.metrics_update_accumulator >= 1.0 || state.displayed_metrics.fps <= 0.0) {
        state.displayed_metrics = sampled_metrics;
        state.metrics_update_accumulator = 0.0;
    }

    // Log to CSV every 60 frames (~1 second at 60 FPS)
    state.frame_count++;
    if (state.perf_logger && state.perf_logger->is_open() && (state.frame_count % 60 == 0)) {
        state.perf_logger->log(state.frame_count, sampled_metrics);
    }
}

}  // namespace ahamkara::client
