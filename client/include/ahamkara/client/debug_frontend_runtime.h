#pragma once

#include "ae/core/frame_pacer.h"
#include "ae/core/frame_profiler.h"
#include "ae/core/memory_budget.h"
#include "ae/runtime/metrics.h"
#include "ae/runtime/performance_logger.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/controller_bindings.h"

#include <memory>
#include <string>

namespace ae {
class PlatformWindow;
}

namespace ahamkara::client {

struct DebugFrontendState {
    CameraMode camera_mode {CameraMode::FirstPerson};
    bool metrics_visible {false};
    bool gpu_profiler_visible {false};
    bool always_day {false};
    double last_time_seconds {0.0};
    float smoothed_delta_seconds {0.0F};
    double metrics_update_accumulator {0.0};
    ae::RuntimeMetricsCollector metrics_collector {};
    ae::RuntimeMetricsSnapshot displayed_metrics {};
    ae::FramePacer frame_pacer {16.7, 2.0};
    ae::FrameProfiler frame_profiler {};
    ae::FrameProfileSnapshot last_profile_snapshot {};
    ae::MemoryBudgetTracker budget_tracker {};
    std::unique_ptr<ae::PerformanceLogger> perf_logger;
    uint64_t frame_count {0};
};

[[nodiscard]] DebugFrontendState make_debug_frontend_state();
[[nodiscard]] std::string build_debug_window_title(
    const std::string& base_title,
    CameraMode camera_mode,
    bool metrics_visible,
    const ae::RuntimeMetricsSnapshot& displayed_metrics);

void process_debug_hotkeys(
    const ae::PlatformWindow& window,
    const ControllerBindings& controller_bindings,
    const std::string& base_title,
    DebugFrontendState& state,
    std::string& in_out_window_title);

float update_debug_frame_timing(DebugFrontendState& state);
void update_debug_metrics(DebugFrontendState& state, float frame_delta_seconds);

}  // namespace ahamkara::client
