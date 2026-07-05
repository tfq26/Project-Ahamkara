#include "ae/core/log.h"
#include "ahamkara/client/client_frame_pipeline.h"

#include "ae/core/math.h"
#include "ae/platform/window.h"
#include "ae/ui/ahamkara_ui.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/debug_render_runtime.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#define AE_LOG_CATEGORY "Client"

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
    const ae::render::LevelRenderScene* level_scene,
    const ae::render::LevelAsset* level_asset)
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
    , level_scene_(level_scene)
    , level_asset_(level_asset) {
    if (menu_system_.load_from_directory("assets/menus")) {
        // Seed settings from current config
        menu_system_.set_variable("gamma", client_config_.gamma);
        menu_system_.set_variable("fullscreen", client_config_.fullscreen ? 1.0f : 0.0f);
        menu_system_.set_variable("master_volume", client_config_.audio.master_volume);
        menu_system_.set_variable("sfx_volume", client_config_.audio.sfx_volume);
        menu_system_.set_variable("audio_enabled", client_config_.audio.enabled ? 1.0f : 0.0f);
        menu_system_.set_variable("mouse_sensitivity", client_config_.mouse_sensitivity);

        // ── Gameplay actions ──────────────────────────────────────────
        menu_system_.register_action("start_game", [this](std::string_view param) {
            gameplay_active_ = true;
            std::string path(param.empty() ? "assets/compiled/levels/javelin4.aelevel" : std::string(param));
            // Show loading screen while loading
            menu_system_.show_screen("loading_screen");
            menu_system_.set_variable("loading_map_name", path);

            // Use a simple staged simulation: load level in background, then unpause
            simulation_.set_paused(true);
            bool loaded = simulation_.load_level(path);
            simulation_.set_paused(!loaded);
            menu_system_.set_variable("loading_progress", loaded ? "1.0" : "0.5");
            menu_system_.set_variable("loading_status", loaded ? "Ready." : "Failed to load map.");

            // Small delay so loading screen is visible, then pop to gameplay
            // In production this would be async with progress callbacks
            menu_system_.pop_to_root();
            simulation_.set_paused(false);
        });
        menu_system_.register_action("start_sandbox", [this](std::string_view) {
            gameplay_active_ = true;
            menu_system_.pop_to_root();
            simulation_.set_paused(false);
        });
        menu_system_.register_action("resume_game", [this](std::string_view) {
            menu_system_.pop_screen();
            simulation_.set_paused(false);
        });
        menu_system_.register_action("quit_application", [this](std::string_view) {
            application_.shutdown();
        });

        // ── Settings: apply immediately ───────────────────────────────
        auto apply_settings = [this]() {
            const auto& fv = menu_system_.float_vars();
            auto get = [&fv](const std::string& k, float def) { auto it = fv.find(k); return it != fv.end() ? it->second : def; };
            client_config_.gamma = get("gamma", 1.0f);
            float master = get("master_volume", 1.0f);
            float sfx = get("sfx_volume", 1.0f);
            bool audio_on = get("audio_enabled", 1.0f) > 0.5f;
            float sens = get("mouse_sensitivity", 1.0f);

            audio_engine_.set_master_volume(audio_on ? master : 0.0f);
            client_config_.audio.master_volume = master;
            client_config_.audio.sfx_volume = sfx;
            client_config_.audio.enabled = audio_on;
            client_config_.mouse_sensitivity = sens;
            window_input_.set_mouse_sensitivity(sens);

            // Save to config file
            client_config_.save_to_file("client/config/ahamkara.cfg");
        };
        menu_system_.register_action("apply_settings", [apply_settings](std::string_view) { apply_settings(); });
        menu_system_.register_action("setting_changed", [apply_settings](std::string_view) { apply_settings(); });

        menu_initialized_ = true;
        menu_system_.show_screen("main_menu");
    }

    // Load HUD layout
    hud_loaded_ = hud_system_.load("assets/menus/hud.json");
}

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
    const auto& debug_state = window_.gamepad_debug_state();
    GLFWwindow* glfw_win = static_cast<GLFWwindow*>(window_.native_handle());

    const bool menu_toggle =
        window_.is_key_pressed(ae::KeyCode::Escape)
        || debug_state.is_code_pressed(controller_bindings_.menu);

    if (menu_toggle && menu_initialized_) {
        if (gameplay_active_ && !menu_state_.visible()) {
            menu_system_.set_active_screen("pause_menu", true);
            simulation_.set_paused(true);
        } else if (menu_state_.visible()) {
            menu_system_.pop_screen();
            simulation_.set_paused(false);
        }
        menu_state_.toggle_menu();
    }

    if (glfw_win) {
        glfwSetInputMode(glfw_win, GLFW_CURSOR,
            menu_state_.cursor_should_capture() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

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
    simulation_.set_paused(menu_state_.simulation_should_pause());
    raw_input_ = window_input_.gather_input(smoothed_delta_);
    if (menu_state_.gameplay_input_enabled()) {
        simulation_.submit_input(raw_input_);
    }
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
    // Track damage feedback for VFX
    static float damage_feedback_timer = 0.0F;
    static float damage_flash_timer = 0.0F;
    static float prev_health = 100.0F;

    scene_ = build_debug_scene(prev_snap_, curr_snap_,
        DebugSceneBuildInputs {
            .camera_mode         = frontend_state_.camera_mode,
            .metrics_visible     = frontend_state_.metrics_visible,
            .gpu_profiler_visible = frontend_state_.gpu_profiler_visible,
            .always_day          = frontend_state_.always_day,
            .menu_visible        = menu_state_.visible(),
            .menu_tab            = ui_controller_.active_menu_tab(),
            .gamma               = client_config_.gamma,
            .displayed_metrics   = &frontend_state_.displayed_metrics,
            .alpha               = alpha_,
            .level_asset         = level_asset_,
        });

    weapon_animation_.tick(
        smoothed_delta_,
        curr_snap_,
        raw_input_);

    weapon_presentation_.set_backend(renderer_.backend());
    weapon_presentation_.tick(smoothed_delta_);
    scene_.weapon_model = weapon_presentation_.resolve_viewmodel(curr_snap_.weapon_index);

    const auto* joints = weapon_presentation_.joint_matrices(curr_snap_.weapon_index);
    int jc = weapon_presentation_.joint_count(curr_snap_.weapon_index);
    if (joints && jc > 0) {
        constexpr int kMaxWeaponJoints = static_cast<int>(sizeof(scene_.weapon_joint_matrices) / sizeof(scene_.weapon_joint_matrices[0]) / 16);
        const int copy_count = std::min(jc, kMaxWeaponJoints);
        scene_.weapon_joint_count = copy_count;
        std::memcpy(scene_.weapon_joint_matrices, joints, static_cast<std::size_t>(copy_count) * sizeof(ae::render::Mat4));
    } else {
        scene_.weapon_joint_count = 0;
    }

    scene_.weapon_animation_override = weapon_animation_.has_transform();
    if (scene_.weapon_animation_override) {
        const auto& transform = weapon_animation_.transform();
        for (int i = 0; i < 16; ++i) {
            scene_.weapon_animation_transform[i] = transform[static_cast<std::size_t>(i)];
        }
    }

    // --- AnimationAdapter: drive character animation from gameplay state ---
    const float hspeed = std::sqrt(
        curr_snap_.player_state.velocity.x * curr_snap_.player_state.velocity.x +
        curr_snap_.player_state.velocity.z * curr_snap_.player_state.velocity.z);
    const bool is_moving = hspeed > 0.1F;
    const bool is_sprinting = curr_snap_.player_state.movement_state == ahamkara::game::MovementState::Sprinting;

    anim_adapter_.set_movement(hspeed, is_moving, is_sprinting);
    constexpr float kNoPitch = 0.0F;  // pitch not replicated in ReplicatedPlayerState
    anim_adapter_.set_aim(curr_snap_.player_state.yaw, kNoPitch);
    const bool is_firing = raw_input_.fire_held || curr_snap_.player_state.health < prev_health - 1.0F;
    const bool is_reloading = raw_input_.reload_pressed && curr_snap_.ammo_current < curr_snap_.ammo_max;
    anim_adapter_.set_weapon(curr_snap_.weapon_index,
                              is_firing,
                              is_reloading);
    anim_adapter_.set_health(curr_snap_.player_state.health, 100.0F);

    // Detect health drop → trigger hit reaction
    const float snap_health = curr_snap_.player_state.health;
    if (snap_health < prev_health && snap_health > 0.0F) {
        anim_adapter_.trigger_hit_reaction();
        damage_feedback_timer = 1.0F;
        damage_flash_timer = 0.4F;
    }
    prev_health = snap_health;

    // Also check snapshot damage_feedback_timer for hit reaction
    if (curr_snap_.damage_feedback_timer > 0.0F && damage_feedback_timer <= 0.0F) {
        anim_adapter_.trigger_hit_reaction();
        damage_feedback_timer = 1.0F;
        damage_flash_timer = 0.4F;
    }

    anim_adapter_.tick(smoothed_delta_);

    // Copy character joint matrices into debug scene
    const auto& pose = anim_adapter_.joint_pose();
    const int char_jc = anim_adapter_.joint_count();
    constexpr int kMaxCharJoints = static_cast<int>(sizeof(scene_.character_joint_matrices) / sizeof(scene_.character_joint_matrices[0]) / 16);
    scene_.character_joint_count = std::min(char_jc, kMaxCharJoints);
    for (int i = 0; i < scene_.character_joint_count; ++i) {
        std::memcpy(&scene_.character_joint_matrices[i * 16], &pose[static_cast<std::size_t>(i)], 16 * sizeof(float));
    }

    // --- Screen shake ---
    // Decay
    scene_.screen_shake_intensity = std::max(0.0F, scene_.screen_shake_intensity - smoothed_delta_ * 3.0F);
    // Muzzle flash triggers small shake when firing
    if (raw_input_.fire_held) {
        scene_.screen_shake_intensity = std::min(1.0F, scene_.screen_shake_intensity + 0.005F);
    }
    // Damage triggers larger shake
    if (damage_feedback_timer > 0.0F) {
        damage_feedback_timer = std::max(0.0F, damage_feedback_timer - smoothed_delta_);
        scene_.screen_shake_intensity = std::min(1.0F, scene_.screen_shake_intensity + 0.015F);
    }
    scene_.screen_shake_angle = scene_.screen_shake_intensity * 3.0F;
    scene_.screen_shake_frequency = 15.0F;

    // --- Damage flash ---
    if (damage_flash_timer > 0.0F) {
        damage_flash_timer = std::max(0.0F, damage_flash_timer - smoothed_delta_);
        scene_.damage_flash_intensity = std::min(damage_flash_timer * 3.0F, 0.4F);
    } else {
        scene_.damage_flash_intensity = 0.0F;
    }

    // --- Melee active ---
    scene_.melee_active = anim_adapter_.is_melee_active();

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

    // Poll for hot-reloads
    if (menu_initialized_) menu_system_.poll_hot_reload();
    if (hud_loaded_)      hud_system_.poll_hot_reload();

    // Update dynamic menu variables
    if (menu_initialized_) {
        menu_system_.set_variable("build_date", __DATE__ " " __TIME__);
        menu_system_.set_variable("fps", static_cast<int>(frontend_state_.displayed_metrics.fps));
    }

    // Render menus (main, pause, settings, map select, loading)
    if (menu_initialized_ && menu_system_.is_visible()) {
        menu_system_.render();
    }

    // Render gameplay HUD (new JSON-driven system + legacy crosshair fallback)
    if (hud_loaded_ && gameplay_active_ && !menu_system_.is_visible()) {
        ae::ui::HudState hud_state;
        hud_state.health = curr_snap_.player_state.health;
        hud_state.max_health = 100.0f;
        hud_state.ammo_current = static_cast<int>(curr_snap_.ammo_current);
        hud_state.ammo_max = static_cast<int>(curr_snap_.ammo_max);
        hud_state.reserve_ammo = static_cast<int>(curr_snap_.reserve_ammo);
        hud_state.weapon_index = curr_snap_.weapon_index;
        hud_state.weapon_name = ahamkara::game::weapon_name(curr_snap_.weapon_index);
        hud_state.crosshair_visible = render_submission_.scene.show_crosshair && !render_submission_.scene.menu_visible;
        hud_state.crosshair_spread = raw_input_.move_axis.x * raw_input_.move_axis.x + raw_input_.move_axis.y * raw_input_.move_axis.y;

        // Ability state from combat runtime
        hud_state.grenade_cooldown = curr_snap_.grenade_cooldown;
        hud_state.grenade_count = curr_snap_.grenade_count;
        hud_state.grenade_available = curr_snap_.grenade_available;
        hud_state.special_cooldown = curr_snap_.special_cooldown;
        hud_state.special_available = curr_snap_.special_available;
        hud_state.artifact_cooldown = curr_snap_.artifact_cooldown;
        hud_state.ultimate_charge = curr_snap_.ultimate_charge;
        hud_state.ultimate_ready = curr_snap_.ultimate_ready;

        auto& io = ImGui::GetIO();
        hud_system_.render(io.DisplaySize.x, io.DisplaySize.y, hud_state);
    }

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
