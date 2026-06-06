#include "ae/core/log.h"
#include "ae/core/math.h"
#include "ae/core/time.h"
#include "ae/network/udp_socket.h"
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
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

constexpr float kTickRate = 60.0F;
constexpr float kDeltaSeconds = 1.0F / kTickRate;
constexpr const char* kClientConfigRelativePath = "client/config/ahamkara.cfg";
constexpr ae::KeyCode kPerspectiveToggleKey = ae::KeyCode::V;
constexpr float kFirstPersonTargetDistance = 12.0F;
constexpr float kDebugFollowDistance = 4.5F;
constexpr float kDebugFollowLift = 1.8F;
constexpr float kDebugFollowShoulderOffset = 0.75F;
constexpr float kDebugFollowLeadDistance = 1.5F;
constexpr const char* kControllerBindingsRelativePath = "client/config/controller_bindings.cfg";

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

std::optional<std::filesystem::path> current_executable_path(const char* argv0) {
#if defined(__APPLE__)
    uint32_t buffer_size = 0;
    _NSGetExecutablePath(nullptr, &buffer_size);
    if (buffer_size > 0) {
        std::vector<char> buffer(buffer_size);
        if (_NSGetExecutablePath(buffer.data(), &buffer_size) == 0) {
            return std::filesystem::weakly_canonical(buffer.data());
        }
    }
#elif defined(__linux__)
    char buffer[PATH_MAX] {};
    const ssize_t bytes_written = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (bytes_written > 0) {
        buffer[bytes_written] = '\0';
        return std::filesystem::weakly_canonical(buffer);
    }
#endif

    if (argv0 != nullptr && argv0[0] != '\0') {
        return std::filesystem::absolute(std::filesystem::path(argv0));
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> find_client_config_path(const char* executable_path) {
    const auto cwd_candidate = std::filesystem::path(kClientConfigRelativePath);
    if (std::filesystem::exists(cwd_candidate)) {
        return cwd_candidate;
    }

    if (const auto resolved_executable_path = current_executable_path(executable_path);
        resolved_executable_path.has_value()) {
        const auto executable_dir = resolved_executable_path->parent_path();
        const auto repo_relative_candidate =
            executable_dir / ".." / ".." / ".." / kClientConfigRelativePath;
        if (std::filesystem::exists(repo_relative_candidate)) {
            return repo_relative_candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> find_controller_bindings_path(const char* executable_path) {
    const auto cwd_candidate = std::filesystem::path(kControllerBindingsRelativePath);
    if (std::filesystem::exists(cwd_candidate)) {
        return cwd_candidate;
    }

    if (const auto resolved_executable_path = current_executable_path(executable_path);
        resolved_executable_path.has_value()) {
        const auto executable_dir = resolved_executable_path->parent_path();
        const auto repo_relative_candidate =
            executable_dir / ".." / ".." / ".." / kControllerBindingsRelativePath;
        if (std::filesystem::exists(repo_relative_candidate)) {
            return repo_relative_candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

void print_sandbox_help() {
    std::cout << "Sandbox commands:\n"
              << "  help                     Show this help\n"
              << "  status                   Print current player state\n"
              << "  step <ticks> <inputs>    Simulate ticks with inputs\n"
              << "                           Inputs: w a s d sprint jump slide crouch fire reload ability\n"
              << "                           Example: step 60 w sprint\n"
              << "  quit                     Exit sandbox\n";
}

std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        for (char& character : token) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        tokens.push_back(token);
    }

    return tokens;
}

std::optional<int> parse_tick_count(const std::string& token) {
    try {
        const int ticks = std::stoi(token);
        if (ticks <= 0) {
            return std::nullopt;
        }

        return ticks;
    } catch (...) {
        return std::nullopt;
    }
}

bool apply_input_token(const std::string& token, ahamkara::game::PlayerInputCommand& command) {
    if (token == "w") {
        command.move_axis.y += 1.0F;
        return true;
    }

    if (token == "s") {
        command.move_axis.y -= 1.0F;
        return true;
    }

    if (token == "a") {
        command.move_axis.x -= 1.0F;
        return true;
    }

    if (token == "d") {
        command.move_axis.x += 1.0F;
        return true;
    }

    if (token == "sprint") {
        command.sprint_held = true;
        return true;
    }

    if (token == "jump") {
        command.jump_pressed = true;
        return true;
    }

    if (token == "slide") {
        command.slide_pressed = true;
        return true;
    }

    if (token == "crouch") {
        command.crouch_held = true;
        return true;
    }

    if (token == "fire") {
        command.fire_held = true;
        return true;
    }

    if (token == "reload") {
        command.reload_pressed = true;
        return true;
    }

    if (token == "ability") {
        command.ability_pressed = true;
        return true;
    }

    return false;
}

void print_player_state(const ahamkara::game::ReplicatedPlayerState& player_state) {
    std::ostringstream message;
    message << "Player position=("
            << player_state.position.x << ", "
            << player_state.position.y << ", "
            << player_state.position.z << ") velocity=("
            << player_state.velocity.x << ", "
            << player_state.velocity.y << ", "
            << player_state.velocity.z << ") state="
            << static_cast<int>(player_state.movement_state);
    ae::log_info(message.str());
}

int run_sandbox_client() {
    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ahamkara::game::World world;
    ae::u32 sequence = 0;
    ae::u32 client_tick = 0;

    ae::log_info("Ahamkara local sandbox started.");
    print_sandbox_help();
    print_player_state(world.get_player_state());

    std::string line;
    while (application.is_running()) {
        std::cout << "sandbox> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }

        const std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) {
            continue;
        }

        if (tokens[0] == "quit" || tokens[0] == "exit") {
            break;
        }

        if (tokens[0] == "help") {
            print_sandbox_help();
            continue;
        }

        if (tokens[0] == "status") {
            print_player_state(world.get_player_state());
            continue;
        }

        if (tokens[0] != "step") {
            ae::log_warning("Unknown sandbox command. Use 'help' for usage.");
            continue;
        }

        if (tokens.size() < 2) {
            ae::log_warning("Sandbox step requires a positive tick count.");
            continue;
        }

        const std::optional<int> tick_count = parse_tick_count(tokens[1]);
        if (!tick_count.has_value()) {
            ae::log_warning("Sandbox step tick count must be a positive integer.");
            continue;
        }

        ahamkara::game::PlayerInputCommand command {};
        command.sequence = sequence++;
        command.client_tick = client_tick;
        command.client_time = static_cast<float>(ae::now_seconds());

        bool invalid_token = false;
        for (ae::usize index = 2; index < tokens.size(); ++index) {
            if (!apply_input_token(tokens[index], command)) {
                ae::log_warning("Sandbox step received an unknown input token.");
                invalid_token = true;
                break;
            }
        }

        if (invalid_token) {
            continue;
        }

        for (int tick = 0; tick < *tick_count; ++tick) {
            command.client_tick = client_tick++;
            world.tick(kDeltaSeconds, command);
        }

        print_player_state(world.get_player_state());
    }

    application.shutdown();
    return EXIT_SUCCESS;
}

int run_network_client(const std::string& server_ip) {
    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ae::UdpSocket socket;
    if (!socket.open(0)) {
        ae::log_error("Client failed to open an ephemeral UDP port.");
        return 1;
    }

    const ae::NetAddress server_address {server_ip, 7777};

    {
        std::ostringstream startup_message;
        startup_message << "Client sending input to " << server_address.ip << ":" << server_address.port << ".";
        ae::log_info(startup_message.str());
    }

    const auto tick_duration = std::chrono::duration<double>(kDeltaSeconds);
    auto next_tick = std::chrono::steady_clock::now();

    ae::u32 input_sequence = 0;
    ae::u32 client_tick = 0;
    ahamkara::game::PlayerInputPacketBuffer input_buffer {};
    ahamkara::game::ServerSnapshotPacketBuffer snapshot_buffer {};

    while (application.is_running()) {
        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);

        ahamkara::game::PlayerInputCommand input_command {};
        input_command.sequence = input_sequence++;
        input_command.client_tick = client_tick++;
        input_command.client_time = static_cast<float>(ae::now_seconds());
        input_command.move_axis.y = 1.0F;
        input_command.sprint_held = true;

        if (!ahamkara::game::serialize_player_input_packet(input_command, input_buffer)
            || !socket.send_to(server_address, input_buffer.data(), input_buffer.size())) {
            ae::log_warning("Client failed to send input command.");
        }

        while (true) {
            ae::NetAddress from {};
            const ae::i32 received = socket.receive_from(from, snapshot_buffer.data(), snapshot_buffer.size());
            if (received <= 0) {
                break;
            }

            if (received != static_cast<ae::i32>(snapshot_buffer.size())) {
                ae::log_warning("Client received an unexpected snapshot size.");
                continue;
            }

            ahamkara::game::ServerSnapshot snapshot {};
            if (!ahamkara::game::deserialize_server_snapshot_packet(snapshot_buffer, snapshot)) {
                ae::log_warning("Client rejected an invalid snapshot packet.");
                continue;
            }

            std::ostringstream snapshot_message;
            snapshot_message << "Snapshot tick=" << snapshot.server_tick
                             << " position=("
                             << snapshot.local_player.position.x << ", "
                             << snapshot.local_player.position.y << ", "
                             << snapshot.local_player.position.z << ")";
            ae::log_info(snapshot_message.str());
        }

        std::this_thread::sleep_until(next_tick);
    }

    application.shutdown();
    return EXIT_SUCCESS;
}

}  // namespace

const char* key_code_name(ae::KeyCode key) {
    switch (key) {
        case ae::KeyCode::W:           return "W";
        case ae::KeyCode::A:           return "A";
        case ae::KeyCode::S:           return "S";
        case ae::KeyCode::D:           return "D";
        case ae::KeyCode::LeftShift:   return "LShift";
        case ae::KeyCode::RightShift:  return "RShift";
        case ae::KeyCode::Space:       return "Space";
        case ae::KeyCode::LeftControl:  return "LCtrl";
        case ae::KeyCode::RightControl: return "RCtrl";
        case ae::KeyCode::Escape:      return "Escape";
        default:                       return nullptr;
    }
}

int run_windowed_client(const ahamkara::client::ClientConfig& client_config) {
    ae::WindowConfig window_config {};
    window_config.title = "Ahamkara";
    window_config.width = client_config.window_width;
    window_config.height = client_config.window_height;
    window_config.fullscreen = client_config.fullscreen;

    std::unique_ptr<ae::PlatformWindow> window;
    try {
        window = ae::PlatformWindow::create(window_config);
    } catch (const std::exception& ex) {
        ae::log_error(ex.what());
        return 1;
    }

    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ahamkara::game::World world;
    auto previous_frame = std::chrono::steady_clock::now();

    std::ostringstream startup_msg;
    startup_msg << "Windowed client running " << window_config.width << "x" << window_config.height
                << " | mouse_sensitivity=" << client_config.mouse_sensitivity
                << " — press Escape to quit.";
    ae::log_info(startup_msg.str());

    while (application.is_running() && window->poll_events()) {
        if (window->is_key_pressed(ae::KeyCode::Escape)) {
            ae::log_info("Escape pressed, shutting down.");
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        float delta_seconds = std::chrono::duration<float>(now - previous_frame).count();
        previous_frame = now;
        if (delta_seconds > 0.05F) {
            delta_seconds = 0.05F;
        }

        ahamkara::game::PlayerInputCommand cmd {};
        cmd.client_time = static_cast<float>(ae::now_seconds());

        if (window->is_key_down(ae::KeyCode::W)) cmd.move_axis.y += 1.0F;
        if (window->is_key_down(ae::KeyCode::S)) cmd.move_axis.y -= 1.0F;
        if (window->is_key_down(ae::KeyCode::A)) cmd.move_axis.x -= 1.0F;
        if (window->is_key_down(ae::KeyCode::D)) cmd.move_axis.x += 1.0F;

        const bool sprint = window->is_key_down(ae::KeyCode::LeftShift)
                         || window->is_key_down(ae::KeyCode::RightShift);
        cmd.sprint_held  = sprint;
        cmd.jump_pressed = window->is_key_pressed(ae::KeyCode::Space);
        cmd.crouch_held  = window->is_key_down(ae::KeyCode::LeftControl)
                        || window->is_key_down(ae::KeyCode::RightControl);

        const auto mouse = window->mouse_state();
        cmd.look_delta.x = mouse.delta_x * client_config.mouse_sensitivity;
        cmd.look_delta.y = mouse.delta_y * client_config.mouse_sensitivity;

        world.tick(delta_seconds, cmd);

        std::ostringstream status;
        const auto& player = world.get_player_state();
        status << "[World] pos=(" << player.position.x << ", " << player.position.y << ", " << player.position.z << ")"
               << " | [Input] move=(" << cmd.move_axis.x << ", " << cmd.move_axis.y << ")"
               << " look=(" << cmd.look_delta.x << ", " << cmd.look_delta.y << ")"
               << " sprint=" << cmd.sprint_held;
        
        ae::log_info(status.str());
    }
    application.shutdown();
    return EXIT_SUCCESS;
}

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
        "X slide, Y reload, RB ability, L3+R3 toggle perspective, Back metrics, Start exit.");

    window->set_title(build_debug_window_title(window_config.title, view_state.camera_mode, metrics_visible, displayed_metrics));

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
                build_debug_scene(simulation, view_state.camera_mode, metrics_visible, always_day, menu_visible, menu_tab, displayed_metrics);
            // Controller button display (pack gamepad state)
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

        // EMA-smooth the delta time to reduce physics jitter from frame spikes.
        // Uses a ~10-frame window (alpha = 0.18) for responsive yet stable timing.
        if (smoothed_delta <= 0.0F) {
            smoothed_delta = raw_delta;
        } else {
            smoothed_delta += 0.18F * (raw_delta - smoothed_delta);
        }
        float delta_seconds = smoothed_delta;

        simulation.tick(delta_seconds);
        const bool compute_percentiles = (metrics_update_accumulator >= 1.0 || displayed_metrics.fps <= 0.0);
        const ae::RuntimeMetricsSnapshot sampled_metrics = metrics_collector.sample(delta_seconds, compute_percentiles);
        metrics_update_accumulator += delta_seconds;
        if (metrics_update_accumulator >= 1.0 || displayed_metrics.fps <= 0.0) {
            displayed_metrics = sampled_metrics;
            metrics_update_accumulator = 0.0;
        }

        const ae::render::DebugScene scene =
            build_debug_scene(simulation, view_state.camera_mode, metrics_visible, always_day, menu_visible, menu_tab, displayed_metrics);
        // Controller button display (pack gamepad state)
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

int main(int argc, char** argv) {
    ahamkara::client::ClientConfig client_config {};
    ahamkara::client::ControllerBindings controller_bindings {};
    if (const auto config_path = find_client_config_path(argc > 0 ? argv[0] : nullptr); config_path.has_value()) {
        (void)client_config.load_from_file(config_path->string());
    } else {
        ae::log_info("No client config file found, using defaults.");
    }
    if (const auto bindings_path = find_controller_bindings_path(argc > 0 ? argv[0] : nullptr); bindings_path.has_value()) {
        (void)controller_bindings.load_from_file(bindings_path->string());
    } else {
        ae::log_info("No controller bindings file found, using defaults.");
    }

    if (argc > 1 && (std::string(argv[1]) == "--local" || std::string(argv[1]) == "--debug-view")) {
        return run_local_client(client_config, controller_bindings);
    }

    if (argc > 1 && std::string(argv[1]) == "--window") {
        return run_windowed_client(client_config);
    }

    if (argc > 1 && std::string(argv[1]) == "--sandbox") {
        return run_sandbox_client();
    }

    const std::string server_ip = argc > 1 ? argv[1] : client_config.server_ip;
    return run_network_client(server_ip);
}
