#include "ahamkara/client/client_frame_pipeline.h"

#include "ae/core/log.h"
#include "ae/core/math.h"
#include "ae/platform/window.h"
#include "ae/ui/ahamkara_ui.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/debug_render_runtime.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <string>

namespace ahamkara::client {

ClientFramePipeline::ClientFramePipeline(
    ae::PlatformWindow& window,
    ae::Application& application,
    ae::render::DebugRenderer& renderer,
    ThreadedLocalRuntime& simulation,
    WindowInputProvider& window_input,
    DebugFrontendState& frontend_state,
    DebugUiController& ui_controller,
    ClientMenuState& menu_state,
    ClientConfig& client_config,
    const ControllerBindings& controller_bindings,
    ae::audio::AudioEngine& audio_engine,
    ae::render::ShadowPass& shadow_pass,
    ae::render::PbrRenderer& pbr_renderer,
    const ae::render::LevelRenderScene* level_scene)
    : window_(window)
    , application_(application)
    , renderer_(renderer)
    , simulation_(simulation)
    , window_input_(window_input)
    , frontend_state_(frontend_state)
    , ui_controller_(ui_controller)
    , menu_state_(menu_state)
    , client_config_(client_config)
    , controller_bindings_(controller_bindings)
    , audio_engine_(audio_engine)
    , shadow_pass_(shadow_pass)
    , pbr_renderer_(pbr_renderer)
    , level_scene_(level_scene) {}

bool ClientFramePipeline::run_one_frame() {
    // =====================================================================
    // Stage 1 — poll OS events and raw input
    // =====================================================================
    stage_poll_input();

    // =====================================================================
    // Stage 2 — menu toggle, hotkeys, cursor mode
    // =====================================================================
    stage_handle_menu_and_hotkeys();

    // =====================================================================
    // Stage 3 — gather gameplay input, submit to simulation
    // =====================================================================
    stage_gather_gameplay_input();

    // =====================================================================
    // Stage 4 — pull simulation snapshots
    // =====================================================================
    stage_pull_snapshots();

    // =====================================================================
    // Stage 5 — build debug scene from snapshots
    // =====================================================================
    stage_build_scene();

    // =====================================================================
    // Stage 6 — footstep / procedural audio
    // =====================================================================
    stage_gameplay_audio();

    // =====================================================================
    // Stage 7 — render 3D world
    // =====================================================================
    stage_render_world();

    // =====================================================================
    // Stage 8 — render ImGui UI
    // =====================================================================
    stage_render_ui();

    // =====================================================================
    // Stage 9 — present to screen
    // =====================================================================
    stage_present();

    // =====================================================================
    // Stage 10 — post-frame actions
    // =====================================================================
    stage_post_frame();

    return true;
}

// ---------------------------------------------------------------------------
// Stage implementations
// ---------------------------------------------------------------------------

void ClientFramePipeline::stage_poll_input() {
    if (!window_.poll_events() || !application_.is_running()) {
        application_.shutdown();
    }
}

void ClientFramePipeline::stage_handle_menu_and_hotkeys() {
    const auto& gamepad = window_.gamepad_state();
    const auto& debug_state = window_.gamepad_debug_state();

    // ESC / controller start → menu toggle.
    // ESC edge detection is owned by the platform window: is_key_pressed() is
    // edge-triggered (window_glfw.cpp resets per-frame edge state in
    // poll_events()). This previously also kept a raw glfwGetKey + process-static
    // edge-detect that duplicated the same press and bypassed the platform
    // abstraction; removed in favor of the single is_key_pressed path.
    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window_.native_handle());

    const bool menu_toggle =
        window_.is_key_pressed(ae::KeyCode::Escape)
        || debug_state.is_code_pressed(controller_bindings_.menu);

    const auto toggle_actions = ui_controller_.handle_menu_toggle(menu_toggle, client_config_);
    if (toggle_actions.config_applied) {
        window_input_.set_mouse_sensitivity(client_config_.mouse_sensitivity);
        audio_engine_.set_master_volume(client_config_.audio.master_volume);
    }
    if (toggle_actions.quit_application) {
        application_.shutdown();
    }

    // Cursor mode
    if (glfw_win) {
        glfwSetInputMode(glfw_win, GLFW_CURSOR,
            menu_state_.visible() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    // Hotkeys + metrics
    const std::string base_title = "Flashback";
    if (window_title_.empty()) {
        window_title_ = build_debug_window_title(base_title, frontend_state_.camera_mode,
            frontend_state_.metrics_visible, frontend_state_.displayed_metrics);
    }
    process_debug_hotkeys(window_, controller_bindings_, base_title, frontend_state_, window_title_);
    smoothed_delta_ = update_debug_frame_timing(frontend_state_);
    update_debug_metrics(frontend_state_, smoothed_delta_);
    window_.set_title(window_title_);
}

void ClientFramePipeline::stage_gather_gameplay_input() {
    raw_input_ = window_input_.gather_input(smoothed_delta_);
    simulation_.submit_input(raw_input_);
}

void ClientFramePipeline::stage_pull_snapshots() {
    simulation_.get_snapshots(prev_snap_, curr_snap_, alpha_);

    static bool was_menu_visible = true;
    if (was_menu_visible && !menu_state_.visible() && curr_snap_.match_over) {
        simulation_.restart_match();
    }
    was_menu_visible = menu_state_.visible();
}

void ClientFramePipeline::stage_build_scene() {
    scene_ = build_debug_scene(prev_snap_, curr_snap_,
        DebugSceneBuildInputs {
            .camera_mode         = frontend_state_.camera_mode,
            .metrics_visible     = frontend_state_.metrics_visible,
            .gpu_profiler_visible = frontend_state_.gpu_profiler_visible,
            .always_day          = frontend_state_.always_day,
            .menu_visible        = ui_controller_.visible(),
            .menu_tab            = ui_controller_.active_menu_tab(),
            .gamma               = client_config_.gamma,
            .displayed_metrics   = &frontend_state_.displayed_metrics,
            .alpha               = alpha_,
        });

    render_submission_ = build_debug_render_submission(
        curr_snap_, window_.gamepad_state(), scene_);
}

void ClientFramePipeline::stage_gameplay_audio() {
    static float step_timer = 0.0F;
    float speed = std::sqrt(raw_input_.move_axis.x * raw_input_.move_axis.x +
                            raw_input_.move_axis.y * raw_input_.move_axis.y);
    if (speed > 0.1F) {
        step_timer -= smoothed_delta_;
        float interval = speed > 0.7F ? 0.35F : 0.55F;
        if (step_timer <= 0.0F) {
            audio_engine_.play_sound(0, {
                curr_snap_.player_position.x,
                curr_snap_.player_position.y,
                curr_snap_.player_position.z, 0.3F});
            step_timer = interval;
        }
    }
}

void ClientFramePipeline::stage_render_world() {
    render_local_debug_frame(render_submission_, audio_engine_, shadow_pass_, renderer_,
                             pbr_renderer_, level_scene_);
}

void ClientFramePipeline::stage_render_ui() {
    auto* glfw_win = static_cast<GLFWwindow*>(window_.native_handle());
    ae::ui::sync_input_to_imgu(glfw_win);
    ae::ui::begin_ui_frame();

    ui_actions_ = ui_controller_.render(
        input_map_, window_, window_.gamepad_state(),
        curr_snap_, render_submission_.scene, client_config_);

    ae::ui::end_ui_frame();
}

void ClientFramePipeline::stage_present() {
    renderer_.present();
}

void ClientFramePipeline::stage_post_frame() {
    if (ui_actions_.config_applied) {
        window_input_.set_mouse_sensitivity(client_config_.mouse_sensitivity);
        audio_engine_.set_master_volume(client_config_.audio.master_volume);
    }
    if (ui_actions_.quit_application) {
        application_.shutdown();
    }
    if (ui_actions_.restart_match) {
        simulation_.restart_match();
    }

    simulation_.set_paused(menu_state_.simulation_should_pause());
}

}  // namespace ahamkara::client
