#include "ae/core/cli_utils.h"
#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/network/cli_helpers.h"
#include "ae/network/network_clock.h"
#include "ae/network/network_simulator.h"
#include "ae/network/snapshot_interpolator.h"
#include "ae/network/udp_socket.h"
#include "ae/platform/window.h"
#include "ae/runtime/application.h"
#include "ahamkara/client/client_config.h"
#include "ahamkara/game/client_prediction.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr float kTickRate = 60.0F;
constexpr float kDeltaSeconds = 1.0F / kTickRate;

// ── Tick-loop helpers ────────────────────────────────────────────────────────

/**
 * @brief Compute elapsed seconds since the previous frame and update the
 *        previous-frame timestamp in-place.
 */
inline float compute_frame_dt(std::chrono::steady_clock::time_point& previous) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - previous).count();
    previous = now;
    return dt;
}

// ── Sandbox helpers ──────────────────────────────────────────────────────────

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

// ── Keyboard key-name helper ─────────────────────────────────────────────────

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

/**
 * @brief Diagnostic log comparing the three state representations.
 *
 * State ownership:
 * - `interpolated`: Smoothed render state from SnapshotInterpolator
 *   (derived from authoritative server snapshots).
 * - `authoritative`: Raw newest server snapshot — the ground truth
 *   the server published at `snap_tick`.
 * - `predicted`:   Client's locally predicted state from
 *   ClientPredictionManager, ahead of the most recent authoritative.
 */
void log_state_comparison(
    const ahamkara::game::ReplicatedPlayerState& interpolated,
    const ahamkara::game::ReplicatedPlayerState& authoritative,
    ae::u32 snap_tick,
    const ahamkara::game::ReplicatedPlayerState* predicted,
    float interp_delay,
    ae::u32 tick)
{
    if (tick % 60 != 0) return;  // Log every 60 ticks (1 second).

    std::ostringstream msg;
    msg << "[Client] tick=" << tick
        << " interp_delay=" << interp_delay << "s"
        << " | interp_pos=(" << interpolated.position.x << ", " << interpolated.position.z << ")"
        << " | auth_pos=(" << authoritative.position.x << ", "
                           << authoritative.position.z << ")"
        << " | auth_tick=" << snap_tick;

    if (predicted) {
        msg << " | pred_pos=(" << predicted->position.x << ", "
            << predicted->position.z << ")";
    }

    ae::log_info(msg.str());
}

}  // namespace

// ── Client entry points ──────────────────────────────────────────────────────

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

int run_network_client(const std::string& server_ip, int argc, char** argv) {
    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ae::UdpSocket socket;
    if (!socket.open(0)) {
        ae::log_error("Client failed to open an ephemeral UDP port.");
        return 1;
    }

    const ae::NetAddress server_address {server_ip, 7777};

    // ── Simulator config from CLI ─────────────────────────────────────────
    ae::SimulatorConfig sim_config = ae::build_sim_config(argc, argv);
    ae::NetworkSimulator sim(socket);
    sim.configure(sim_config);

    {
        std::ostringstream startup_message;
        startup_message << "Client sending input to " << server_address.ip << ":" << server_address.port << ".";
        ae::log_info(startup_message.str());
    }

    // ── Netcode pipeline: three state layers ────────────────────────────
    //
    // Layer 1 — PREDICTED:  ClientPredictionManager owns a local World that
    //                        applies inputs immediately for responsiveness.
    //                        Represents the client's best guess of where
    //                        the player *will be* once the server confirms.
    //
    // Layer 2 — AUTHORITATIVE:  ServerSnapshot::local_player is the ground
    //                           truth from the dedicated server.  The
    //                           interpolator buffers these snapshots.
    //
    // Layer 3 — INTERPOLATED:  `SnapshotInterpolator` lerps between two
    //                          bracketing authoritative snapshots to produce
    //                          a smooth render state at `now - delay`.
    //                          This is what the player *sees*.
    //
    // Reconciliation: When predicted drifts too far from authoritative,
    // the prediction world is reset to authoritative and pending inputs
    // are replayed.  See client_prediction.cpp.
    ae::NetworkClock clock;
    ae::SnapshotInterpolator<ahamkara::game::ServerSnapshot, 3> interpolator;
    ahamkara::game::ClientPredictionManager prediction;

    const auto tick_duration = std::chrono::duration<double>(kDeltaSeconds);
    auto next_tick = std::chrono::steady_clock::now();

    ae::u32 input_sequence = 0;
    ae::u32 client_tick = 0;
    bool connected = false;
    ahamkara::game::PlayerInputPacketBuffer input_buffer {};
    ahamkara::game::ServerSnapshotPacketBuffer snapshot_buffer {};
    ahamkara::game::ClientHelloPacketBuffer hello_buffer {};
    ahamkara::game::PacketEnvelope envelope {};
    ahamkara::game::ClientHelloPacket hello_packet {};
    hello_packet.protocol_version = ahamkara::game::kProtocolVersion;
    hello_packet.session_token = 0;

    auto previous_frame = std::chrono::steady_clock::now();
    float interpolation_delay = 1.0F / kTickRate;  // Starts at 1 tick; tuned by interpolator.

    while (application.is_running()) {
        const float frame_dt = compute_frame_dt(previous_frame);

        // Process simulator delayed packets.
        sim.update(frame_dt);

        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);

        if (!connected) {
            envelope.sequence++;
            if (!ahamkara::game::serialize_client_hello_packet(envelope, hello_packet, hello_buffer)
                || !sim.send_to(server_address, hello_buffer.data(), hello_buffer.size())) {
                ae::log_warning("Client failed to send handshake hello.");
            }

            while (true) {
                ae::NetAddress from {};
                const ae::i32 received = sim.receive_from(from, snapshot_buffer.data(), snapshot_buffer.size());
                if (received <= 0) {
                    break;
                }

                const auto packet_span = std::span<const std::byte>(
                    snapshot_buffer.data(), static_cast<ae::usize>(received));

                if (received == static_cast<ae::i32>(ahamkara::game::server_welcome_packet_size())) {
                    ahamkara::game::ServerWelcomePacket welcome {};
                    ahamkara::game::PacketEnvelope in_envelope {};
                    if (!ahamkara::game::deserialize_server_welcome_packet(packet_span, in_envelope, welcome)) {
                        ae::log_warning("Client rejected an invalid handshake welcome.");
                        continue;
                    }

                    if (!ahamkara::game::is_supported_protocol_version(welcome.protocol_version)) {
                        ae::log_warning("Client received a welcome with an unsupported protocol version.");
                        return EXIT_FAILURE;
                    }

                    connected = true;
                    break;
                }

                if (received == static_cast<ae::i32>(ahamkara::game::server_reject_packet_size())) {
                    ahamkara::game::ServerRejectPacket reject {};
                    ahamkara::game::PacketEnvelope in_envelope {};
                    if (!ahamkara::game::deserialize_server_reject_packet(packet_span, in_envelope, reject)) {
                        ae::log_warning("Client rejected an invalid handshake reject.");
                        continue;
                    }

                    if (reject.reason == ahamkara::game::HandshakeRejectReason::VersionMismatch) {
                        ae::log_warning("Client handshake rejected because the protocol version does not match.");
                    } else {
                        ae::log_warning("Client handshake rejected by the server.");
                    }
                    return EXIT_FAILURE;
                }

                ae::log_warning("Client received an unexpected handshake packet size.");
            }

            std::this_thread::sleep_until(next_tick);
            continue;
        }

        // ── PREDICTED LAYER: Build, send, and locally apply input ────
        // The input is sent to the server for authoritative simulation,
        // and also applied locally for responsive client-side prediction.
        ahamkara::game::PlayerInputCommand input_command {};
        input_command.sequence = input_sequence++;
        input_command.client_tick = client_tick++;
        input_command.client_time = static_cast<float>(ae::now_seconds());
        input_command.move_axis.y = 1.0F;
        input_command.sprint_held = true;

        // Client-side prediction: apply input to local world immediately.
        prediction.apply_input(input_command, kDeltaSeconds);

        envelope.sequence++;
        if (!ahamkara::game::serialize_player_input_packet(envelope, input_command, input_buffer)
            || !sim.send_to(server_address, input_buffer.data(), input_buffer.size())) {
            ae::log_warning("Client failed to send input command.");
        }

        // ── AUTHORITATIVE LAYER: Receive server snapshots ────────────
        // Snapshot::local_player is the ground truth from the server.
        // Each snapshot feeds the interpolator (for smooth rendering)
        // and the prediction manager (for reconciliation).
        while (true) {
            ae::NetAddress from {};
            const ae::i32 received = sim.receive_from(from, snapshot_buffer.data(), snapshot_buffer.size());
            if (received <= 0) {
                break;
            }

            if (received != static_cast<ae::i32>(snapshot_buffer.size())) {
                ae::log_warning("Client received an unexpected snapshot size.");
                continue;
            }

            ahamkara::game::PacketEnvelope in_envelope {};
            ahamkara::game::ServerSnapshot snapshot {};
            if (!ahamkara::game::deserialize_server_snapshot_packet(snapshot_buffer, in_envelope, snapshot)) {
                ae::log_warning("Client rejected an invalid snapshot packet.");
                continue;
            }

            const double arrival_time = ae::now_seconds();

            // Feed clock synchronization.
            clock.record_snapshot(snapshot.server_tick, kTickRate, arrival_time);

            // Push into interpolator.
            interpolator.push(snapshot, arrival_time);

            // Update interpolation delay from jitter measurement.
            interpolation_delay = interpolator.suggest_delay_seconds(kTickRate);

            // ── Reconciliation boundary ────────────────────────────
            // If the predicted state has diverged from authoritative,
            // reset to authoritative and replay unacknowledged inputs.
            prediction.reconcile(snapshot);
        }

        // ── INTERPOLATED LAYER: Smooth render state ──────────────────
        // Lerp between two bracketing server snapshots at render_time
        // (now minus jitter-aware interpolation delay).  This produces
        // the smoothest visual result by hiding network jitter.
        const double render_time = ae::now_seconds() - interpolation_delay;
        ahamkara::game::ReplicatedPlayerState interpolated_player {};
        if (interpolator.interpolate(render_time, interpolated_player)) {
            // Get the authoritative state from the newest snapshot
            // for diagnostic comparison.
            ahamkara::game::ServerSnapshot older_snap {}, newer_snap {};
            (void)interpolator.get_bracketing_snapshots(render_time, &older_snap, &newer_snap);

            const auto& pred_state = prediction.world().get_player_state();
            log_state_comparison(interpolated_player, newer_snap.local_player,
                                 newer_snap.server_tick, &pred_state,
                                 interpolation_delay, client_tick);
        }

        std::this_thread::sleep_until(next_tick);
    }

    application.shutdown();
    return EXIT_SUCCESS;
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

        float delta_seconds = compute_frame_dt(previous_frame);
        if (delta_seconds > 0.05F) {
            delta_seconds = 0.05F;   // Clamp to avoid huge steps after pause.
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
