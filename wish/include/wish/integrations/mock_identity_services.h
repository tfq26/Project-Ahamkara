#pragma once

#include "wish/core/identity_service.h"
#include "wish/core/invite_service.h"
#include "wish/core/roster_service.h"
#include "wish/core/validation_telemetry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wish::integrations {

// ---------------------------------------------------------------------------
// NoopIdentityService — accepts everything, returns synthetic identities
// ---------------------------------------------------------------------------
class NoopIdentityService final : public wish::core::IdentityService {
public:
    [[nodiscard]] wish::core::IdentityResult resolve(
        const wish::core::IdentityRequest& request) const override {
        wish::core::IdentityResult result {};
        result.found = true;
        result.identity.player_id = "player-" + (request.remote_endpoint.empty()
                                                      ? "unknown"
                                                      : request.remote_endpoint);
        result.identity.display_name = "Player";
        result.identity.realm = "local";
        result.identity.created_at = std::chrono::system_clock::now();
        return result;
    }

    [[nodiscard]] wish::core::IdentityResult lookup_by_id(
        const std::string& player_id) const override {
        wish::core::IdentityResult result {};
        result.found = true;
        result.identity.player_id = player_id;
        result.identity.display_name = "Player-" + player_id.substr(0, 8);
        result.identity.realm = "local";
        result.identity.created_at = std::chrono::system_clock::now();
        return result;
    }

    [[nodiscard]] std::vector<wish::core::IdentityResult> lookup_batch(
        const std::vector<std::string>& player_ids) const override {
        std::vector<wish::core::IdentityResult> results;
        results.reserve(player_ids.size());
        for (const auto& pid : player_ids) {
            results.push_back(lookup_by_id(pid));
        }
        return results;
    }
};

// ---------------------------------------------------------------------------
// NoopRosterService — in-memory roster with no backend
// ---------------------------------------------------------------------------
class NoopRosterService final : public wish::core::RosterService {
public:
    [[nodiscard]] wish::core::RosterResult get_roster(
        const std::string& player_id) const override {
        wish::core::RosterResult result {};
        result.ok = true;
        auto it = rosters_.find(player_id);
        if (it != rosters_.end()) {
            result.entries = it->second;
        }
        return result;
    }

    [[nodiscard]] wish::core::RosterResult get_online_roster(
        const std::string& player_id) const override {
        wish::core::RosterResult result {};
        result.ok = true;
        auto it = rosters_.find(player_id);
        if (it != rosters_.end()) {
            for (const auto& entry : it->second) {
                if (entry.status != wish::core::PresenceStatus::Offline) {
                    result.entries.push_back(entry);
                }
            }
        }
        return result;
    }

    [[nodiscard]] wish::core::RosterResult add_entry(
        const std::string& owner_id,
        const std::string& target_player_id) override {
        wish::core::RosterResult result {};

        // Check not blocked
        if (is_blocked(owner_id, target_player_id)) {
            result.ok = false;
            result.error_message = "target player is blocked";
            return result;
        }

        // Check not already present
        auto& roster = rosters_[owner_id];
        for (const auto& e : roster) {
            if (e.player_id == target_player_id) {
                result.ok = false;
                result.error_message = "already in roster";
                return result;
            }
        }

        wish::core::RosterEntry entry {};
        entry.player_id = target_player_id;
        entry.display_name = "Player-" + target_player_id.substr(0, 8);
        entry.status = wish::core::PresenceStatus::Online;
        entry.is_friend = true;
        roster.push_back(entry);

        result.ok = true;
        result.entries = roster;
        return result;
    }

    [[nodiscard]] wish::core::RosterResult remove_entry(
        const std::string& owner_id,
        const std::string& target_player_id) override {
        wish::core::RosterResult result {};
        auto& roster = rosters_[owner_id];
        auto it = std::remove_if(roster.begin(), roster.end(),
            [&](const wish::core::RosterEntry& e) {
                return e.player_id == target_player_id;
            });
        if (it != roster.end()) {
            roster.erase(it, roster.end());
            result.ok = true;
        } else {
            result.ok = false;
            result.error_message = "entry not found";
        }
        result.entries = roster;
        return result;
    }

    [[nodiscard]] wish::core::RosterResult block_player(
        const std::string& owner_id,
        const std::string& target_player_id) override {
        // Remove from roster first (ignore result — may or may not exist)
        (void)remove_entry(owner_id, target_player_id);

        // Add to blocked set
        blocked_[owner_id].insert(target_player_id);

        wish::core::RosterResult result {};
        result.ok = true;
        result.entries = rosters_[owner_id];
        return result;
    }

    [[nodiscard]] wish::core::RosterResult unblock_player(
        const std::string& owner_id,
        const std::string& target_player_id) override {
        wish::core::RosterResult result {};
        auto it = blocked_.find(owner_id);
        if (it != blocked_.end()) {
            it->second.erase(target_player_id);
            result.ok = true;
        } else {
            result.ok = false;
            result.error_message = "not blocked";
        }
        result.entries = rosters_[owner_id];
        return result;
    }

    [[nodiscard]] bool is_blocked(const std::string& owner_id,
                                   const std::string& target_player_id) const override {
        auto it = blocked_.find(owner_id);
        if (it != blocked_.end()) {
            return it->second.find(target_player_id) != it->second.end();
        }
        return false;
    }

private:
    mutable std::unordered_map<std::string, std::vector<wish::core::RosterEntry>> rosters_;
    std::unordered_map<std::string, std::unordered_set<std::string>> blocked_;
};

// ---------------------------------------------------------------------------
// NoopInviteService — in-memory invite management
// ---------------------------------------------------------------------------
class NoopInviteService final : public wish::core::InviteService {
public:
    [[nodiscard]] wish::core::InviteResult send_invite(
        const wish::core::SendInviteRequest& request) override {
        wish::core::InviteResult result {};

        // Check for duplicate pending invite
        for (const auto& [id, inv] : invites_) {
            if (inv.status == wish::core::InviteStatus::Pending &&
                inv.from_player_id == request.from_player_id &&
                inv.to_player_id == request.to_player_id &&
                inv.activity_type == request.activity_type) {
                result.ok = false;
                result.error_message = "pending invite already exists";
                return result;
            }
        }

        // Check for a reverse pending invite (auto-accept mutual invites)
        for (const auto& [id, inv] : invites_) {
            if (inv.status == wish::core::InviteStatus::Pending &&
                inv.from_player_id == request.to_player_id &&
                inv.to_player_id == request.from_player_id &&
                inv.activity_type == request.activity_type) {
                // Auto-accept both
                auto now = std::chrono::steady_clock::now();
                auto& existing = invites_[id];
                existing.status = wish::core::InviteStatus::Accepted;

                wish::core::Invite new_inv {};
                new_inv.invite_id = next_id_++;
                new_inv.from_player_id = request.from_player_id;
                new_inv.to_player_id = request.to_player_id;
                new_inv.activity_type = request.activity_type;
                new_inv.party_id = request.party_id;
                new_inv.status = wish::core::InviteStatus::Accepted;
                new_inv.created_at = now;
                new_inv.expires_at = now + request.ttl;
                invites_[new_inv.invite_id] = new_inv;

                result.ok = true;
                result.invite = new_inv;
                return result;
            }
        }

        auto now = std::chrono::steady_clock::now();
        wish::core::Invite invite {};
        invite.invite_id = next_id_++;
        invite.from_player_id = request.from_player_id;
        invite.to_player_id = request.to_player_id;
        invite.activity_type = request.activity_type;
        invite.party_id = request.party_id;
        invite.status = wish::core::InviteStatus::Pending;
        invite.created_at = now;
        invite.expires_at = now + request.ttl;
        invites_[invite.invite_id] = invite;

        result.ok = true;
        result.invite = invite;
        return result;
    }

    [[nodiscard]] wish::core::InviteResult accept_invite(
        const wish::core::RespondInviteRequest& request) override {
        wish::core::InviteResult result {};
        auto it = invites_.find(request.invite_id);
        if (it == invites_.end()) {
            result.ok = false;
            result.error_message = "invite not found";
            return result;
        }
        if (it->second.to_player_id != request.player_id) {
            result.ok = false;
            result.error_message = "not the intended recipient";
            return result;
        }
        if (it->second.status != wish::core::InviteStatus::Pending) {
            result.ok = false;
            result.error_message = "invite is not pending";
            return result;
        }
        if (it->second.expires_at < std::chrono::steady_clock::now()) {
            it->second.status = wish::core::InviteStatus::Expired;
            result.ok = false;
            result.error_message = "invite has expired";
            return result;
        }
        it->second.status = wish::core::InviteStatus::Accepted;
        result.ok = true;
        result.invite = it->second;
        return result;
    }

    [[nodiscard]] wish::core::InviteResult reject_invite(
        const wish::core::RespondInviteRequest& request) override {
        wish::core::InviteResult result {};
        auto it = invites_.find(request.invite_id);
        if (it == invites_.end()) {
            result.ok = false;
            result.error_message = "invite not found";
            return result;
        }
        it->second.status = wish::core::InviteStatus::Rejected;
        result.ok = true;
        result.invite = it->second;
        return result;
    }

    [[nodiscard]] wish::core::InviteResult cancel_invite(
        const std::string& invite_id,
        const std::string& player_id) override {
        wish::core::InviteResult result {};
        auto it = invites_.find(invite_id);
        if (it == invites_.end()) {
            result.ok = false;
            result.error_message = "invite not found";
            return result;
        }
        if (it->second.from_player_id != player_id) {
            result.ok = false;
            result.error_message = "only the sender can cancel";
            return result;
        }
        it->second.status = wish::core::InviteStatus::Cancelled;
        result.ok = true;
        result.invite = it->second;
        return result;
    }

    [[nodiscard]] std::vector<wish::core::Invite> get_pending_invites(
        const std::string& player_id) const override {
        std::vector<wish::core::Invite> result;
        for (const auto& [id, inv] : invites_) {
            if (inv.status == wish::core::InviteStatus::Pending &&
                (inv.from_player_id == player_id || inv.to_player_id == player_id)) {
                result.push_back(inv);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<wish::core::Invite> get_outgoing_invites(
        const std::string& player_id) const override {
        std::vector<wish::core::Invite> result;
        for (const auto& [id, inv] : invites_) {
            if (inv.from_player_id == player_id) {
                result.push_back(inv);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<wish::core::Invite> get_incoming_invites(
        const std::string& player_id) const override {
        std::vector<wish::core::Invite> result;
        for (const auto& [id, inv] : invites_) {
            if (inv.to_player_id == player_id) {
                result.push_back(inv);
            }
        }
        return result;
    }

    [[nodiscard]] bool has_pending_invite(
        const std::string& from_player_id,
        const std::string& to_player_id,
        const std::string& activity_type) const override {
        for (const auto& [id, inv] : invites_) {
            if (inv.status == wish::core::InviteStatus::Pending &&
                inv.from_player_id == from_player_id &&
                inv.to_player_id == to_player_id &&
                inv.activity_type == activity_type) {
                return true;
            }
        }
        return false;
    }

    void expire_stale_invites() override {
        auto now = std::chrono::steady_clock::now();
        for (auto& [id, inv] : invites_) {
            if (inv.status == wish::core::InviteStatus::Pending &&
                inv.expires_at < now) {
                inv.status = wish::core::InviteStatus::Expired;
            }
        }
    }

private:
    std::unordered_map<std::string, wish::core::Invite> invites_;
    std::uint64_t next_id_ {1};
};

// ---------------------------------------------------------------------------
// NoopValidationTelemetry — in-memory ring buffer for validation events
// ---------------------------------------------------------------------------
class NoopValidationTelemetry final : public wish::core::ValidationTelemetry {
public:
    static constexpr std::size_t kMaxEvents = 256;

    void report_event(const wish::core::ValidationEvent& event) override {
        auto& ev = events_[write_pos_];
        ev = event;
        ev.event_id = next_event_id_++;
        if (!ev.timestamp.time_since_epoch().count()) {
            ev.timestamp = std::chrono::steady_clock::now();
        }
        write_pos_ = (write_pos_ + 1) % kMaxEvents;
        if (count_ < kMaxEvents) {
            ++count_;
        } else {
            // Overwriting oldest; advance read position implicitly
            read_pos_ = (read_pos_ + 1) % kMaxEvents;
        }

        ++total_events_;
        switch (event.severity) {
        case wish::core::ValidationSeverity::Warning:
            ++warning_count_;
            break;
        case wish::core::ValidationSeverity::Error:
            ++error_count_;
            break;
        case wish::core::ValidationSeverity::Critical:
            ++critical_count_;
            break;
        default:
            break;
        }
    }

    void report(wish::core::ValidationCategory category,
                wish::core::ValidationSeverity severity,
                const std::string& player_id,
                const std::string& session_id,
                const std::string& detail) override {
        wish::core::ValidationEvent event {};
        event.category = category;
        event.severity = severity;
        event.player_id = player_id;
        event.session_id = session_id;
        event.detail = detail;
        event.timestamp = std::chrono::steady_clock::now();
        report_event(event);
    }

    [[nodiscard]] std::vector<wish::core::ValidationEvent> recent_events(
        std::size_t max_count) const override {
        return collect_sorted(max_count);
    }

    [[nodiscard]] std::vector<wish::core::ValidationEvent> events_by_category(
        wish::core::ValidationCategory category,
        std::size_t max_count) const override {
        auto all = collect_sorted(max_count);
        std::vector<wish::core::ValidationEvent> filtered;
        for (const auto& ev : all) {
            if (ev.category == category) {
                filtered.push_back(ev);
                if (filtered.size() >= max_count) break;
            }
        }
        return filtered;
    }

    [[nodiscard]] std::vector<wish::core::ValidationEvent> events_by_player(
        const std::string& player_id,
        std::size_t max_count) const override {
        auto all = collect_sorted(max_count);
        std::vector<wish::core::ValidationEvent> filtered;
        for (const auto& ev : all) {
            if (ev.player_id == player_id) {
                filtered.push_back(ev);
                if (filtered.size() >= max_count) break;
            }
        }
        return filtered;
    }

    [[nodiscard]] wish::core::ValidationTelemetrySnapshot snapshot() override {
        wish::core::ValidationTelemetrySnapshot snap {};
        snap.events = collect_sorted(64);
        snap.total_events = total_events_;
        snap.critical_count = critical_count_;
        snap.warning_count = warning_count_;
        snap.error_count = error_count_;
        snap.timestamp = std::chrono::steady_clock::now();

        // Reset counters
        total_events_ = 0;
        critical_count_ = 0;
        warning_count_ = 0;
        error_count_ = 0;

        return snap;
    }

    [[nodiscard]] std::uint64_t total_event_count() const override {
        return total_events_;
    }

    [[nodiscard]] std::uint64_t severity_count(
        wish::core::ValidationSeverity severity) const override {
        switch (severity) {
        case wish::core::ValidationSeverity::Critical:
            return critical_count_;
        case wish::core::ValidationSeverity::Error:
            return error_count_;
        case wish::core::ValidationSeverity::Warning:
            return warning_count_;
        default:
            return 0;
        }
    }

    void clear() override {
        for (auto& ev : events_) {
            ev = wish::core::ValidationEvent {};
        }
        write_pos_ = 0;
        read_pos_ = 0;
        count_ = 0;
        total_events_ = 0;
        critical_count_ = 0;
        warning_count_ = 0;
        error_count_ = 0;
    }

private:
    /// Collect stored events in insertion order (newest first), up to max_count.
    [[nodiscard]] std::vector<wish::core::ValidationEvent> collect_sorted(
        std::size_t max_count) const {
        std::vector<wish::core::ValidationEvent> result;
        result.reserve(std::min(count_, max_count));

        if (count_ == 0) return result;

        // Walk from newest to oldest
        std::size_t idx = (write_pos_ == 0) ? kMaxEvents - 1 : write_pos_ - 1;
        for (std::size_t i = 0; i < count_ && result.size() < max_count; ++i) {
            if (events_[idx].event_id > 0) {
                result.push_back(events_[idx]);
            }
            idx = (idx == 0) ? kMaxEvents - 1 : idx - 1;
        }
        return result;
    }

    std::array<wish::core::ValidationEvent, kMaxEvents> events_ {};
    std::size_t write_pos_ {0};
    std::size_t read_pos_ {0};
    std::size_t count_ {0};
    std::uint64_t next_event_id_ {1};
    std::uint64_t total_events_ {0};
    std::uint64_t critical_count_ {0};
    std::uint64_t warning_count_ {0};
    std::uint64_t error_count_ {0};
};

} // namespace wish::integrations
