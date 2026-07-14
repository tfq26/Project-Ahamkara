#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/network/cli_helpers.h"
#include "ae/network/network_simulator.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/udp_socket.h"
#include "ae/runtime/application.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr int kMaxHandshakeTicks = 60;   // ~1 s at 60 Hz

struct ClientOptions {
    std::string server_ip {"127.0.0.1"};
    ae::u16 server_port {7777};
    std::string client_name {"bot"};
    int client_count {1};
    double duration_seconds {5.0};
    double tick_rate_hz {60.0};
    float move_x {0.0F};
    float move_y {1.0F};
    float look_x {0.0F};
    float look_y {0.0F};
    bool fire_held {false};
    bool print_events {true};
    bool print_snapshots {true};
    std::string nakama_token;
};

struct ClientSlot {
    std::string label;
    ae::UdpSocket socket;
    std::unique_ptr<ae::NetworkSimulator> simulator;
    ae::SequenceTracker sequence_tracker;
    ae::u32 next_client_tick {0};
    bool handshake_done {false};
    int handshake_ticks {0};
    bool connected_banner_printed {false};
    std::string nakama_token;
};

bool is_flag(std::string_view arg, std::string_view flag) {
    return arg == flag;
}

bool is_prefixed(std::string_view arg, std::string_view prefix) {
    return arg.starts_with(prefix);
}

std::optional<int> parse_int_value(std::string_view value) {
    try {
        std::size_t consumed = 0;
        const int parsed = std::stoi(std::string(value), &consumed);
        if (consumed != value.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> parse_double_value(std::string_view value) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(std::string(value), &consumed);
        if (consumed != value.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parse_float_value(std::string_view value) {
    const auto parsed = parse_double_value(value);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return static_cast<float>(*parsed);
}

void emit_line(const std::string& line) {
    std::cout << line << '\n' << std::flush;
}

std::string format_vec2(float x, float y) {
    std::ostringstream out;
    out << '(' << x << ',' << y << ')';
    return out.str();
}

std::string format_vec3(const ahamkara::game::Vec3& value) {
    std::ostringstream out;
    out << '(' << value.x << ',' << value.y << ',' << value.z << ')';
    return out.str();
}

std::string reject_reason_str(ahamkara::game::HandshakeRejectReason reason) {
    switch (reason) {
    case ahamkara::game::HandshakeRejectReason::VersionMismatch: return "version_mismatch";
    case ahamkara::game::HandshakeRejectReason::ServerBusy:     return "server_busy";
    default: return "unknown";
    }
}

std::string build_label(const ClientOptions& options, int index) {
    if (options.client_count == 1) {
        return options.client_name;
    }

    std::ostringstream out;
    out << options.client_name << '-' << (index + 1);
    return out.str();
}

void print_usage() {
    std::cout
        << "wish_test_client --server=127.0.0.1 --port=7777 [options]\n"
        << "Options:\n"
        << "  --name=<label>          Base client label (default: bot)\n"
        << "  --count=<n>             Spawn n clients in one process (default: 1)\n"
        << "  --duration=<seconds>    Run time before exit (default: 5)\n"
        << "  --rate=<hz>             Input tick rate (default: 60)\n"
        << "  --move-x=<float>        Horizontal move axis (default: 0)\n"
        << "  --move-y=<float>        Vertical move axis (default: 1)\n"
        << "  --look-x=<float>        Look input X delta (default: 0)\n"
        << "  --look-y=<float>        Look input Y delta (default: 0)\n"
        << "  --fire                  Hold fire input on every tick\n"
        << "  --nakama-token=<jwt>    Nakama session token for auth\n"
        << "  --no-events             Suppress input/event logs\n"
        << "  --no-snapshots          Suppress snapshot logs\n"
        << "  --simulate*             Forwarded to the network simulator\n"
        << "Examples:\n"
        << "  ./build/tools/wish_test_client --server=127.0.0.1 --duration=8\n"
        << "  ./build/tools/wish_test_client --server=127.0.0.1 --count=2 --move-y=1 --fire\n";
}

bool parse_options(int argc, char** argv, ClientOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg {argv[i]};

        if (is_flag(arg, "--help") || is_flag(arg, "-h")) {
            print_usage();
            return false;
        }

        if (is_prefixed(arg, "--server=")) {
            options.server_ip = std::string(arg.substr(std::string_view("--server=").size()));
            continue;
        }

        if (is_prefixed(arg, "--port=")) {
            const auto parsed = parse_int_value(arg.substr(std::string_view("--port=").size()));
            if (!parsed.has_value() || *parsed < 1 || *parsed > std::numeric_limits<ae::u16>::max()) {
                ae::log_error("Invalid --port value.");
                return false;
            }
            options.server_port = static_cast<ae::u16>(*parsed);
            continue;
        }

        if (is_prefixed(arg, "--name=")) {
            options.client_name = std::string(arg.substr(std::string_view("--name=").size()));
            continue;
        }

        if (is_prefixed(arg, "--count=")) {
            const auto parsed = parse_int_value(arg.substr(std::string_view("--count=").size()));
            if (!parsed.has_value() || *parsed < 1) {
                ae::log_error("Invalid --count value.");
                return false;
            }
            options.client_count = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--duration=")) {
            const auto parsed = parse_double_value(arg.substr(std::string_view("--duration=").size()));
            if (!parsed.has_value()) {
                ae::log_error("Invalid --duration value.");
                return false;
            }
            options.duration_seconds = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--rate=")) {
            const auto parsed = parse_double_value(arg.substr(std::string_view("--rate=").size()));
            if (!parsed.has_value() || *parsed <= 0.0) {
                ae::log_error("Invalid --rate value.");
                return false;
            }
            options.tick_rate_hz = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--move-x=")) {
            const auto parsed = parse_float_value(arg.substr(std::string_view("--move-x=").size()));
            if (!parsed.has_value()) {
                ae::log_error("Invalid --move-x value.");
                return false;
            }
            options.move_x = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--move-y=")) {
            const auto parsed = parse_float_value(arg.substr(std::string_view("--move-y=").size()));
            if (!parsed.has_value()) {
                ae::log_error("Invalid --move-y value.");
                return false;
            }
            options.move_y = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--look-x=")) {
            const auto parsed = parse_float_value(arg.substr(std::string_view("--look-x=").size()));
            if (!parsed.has_value()) {
                ae::log_error("Invalid --look-x value.");
                return false;
            }
            options.look_x = *parsed;
            continue;
        }

        if (is_prefixed(arg, "--look-y=")) {
            const auto parsed = parse_float_value(arg.substr(std::string_view("--look-y=").size()));
            if (!parsed.has_value()) {
                ae::log_error("Invalid --look-y value.");
                return false;
            }
            options.look_y = *parsed;
            continue;
        }

        if (is_flag(arg, "--fire")) {
            options.fire_held = true;
            continue;
        }

        if (is_prefixed(arg, "--nakama-token=")) {
            options.nakama_token = std::string(arg.substr(std::string_view("--nakama-token=").size()));
            continue;
        }

        if (is_flag(arg, "--no-events")) {
            options.print_events = false;
            continue;
        }

        if (is_flag(arg, "--no-snapshots")) {
            options.print_snapshots = false;
            continue;
        }

        if (is_prefixed(arg, "--simulate")) {
            continue;
        }

        ae::log_error(std::string("Unknown argument: ") + std::string(arg));
        return false;
    }

    return true;
}

std::unique_ptr<ClientSlot> make_client_slot(const ClientOptions& options, int index, const ae::SimulatorConfig& sim_config) {
    auto slot = std::make_unique<ClientSlot>();
    slot->label = build_label(options, index);
    slot->nakama_token = options.nakama_token;

    if (!slot->socket.open(0)) {
        throw std::runtime_error("failed to open an ephemeral UDP socket");
    }

    slot->simulator = std::make_unique<ae::NetworkSimulator>(slot->socket);
    slot->simulator->configure(sim_config);
    return slot;
}

void log_start_banner(const ClientOptions& options, const ClientSlot& slot) {
    std::ostringstream out;
    out << "client=" << slot.label
        << " event=connect"
        << " server=" << options.server_ip << ':' << options.server_port
        << " rate=" << options.tick_rate_hz
        << " duration=" << options.duration_seconds
        << " count=" << options.client_count;
    emit_line(out.str());
}

// ── Handshake phase ─────────────────────────────────────────────────────

bool send_hello(ClientSlot& slot, const ae::NetAddress& server_address) {
    ahamkara::game::ClientHelloPacket hello {};
    hello.protocol_version = ahamkara::game::kProtocolVersion;

    if (!slot.nakama_token.empty() && slot.nakama_token.size() <= ahamkara::game::kMaxAuthTokenLength) {
        hello.auth_token_length = static_cast<ae::u16>(slot.nakama_token.size());
        std::memcpy(hello.auth_token, slot.nakama_token.data(), slot.nakama_token.size());
    }

    const ahamkara::game::PacketEnvelope envelope = slot.sequence_tracker.prepare_outgoing();
    ahamkara::game::ClientHelloPacketBuffer buffer {};
    if (!ahamkara::game::serialize_client_hello_packet(envelope, hello, buffer)) {
        ae::log_error("Failed to serialize client hello packet.");
        return false;
    }

    if (!slot.simulator->send_to(server_address, buffer.data(), buffer.size())) {
        ae::log_error("Failed to send client hello packet.");
        return false;
    }

    // Print the hello event only on the first attempt
    if (slot.handshake_ticks == 0) {
        std::ostringstream out;
        out << "client=" << slot.label
            << " event=hello"
            << " proto=" << hello.protocol_version;
        emit_line(out.str());
    }

    ++slot.handshake_ticks;
    return true;
}

/// Drain handshake responses (ServerWelcome / ServerReject) for one client.
/// Returns: true if handshake completed or still in progress,
///          false if the handshake was rejected (client should exit).
bool drain_handshake(ClientSlot& slot) {
    // Use the snapshot buffer as the receive window — it's the largest packet.
    ahamkara::game::ServerSnapshotPacketBuffer buf {};

    while (true) {
        ae::NetAddress from {};
        const ae::i32 received = slot.simulator->receive_from(from, buf.data(), buf.size());
        if (received <= 0) {
            break;
        }

        const auto span = std::span<const std::byte>(buf.data(), static_cast<ae::usize>(received));

        // ServerWelcome
        if (received == static_cast<ae::i32>(ahamkara::game::server_welcome_packet_size())) {
            ahamkara::game::ServerWelcomePacket welcome {};
            ahamkara::game::PacketEnvelope in_env {};
            if (!ahamkara::game::deserialize_server_welcome_packet(span, in_env, welcome)) {
                ae::log_warning(std::string("client=") + slot.label + " invalid welcome packet.");
                continue;
            }

            slot.sequence_tracker.process_incoming(in_env);
            slot.handshake_done = true;

            std::ostringstream out;
            out << "client=" << slot.label
                << " event=welcome"
                << " proto=" << welcome.protocol_version;
            emit_line(out.str());
            continue;
        }

        // ServerReject
        if (received == static_cast<ae::i32>(ahamkara::game::server_reject_packet_size())) {
            ahamkara::game::ServerRejectPacket reject {};
            ahamkara::game::PacketEnvelope in_env {};
            if (!ahamkara::game::deserialize_server_reject_packet(span, in_env, reject)) {
                ae::log_warning(std::string("client=") + slot.label + " invalid reject packet.");
                continue;
            }

            slot.sequence_tracker.process_incoming(in_env);

            std::ostringstream out;
            out << "client=" << slot.label
                << " event=reject"
                << " reason=" << reject_reason_str(reject.reason);
            emit_line(out.str());
            return false;
        }

        // Ignore other packet types during handshake (e.g. stray snapshots).
    }

    return true;
}

// ── Gameplay phase ──────────────────────────────────────────────────────

bool send_input_packet(
    ClientSlot& slot,
    const ClientOptions& options,
    const ae::NetAddress& server_address) {
    ahamkara::game::PlayerInputCommand command {};
    command.sequence = slot.next_client_tick;
    command.client_tick = slot.next_client_tick;
    command.client_time = static_cast<float>(ae::now_seconds());
    command.move_axis.x = options.move_x;
    command.move_axis.y = options.move_y;
    command.look_delta.x = options.look_x;
    command.look_delta.y = options.look_y;
    command.fire_held = options.fire_held;

    const ahamkara::game::PacketEnvelope envelope = slot.sequence_tracker.prepare_outgoing();
    ahamkara::game::PlayerInputPacketBuffer buffer {};
    if (!ahamkara::game::serialize_player_input_packet(envelope, command, buffer)) {
        ae::log_error("Failed to serialize player input packet.");
        return false;
    }

    if (!slot.simulator->send_to(server_address, buffer.data(), buffer.size())) {
        ae::log_error("Failed to send player input packet.");
        return false;
    }

    if (options.print_events) {
        std::ostringstream out;
        out << "client=" << slot.label
            << " event=input"
            << " seq=" << command.sequence
            << " tick=" << command.client_tick
            << " move=" << format_vec2(command.move_axis.x, command.move_axis.y)
            << " look=" << format_vec2(command.look_delta.x, command.look_delta.y)
            << " fire=" << (command.fire_held ? 1 : 0);
        emit_line(out.str());
    }

    ++slot.next_client_tick;
    return true;
}

void drain_snapshots(
    ClientSlot& slot,
    const ClientOptions& options) {

    while (true) {
        ae::NetAddress from {};
        ahamkara::game::ServerSnapshotPacketBuffer buffer {};
        const ae::i32 received = slot.simulator->receive_from(from, buffer.data(), buffer.size());
        if (received <= 0) {
            break;
        }

        if (received != static_cast<ae::i32>(buffer.size())) {
            ae::log_warning("Ignored snapshot with unexpected size.");
            continue;
        }

        ahamkara::game::PacketEnvelope envelope {};
        ahamkara::game::ServerSnapshot snapshot {};
        if (!ahamkara::game::deserialize_server_snapshot_packet(buffer, envelope, snapshot)) {
            ae::log_warning("Ignored invalid snapshot packet.");
            continue;
        }

        slot.sequence_tracker.process_incoming(envelope);

        if (!slot.connected_banner_printed) {
            slot.connected_banner_printed = true;
            emit_line(std::string("client=") + slot.label + " event=connected");
        }

        if (!options.print_snapshots) {
            continue;
        }

        std::ostringstream out;
        out << "client=" << slot.label
            << " event=snapshot"
            << " wire_seq=" << envelope.sequence
            << " server_tick=" << snapshot.server_tick
            << " last_input=" << snapshot.last_processed_input
            << " pos=" << format_vec3(snapshot.local_player.position)
            << " vel=" << format_vec3(snapshot.local_player.velocity)
            << " health=" << snapshot.local_player.health
            << " shield=" << snapshot.local_player.shield;
        emit_line(out.str());
    }
}

}  // namespace

int main(int argc, char** argv) {
    ClientOptions options {};
    if (!parse_options(argc, argv, options)) {
        return EXIT_FAILURE;
    }

    if (options.client_count < 1 || options.tick_rate_hz <= 0.0 || options.duration_seconds < 0.0) {
        ae::log_error("Invalid client harness configuration.");
        return EXIT_FAILURE;
    }

    ae::Application application(ae::RuntimeMode::Client);
    (void)application.start();

    const ae::SimulatorConfig sim_config = ae::build_sim_config(argc, argv);
    const ae::NetAddress server_address {options.server_ip, options.server_port};

    std::vector<std::unique_ptr<ClientSlot>> clients;
    clients.reserve(static_cast<std::size_t>(options.client_count));
    try {
        for (int i = 0; i < options.client_count; ++i) {
            clients.push_back(make_client_slot(options, i, sim_config));
        }
    } catch (const std::exception& ex) {
        ae::log_error(ex.what());
        application.shutdown();
        return EXIT_FAILURE;
    }

    for (auto& client : clients) {
        log_start_banner(options, *client);
    }

    const auto tick_duration = std::chrono::duration<double>(1.0 / options.tick_rate_hz);
    const auto start_time = std::chrono::steady_clock::now();
    const auto deadline = options.duration_seconds > 0.0
        ? start_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
              std::chrono::duration<double>(options.duration_seconds))
        : std::chrono::steady_clock::time_point::max();

    auto previous_frame = std::chrono::steady_clock::now();
    auto next_tick = previous_frame;

    // ── Phase 1: Handshake ────────────────────────────────────────────
    bool all_handshakes_done = false;
    while (!all_handshakes_done && std::chrono::steady_clock::now() < deadline) {
        const auto frame_now = std::chrono::steady_clock::now();
        const float frame_dt = std::chrono::duration<float>(frame_now - previous_frame).count();
        previous_frame = frame_now;

        for (auto& client : clients) {
            client->simulator->update(frame_dt);
        }

        all_handshakes_done = true;

        for (auto& client : clients) {
            if (client->handshake_done) {
                continue;
            }

            all_handshakes_done = false;

            if (client->handshake_ticks >= kMaxHandshakeTicks) {
                std::ostringstream out;
                out << "client=" << client->label
                    << " event=timeout"
                    << " reason=handshake_timeout";
                emit_line(out.str());
                application.shutdown();
                return EXIT_FAILURE;
            }

            if (!send_hello(*client, server_address)) {
                application.shutdown();
                return EXIT_FAILURE;
            }

            if (!drain_handshake(*client)) {
                application.shutdown();
                return EXIT_FAILURE;
            }
        }

        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);
        std::this_thread::sleep_until(next_tick);
    }

    // ── Phase 2: Gameplay ─────────────────────────────────────────────
    while (std::chrono::steady_clock::now() < deadline) {
        const auto frame_now = std::chrono::steady_clock::now();
        const float frame_dt = std::chrono::duration<float>(frame_now - previous_frame).count();
        previous_frame = frame_now;

        for (auto& client : clients) {
            client->simulator->update(frame_dt);
        }

        for (auto& client : clients) {
            if (!send_input_packet(*client, options, server_address)) {
                application.shutdown();
                return EXIT_FAILURE;
            }
        }

        for (auto& client : clients) {
            drain_snapshots(*client, options);
        }

        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);
        std::this_thread::sleep_until(next_tick);
    }

    // Drain any remaining packets.
    for (auto& client : clients) {
        client->simulator->update(0.0F);
        drain_snapshots(*client, options);
    }

    application.shutdown();
    return EXIT_SUCCESS;
}
