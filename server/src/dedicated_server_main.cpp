#include "ae/core/cli_utils.h"
#include "ae/core/log.h"
#include "ae/network/cli_helpers.h"
#include "ae/network/network_simulator.h"
#include "ae/network/server_history.h"
#include "ae/network/udp_socket.h"
#include "ae/runtime/application.h"
#include "ahamkara/game/game_module.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"
#include "wish/admin/admin_server.h"
#include "wish/admin/server_config.h"
#include "wish/integrations/nakama/mock_session_services.h"
#include "wish/session/session_runtime.h"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

inline float compute_frame_dt(std::chrono::steady_clock::time_point& previous) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - previous).count();
    previous = now;
    return dt;
}

inline std::string build_remote_endpoint(const ae::NetAddress& address) {
    return address.ip + ":" + std::to_string(address.port);
}

}  // namespace

int main(int argc, char** argv) {
    ae::Application application(ae::RuntimeMode::DedicatedServer);
    application.start();

    const wish::admin::ServerConfig server_config = wish::admin::load_server_config(argc, argv);

    ae::UdpSocket socket;
    if (!socket.open(server_config.port)) {
        ae::log_error("Dedicated server failed to open the configured UDP port.");
        return 1;
    }

    // ── Simulator config from CLI ─────────────────────────────────────────
    const ae::SimulatorConfig sim_config = ae::build_sim_config(argc, argv);

    ae::NetworkSimulator sim(socket);
    sim.configure(sim_config);

    // ── Server history buffer ─────────────────────────────────────────────
    ae::ServerHistoryBuffer<ahamkara::game::HistoricalState, 1024> history_buffer;

    // ── Authoritative world ─────────────────────────────────────────────
    // The server owns the one true simulation.  All state originates here
    // and is snapshotted for clients.  set_is_client(false) disables
    // cosmetic-only client effects (hitmarkers) that the server never renders.
    ahamkara::game::World world;
    world.set_is_client(false);

    ahamkara::game::ServerSnapshot snapshot {};
    wish::session::SessionRuntime session_runtime(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(server_config.disconnect_timeout_seconds)));
    wish::integrations::nakama::NoopAuthValidator auth_validator;
    wish::integrations::nakama::NoopSessionAdmissionService session_admission_service;
    wish::integrations::nakama::NoopMatchResultReporter match_result_reporter;
    bool session_admitted = false;
    std::string authenticated_player_id;
    std::string match_id = "wish-match";

    const float tick_rate = server_config.tick_rate > 0.0F ? server_config.tick_rate : 60.0F;
    const float delta_seconds = 1.0F / tick_rate;
    const auto tick_duration = std::chrono::duration<double>(delta_seconds);
    auto next_tick = std::chrono::steady_clock::now();
    const auto server_start = next_tick;
    const auto match_start = next_tick;

    ahamkara::game::PlayerInputPacketBuffer packet_buffer {};
    ahamkara::game::ServerSnapshotPacketBuffer snapshot_buffer {};
    ahamkara::game::ServerRejectPacketBuffer reject_buffer {};
    ahamkara::game::ServerWelcomePacketBuffer welcome_buffer {};
    ahamkara::game::PacketEnvelope envelope {};
    ae::NetAddress last_client {};
    bool client_connected = false;
    ae::u32 server_tick = 0;
    wish::admin::HttpAdminServer admin_server;
    wish::admin::ServerStatus admin_status {};
    std::mutex admin_status_mutex;

    auto refresh_admin_status = [&]() {
        const auto now = std::chrono::steady_clock::now();
        wish::admin::ServerStatus status {};
        status.game_name = ahamkara::game::game_name();
        status.game_port = server_config.port;
        status.admin_port = server_config.admin_port;
        status.tick_rate = tick_rate;
        status.server_tick = server_tick;
        status.max_players = server_config.max_players;
        status.disconnect_timeout_seconds = server_config.disconnect_timeout_seconds;
        status.match_duration_seconds = server_config.match_duration_seconds;
        status.uptime_seconds = std::chrono::duration<float>(now - server_start).count();
        status.match_elapsed_seconds = std::chrono::duration<float>(now - match_start).count();
        if (server_config.match_duration_seconds > 0.0F) {
            const float remaining = server_config.match_duration_seconds - status.match_elapsed_seconds;
            status.match_remaining_seconds = remaining > 0.0F ? remaining : 0.0F;
            status.match_active = remaining > 0.0F;
        } else {
            status.match_remaining_seconds.reset();
            status.match_active = true;
        }

        session_runtime.for_each_client([&](const wish::session::ClientSession& client) {
            if (client.connection_state != wish::session::ClientConnectionState::Connected) {
                return;
            }

            wish::admin::PlayerStatus player {};
            player.endpoint = client.identity;
            player.seconds_since_seen = std::chrono::duration<float>(now - client.last_seen).count();
            status.players.push_back(std::move(player));
        });

        std::lock_guard<std::mutex> lock(admin_status_mutex);
        admin_status = std::move(status);
    };

    if (!admin_server.start(server_config.admin_port, [&]() {
            std::lock_guard<std::mutex> lock(admin_status_mutex);
            return admin_status;
        })) {
        ae::log_error("Dedicated server failed to start the admin HTTP surface.");
        return 1;
    }

    std::ostringstream startup_message;
    startup_message << ahamkara::game::game_name()
                    << " dedicated server listening on UDP " << server_config.port
                    << " and admin HTTP " << server_config.admin_port
                    << " (tick=" << tick_rate
                    << "Hz, maxPlayers=" << server_config.max_players
                    << ", disconnectTimeout=" << server_config.disconnect_timeout_seconds
                    << "s, matchDuration=" << server_config.match_duration_seconds << "s).";
    ae::log_info(startup_message.str());

    auto previous_frame = std::chrono::steady_clock::now();

    while (application.is_running()) {
        const float frame_dt = compute_frame_dt(previous_frame);
        const auto frame_now = std::chrono::steady_clock::now();

        // Process simulator delayed packets.
        sim.update(frame_dt);

        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);

        while (true) {
            ae::NetAddress from {};
            const ae::i32 received = sim.receive_from(from, packet_buffer.data(), packet_buffer.size());
            if (received <= 0) {
                break;
            }

            const auto packet_span = std::span<const std::byte>(
                packet_buffer.data(), static_cast<ae::usize>(received));

            if (received == static_cast<ae::i32>(ahamkara::game::client_hello_packet_size())) {
                ahamkara::game::ClientHelloPacket hello {};
                ahamkara::game::PacketEnvelope in_envelope {};
                if (!ahamkara::game::deserialize_client_hello_packet(packet_span, in_envelope, hello)) {
                    ae::log_warning("Dedicated server rejected an invalid handshake packet.");
                    continue;
                }

                if (!ahamkara::game::is_supported_protocol_version(hello.protocol_version)) {
                    ahamkara::game::ServerRejectPacket reject {};
                    reject.protocol_version = ahamkara::game::kProtocolVersion;
                    reject.session_token = hello.session_token;
                    reject.reason = ahamkara::game::HandshakeRejectReason::VersionMismatch;

                    envelope.sequence++;
                    if (!ahamkara::game::serialize_server_reject_packet(envelope, reject, reject_buffer)
                        || !sim.send_to(from, reject_buffer.data(), reject_buffer.size())) {
                        ae::log_warning("Dedicated server failed to reject an unsupported protocol version.");
                    }
                    continue;
                }

                if (client_connected && from != last_client) {
                    ahamkara::game::ServerRejectPacket reject {};
                    reject.protocol_version = ahamkara::game::kProtocolVersion;
                    reject.session_token = hello.session_token;
                    reject.reason = ahamkara::game::HandshakeRejectReason::ServerBusy;

                    envelope.sequence++;
                    if (!ahamkara::game::serialize_server_reject_packet(envelope, reject, reject_buffer)
                        || !sim.send_to(from, reject_buffer.data(), reject_buffer.size())) {
                        ae::log_warning("Dedicated server failed to reject an extra client.");
                    }
                    continue;
                }

                client_connected = true;
                last_client = from;

                ahamkara::game::ServerWelcomePacket welcome {};
                welcome.protocol_version = ahamkara::game::kProtocolVersion;
                welcome.session_token = hello.session_token;

                envelope.sequence++;
                if (!ahamkara::game::serialize_server_welcome_packet(envelope, welcome, welcome_buffer)
                    || !sim.send_to(from, welcome_buffer.data(), welcome_buffer.size())) {
                    ae::log_warning("Dedicated server failed to send a handshake welcome.");
                }
                continue;
            }

            if (received != static_cast<ae::i32>(packet_buffer.size())) {
                ae::log_warning("Dedicated server received an unexpected packet size.");
                continue;
            }

            ahamkara::game::PlayerInputCommand command {};
            ahamkara::game::PacketEnvelope in_envelope {};
            if (!ahamkara::game::deserialize_player_input_packet(packet_span, in_envelope, command)) {
                ae::log_warning("Dedicated server rejected an invalid input packet.");
                continue;
            }

            if (!client_connected || from != last_client) {
                ae::log_warning("Dedicated server received input before handshake completion.");
                continue;
            }

            if (!session_admitted) {
                const std::string remote_endpoint = build_remote_endpoint(from);
                const wish::core::AuthResult auth_result = auth_validator.validate(wish::core::AuthRequest {
                    .token = "wish-placeholder-token",
                    .remote_endpoint = remote_endpoint,
                });
                if (!auth_result.accepted) {
                    ae::log_warning("Dedicated server rejected an unauthenticated session.");
                    continue;
                }

                const wish::core::SessionAdmissionResult admission_result = session_admission_service.admit(
                    wish::core::SessionAdmissionRequest {
                        .player_id = auth_result.player_id,
                        .session_id = auth_result.session_id,
                        .remote_endpoint = remote_endpoint,
                    });
                if (!admission_result.admitted) {
                    ae::log_warning("Dedicated server rejected a session admission request.");
                    continue;
                }

                authenticated_player_id = auth_result.player_id;
                match_id = admission_result.match_id;
                session_admitted = true;
            }

            auto& session = session_runtime.record_input(from, in_envelope, command, frame_now);

            // ── Authoritative tick ────────────────────────────────────
            // The server simulates the one true state from the received
            // input.  This is the authoritative ground truth.
            world.tick(delta_seconds, command);
            session_runtime.mark_input_processed(session, command.sequence);
        }

        session_runtime.prune_timed_out_clients(frame_now);

        // ── Snapshot boundary ────────────────────────────────────────
        // Copy authoritative state into the snapshot for transmission.
        // From this point on, `snapshot.local_player` is a frozen
        // representation; the authoritative world continues to evolve
        // independently on the next tick.
        snapshot.server_tick = server_tick++;
        snapshot.local_player = world.get_player_state();

        // ── History record ───────────────────────────────────────────
        // Persist authoritative state into the history buffer for
        // future replay, lag compensation, and anti-cheat validation.
        ahamkara::game::HistoricalState hist {};
        hist.tick = snapshot.server_tick;
        hist.player_position = snapshot.local_player.position;
        // Copy dummy state (existing 4-dummy layout).
        for (int d = 0; d < ahamkara::game::World::kMaxDummies; ++d) {
            const auto& dummies = world.get_dummies();
            hist.dummy_positions[d] = dummies[d].position;
            hist.dummy_alive[d] = dummies[d].alive;
        }
        history_buffer.record(snapshot.server_tick, hist);

        // Log history stats every 300 ticks (~5 seconds).
        if (server_tick % 300 == 0) {
            std::ostringstream hist_msg;
            hist_msg << "Server history buffer: ticks=[" << history_buffer.oldest_tick()
                     << ", " << history_buffer.newest_tick()
                     << "] size=" << history_buffer.size()
                     << "/" << history_buffer.capacity();
            ae::log_info(hist_msg.str());

            if (sim_config.enabled) {
                const auto& st = sim.stats();
                std::ostringstream sim_msg;
                sim_msg << "Simulator: sent=" << st.packets_sent
                        << " dropped=" << st.packets_dropped
                        << " delayed=" << st.packets_delayed
                        << " expired=" << st.packets_expired;
                ae::log_info(sim_msg.str());
            }
        }

        session_runtime.for_each_connected_client([&](wish::session::ClientSession& client) {
            snapshot.last_processed_input = client.last_processed_input_sequence;

            const auto envelope = client.sequence_tracker.prepare_outgoing();
            if (!ahamkara::game::serialize_server_snapshot_packet(envelope, snapshot, snapshot_buffer)
                || !sim.send_to(client.address, snapshot_buffer.data(), snapshot_buffer.size())) {
                std::ostringstream warn_msg;
                warn_msg << "Dedicated server failed to send snapshot to client " << client.identity << ".";
                ae::log_warning(warn_msg.str());
            }
        });

        std::this_thread::sleep_until(next_tick);
    }

    if (session_admitted) {
        match_result_reporter.report_match_result(wish::core::MatchResult {
            .match_id = match_id,
            .player_id = authenticated_player_id,
            .completed = false,
            .summary = "dedicated server shutdown",
        });
    }

    application.shutdown();
    return 0;
}
