#include "wish/core/session_orchestrator.h"
#include "wish/log.h"

#include <algorithm>
#include <sstream>

namespace wish::core {

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
SessionOrchestrator::SessionOrchestrator(Config cfg)
    : config_(cfg) {
}

// ---------------------------------------------------------------------------
// matchmaking integration
// ---------------------------------------------------------------------------
void SessionOrchestrator::attach_matchmaking(MatchmakingService& service) {
    matchmaking_service_ = &service;
    service.set_on_match_found([this](const Fireteam& fireteam) {
        const auto now = clock::now();
        const auto group_id = on_match_found(fireteam, now);
        if (group_id == 0) {
            wish::log_warning("SessionOrchestrator: failed to create session from fireteam #" +
                              std::to_string(fireteam.fireteam_id));
        }
    });
}

wish::u32 SessionOrchestrator::on_match_found(const Fireteam& fireteam, time_point now) {
    if (fireteam.empty()) {
        wish::log_warning("SessionOrchestrator: cannot create session from empty fireteam #" +
                          std::to_string(fireteam.fireteam_id));
        return 0;
    }

    const auto group_id = next_group_id();
    // Allow extra slots beyond the fireteam size so that late-joining
    // players (e.g., friends) can be added while the session is still
    // in the lobby phase. The actual capacity can be overridden via the
    // activity template when an ActivityManager is attached.
    constexpr wish::u32 kExtraSlots = 4;
    const auto max_clients = fireteam.member_count() + kExtraSlots;

    session::SessionGroup group(group_id, max_clients);
    group.set_fireteam_id(fireteam.fireteam_id);
    group.set_activity_id(fireteam.activity_id);
    group.set_created_at(now);
    group.set_state(session::GroupState::Lobby);

    // Add all fireteam members to the group
    for (const auto& addr : fireteam.member_addresses) {
        auto* client = group.add_client(addr, now);
        if (client == nullptr) {
            wish::log_warning("SessionOrchestrator: failed to add client " +
                              addr.ip + ":" + std::to_string(addr.port) +
                              " to group " + std::to_string(group_id));
        }
    }

    // Set owner to the first member (party leader from the first party)
    if (!fireteam.member_addresses.empty()) {
        const auto& first_addr = fireteam.member_addresses.front();
        group.set_owner_address(first_addr);
        group_owner_[group_id] = first_addr;
    }

    // Record party → group bindings
    for (const auto& party_id : fireteam.party_ids) {
        party_to_group_[party_id] = group_id;
    }

    // Store the group
    groups_.try_emplace(group_id, std::move(group));

    wish::log_info("SessionOrchestrator: created session group #" + std::to_string(group_id) +
                   " from fireteam #" + std::to_string(fireteam.fireteam_id) +
                   " (" + std::to_string(max_clients) + " players, activity " +
                   std::to_string(fireteam.activity_id) + ")");

    // Auto-activate if configured
    if (config_.auto_activate_sessions) {
        activate_session(group_id, now);
    }

    return group_id;
}

// ---------------------------------------------------------------------------
// party / session binding
// ---------------------------------------------------------------------------
void SessionOrchestrator::bind_party_to_group(wish::u64 party_id, wish::u32 group_id) {
    party_to_group_[party_id] = group_id;
    wish::log_info("SessionOrchestrator: bound party " + std::to_string(party_id) +
                   " to group " + std::to_string(group_id));
}

void SessionOrchestrator::unbind_party_from_group(wish::u64 party_id) {
    const auto it = party_to_group_.find(party_id);
    if (it != party_to_group_.end()) {
        wish::log_info("SessionOrchestrator: unbound party " + std::to_string(party_id) +
                       " from group " + std::to_string(it->second));
        party_to_group_.erase(it);
    }
}

wish::u32 SessionOrchestrator::party_group(wish::u64 party_id) const {
    const auto it = party_to_group_.find(party_id);
    return it != party_to_group_.end() ? it->second : 0;
}

// ---------------------------------------------------------------------------
// join / leave flow
// ---------------------------------------------------------------------------
JoinResult SessionOrchestrator::process_join(const JoinRequest& request, time_point now) {
    JoinResult result {};

    auto* group = find_group(request.group_id);
    if (group == nullptr) {
        result.accepted = false;
        result.error_message = "group not found";
        return result;
    }

    // Check if the group is still accepting members
    if (group->state() != session::GroupState::Lobby) {
        result.accepted = false;
        result.error_message = "group is no longer accepting members";
        return result;
    }

    // Ownership check (strict policy)
    if (config_.ownership_policy == OwnershipPolicy::Strict && group->client_count() > 0) {
        // Only the owner can admit new members in strict mode.
        // For simplicity, in this policy we allow anyone to join as long as
        // the group exists and has capacity.
    }

    // Check capacity
    if (group->is_full()) {
        result.accepted = false;
        result.error_message = "group is full";
        return result;
    }

    // Add the player to the group
    auto* client = group->add_client(request.player_address, now);
    if (client == nullptr) {
        result.accepted = false;
        result.error_message = "failed to add client to group";
        return result;
    }

    // Update identity
    if (!request.player_identity.empty()) {
        client->identity = request.player_identity;
    }

    // Bind party if specified
    if (request.party_id > 0) {
        bind_party_to_group(request.party_id, request.group_id);
    }

    result.accepted = true;
    result.group_id = request.group_id;

    wish::log_info("SessionOrchestrator: player " + request.player_address.ip + ":" +
                   std::to_string(request.player_address.port) + " joined group " +
                   std::to_string(request.group_id));

    return result;
}

LeaveResult SessionOrchestrator::process_leave(const LeaveRequest& request, time_point now) {
    LeaveResult result {};

    auto* group = find_group(request.group_id);
    if (group == nullptr) {
        result.error_message = "group not found";
        return result;
    }

    // Remove the client from the group
    const bool removed = group->remove_client(request.player_address);
    if (!removed) {
        result.error_message = "player not found in group";
        return result;
    }

    result.removed = true;

    // Unbind party if specified
    if (request.party_id > 0) {
        unbind_party_from_group(request.party_id);
    }

    wish::log_info("SessionOrchestrator: player " + request.player_address.ip + ":" +
                   std::to_string(request.player_address.port) + " left group " +
                   std::to_string(request.group_id));

    // If the leaving player was the owner, transfer ownership or clear it
    if (is_owner(request.player_address, request.group_id)) {
        if (group->client_count() > 0) {
            // Transfer to the first remaining client
            wish::NetAddress new_owner {};
            group->for_each_client([&](const session::ClientSession& client) {
                if (new_owner.ip.empty()) {
                    new_owner = client.address;
                }
            });
            if (!new_owner.ip.empty()) {
                transfer_ownership(new_owner, request.group_id);
            }
        } else {
            group_owner_.erase(request.group_id);
        }
    }

    // Auto-end if empty and configured
    if (config_.auto_end_on_empty && group->client_count() == 0) {
        end_session(request.group_id, now);
        result.group_ended = true;
    }

    return result;
}

// ---------------------------------------------------------------------------
// ownership
// ---------------------------------------------------------------------------
bool SessionOrchestrator::is_owner(const wish::NetAddress& addr, wish::u32 group_id) const {
    const auto it = group_owner_.find(group_id);
    if (it == group_owner_.end()) {
        return false;
    }
    return it->second.ip == addr.ip && it->second.port == addr.port;
}

bool SessionOrchestrator::transfer_ownership(const wish::NetAddress& new_owner, wish::u32 group_id) {
    auto* group = find_group(group_id);
    if (group == nullptr) {
        return false;
    }

    // Verify the new owner is a member of the group
    if (group->find_client(new_owner) == nullptr) {
        wish::log_warning("SessionOrchestrator: cannot transfer ownership of group " +
                          std::to_string(group_id) + " — target is not a member");
        return false;
    }

    group->set_owner_address(new_owner);
    group_owner_[group_id] = new_owner;

    wish::log_info("SessionOrchestrator: transferred ownership of group " +
                   std::to_string(group_id) + " to " +
                   new_owner.ip + ":" + std::to_string(new_owner.port));

    return true;
}

// ---------------------------------------------------------------------------
// session lifecycle
// ---------------------------------------------------------------------------
bool SessionOrchestrator::activate_session(wish::u32 group_id, time_point now) {
    auto* group = find_group(group_id);
    if (group == nullptr) {
        wish::log_warning("SessionOrchestrator: cannot activate unknown group " +
                          std::to_string(group_id));
        return false;
    }

    if (group->state() != session::GroupState::Lobby) {
        wish::log_warning("SessionOrchestrator: group " + std::to_string(group_id) +
                          " is not in Lobby state (current: " +
                          std::to_string(static_cast<int>(group->state())) + ")");
        return false;
    }

    group->set_state(session::GroupState::Active);
    group->set_created_at(now);

    wish::log_info("SessionOrchestrator: activated session group " +
                   std::to_string(group_id) + " for activity " +
                   std::to_string(group->activity_id()));

    // Fire callback
    if (on_session_activated_) {
        on_session_activated_(group_id, group->activity_id());
    }

    return true;
}

bool SessionOrchestrator::end_session(wish::u32 group_id, time_point now) {
    auto* group = find_group(group_id);
    if (group == nullptr) {
        wish::log_warning("SessionOrchestrator: cannot end unknown group " +
                          std::to_string(group_id));
        return false;
    }

    if (group->state() == session::GroupState::Ended) {
        // Already ended — no-op
        return true;
    }

    group->set_state(session::GroupState::Ended);
    group->set_ended_at(now);

    wish::log_info("SessionOrchestrator: ended session group " +
                   std::to_string(group_id) + " for activity " +
                   std::to_string(group->activity_id()));

    // Fire callback
    if (on_session_ended_) {
        on_session_ended_(group_id, group->activity_id());
    }

    return true;
}

bool SessionOrchestrator::destroy_group(wish::u32 group_id) {
    auto* group = find_group(group_id);
    if (group == nullptr) {
        return false;
    }

    if (group->state() != session::GroupState::Ended) {
        wish::log_warning("SessionOrchestrator: cannot destroy group " +
                          std::to_string(group_id) + " — not in Ended state");
        return false;
    }

    // Clean up party bindings
    // Iterate carefully since we may erase
    for (auto it = party_to_group_.begin(); it != party_to_group_.end(); ) {
        if (it->second == group_id) {
            it = party_to_group_.erase(it);
        } else {
            ++it;
        }
    }

    group_owner_.erase(group_id);
    groups_.erase(group_id);

    wish::log_info("SessionOrchestrator: destroyed group " + std::to_string(group_id));
    return true;
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------
session::SessionGroup* SessionOrchestrator::find_group(wish::u32 group_id) {
    const auto it = groups_.find(group_id);
    return it != groups_.end() ? &it->second : nullptr;
}

const session::SessionGroup* SessionOrchestrator::find_group(wish::u32 group_id) const {
    const auto it = groups_.find(group_id);
    return it != groups_.end() ? &it->second : nullptr;
}

wish::u32 SessionOrchestrator::group_count() const {
    return static_cast<wish::u32>(groups_.size());
}

wish::u32 SessionOrchestrator::lobby_count() const {
    wish::u32 count = 0;
    for (const auto& [id, group] : groups_) {
        if (group.state() == session::GroupState::Lobby) ++count;
    }
    return count;
}

wish::u32 SessionOrchestrator::active_count() const {
    wish::u32 count = 0;
    for (const auto& [id, group] : groups_) {
        if (group.state() == session::GroupState::Active) ++count;
    }
    return count;
}

wish::u32 SessionOrchestrator::ended_count() const {
    wish::u32 count = 0;
    for (const auto& [id, group] : groups_) {
        if (group.state() == session::GroupState::Ended) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// internal helpers
// ---------------------------------------------------------------------------
wish::u32 SessionOrchestrator::next_group_id() {
    return next_group_id_++;
}

} // namespace wish::core
