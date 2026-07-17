#pragma once

#include "wish/types.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace wish::admin {

namespace detail {

using clock = std::chrono::steady_clock;

} // namespace detail

struct ServerInfo {
    std::string id;
    std::string address;
    wish::u16 port;
    detail::clock::time_point last_heartbeat;
    bool alive;
};

class HeartbeatService {
  public:
    using clock = detail::clock;

    explicit HeartbeatService(clock::duration timeout = std::chrono::seconds(30));

    // Register or update a server's heartbeat
    void report_heartbeat(const std::string& server_id,
                          const std::string& address,
                          wish::u16 port);

    // Get list of registered servers
    [[nodiscard]] std::vector<ServerInfo> get_servers() const;

    // Check if a specific server is alive
    [[nodiscard]] bool is_alive(const std::string& server_id) const;

    // Prune dead servers
    void prune_dead_servers();

  private:
    clock::duration timeout_;
    std::unordered_map<std::string, ServerInfo> servers_;
};

} // namespace wish::admin
