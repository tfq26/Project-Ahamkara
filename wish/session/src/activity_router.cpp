#include "wish/session/activity_router.h"

namespace wish::session {

// ── Session factory ────────────────────────────────────────────────────────

ActivitySession* ActivityRouter::create_session(wish::u32 id,
                                                 const ActivitySessionConfig& config) {
    if (sessions_.find(id) != sessions_.end()) {
        return nullptr;  // already exists
    }

    auto session = std::make_unique<ActivitySession>(id, config);
    auto* ptr = session.get();
    sessions_[id] = std::move(session);
    return ptr;
}

ActivitySession* ActivityRouter::create_session(wish::u32 id,
                                                 wish::core::ActivityCategory category,
                                                 wish::u32 max_players) {
    ActivitySessionConfig config;
    config.category = category;
    config.max_players = max_players;
    return create_session(id, config);
}

// ── Lookup ─────────────────────────────────────────────────────────────────

ActivitySession* ActivityRouter::find_session(wish::u32 id) {
    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

const ActivitySession* ActivityRouter::find_session(wish::u32 id) const {
    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

std::vector<ActivitySession*> ActivityRouter::find_sessions_by_category(wish::core::ActivityCategory category) {
    std::vector<ActivitySession*> result;
    for (auto& [id, session] : sessions_) {
        if (session->category() == category) {
            result.push_back(session.get());
        }
    }
    return result;
}

std::vector<ActivitySession*> ActivityRouter::find_sessions_by_state(ActivitySessionState state) {
    std::vector<ActivitySession*> result;
    for (auto& [id, session] : sessions_) {
        if (session->state() == state) {
            result.push_back(session.get());
        }
    }
    return result;
}

ActivitySession* ActivityRouter::find_available_lobby(wish::core::ActivityCategory category) const {
    for (const auto& [id, session] : sessions_) {
        if (session->state() == ActivitySessionState::Lobby &&
            session->category() == category &&
            !session->is_full()) {
            return session.get();
        }
    }
    return nullptr;
}

// ── Routing ────────────────────────────────────────────────────────────────

bool ActivityRouter::route_client(const wish::NetAddress& addr,
                                   wish::core::ActivityCategory category,
                                   time_point now,
                                   ActivitySession** out_session) {
    // Try to find an existing lobby that matches and has room
    auto* session = find_available_lobby(category);

    // If no suitable lobby exists, create one with a default config
    if (session == nullptr) {
        // Generate a unique id based on category and time
        wish::u32 new_id = static_cast<wish::u32>(
            static_cast<wish::u32>(category) + 1) * 1000 +
            static_cast<wish::u32>(sessions_.size() + 1);

        ActivitySessionConfig config;
        config.category = category;
        session = create_session(new_id, config);
        if (session == nullptr) {
            return false;
        }

        if (!session->start_lobby(now)) {
            remove_session(new_id);
            return false;
        }
    }

    auto* client = session->add_client(addr, now);
    if (client == nullptr) {
        return false;
    }

    if (out_session != nullptr) {
        *out_session = session;
    }
    return true;
}

// ── Removal ────────────────────────────────────────────────────────────────

bool ActivityRouter::remove_session(wish::u32 id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return false;
    }
    sessions_.erase(it);
    return true;
}

// ── Global operations ──────────────────────────────────────────────────────

void ActivityRouter::tick_all(float dt, time_point now) {
    // Collect ids to avoid iterator invalidation during removal
    std::vector<wish::u32> to_remove;
    for (auto& [id, session] : sessions_) {
        session->tick(dt, now);

        // Auto-remove completed/cancelled sessions
        if (session->state() == ActivitySessionState::Completed ||
            session->state() == ActivitySessionState::Cancelled) {
            to_remove.push_back(id);
        }
    }
    for (auto id : to_remove) {
        sessions_.erase(id);
    }
}

wish::u32 ActivityRouter::count_by_state(ActivitySessionState state) const {
    wish::u32 count = 0;
    for (const auto& [id, session] : sessions_) {
        if (session->state() == state) {
            ++count;
        }
    }
    return count;
}

wish::u32 ActivityRouter::count_by_category(wish::core::ActivityCategory category) const {
    wish::u32 count = 0;
    for (const auto& [id, session] : sessions_) {
        if (session->category() == category) {
            ++count;
        }
    }
    return count;
}

}  // namespace wish::session
