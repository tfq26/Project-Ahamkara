#include "ae/core/log.h"
#include "ae/platform/window.h"
#include "ae/render/compiled_level.h"
#include "ae/render/debug_renderer.h"
#include "ae/render/level_render.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/shadow_pass.h"
#include "ae/runtime/application.h"
#include "ae/ui/ahamkara_ui.h"
#include "ae/audio/audio_engine.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/client_config.h"
#include "ahamkara/client/client_frame_pipeline.h"
#include "ahamkara/client/client_menu_state.h"
#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/debug_frontend_runtime.h"
#include "ahamkara/client/debug_ui_controller.h"
#include "ahamkara/client/playtest_harness.h"
#include "ahamkara/client/threaded_local_runtime.h"
#include "ahamkara/client/window_input_provider.h"
#include "ahamkara/client/audio_player.h"
#include <GLFW/glfw3.h>

#include <exception>
#include <memory>
#include <string>
#include <vector>

int run_local_client(
    ahamkara::client::ClientConfig& client_config,
    const ahamkara::client::ControllerBindings& controller_bindings,
    const char* level_path,
    const ahamkara::client::PlaytestScenario* autoplay_scenario) {

    ae::init_file_logging("logs");

    // ── Window + ImGui ──────────────────────────────────────────────────
    ae::WindowConfig window_config {};
    window_config.title = client_config.app_name;
    window_config.width  = client_config.window_width;
    window_config.height = client_config.window_height;
    window_config.fullscreen = client_config.fullscreen;
    window_config.create_opengl_context = true;

    std::unique_ptr<ae::PlatformWindow> window;
    try {
        window = ae::PlatformWindow::create(window_config);
        GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window->native_handle());
        // Keep GLFW_RAW_MOUSE_MOTION OFF: under GLFW_CURSOR_DISABLED (in-game
        // look mode) raw motion stops delivering trackpad deltas on macOS — the
        // cursor-pos callback goes silent and mouse-look dies. With raw motion
        // off, GLFW reports virtual (accelerated) deltas, which work for both
        // trackpads and mice.
        if (glfw_win)
            glfwSetInputMode(glfw_win, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        ae::ui::initialize_ui(glfw_win, "#version 330 core");
    } catch (const std::exception& ex) {
        ae::log_error(ex.what());
        return 1;
    }

    // ── Application + renderer ──────────────────────────────────────────
    ae::Application application(ae::RuntimeMode::Client);
    (void)application.start();

    ae::render::DebugRenderer renderer;
    if (!renderer.initialize(*window)) {
        ae::log_error("Failed to initialize debug renderer.");
        return 1;
    }
    renderer.set_auto_present(false);

    ae::render::PbrRenderer pbr_renderer;
    pbr_renderer.initialize(renderer.backend());
    ae::render::ShadowPass shadow_pass;
    shadow_pass.initialize(renderer.backend(), 2048);

    // ── Simulation + input ──────────────────────────────────────────────
    std::unique_ptr<ahamkara::client::IInputProvider> input_provider;
    if (autoplay_scenario != nullptr) {
        input_provider = std::make_unique<ahamkara::client::ScenarioInputProvider>(*autoplay_scenario);
    } else {
        input_provider = std::make_unique<ahamkara::client::WindowInputProvider>(
            *window, client_config.mouse_sensitivity, controller_bindings);
    }

    ahamkara::client::ThreadedLocalRuntime simulation;

    ae::render::LevelRenderScene level_scene;
    ae::render::LevelAsset loaded_level_asset;
    const ae::render::LevelAsset* loaded_level_asset_view = nullptr;
    if (level_path != nullptr && level_path[0] != '\0') {
        simulation.load_level(level_path);
        ae::render::CompiledLevelLoader level_loader;
        if (level_loader.load(level_path, loaded_level_asset)) {
            loaded_level_asset_view = &loaded_level_asset;
            level_scene.build(loaded_level_asset, *renderer.backend(), "");
            renderer.set_level_environment(
                loaded_level_asset.sky_color_r, loaded_level_asset.sky_color_g, loaded_level_asset.sky_color_b,
                loaded_level_asset.ambient_r, loaded_level_asset.ambient_g, loaded_level_asset.ambient_b,
                loaded_level_asset.fog_density);
            ae::log_info("Loaded level '" + loaded_level_asset.name + "' from " +
                std::string(level_path) + " (" +
                std::to_string(loaded_level_asset.mesh_instances.size()) + " mesh instances in spec, " +
                std::to_string(level_scene.instance_count()) + " rendered).");
            if (!loaded_level_asset.mesh_instances.empty() && level_scene.instance_count() == 0) {
                ae::log_warning("Level has mesh instances but NONE resolved to GPU meshes; "
                    "mesh asset paths likely did not resolve (run from repo root).");
            }
        } else {
            ae::log_warning("Failed to load level from " + std::string(level_path) +
                " (path is resolved relative to the working directory; launch via "
                "./scripts/start.sh local so cwd is the repo root).");
        }
    } else {
        ae::log_info("No --level specified; starting with an empty world (no level meshes).");
    }

    if (autoplay_scenario != nullptr) {
        ahamkara::client::ClientSimulationSnapshot previous_snapshot {};
        ahamkara::client::ClientSimulationSnapshot current_snapshot {};
        float alpha = 0.0F;
        simulation.get_snapshots(previous_snapshot, current_snapshot, alpha);

        std::vector<ahamkara::game::InteractionTargetDefinition> interaction_targets =
            autoplay_scenario->interaction_targets;
        if (autoplay_scenario->add_spawn_training_target) {
            interaction_targets.push_back(ahamkara::game::InteractionTargetDefinition {
                5001,
                {
                    current_snapshot.player_position.x + autoplay_scenario->spawn_training_target_offset.x,
                    current_snapshot.player_position.y + autoplay_scenario->spawn_training_target_offset.y,
                    current_snapshot.player_position.z + autoplay_scenario->spawn_training_target_offset.z
                },
                1.5F,
                true,
                "flashback_terminal"
            });
        }
        if (!interaction_targets.empty()) {
            simulation.set_interaction_targets(interaction_targets.data(), interaction_targets.size());
        }
    }

    ahamkara::client::AudioPlayer audio_player;
    audio_player.apply_config(client_config.audio);
    simulation.set_audio_player(&audio_player);

    ae::audio::AudioEngine audio_engine;
    audio_engine.initialize();
    audio_engine.set_master_volume(client_config.audio.master_volume);

    // ── Menu state + UI controller ──────────────────────────────────────
    ahamkara::client::ClientMenuState menu_state;
    ahamkara::client::DebugFrontendState frontend_state =
        ahamkara::client::make_debug_frontend_state();
    ahamkara::client::DebugUiController ui_controller(menu_state, client_config);

    simulation.start();
    simulation.set_paused(menu_state.simulation_should_pause());

    ae::log_info("Debug view started. WASD move, mouse look, LMB fire, R reload, "
        "Space jump, Shift sprint, Ctrl crouch, C slide, F interact, 1-3 weapons, "
        "Esc menu, Tab scoreboard, F3 metrics, V third-person.");

    // ── Frame pipeline — explicit stage order ───────────────────────────
    ahamkara::client::ClientFramePipeline pipeline(
        *window, application, renderer, simulation, *input_provider,
        frontend_state, ui_controller, menu_state, client_config,
        controller_bindings, audio_engine, shadow_pass, pbr_renderer,
        &level_scene, loaded_level_asset_view, autoplay_scenario != nullptr);

    while (application.is_running()) {
        if (!pipeline.run_one_frame()) break;
    }

    // ── Shutdown ────────────────────────────────────────────────────────
    simulation.stop();
    level_scene.destroy(*renderer.backend());
    pbr_renderer.shutdown();
    renderer.shutdown();
    application.shutdown();
    ae::shutdown_file_logging();
    return EXIT_SUCCESS;
}
