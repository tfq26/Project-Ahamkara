#pragma once

#include "ahamkara/client/client_config.h"
#include "ahamkara/client/client_menu_state.h"
#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/debug_frontend_runtime.h"
#include "ahamkara/client/debug_render_runtime.h"
#include "ahamkara/client/debug_ui_controller.h"
#include "ahamkara/client/weapon_presentation.h"
#include "ahamkara/client/threaded_local_runtime.h"
#include "ahamkara/client/weapon_animation_controller.h"
#include "ahamkara/game/animation_adapter.h"
#include "ahamkara/client/window_input_provider.h"
#include "ahamkara/client/local_play.h"
#include "ahamkara/client/audio_player.h"
#include "ahamkara/game/net_types.h"
#include "ae/audio/audio_engine.h"
#include "ae/input/input_map.h"
#include "ae/render/compiled_level.h"
#include "ae/render/debug_renderer.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/shadow_pass.h"
#include "ae/runtime/application.h"
#include "ae/ui/menu_system.h"
#include "ae/ui/hud_system.h"

namespace ae {
class PlatformWindow;
}

namespace ahamkara::client {

// ============================================================================
// ClientFramePipeline — explicit per-frame orchestration.
//
// Each frame stage is a named step, called in order inside run_one_frame().
// This replaces the monolithic loop in debug_client.cpp with a readable
// pipeline that makes ownership and stage order obvious.
// ============================================================================

class ClientFramePipeline {
public:
    ClientFramePipeline(
        ae::PlatformWindow& window,
        ae::Application& application,
        ae::render::DebugRenderer& renderer,
        ThreadedLocalRuntime& simulation,
        IInputProvider& input_provider,
        DebugFrontendState& frontend_state,
        DebugUiController& ui_controller,
        ClientMenuState& menu_state,
        ClientConfig& client_config,
        const ControllerBindings& controller_bindings,
        ae::audio::AudioEngine& audio_engine,
        ae::render::ShadowPass& shadow_pass,
        ae::render::PbrRenderer& pbr_renderer,
        const ae::render::LevelRenderScene* level_scene,
        const ae::render::LevelAsset* level_asset,
        bool autoplay_mode = false);

    /// Run one full frame. Returns false when the window or application
    /// requests shutdown.
    [[nodiscard]] bool run_one_frame();

private:
    // Stage 1 — poll OS events and gather raw input state
    void stage_poll_input();
    // Stage 2 — process menu toggle, hotkeys, cursor mode
    void stage_handle_menu_and_hotkeys();
    // Stage 3 — gather gameplay input and submit to simulation
    void stage_gather_gameplay_input();
    // Stage 4 — pull simulation snapshots for this frame
    void stage_pull_snapshots();
    // Stage 5 — build debug scene from snapshots
    void stage_build_scene();
    // Stage 6 — footstep / procedural audio
    void stage_gameplay_audio();
    // Stage 7 — render the 3D world
    void stage_render_world();
    // Stage 8 — render ImGui UI and menus
    void stage_render_ui();
    // Stage 9 — present to screen
    void stage_present();
    // Stage 10 — apply post-frame actions
    void stage_post_frame();

    ae::PlatformWindow& window_;
    ae::Application& application_;
    ae::render::DebugRenderer& renderer_;
    ThreadedLocalRuntime& simulation_;
    IInputProvider& input_provider_;
    DebugFrontendState& frontend_state_;
    DebugUiController& ui_controller_;
    ClientMenuState& menu_state_;
    ClientConfig& client_config_;
    const ControllerBindings& controller_bindings_;
    ae::audio::AudioEngine& audio_engine_;
    ae::render::ShadowPass& shadow_pass_;
    ae::render::PbrRenderer& pbr_renderer_;
    const ae::render::LevelRenderScene* level_scene_ {nullptr};
    const ae::render::LevelAsset* level_asset_ {nullptr};
    ae::input::InputMap input_map_ {};

    float smoothed_delta_ {0.0F};
    ahamkara::game::PlayerInputCommand raw_input_ {};
    ClientSimulationSnapshot prev_snap_ {};
    ClientSimulationSnapshot curr_snap_ {};
    float alpha_ {0.0F};
    ae::render::DebugScene scene_ {};
    DebugRenderSubmission render_submission_ {};
    WeaponAnimationController weapon_animation_ {};
    WeaponViewmodelPresentation weapon_presentation_ {};
    ahamkara::game::AnimationAdapter anim_adapter_ {};
    std::string window_title_ {};
    DebugUiActions ui_actions_ {};
    ae::ui::MenuSystem menu_system_;
    ae::ui::HudSystem hud_system_;
    bool gameplay_active_{false};
    bool menu_initialized_{false};
    bool hud_loaded_{false};
    bool autoplay_mode_{false};
};

}  // namespace ahamkara::client
