#include "ae/core/log.h"
#include "ae/core/console.h"
#include "ahamkara/client/client_frame_pipeline.h"

#include "ae/core/math.h"
#include <imgui.h>
#include <algorithm>
#include "ae/platform/window.h"
#include "ae/ui/ahamkara_ui.h"
#include "ahamkara/client/camera_mode.h"
#include "ahamkara/client/window_input_provider.h"
#include "ahamkara/client/debug_render_runtime.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/client/weapon_viewmodel_data.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_types.h"
#if defined(AHAMKARA_ENABLE_GAME_MCP)
#include "ae/render/gl_platform.h"
#endif

#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {

void ClientFramePipeline::shutdown_render_resources() {
    if (renderer_.backend() != nullptr) {
        animated_character_.destroy();
        caligo_director_.destroy(*renderer_.backend());
    }
}

ClientFramePipeline::ClientFramePipeline(
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
    ae::render::LevelRenderScene* level_scene,
    ae::render::LevelAsset* level_asset,
    bool autoplay_mode)
    : window_(window), application_(application), renderer_(renderer), simulation_(simulation), input_provider_(input_provider), frontend_state_(frontend_state), ui_controller_(ui_controller), menu_state_(menu_state), client_config_(client_config), controller_bindings_(controller_bindings), audio_engine_(audio_engine), shadow_pass_(shadow_pass), pbr_renderer_(pbr_renderer), level_scene_(level_scene), level_asset_(level_asset), autoplay_mode_(autoplay_mode) {
    caligo_director_.initialize(*renderer.backend());
    animated_character_.initialize(
        *renderer.backend(),
        "assets/compiled/models/operative_human_base.aemesh",
        "assets/compiled/models/operative_human_rifle_idle_static.aemesh");
    animated_character_.set_appearance_source(client_config_.character);
    if (menu_system_.load_from_directory("assets/menus")) {
        // Seed settings from current config
        menu_system_.set_variable("gamma", client_config_.gamma);
        menu_system_.set_variable("fullscreen", client_config_.fullscreen ? 1.0f : 0.0f);
        menu_system_.set_variable("master_volume", client_config_.audio.master_volume);
        menu_system_.set_variable("sfx_volume", client_config_.audio.sfx_volume);
        menu_system_.set_variable("audio_enabled", client_config_.audio.enabled ? 1.0f : 0.0f);
        menu_system_.set_variable("mouse_sensitivity", client_config_.mouse_sensitivity);
        menu_system_.set_variable("suit_color_r", client_config_.character.suit_color_r);
        menu_system_.set_variable("suit_color_g", client_config_.character.suit_color_g);
        menu_system_.set_variable("suit_color_b", client_config_.character.suit_color_b);
        menu_system_.set_variable("quest_first_contact", 0.65f);
        menu_system_.set_variable("quest_first_contact_percent", 65.0f);

        // ── Gameplay actions ──────────────────────────────────────────
        menu_system_.register_action("start_game", [this](std::string_view param) {
            std::string path(param.empty() ? "assets/compiled/levels/javelin4.aelevel" : std::string(param));
            // Show loading screen while loading
            menu_system_.show_screen("loading_screen");
            menu_system_.set_variable("loading_map_name", path);

            // Use a simple staged simulation: load level in background, then unpause
            simulation_.set_paused(true);
            bool loaded = simulation_.load_level(path);
            if (loaded && level_scene_ != nullptr && level_asset_ != nullptr) {
                ae::render::CompiledLevelLoader level_loader;
                loaded = level_loader.load(path, *level_asset_);
                if (loaded) {
                    level_scene_->destroy(*renderer_.backend());
                    level_scene_->build(*level_asset_, *renderer_.backend(), "");
                    renderer_.set_level_environment(
                        level_asset_->sky_color_r, level_asset_->sky_color_g, level_asset_->sky_color_b,
                        level_asset_->ambient_r, level_asset_->ambient_g, level_asset_->ambient_b);
                    renderer_.clear_level_skybox_texture();
                    if (!level_asset_->skybox_material.empty()) {
                        (void)renderer_.set_level_skybox_texture(level_asset_->skybox_material);
                    }
                }
            }
            gameplay_active_ = loaded;
            menu_system_.set_variable("loading_progress", loaded ? "1.0" : "0.5");
            menu_system_.set_variable("loading_status", loaded ? "Ready." : "Failed to load map.");

            if (loaded) {
                // Small delay so loading screen is visible, then close every
                // menu layer and enter gameplay. In production this would be
                // async with progress callbacks.
                menu_system_.pop_to_root();
                menu_system_.pop_screen();
                menu_state_.start_gameplay();
                simulation_.set_paused(false);
            } else {
                // Keep the player in the Director when an activity cannot be
                // loaded. Do not expose a partially initialized game world.
                menu_system_.show_screen("director");
                simulation_.set_paused(true);
            }
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
        menu_system_.register_action("director_exterior", [this](std::string_view) {
            caligo_director_.set_view(CaligoDirector::View::Exterior);
        });
        menu_system_.register_action("director_cutaway", [this](std::string_view) {
            caligo_director_.set_view(CaligoDirector::View::Cutaway);
        });

        // ── Settings: apply immediately ───────────────────────────────
        auto apply_settings = [this]() {
            const auto& fv = menu_system_.float_vars();
            auto get = [&fv](const std::string& k, float def) { auto it = fv.find(k); return it != fv.end() ? it->second : def; };
            client_config_.gamma = get("gamma", 1.0f);
            client_config_.fullscreen = get("fullscreen", 0.0f) > 0.5f;
            float master = get("master_volume", 1.0f);
            float sfx = get("sfx_volume", 1.0f);
            bool audio_on = get("audio_enabled", 1.0f) > 0.5f;
            float sens = get("mouse_sensitivity", 1.0f);
            client_config_.character.suit_color_r = get("suit_color_r", 0.62F);
            client_config_.character.suit_color_g = get("suit_color_g", 0.37F);
            client_config_.character.suit_color_b = get("suit_color_b", 0.17F);
            client_config_.character.suit_color_r =
                clamp_character_color(client_config_.character.suit_color_r);
            client_config_.character.suit_color_g =
                clamp_character_color(client_config_.character.suit_color_g);
            client_config_.character.suit_color_b =
                clamp_character_color(client_config_.character.suit_color_b);

            audio_engine_.set_master_volume(audio_on ? master : 0.0f);
            client_config_.audio.master_volume = master;
            client_config_.audio.sfx_volume = sfx;
            client_config_.audio.enabled = audio_on;
            client_config_.mouse_sensitivity = sens;
            if (auto* window_input = dynamic_cast<WindowInputProvider*>(&input_provider_)) {
                window_input->set_mouse_sensitivity(sens);
            }

            // Save to config file
            (void)client_config_.save_to_file("client/config/ahamkara.cfg");
        };
        menu_system_.register_action("apply_settings", [apply_settings](std::string_view) { apply_settings(); });
        menu_system_.register_action("setting_changed", [apply_settings](std::string_view) { apply_settings(); });
        menu_system_.register_action("suit_preset", [this, apply_settings](std::string_view preset) {
            float red = 0.62F;
            float green = 0.37F;
            float blue = 0.17F;
            if (preset == "storm") {
                red = 0.16F;
                green = 0.30F;
                blue = 0.42F;
            } else if (preset == "signal") {
                red = 0.72F;
                green = 0.16F;
                blue = 0.08F;
            } else if (preset == "bone") {
                red = 0.70F;
                green = 0.64F;
                blue = 0.52F;
            }
            menu_system_.set_variable("suit_color_r", red);
            menu_system_.set_variable("suit_color_g", green);
            menu_system_.set_variable("suit_color_b", blue);
            apply_settings();
        });
        menu_initialized_ = true;
        menu_system_.show_screen("main_menu");

        if (autoplay_mode_ || (level_scene_ != nullptr && !level_scene_->empty())) {
            menu_system_.pop_to_root();
            menu_system_.pop_screen();
            menu_state_.start_gameplay();
            gameplay_active_ = true;
        }
    }

    // Load HUD layout
    hud_loaded_ = hud_system_.load("assets/menus/hud.json");

    // ── Authoring: Developer Console ────────────────────────────────────
    console_.register_builtins();
    console_.set_config_registry(&ae::ConfigRegistry::instance());

    // ── Authoring: File Watcher ─────────────────────────────────────────
    // Watch the config file for live-reload.
    // If the config file is invalid (parse error, malformed lines, etc.), the
    // last valid state is preserved and a warning diagnostic is emitted by
    // reload_from_file internally.  No silent partial update occurs.
    file_watcher_.watch("client/config/ahamkara.cfg", [this](const std::string& path) {
        ae::log_info_cat("Client", "Config file changed: " + path);
        ae::ConfigRegistry::instance().reload_from_file(path);
    });

#if defined(AHAMKARA_ENABLE_GAME_MCP)
    if (auto bridge_config = GameMcpBridge::config_from_environment(); bridge_config.has_value()) {
        game_mcp_bridge_ = std::make_unique<GameMcpBridge>(std::move(*bridge_config));
        if (!game_mcp_bridge_->start()) {
            game_mcp_bridge_.reset();
        }
    }
    if (game_mcp_bridge_ != nullptr) {
        const char* start_screen = std::getenv("AHAMKARA_GAME_MCP_START_SCREEN");
        if (start_screen != nullptr && std::string_view(start_screen) == "character_view") {
            menu_system_.show_screen("character_view");
            simulation_.set_paused(true);
        }
        const char* qa_color = std::getenv("AHAMKARA_GAME_MCP_SUIT_COLOR");
        float red = 0.0F;
        float green = 0.0F;
        float blue = 0.0F;
        if (qa_color != nullptr &&
            std::sscanf(qa_color, "%f,%f,%f", &red, &green, &blue) == 3) {
            red = clamp_character_color(red);
            green = clamp_character_color(green);
            blue = clamp_character_color(blue);
            menu_system_.set_variable("suit_color_r", red);
            menu_system_.set_variable("suit_color_g", green);
            menu_system_.set_variable("suit_color_b", blue);
        }
        const char* start_emote = std::getenv("AHAMKARA_GAME_MCP_START_EMOTE");
        if (start_emote != nullptr && std::string_view(start_emote) == "dance") {
            character_dance_active_ = true;
        }
    }
#endif

    ae::log_info_cat("Client", "Developer console initialized. Press ` (backtick) to open.");
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

    // ── Console toggle (backtick/tilde) ────────────────────────────────
    if (window_.is_key_pressed(ae::KeyCode::Backtick)) {
        console_open_ = !console_open_;
        console_history_pos_ = -1;
        console_input_buffer_[0] = '\0';
        if (console_open_) {
            // Show cursor for console input
            if (glfw_win) {
                glfwSetInputMode(glfw_win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        ae::log_info_cat("Client", console_open_ ? "Console opened." : "Console closed.");
    }

    // ── Inspector toggle (F2) ───────────────────────────────────────────
    if (window_.is_key_pressed(ae::KeyCode::F2)) {
        inspector_.toggle();
        ae::log_info_cat("Client", inspector_.visible() ? "Inspector opened." : "Inspector closed.");
    }

    // The emote is deliberately available in both the dedicated Character
    // View and the developer third-person gameplay view. This lets art and
    // animation be checked against the exact same skinned asset before a
    // later gameplay-facing emote wheel is introduced.
    if (window_.is_key_pressed(ae::KeyCode::F6)) {
        character_dance_active_ = !character_dance_active_;
        ae::log_info_cat(
            "Character",
            character_dance_active_ ? "Character dance emote enabled." : "Character dance emote stopped.");
    }

    const std::string base_title = client_config_.app_name;
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
#if defined(AHAMKARA_ENABLE_GAME_MCP)
    // When the bridge is enabled it is exclusive: a missing command means
    // neutral input, preventing a remote test from accidentally mixing with
    // a developer's keyboard or mouse input.
    if (game_mcp_bridge_ && game_mcp_bridge_->running()) {
        raw_input_ = {};
        (void)game_mcp_bridge_->poll_input(raw_input_);
    } else {
        raw_input_ = input_provider_.gather_input(smoothed_delta_);
    }
#else
    raw_input_ = input_provider_.gather_input(smoothed_delta_);
#endif
    if (gameplay_active_ && menu_state_.gameplay_input_enabled()) {
        simulation_.submit_input(raw_input_);
    }
}

void ClientFramePipeline::stage_pull_snapshots() {
    simulation_.get_snapshots(prev_snap_, curr_snap_, alpha_);

#if defined(AHAMKARA_ENABLE_GAME_MCP)
    if (game_mcp_bridge_ && game_mcp_bridge_->running()) {
        game_mcp_bridge_->publish_snapshot(
            curr_snap_, application_.frame_index(),
            static_cast<float>(application_.frame_index()) * smoothed_delta_,
            frontend_state_.displayed_metrics.fps);
    }
#endif

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
    static ae::u32 last_vfx_event_id = 0;

    const auto& pacer = frontend_state_.frame_pacer;
    const auto& budget = frontend_state_.budget_tracker;

    scene_ = build_debug_scene(prev_snap_, curr_snap_,
                               DebugSceneBuildInputs {
                                   .camera_mode = frontend_state_.camera_mode,
                                   .metrics_visible = frontend_state_.metrics_visible,
                                   .gpu_profiler_visible = frontend_state_.gpu_profiler_visible,
                                   .always_day = frontend_state_.always_day,
                                   // The JSON MenuSystem owns the menu UI when
                                   // initialized. Keep the legacy renderer
                                   // menu hidden so both systems do not stack
                                   // over one another.
                                   .menu_visible = menu_state_.visible() && !menu_initialized_,
                                   .menu_tab = ui_controller_.active_menu_tab(),
                                   .gamma = client_config_.gamma,
                                   .displayed_metrics = &frontend_state_.displayed_metrics,
                                   .alpha = alpha_,
                                   .level_asset = level_asset_,
                                   .profile_snapshot = &frontend_state_.last_profile_snapshot,
                                   .frame_budget_ms = pacer.budget_ms(),
                                   .frame_p1_low_ms = pacer.percentile_01_low_ms(),
                                   .frame_rolling_avg_ms = pacer.rolling_avg_ms(),
                                   .frame_budget_compliance = pacer.budget_compliance(),
                                   .frame_pacing_healthy = pacer.pacing_healthy(),
                                   .frame_regression = pacer.regression_detected(),
                                   .rss_pressure = static_cast<std::uint8_t>(budget.rss_pressure()),
                                   .rss_bytes = static_cast<double>(budget.rss_bytes()),
                                   .rss_peak_bytes = static_cast<double>(budget.peak_rss_bytes()),
                                   .rss_soft_budget = static_cast<double>(budget.rss_soft_bytes()),
                                   .rss_hard_budget = static_cast<double>(budget.rss_hard_bytes()),
                                   .frame_alloc_pressure = 0, // populated per-frame if tracking a frame allocator
                                   .frame_alloc_peak_bytes = 0.0,
                                   .frame_alloc_capacity_bytes = 0.0,
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

    // Populate viewmodel offset tuning from presentation-layer per-weapon data.
    // These are additive to the base viewmodel position/rotation computed from
    // the camera anchor.  No fields leak into gameplay or WeaponRuntime.
    //
    // During ADS, the viewmodel offset is linearly interpolated between the
    // hip-fire position and the hip-fire position + ADS offset, using the
    // ads_blend value (0=hip, 1=fully aimed) from the weapon animation state.
    {
        const auto& vm = weapon_viewmodel_transform(curr_snap_.weapon_index);
        const auto& ads = weapon_ads_transform(curr_snap_.weapon_index);
        const float ads_blend = weapon_animation_.ads_blend();

        // Linearly blend between hip and ADS position offset
        const float blended_right   = vm.pos_right   + ads.ads_pos_right   * ads_blend;
        const float blended_up      = vm.pos_up      + ads.ads_pos_up      * ads_blend;
        const float blended_forward = vm.pos_forward  + ads.ads_pos_forward * ads_blend;

        scene_.viewmodel_position_offset = {
            blended_right,
            blended_up,
            blended_forward,
        };
        scene_.viewmodel_fov_scale   = vm.fov_scale;

        // Blend rotation offsets
        scene_.viewmodel_pitch_deg   = vm.pitch_deg   + ads.ads_pitch_deg   * ads_blend;
        scene_.viewmodel_yaw_deg     = vm.yaw_deg     + ads.ads_yaw_deg     * ads_blend;
        scene_.viewmodel_roll_deg    = vm.roll_deg    + ads.ads_roll_deg    * ads_blend;

        // Camera FOV zoom: interpolate from base FOV down to base * ads_fov_scale
        constexpr float kBaseFovDeg = 60.0F;
        const float target_ads_fov = kBaseFovDeg * ads.ads_fov_scale;
        scene_.camera_fov_override_deg = kBaseFovDeg - (kBaseFovDeg - target_ads_fov) * ads_blend;
    }

    // Apply viewmodel arm IK to position hands at weapon grip sockets.
    // This runs on the scene copy of joint matrices (not the cache originals)
    // and adjusts shoulder/elbow rotations so the hand reaches the grip target.
    // If a reload animation is active, an IK offset moves the hand from the
    // grip socket to the magazine position during reload phases.
    if (scene_.weapon_joint_count > 0) {
        const float* reload_ik = weapon_animation_.reload_ik_offset();
        const bool has_reload_offset = reload_ik != nullptr &&
            (reload_ik[0] != 0.0F || reload_ik[1] != 0.0F || reload_ik[2] != 0.0F);
        apply_viewmodel_arm_ik(curr_snap_.weapon_index,
                                scene_.weapon_joint_matrices,
                                scene_.weapon_joint_count,
                                has_reload_offset ? reload_ik : nullptr);
    }

    // Populate debug IK visualization fields.
    {
        auto grip = weapon_grip_sockets(curr_snap_.weapon_index);
        scene_.ik_target_position = {grip.grip_right_x, grip.grip_right_y, grip.grip_right_z};
        scene_.show_ik_target = true;
        if (scene_.weapon_joint_count > 5) {
            const float* jm = scene_.weapon_joint_matrices;
            scene_.arm_shoulder_pos = {jm[2*16+12], jm[2*16+13], jm[2*16+14]};
            scene_.arm_elbow_pos    = {jm[3*16+12], jm[3*16+13], jm[3*16+14]};
            scene_.arm_hand_pos     = {jm[5*16+12], jm[5*16+13], jm[5*16+14]};
            scene_.show_arm_chain = true;
        }
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

    // Detect health drop → trigger hit reaction (legacy path)
    const float snap_health = curr_snap_.player_state.health;
    if (snap_health < prev_health && snap_health > 0.0F) {
        anim_adapter_.trigger_hit_reaction();
    }
    prev_health = snap_health;

    // Authoritative VFX feedback from snapshot (one-shot via event_id).
    // The simulation emits VfxFeedback with invariant envelope parameters and
    // a monotonic event_id.  We only start a new local animation when the
    // event_id advances, which prevents replay on rollback/reconciliation.
    const auto& vfx = curr_snap_.vfx_feedback;
    if (vfx.event_id != 0 && vfx.event_id != last_vfx_event_id) {
        last_vfx_event_id = vfx.event_id;
        anim_adapter_.trigger_hit_reaction();
        damage_feedback_timer = vfx.screen_shake_duration > 0.0F
                                    ? vfx.screen_shake_duration
                                    : curr_snap_.damage_feedback_timer;
        damage_flash_timer = vfx.damage_flash_duration > 0.0F
                                 ? vfx.damage_flash_duration
                                 : 0.4F;
    }

    // Legacy fallback: check snapshot damage_feedback_timer for hit reaction
    // when no VfxFeedback event_id is present (older snapshots without VFX).
    if (vfx.event_id == 0 && curr_snap_.damage_feedback_timer > 0.0F && damage_feedback_timer <= 0.0F && last_vfx_event_id == 0) {
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

    // The JSON-driven HUD owns gameplay presentation. Keep the renderer's
    // legacy diagnostic HUD disabled so health, ammo, and abilities are not
    // drawn twice beneath the authored interface.
    if (hud_loaded_) {
        scene_.hud_visible = false;
        render_submission_.scene.hud_visible = false;
    }

    const bool director_visible = menu_initialized_ && menu_system_.current_screen() == "director";
    const bool menu_only_world = !gameplay_active_ || director_visible || menu_state_.visible();
    if (menu_only_world) {
        // The simulation snapshot is still available while menus are open,
        // but it must never leak gameplay presentation into a menu frame.
        scene_.show_player_marker = false;
        scene_.show_crosshair = false;
        scene_.draw_default_map = false;
        scene_.debug_world_visible = false;
        scene_.hud_visible = false;
        scene_.metrics_visible = false;
        scene_.gpu_profiler_visible = false;
        scene_.level_box_count = 0;
        scene_.dummy_count = 0;
        scene_.projectile_count = 0;
        scene_.particle_count = 0;
        scene_.decal_count = 0;
        scene_.hit_number_count = 0;
        scene_.menu_visible = false;
        render_submission_ = build_debug_render_submission(
            curr_snap_, window_.gamepad_state(), scene_);
    }

    if (director_visible) {
        caligo_director_.apply_camera(scene_);
        render_submission_ = build_debug_render_submission(
            curr_snap_, window_.gamepad_state(), scene_);
    }
}

void ClientFramePipeline::stage_gameplay_audio() {
    if (!gameplay_active_ || menu_state_.visible()) {
        return;
    }

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
    // MenuSystem is the sole menu renderer when its assets loaded. Enforce the
    // boundary on the final submission as well as during scene construction so
    // no legacy debug-menu state can leak into the world render pass.
    if (menu_initialized_) {
        render_submission_.scene.menu_visible = false;
    }
    const bool director_visible = menu_initialized_ && menu_system_.current_screen() == "director";
    const bool character_view_visible = menu_initialized_ &&
                                        menu_system_.current_screen() == "character_view";
    const auto* render_scene = director_visible
                                   ? caligo_director_.render_scene()
                                   : (gameplay_active_ && !menu_state_.visible() ? level_scene_ : nullptr);

    const bool third_person_character_visible = gameplay_active_ &&
                                                !menu_state_.visible() &&
                                                frontend_state_.camera_mode == CameraMode::DebugThirdPerson;
    const bool character_visible = character_view_visible || third_person_character_visible;
    animated_character_.set_motion(
        character_dance_active_ && character_visible
            ? AnimatedCharacter::Motion::Dance
            : AnimatedCharacter::Motion::Idle);
    animated_character_.tick(
        character_visible && character_dance_active_ ? smoothed_delta_ : 0.0F);
    static bool preview_was_visible = false;
    if (character_view_visible && !preview_was_visible) {
        ae::log_info_cat(
            "Character",
            "Character View preview active: ready=" +
                std::to_string(animated_character_.ready() ? 1 : 0) +
                " clip=" + std::string(animated_character_.current_clip()));
    }
    preview_was_visible = character_view_visible;

    // Character View is a standalone 3D presentation pass. Its avatar reads
    // ClientConfig::character directly, which is also the saved profile state.
    if (character_view_visible) {
        render_submission_.scene.debug_world_visible = false;
        render_submission_.scene.director_space_background = false;
        render_submission_.scene.character_presentation_background = true;
        render_submission_.scene.show_player_marker = false;
        render_submission_.scene.show_crosshair = false;
        // Character View is intentionally a full-screen equipment showcase:
        // the neutral operative gets the complete canvas, with no dashboard
        // panels competing for silhouette or future armour customization.
        render_submission_.scene.camera_position = {0.0F, 0.80F, -4.5F};
        render_submission_.scene.camera_target = {0.0F, 0.80F, 0.0F};
        render_submission_.scene.camera_fov_override_deg = 42.0F;
        // Compiled mesh v3 preserves the glTF/Blender bind transforms, so the
        // exported Y-up human base is presented directly. Do not apply the
        // legacy 180-degree correction: it turns the upright operative upside
        // down and pushes the head outside the profile frame.
        const float preview[16] = {
            1.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            1.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            1.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            1.0F,
        };
        std::memcpy(character_preview_model_matrix_, preview, sizeof(preview));
    } else if (third_person_character_visible) {
        // The default gameplay camera is first-person.  The existing V debug
        // camera supplies the safe external view used to validate the player
        // character until a production third-person mode is designed.
        const float yaw = ae::to_radians(curr_snap_.player_state.yaw);
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        const float player_model[16] = {
            cosine,
            0.0F,
            -sine,
            0.0F,
            0.0F,
            1.0F,
            0.0F,
            0.0F,
            sine,
            0.0F,
            cosine,
            0.0F,
            curr_snap_.player_position.x,
            curr_snap_.player_position.y,
            curr_snap_.player_position.z,
            1.0F,
        };
        std::memcpy(character_preview_model_matrix_, player_model, sizeof(player_model));
    }

    bool mcp_visual_validation = false;
#if defined(AHAMKARA_ENABLE_GAME_MCP)
    mcp_visual_validation = game_mcp_bridge_ != nullptr &&
                            !game_mcp_bridge_->first_person_camera();
#endif
    if (mcp_visual_validation && gameplay_active_ &&
        !menu_state_.visible() && !menu_system_.is_visible()) {
        const float yaw = ae::to_radians(curr_snap_.player_state.yaw);
        const float forward_x = std::sin(yaw);
        const float forward_z = std::cos(yaw);
        render_submission_.scene.camera_position = {
            curr_snap_.player_position.x - forward_x * 3.2F,
            curr_snap_.player_position.y + 1.9F,
            curr_snap_.player_position.z - forward_z * 3.2F,
        };
        render_submission_.scene.camera_target = {
            curr_snap_.player_position.x,
            curr_snap_.player_position.y + 0.9F,
            curr_snap_.player_position.z,
        };
        render_submission_.scene.show_crosshair = false;
    }
    // Character View deliberately uses a later, clipped foreground pass so
    // the surrounding menu is in front while its central presentation bay is
    // behind the avatar. Gameplay third-person remains part of the world pass.
    AnimatedCharacter* character = character_visible && !character_view_visible
                                       ? &animated_character_
                                       : nullptr;
    render_local_debug_frame(render_submission_, audio_engine_, shadow_pass_, renderer_,
                             pbr_renderer_, render_scene, character,
                             character != nullptr ? character_preview_model_matrix_ : nullptr);
}

void ClientFramePipeline::stage_render_ui() {
    auto* glfw_win = static_cast<GLFWwindow*>(window_.native_handle());
    ae::ui::sync_input_to_imgu(glfw_win);
    ae::ui::begin_ui_frame();

    // Poll for hot-reloads
    if (menu_initialized_) menu_system_.poll_hot_reload();
    if (hud_loaded_)      hud_system_.poll_hot_reload();

    // ── Authoring: poll file watcher ────────────────────────────────────
    file_watcher_.poll();

    // Update dynamic menu variables
    if (menu_initialized_) {
        menu_system_.set_variable("build_date", __DATE__ " " __TIME__);
        const int fps_int = static_cast<int>(frontend_state_.displayed_metrics.fps);
        menu_system_.set_variable("fps", static_cast<float>(fps_int));
        menu_system_.set_variable("player_health", curr_snap_.player_state.health);
        menu_system_.set_variable("weapon_name", ahamkara::game::weapon_name(curr_snap_.weapon_index));
        menu_system_.set_variable("ammo_current", static_cast<float>(curr_snap_.ammo_current));
        menu_system_.set_variable("ammo_max", static_cast<float>(curr_snap_.ammo_max));
        menu_system_.set_variable("reserve_ammo", static_cast<float>(curr_snap_.reserve_ammo));
    }

    // Render menus (main, pause, character, director, loading)
    if (menu_initialized_ && menu_system_.is_visible()) {
        menu_system_.render();
    }

    // Render gameplay HUD (new JSON-driven system + legacy crosshair fallback)
    if (hud_loaded_ && gameplay_active_ && !menu_system_.is_visible()) {
        ae::ui::HudState hud_state;
        hud_state.health = curr_snap_.player_state.health;
        hud_state.max_health = 100.0f;
        hud_state.shield = curr_snap_.player_state.shield;
        hud_state.max_shield = 100.0f;
        hud_state.ammo_current = static_cast<int>(curr_snap_.ammo_current);
        hud_state.ammo_max = static_cast<int>(curr_snap_.ammo_max);
        hud_state.reserve_ammo = static_cast<int>(curr_snap_.reserve_ammo);
        hud_state.weapon_index = curr_snap_.weapon_index;
        hud_state.weapon_name = ahamkara::game::weapon_name(curr_snap_.weapon_index);
        hud_state.is_reloading = weapon_animation_.reload_phase() != ReloadPhase::Idle;
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

    // ── Authoring: Debug Inspector overlay ──────────────────────────────
    inspector_.render(curr_snap_);

    // ── Authoring: Developer Console overlay ────────────────────────────
    if (console_open_) {
        render_console_overlay();
    }

    ae::ui::end_ui_frame();
    render_character_view_foreground();
}

void ClientFramePipeline::render_character_view_foreground() {
    const bool character_view_visible = menu_initialized_ &&
                                        menu_system_.current_screen() == "character_view";
    if (!character_view_visible || !animated_character_.ready() || renderer_.backend() == nullptr) {
        return;
    }

    auto& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0F || io.DisplaySize.y <= 0.0F) {
        return;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    renderer_.backend()->get_framebuffer_size(framebuffer_width, framebuffer_height);
    if (framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    // The page is a clean character stage, so the post-UI avatar draw owns the
    // entire client area. The scissor remains explicit to keep this pass from
    // escaping the game framebuffer on high-density displays.
    const int scissor_x = 0;
    const int scissor_y = 0;
    const int scissor_width = framebuffer_width;
    const int scissor_height = framebuffer_height;

    glEnable(GL_SCISSOR_TEST);
    glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
    renderer_.backend()->set_depth_test(true);
    renderer_.backend()->set_depth_func_lequal();
    renderer_.backend()->set_depth_write(false);
    animated_character_.render_static(renderer_.view_matrix(), renderer_.projection_matrix(),
                                      character_preview_model_matrix_);
    renderer_.backend()->set_depth_write(true);
    glDisable(GL_SCISSOR_TEST);
}

void ClientFramePipeline::render_console_overlay() {
    // Console window — dark semi-transparent overlay at the top of the screen
    const float console_height = ImGui::GetIO().DisplaySize.y * 0.45F;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, console_height), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##console", &console_open_, flags)) {
        // Log area (scrollable)
        ImGui::BeginChild("##console_log", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        const int line_count = console_.log_line_count();
        for (int i = 0; i < line_count; ++i) {
            const auto& line = console_.log_line(i);
            // Simple color tagging
            switch (line.type) {
                case ae::Console::LogLine::kError:
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", line.text.c_str());
                    break;
                case ae::Console::LogLine::kOutput:
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", line.text.c_str());
                    break;
                default:
                    ImGui::TextUnformatted(line.text.c_str());
                    break;
            }
        }

        // Auto-scroll
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() * 0.9f) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // Input line — buffer persists across frames until submission or explicit clear
        ImGui::PushItemWidth(-1.0f);
        if (ImGui::InputText("##console_input", console_input_buffer_, kConsoleBufSize,
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string input(console_input_buffer_);
            if (!input.empty()) {
                console_.execute(input);
                console_input_buffer_[0] = '\0';
                console_history_pos_ = -1;
            }
        }
        ImGui::PopItemWidth();

        // Keep focus on the console input
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
    ImGui::End();
}

void ClientFramePipeline::stage_present() {
#if defined(AHAMKARA_ENABLE_GAME_MCP)
    if (game_mcp_bridge_ && game_mcp_bridge_->capture_requested()) {
        auto* glfw_window = static_cast<GLFWwindow*>(window_.native_handle());
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(glfw_window, &width, &height);
        if (width > 0 && height > 0) {
            std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) *
                                           static_cast<std::size_t>(height) * 4U);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            game_mcp_bridge_->write_frame_ppm(width, height, rgba);
        }
    }
#endif
    renderer_.present();
}

void ClientFramePipeline::stage_post_frame() {
    // Tick the host application (which ticks the game module if one is set).
    // This is called once per frame after the simulation and scene have been
    // updated but before the frame ends.  Products that inject their own game
    // module (e.g. Flashback) get their lifecycle tick called here.
    (void)application_.tick(smoothed_delta_);

    if (ui_actions_.config_applied) {
        if (auto* window_input = dynamic_cast<WindowInputProvider*>(&input_provider_)) {
            window_input->set_mouse_sensitivity(client_config_.mouse_sensitivity);
        }
        audio_engine_.set_master_volume(client_config_.audio.master_volume);
    }
    if (ui_actions_.quit_application) {
        application_.shutdown();
    }
    if (ui_actions_.restart_match) {
        simulation_.restart_match();
    }

    simulation_.set_paused(menu_state_.simulation_should_pause());

    if (input_provider_.finished()) {
        application_.shutdown();
    }
}

}  // namespace ahamkara::client
