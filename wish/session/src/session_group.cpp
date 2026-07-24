#define WISH_LOG_CATEGORY "SessionGroup"
#include "wish/session/session_group.h"
#include "wish/log.h"

namespace wish::session {

SessionGroup::SessionGroup(wish::u32 group_id, wish::u32 max_clients)
    : group_id_(group_id), max_clients_(max_clients), disconnect_timeout_(std::chrono::seconds(10)) {
}

bool SessionGroup::same_address(const wish::NetAddress& lhs, const wish::NetAddress& rhs) {
    return lhs.port == rhs.port && lhs.ip == rhs.ip;
}

ClientSession* SessionGroup::add_client(const wish::NetAddress& addr, time_point now) {
    // If the client already exists, update their last_seen and return.
    if (auto* existing = find_client(addr)) {
        existing->last_seen = now;
        return existing;
    }

    if (is_full()) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Group full, rejecting client " + addr.ip);
        return nullptr;
    }

    clients_.push_back(ClientSession {});
    ClientSession& client = clients_.back();
    client.address = addr;
    client.identity = addr.ip + ":" + std::to_string(addr.port);
    client.connection_state = ClientConnectionState::PendingAdmission;
    client.last_seen = now;

    wish::log_debug_cat(WISH_LOG_CATEGORY,
        "Client added: " + client.identity +
        " (group=" + std::to_string(group_id_) +
        ", count=" + std::to_string(clients_.size()) + ")");
    return &client;
}

bool SessionGroup::remove_client(const wish::NetAddress& addr) {
    const auto it = std::find_if(clients_.begin(), clients_.end(), [&](const ClientSession& client) {
        return same_address(client.address, addr);
    });

    if (it == clients_.end()) {
        return false;
    }

    const std::string id = it->identity;
    clients_.erase(it);

    wish::log_debug_cat(WISH_LOG_CATEGORY,
        "Client removed: " + id +
        " (group=" + std::to_string(group_id_) +
        ", count=" + std::to_string(clients_.size()) + ")");
    return true;
}

ClientSession* SessionGroup::find_client(const wish::NetAddress& addr) {
    const auto it = std::find_if(clients_.begin(), clients_.end(), [&](const ClientSession& client) {
        return same_address(client.address, addr);
    });
    return it == clients_.end() ? nullptr : &(*it);
}

wish::u32 SessionGroup::client_count() const {
    return static_cast<wish::u32>(clients_.size());
}

wish::u32 SessionGroup::connected_count() const {
    return static_cast<wish::u32>(std::count_if(clients_.begin(), clients_.end(), [](const ClientSession& client) {
        return client.connection_state == ClientConnectionState::Connected;
    }));
}

bool SessionGroup::is_full() const {
    return clients_.size() >= max_clients_;
}

void SessionGroup::tick(time_point now) {
    const auto before = clients_.size();
    clients_.erase(std::remove_if(clients_.begin(), clients_.end(), [&](ClientSession& client) {
                       if (now - client.last_seen <= disconnect_timeout_) {
                           return false;
                       }
                       client.connection_state = ClientConnectionState::TimedOut;
                       return true;
                   }),
                   clients_.end());
    const auto pruned = before - clients_.size();
    if (pruned > 0) {
        wish::log_info_cat(WISH_LOG_CATEGORY,
            "Pruned " + std::to_string(pruned) + " timed-out client(s) from group " +
            std::to_string(group_id_));
    }
}

} // namespace wish::session
