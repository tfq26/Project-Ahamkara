#include "ae/core/cli_utils.h"
#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/network/cli_helpers.h"
#include "ae/network/network_simulator.h"
#include "ae/network/server_history.h"
#include "ae/network/udp_socket.h"
#include "ae/render/compiled_level.h"
#include "ae/runtime/application.h"
#include "ahamkara/game/activities/deathmatch_activity.h"
#include "ahamkara/game/activities/horde_activity.h"
#include "ahamkara/game/activities/social_hub_activity.h"
#include "ahamkara/game/game_module.h"
#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"
#include "wish/admin/admin_server.h"
#include "wish/admin/server_config.h"
#include "wish/core/activity.h"
#include "wish/core/activity_manager.h"
#include "wish/integrations/nakama/nakama_bridge.h"
#include "wish/integrations/nakama/mock_session_services.h"
#include "wish/session/session_runtime.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

inline std::string build_remote_endpoint(const ae::NetAddress& address) {
    return address.ip + ":" + std::to_string(address.port);
}

/// State for one peer connected to this server across all activities.
struct PeerState {
    ae::NetAddress address {};
    bool handshake_complete {false};
    std::chrono::steady_clock::time_point last_seen {};
    wish::session::SessionId session_id {};
    wish::core::ActivityId activity_id {0};

    // Auth state
    bool session_admitted {false};
    std::string authenticated_player_id;
    std::string pending_auth_token;
};

}  // namespace

int main(int argc, char** argv) {
    ae::Application application(ae::RuntimeMode::DedicatedServer);
    application.start();

    const wish::admin::ServerConfig server_config = wish::admin::load_server_config(argc, argv);

    // ── Network setup ─────────────────────────────────────────────────────
    ae::UdpSocket socket;
    if (!socket.open(server_config.port)) {
        ae::log_error("Dedicated server failed to open the configured UDP port.");
        return 1;
    }

    const ae::SimulatorConfig sim_config = ae::build_sim_config(argc, argv);
    ae::NetworkSimulator sim(socket);
    sim.configure(sim_config);

    // ── Activity templates ────────────────────────────────────────────────
    wish::core::ActivityManager activity_mgr;

    // Register activity templates (can also be loaded from JSON)
    activity_mgr.register_template(wish::core::ActivityConfig {
        .id = 1,
        .name = "Crucible - Clash",
        .category = wish::core::ActivityCategory::PvP,
        .max_players = static_cast<ae::u32>(server_config.max_players),
        .tick_rate = server_config.tick_rate,
        .map_path = "default",
        .allow_spectators = false,
    });

    activity_mgr.register_template(wish::core::ActivityConfig {
        .id = 2,
        .name = "Horde - Defend the Core",
        .category = wish::core::ActivityCategory::PvE,
        .max_players = 4,
        .tick_rate = 60.0F,
        .map_path = "default",
        .allow_spectators = false,
    });

    activity_mgr.register_template(wish::core::ActivityConfig {
        .id = 3,
        .name = "The Tower",
        .category = wish::core::ActivityCategory::Social,
        .max_players = 64,
        .tick_rate = 30.0F,
        .map_path = "default",
        .allow_spectators = false,
    });

    // ── Start up a deathmatch activity by default ─────────────────────────
    auto dm_activity = std::make_unique<ahamkara::game::activities::DeathmatchActivity>();
    wish::core::ActivityId default_activity_id =
        activity_mgr.start_activity(static_cast<wish::core::ActivityId>(1), std::move(dm_activity));

    // Also start a social hub so players have a lobby
    auto hub_activity = std::make_unique<ahamkara::game::activities::SocialHubActivity>();
    activity_mgr.start_activity(static_cast<wish::core::ActivityId>(3), std::move(hub_activity));

    // ── Auth services ──────────────────────────────────────────────────────
    const wish::integrations::nakama::BridgeSettings nakama_bridge =
        wish::integrations::nakama::load_bridge_settings(argc, argv);
    std::unique_ptr<wish::core::AuthValidator> auth_validator =
        wish::integrations::nakama::make_auth_validator(nakama_bridge);
    wish::integrations::nakama::NoopSessionAdmissionService session_admission_service;
    wish::integrations::nakama::NoopMatchResultReporter match_result_reporter;

    // ── Peer tracking ─────────────────────────────────────────────────────
    std::unordered_map<std::string, PeerState> peers;
    std::vector<ae::NetAddress> handshake_list;  // addresses that completed handshake

    // ── Tick timing ───────────────────────────────────────────────────────
    const float tick_rate = server_config.tick_rate > 0.0F ? server_config.tick_rate : 60.0F;
    const float delta_seconds = 1.0F / tick_rate;
    const auto tick_duration = std::chrono::duration<double>(delta_seconds);
    auto next_tick = std::chrono::steady_clock::now();
    const auto server_start = next_tick;
    const auto match_start = next_tick;

    ae::u32 server_tick = 0;
    ae::u32 main_envelope_seq = 0;

    // ── Packet buffers ────────────────────────────────────────────────────
    ahamkara::game::PlayerInputPacketBuffer packet_buffer {};
    ahamkara::game::ServerWelcomePacketBuffer welcome_buffer {};
    ahamkara::game::ServerRejectPacketBuffer reject_buffer {};

    // ── Admin HTTP server ─────────────────────────────────────────────────
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

        // Report players from all running activities
        for (auto& p : peers) {
            if (!p.second.handshake_complete) continue;
            wish::admin::PlayerStatus player {};
            player.endpoint = p.first;
            player.seconds_since_seen = std::chrono::duration<float>(now - p.second.last_seen).count();
            status.players.push_back(std::move(player));
        }

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

    // ── Startup log ───────────────────────────────────────────────────────
    {
        std::ostringstream msg;
        msg << ahamkara::game::game_name()
            << " dedicated server listening on UDP " << server_config.port
            << " and admin HTTP " << server_config.admin_port
            << " (tick=" << tick_rate
            << "Hz, maxPlayers=" << server_config.max_players
            << ", activities=" << activity_mgr.running_count() << ").";
        if (wish::integrations::nakama::is_enabled(nakama_bridge)) {
            msg << " Nakama bridge enabled at "
                << wish::integrations::nakama::describe_bridge(nakama_bridge) << '.';
        } else {
            msg << " Nakama bridge disabled; using local no-op auth.";
        }
        ae::log_info(msg.str());
    }

    auto previous_frame = std::chrono::steady_clock::now();

    // ── Main loop ─────────────────────────────────────────────────────────
    while (application.is_running()) {
        const float frame_dt = ae::compute_frame_dt(previous_frame);
        const auto frame_now = std::chrono::steady_clock::now();

        sim.update(frame_dt);
        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);

        // ── Receive packets ───────────────────────────────────────────
        while (true) {
            ae::NetAddress from {};
            const ae::i32 received = sim.receive_from(from, packet_buffer.data(), packet_buffer.size());
            if (received <= 0) break;

            const auto packet_span = std::span<const std::byte>(
                packet_buffer.data(), static_cast<ae::usize>(received));

            std::string peer_key = from.ip + ":" + std::to_string(from.port);
            PeerState& peer = peers[peer_key];
            peer.address = from;
            peer.last_seen = frame_now;

            // ── Handshake ──────────────────────────────────────────────
            if (received >= static_cast<ae::i32>(ahamkara::game::kMinClientHelloWireSize)) {
                ahamkara::game::ClientHelloPacket hello {};
                ahamkara::game::PacketEnvelope in_envelope {};
                if (!ahamkara::game::deserialize_client_hello_packet(packet_span, in_envelope, hello)) {
                    ae::log_warning("Dedicated server rejected an invalid handshake packet.");
                    continue;
                }

                if (!ahamkara::game::is_supported_protocol_version(hello.protocol_version)) {
                    ahamkara::game::ServerRejectPacket reject {};
                    reject.protocol_version = ahamkara::game::kProtocolVersion;
                    reject.reason = ahamkara::game::HandshakeRejectReason::VersionMismatch;

                    ahamkara::game::PacketEnvelope env {};
                    env.sequence = ++main_envelope_seq;
                    if (!ahamkara::game::serialize_server_reject_packet(env, reject, reject_buffer)
                        || !sim.send_to(from, reject_buffer.data(), reject_buffer.size())) {
                        ae::log_warning("Dedicated server failed to reject an unsupported protocol version.");
                    }
                    continue;
                }

                if (static_cast<int>(handshake_list.size()) >= server_config.max_players &&
                    std::find(handshake_list.begin(), handshake_list.end(), from) == handshake_list.end()) {
                    ahamkara::game::ServerRejectPacket reject {};
                    reject.protocol_version = ahamkara::game::kProtocolVersion;
                    reject.reason = ahamkara::game::HandshakeRejectReason::ServerBusy;

                    ahamkara::game::PacketEnvelope env {};
                    env.sequence = ++main_envelope_seq;
                    if (!ahamkara::game::serialize_server_reject_packet(env, reject, reject_buffer)
                        || !sim.send_to(from, reject_buffer.data(), reject_buffer.size())) {
                        ae::log_warning("Dedicated server failed to reject an extra client.");
                    }
                    continue;
                }

                if (std::find(handshake_list.begin(), handshake_list.end(), from) == handshake_list.end()) {
                    handshake_list.push_back(from);
                }

                ahamkara::game::ServerWelcomePacket welcome {};
                welcome.protocol_version = ahamkara::game::kProtocolVersion;
                std::snprintf(welcome.player_id, ahamkara::game::kMaxPlayerIdLength, "%s",
                    peer.authenticated_player_id.empty() ? "pending" : peer.authenticated_player_id.c_str());

                ahamkara::game::PacketEnvelope env {};
                env.sequence = ++main_envelope_seq;
                if (!ahamkara::game::serialize_server_welcome_packet(env, welcome, welcome_buffer)
                    || !sim.send_to(from, welcome_buffer.data(), welcome_buffer.size())) {
                    ae::log_warning("Dedicated server failed to send a handshake welcome.");
                }

                // Save the auth token for later validation on first input
                if (hello.auth_token_length > 0 && hello.auth_token_length <= ahamkara::game::kMaxAuthTokenLength) {
                    peer.pending_auth_token.assign(hello.auth_token, hello.auth_token_length);
                }

                peer.handshake_complete = true;
                continue;
            }

            // ── Input packets ──────────────────────────────────────────
            if (received != static_cast<ae::i32>(packet_buffer.size())) {
                // Unknown packet size — could be activity-specific
                continue;
            }

            if (!peer.handshake_complete) {
                ae::log_warning("Dedicated server received input before handshake completion.");
                continue;
            }

            // ── Deserialize input ──────────────────────────────────────
            ahamkara::game::PlayerInputCommand command {};
            ahamkara::game::PacketEnvelope in_envelope {};
            if (!ahamkara::game::deserialize_player_input_packet(packet_span, in_envelope, command)) {
                ae::log_warning("Dedicated server rejected an invalid input packet.");
                continue;
            }

            // ── Auth / admission (first input) ─────────────────────────
            if (!peer.session_admitted) {
                const std::string remote_endpoint = build_remote_endpoint(from);
                const std::string& auth_token = peer.pending_auth_token.empty()
                    ? "wish-placeholder-token" : peer.pending_auth_token;
                const wish::core::AuthResult auth_result = auth_validator->validate(
                    wish::core::AuthRequest {
                        .token = auth_token,
                        .remote_endpoint = remote_endpoint,
                    });
                if (!auth_result.accepted) {
                    ae::log_warning(
                        "Dedicated server rejected an unauthenticated session: " + auth_result.error_message);
                    continue;
                }

                const wish::core::SessionAdmissionResult admission_result =
                    session_admission_service.admit(wish::core::SessionAdmissionRequest {
                        .player_id = auth_result.player_id,
                        .session_id = auth_result.session_id,
                        .remote_endpoint = remote_endpoint,
                    });
                if (!admission_result.admitted) {
                    ae::log_warning("Dedicated server rejected a session admission request.");
                    continue;
                }

                // Admit the player into the default deathmatch activity
                auto* dm = static_cast<ahamkara::game::activities::DeathmatchActivity*>(
                    activity_mgr.get_activity(default_activity_id));
                if (!dm || !dm->admit_player(wish::core::SessionAdmissionRequest {
                        .player_id = auth_result.player_id,
                        .session_id = auth_result.session_id,
                        .remote_endpoint = remote_endpoint,
                    })) {
                    ae::log_warning("Dedicated server could not admit player into activity.");
                    continue;
                }

                // Get the player slot
                auto* slot = dm->first_slot();
                if (slot) {
                    slot->address = from;
                    peer.session_id = slot->session_id;
                    peer.activity_id = default_activity_id;
                    peer.session_admitted = true;
                    peer.authenticated_player_id = auth_result.player_id;
                }
            }

            // ── Route input to the activity ────────────────────────────
            auto* activity = activity_mgr.get_activity(peer.activity_id);
            if (!activity) continue;

            // Record envelope processing
            activity->process_input(peer.session_id, in_envelope, command.sequence);

            // Simulate input in deathmatch activity
            if (auto* dm = dynamic_cast<ahamkara::game::activities::DeathmatchActivity*>(activity)) {
                dm->simulate_input(peer.session_id, delta_seconds, command);
            }
        }

        // ── Prune timed-out peers ──────────────────────────────────────
        for (auto it = peers.begin(); it != peers.end(); ) {
            const auto elapsed = std::chrono::duration<float>(frame_now - it->second.last_seen).count();
            if (elapsed > server_config.disconnect_timeout_seconds) {
                auto* activity = activity_mgr.get_activity(it->second.activity_id);
                if (activity) {
                    activity->remove_player(it->second.session_id);
                }
                handshake_list.erase(
                    std::remove(handshake_list.begin(), handshake_list.end(), it->second.address),
                    handshake_list.end());
                it = peers.erase(it);
            } else {
                ++it;
            }
        }

        // ── Tick all activities ────────────────────────────────────────
        activity_mgr.tick_all(delta_seconds);

        // ── Broadcast snapshots ────────────────────────────────────────
        activity_mgr.broadcast_snapshots([&](wish::session::SessionId sid,
                                              const std::byte* data, ae::usize len) {
            // Find the client address for this session
            for (auto& p : peers) {
                if (p.second.session_id.value == sid.value && p.second.handshake_complete) {
                    sim.send_to(p.second.address, data, static_cast<ae::i32>(len));
                    break;
                }
            }
        });

        // ── Periodic stats ─────────────────────────────────────────────
        server_tick++;
        if (server_tick % 300 == 0) {
            std::ostringstream stats;
            stats << "Server tick=" << server_tick
                  << " peers=" << peers.size()
                  << " activities=" << activity_mgr.running_count()
                  << " players=" << activity_mgr.total_player_count();
            ae::log_info(stats.str());

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

        refresh_admin_status();

        std::this_thread::sleep_until(next_tick);
    }

    // ── Shutdown ──────────────────────────────────────────────────────────
    for (auto& p : peers) {
        if (p.second.session_admitted) {
            match_result_reporter.report_match_result(wish::core::MatchResult {
                .match_id = "wish-match",
                .player_id = p.second.authenticated_player_id,
                .completed = false,
                .summary = "dedicated server shutdown",
            });
        }
    }

    admin_server.stop();
    application.shutdown();
    return 0;
}
