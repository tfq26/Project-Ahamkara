#define WISH_LOG_CATEGORY "Heartbeat"
#include "wish/admin/heartbeat_service.h"
#include "wish/log.h"

#include <algorithm>

namespace wish::admin {

HeartbeatService::HeartbeatService(clock::duration timeout)
    : timeout_(timeout) {}

void HeartbeatService::report_heartbeat(const std::string& server_id,
                                        const std::string& address,
                                        wish::u16 port) {
    const auto now = clock::now();
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        it->second.address = address;
        it->second.port = port;
        it->second.last_heartbeat = now;
        it->second.alive = true;
    } else {
        servers_[server_id] = ServerInfo {
            .id = server_id,
            .address = address,
            .port = port,
            .last_heartbeat = now,
            .alive = true};
    }
}

std::vector<ServerInfo> HeartbeatService::get_servers() const {
    std::vector<ServerInfo> result;
    result.reserve(servers_.size());
    const auto now = clock::now();

    for (const auto& [id, info] : servers_) {
        ServerInfo entry = info;
        entry.alive = (now - info.last_heartbeat) <= timeout_;
        result.push_back(std::move(entry));
    }

    return result;
}

bool HeartbeatService::is_alive(const std::string& server_id) const {
    const auto it = servers_.find(server_id);
    if (it == servers_.end()) {
        return false;
    }
    const auto now = clock::now();
    return (now - it->second.last_heartbeat) <= timeout_;
}

void HeartbeatService::prune_dead_servers() {
    const auto now = clock::now();
    std::size_t erased = 0;

    auto it = servers_.begin();
    while (it != servers_.end()) {
        if ((now - it->second.last_heartbeat) > timeout_) {
            it = servers_.erase(it);
            ++erased;
        } else {
            ++it;
        }
    }

    if (erased > 0) {
        wish::log_info_cat(WISH_LOG_CATEGORY,
            "pruned " + std::to_string(erased) + " dead server(s).");
    }
}

} // namespace wish::admin
