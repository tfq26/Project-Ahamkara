#pragma once

#include "wish/session/session_runtime.h"
#include "wish/types.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace wish::session {

/// Manages a group of client sessions under a single activity.
/// Each group has a unique group_id and tracks:
/// - Multiple ClientSession entries
/// - Group state (Lobby, Active, Ended)
/// - Max players, current players
/// - Activity ID binding
class SessionGroup {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    SessionGroup(wish::u32 group_id, wish::u32 max_clients = 8);

    // Client management
    ClientSession* add_client(const wish::NetAddress& addr, time_point now);
    bool remove_client(const wish::NetAddress& addr);
    ClientSession* find_client(const wish::NetAddress& addr);

    // Group state
    [[nodiscard]] wish::u32 client_count() const;
    [[nodiscard]] wish::u32 connected_count() const;
    [[nodiscard]] bool is_full() const;

    // Group-level operations
    void tick(time_point now); // prune timeouts, etc.

    // Enumeration
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

    [[nodiscard]] wish::u32 group_id() const {
        return group_id_;
    }
    [[nodiscard]] wish::u32 max_clients() const {
        return max_clients_;
    }

  private:
    static bool same_address(const wish::NetAddress& lhs, const wish::NetAddress& rhs);

    wish::u32 group_id_;
    wish::u32 max_clients_;
    std::vector<ClientSession> clients_;
    clock::duration disconnect_timeout_;
};

} // namespace wish::session
