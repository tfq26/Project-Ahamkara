#pragma once

#include "ae/core/types.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/udp_socket.h"
#include "ahamkara/game/net_types.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <string>

namespace wish::session {

enum class ClientConnectionState : ae::u8 {
    PendingAdmission,
    Connected,
    TimedOut
};

struct ClientSession {
    ae::NetAddress address {};
    std::string identity {};
    ClientConnectionState connection_state {ClientConnectionState::PendingAdmission};
    std::chrono::steady_clock::time_point last_seen {};
    ae::u32 last_processed_input_sequence {0};
    ae::u32 last_received_input_sequence {0};
    bool has_received_input {false};
    ae::SequenceTracker sequence_tracker {};
};

class SessionRuntime {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    explicit SessionRuntime(clock::duration disconnect_timeout = std::chrono::seconds(10))
        : disconnect_timeout_(disconnect_timeout) {
    }

    [[nodiscard]] ClientSession* find_client(const ae::NetAddress& address) {
        const auto it = std::find_if(clients_.begin(), clients_.end(), [&](const ClientSession& client) {
            return same_address(client.address, address);
        });
        return it == clients_.end() ? nullptr : &(*it);
    }

    [[nodiscard]] const ClientSession* find_client(const ae::NetAddress& address) const {
        const auto it = std::find_if(clients_.begin(), clients_.end(), [&](const ClientSession& client) {
            return same_address(client.address, address);
        });
        return it == clients_.end() ? nullptr : &(*it);
    }

    ClientSession& touch_client(const ae::NetAddress& address, time_point now) {
        if (auto* client = find_client(address)) {
            client->address = address;
            client->identity = build_identity(address);
            client->last_seen = now;
            if (client->connection_state == ClientConnectionState::TimedOut) {
                client->connection_state = ClientConnectionState::PendingAdmission;
            }
            return *client;
        }

        clients_.push_back(ClientSession {});
        ClientSession& client = clients_.back();
        client.address = address;
        client.identity = build_identity(address);
        client.connection_state = ClientConnectionState::PendingAdmission;
        client.last_seen = now;
        return client;
    }

    ClientSession& record_input(
        const ae::NetAddress& address,
        const ae::PacketEnvelope& envelope,
        const ahamkara::game::PlayerInputCommand& command,
        time_point now) {
        ClientSession& client = touch_client(address, now);
        client.sequence_tracker.process_incoming(envelope);
        client.connection_state = ClientConnectionState::Connected;
        client.last_seen = now;
        client.last_received_input_sequence = command.sequence;
        client.has_received_input = true;
        return client;
    }

    void mark_input_processed(ClientSession& client, ae::u32 sequence) {
        client.last_processed_input_sequence = sequence;
    }

    void prune_timed_out_clients(time_point now) {
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(), [&](ClientSession& client) {
            if (now - client.last_seen <= disconnect_timeout_) {
                return false;
            }

            client.connection_state = ClientConnectionState::TimedOut;
            return true;
        }), clients_.end());
    }

    template <typename Fn>
    void for_each_client(Fn&& fn) {
        for (auto& client : clients_) {
            fn(client);
        }
    }

    template <typename Fn>
    void for_each_client(Fn&& fn) const {
        for (const auto& client : clients_) {
            fn(client);
        }
    }

    template <typename Fn>
    void for_each_connected_client(Fn&& fn) {
        for (auto& client : clients_) {
            if (client.connection_state == ClientConnectionState::Connected) {
                fn(client);
            }
        }
    }

    template <typename Fn>
    void for_each_connected_client(Fn&& fn) const {
        for (const auto& client : clients_) {
            if (client.connection_state == ClientConnectionState::Connected) {
                fn(client);
            }
        }
    }

    [[nodiscard]] std::size_t client_count() const {
        return clients_.size();
    }

    [[nodiscard]] std::size_t connected_client_count() const {
        return static_cast<std::size_t>(std::count_if(clients_.begin(), clients_.end(), [](const ClientSession& client) {
            return client.connection_state == ClientConnectionState::Connected;
        }));
    }

    [[nodiscard]] clock::duration disconnect_timeout() const {
        return disconnect_timeout_;
    }

private:
    static bool same_address(const ae::NetAddress& lhs, const ae::NetAddress& rhs) {
        return lhs.port == rhs.port && lhs.ip == rhs.ip;
    }

    static std::string build_identity(const ae::NetAddress& address) {
        return address.ip + ":" + std::to_string(address.port);
    }

    std::deque<ClientSession> clients_;
    clock::duration disconnect_timeout_;
};

}  // namespace wish::session
