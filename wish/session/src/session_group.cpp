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
        wish::log_debug_cat(WISH_LOG_CATEGORY, "Updated last_seen for existing client " + addr.ip + ":" + std::to_string(addr.port));
        return existing;
    }

    if (is_full()) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Cannot add client " + addr.ip + ":" + std::to_string(addr.port) +
                              " — group " + std::to_string(group_id_) + " is full (" + std::to_string(max_clients_) + " max)");
        return nullptr;
    }

    clients_.push_back(ClientSession {});
    ClientSession& client = clients_.back();
    client.address = addr;
    client.identity = addr.ip + ":" + std::to_string(addr.port);
    client.connection_state = ClientConnectionState::PendingAdmission;
    client.last_seen = now;
    wish::log_info_cat(WISH_LOG_CATEGORY, "Added client " + client.identity + " to group " + std::to_string(group_id_) +
                       " (total=" + std::to_string(client_count()) + ")");
    return &client;
}

bool SessionGroup::remove_client(const wish::NetAddress& addr) {
    const auto it = std::find_if(clients_.begin(), clients_.end(), [&](const ClientSession& client) {
        return same_address(client.address, addr);
    });

    if (it == clients_.end()) {
        wish::log_debug_cat(WISH_LOG_CATEGORY, "remove_client: " + addr.ip + ":" + std::to_string(addr.port) + " not found");
        return false;
    }

    const std::string removed_identity = it->identity;
    clients_.erase(it);
    wish::log_info_cat(WISH_LOG_CATEGORY, "Removed client " + removed_identity +
                       " from group " + std::to_string(group_id_) +
                       " (remaining=" + std::to_string(client_count()) + ")");
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
                       wish::log_info_cat(WISH_LOG_CATEGORY, "Client " + client.identity +
                                          " timed out (group " + std::to_string(group_id_) + ")");
                       client.connection_state = ClientConnectionState::TimedOut;
                       return true;
                   }),
                   clients_.end());
    const auto removed = before - clients_.size();
    if (removed > 0) {
        wish::log_debug_cat(WISH_LOG_CATEGORY, "Tick pruned " + std::to_string(removed) +
                            " timed-out client(s) from group " + std::to_string(group_id_));
    }
}

} // namespace wish::session
