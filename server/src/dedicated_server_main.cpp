#include "ae/core/log.h"
#include "ae/network/udp_socket.h"
#include "ae/runtime/application.h"
#include "ahamkara/game/game_module.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <chrono>
#include <sstream>
#include <thread>

int main() {
    ae::Application application(ae::RuntimeMode::DedicatedServer);
    application.start();

    ae::UdpSocket socket;
    if (!socket.open(7777)) {
        ae::log_error("Dedicated server failed to open UDP port 7777.");
        return 1;
    }

    std::ostringstream startup_message;
    startup_message << ahamkara::game::game_name() << " dedicated server listening on UDP 7777.";
    ae::log_info(startup_message.str());

    ahamkara::game::World world;
    ahamkara::game::ServerSnapshot snapshot {};
    ae::NetAddress last_client {};
    bool has_client = false;

    constexpr float tick_rate = 60.0F;
    constexpr float delta_seconds = 1.0F / tick_rate;
    const auto tick_duration = std::chrono::duration<double>(delta_seconds);
    auto next_tick = std::chrono::steady_clock::now();

    ahamkara::game::PlayerInputPacketBuffer packet_buffer {};
    ahamkara::game::ServerSnapshotPacketBuffer snapshot_buffer {};
    ae::u32 server_tick = 0;

    while (application.is_running()) {
        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);

        while (true) {
            ae::NetAddress from {};
            const ae::i32 received = socket.receive_from(from, packet_buffer.data(), packet_buffer.size());
            if (received <= 0) {
                break;
            }

            if (received != static_cast<ae::i32>(packet_buffer.size())) {
                ae::log_warning("Dedicated server received an unexpected input packet size.");
                continue;
            }

            ahamkara::game::PlayerInputCommand command {};
            if (!ahamkara::game::deserialize_player_input_packet(packet_buffer, command)) {
                ae::log_warning("Dedicated server rejected an invalid input packet.");
                continue;
            }

            has_client = true;
            last_client = from;

            world.tick(delta_seconds, command);
            snapshot.last_processed_input = command.sequence;
        }

        snapshot.server_tick = server_tick++;
        snapshot.local_player = world.get_player_state();

        if (has_client
            && (!ahamkara::game::serialize_server_snapshot_packet(snapshot, snapshot_buffer)
                || !socket.send_to(last_client, snapshot_buffer.data(), snapshot_buffer.size()))) {
            ae::log_warning("Dedicated server failed to send snapshot to the connected client.");
        }

        std::this_thread::sleep_until(next_tick);
    }

    application.shutdown();
    return 0;
}
