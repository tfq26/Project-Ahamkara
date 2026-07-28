#pragma once

/// @file wish_session_adapter.h
/// Flashback-owned adapter that composes the Ahamkara host with Wish
/// activity/session SDK.  Lives in Flashback and depends only on
/// installed/exported Ahamkara (ae_*) and Wish (wish_*) targets.
///
/// Architecture constraints (enforced by include graph):
///   - Wish imports NO Flashback command, snapshot, world, or player types.
///   - Ahamkara imports NO Wish session/activity types.
///   - This adapter bridges the gap, owning the mapping from Wish sessions
///     to Flashback player slots.

#include "wish/core/activity.h"
#include "wish/core/activity_manager.h"
#include "wish/core/error_envelope.h"
#include "wish/core/session_services.h"
#include "wish/session/session_model.h"
#include "wish/types.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace ahamkara::game::adapters {

// ---------------------------------------------------------------------------
// WishSessionAdapter
// ---------------------------------------------------------------------------

/// Composes the Wish activity/session SDK with Flashback game entities.
///
/// Responsibilities (maps 1:1 to the acceptance criteria):
///   - Admission via Wish session services → exact session-to-player mapping.
///   - Stable identity mapping (Wish session ID → Flashback player slot).
///   - Input routing from Wish PacketEnvelope to activity process_input().
///   - Recipient-relative snapshot dispatch via ActivityManager.
///   - Disconnect, reconnect (with stale-identity guard), and result reporting.
///   - Protocol/service failures reported as Wish ErrorEnvelope.
class WishSessionAdapter {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    /// Construct the adapter with the services it will compose.
    /// All pointers/references must remain valid for the adapter's lifetime.
    WishSessionAdapter(
        wish::core::ActivityManager& activity_mgr,
        wish::core::AuthValidator& auth_validator,
        wish::core::SessionAdmissionService& admission_service,
        wish::core::MatchResultReporter& result_reporter);

    // Non-copyable, non-movable (references internal state).
    WishSessionAdapter(const WishSessionAdapter&) = delete;
    WishSessionAdapter& operator=(const WishSessionAdapter&) = delete;
    WishSessionAdapter(WishSessionAdapter&&) = delete;
    WishSessionAdapter& operator=(WishSessionAdapter&&) = delete;

    // -----------------------------------------------------------------------
    // Admission — exact session-to-player mapping
    // -----------------------------------------------------------------------

    struct AdmissionResult {
        bool admitted {false};
        wish::core::ActivityId activity_id {0};
        wish::session::SessionId session_id {};
        wish::ErrorEnvelope error {};
    };

    /// Authenticate and admit a player into the target activity.
    /// Returns the exact session-to-player mapping.
    /// There is NO first-slot shortcut — each call produces an independent
    /// mapping through the full admission pipeline.
    AdmissionResult admit_session(
        const std::string& player_id,
        const std::string& auth_token,
        const std::string& remote_endpoint,
        wish::core::ActivityId activity_id,
        time_point now);

    /// Remove a session — called on disconnect / graceful removal.
    bool remove_session(wish::session::SessionId sid);

    // -----------------------------------------------------------------------
    // Identity mapping — find activity + session from a net address
    // -----------------------------------------------------------------------

    struct SessionRouting {
        bool found {false};
        wish::core::ActivityId activity_id {0};
        wish::session::SessionId session_id {};
    };

    /// Resolve the activity and session that owns a given net address.
    SessionRouting resolve_routing(const wish::NetAddress& address) const;

    /// Return the number of admitted (active) sessions.
    std::size_t active_session_count() const { return sessions_.size(); }

    // -----------------------------------------------------------------------
    // Input routing — from Wish envelope to activity
    // -----------------------------------------------------------------------

    /// Route an incoming input packet to the owning activity.
    /// Completes admission if the session is still pending.
    /// Returns true if the packet was consumed by an activity.
    bool route_input(
        const wish::NetAddress& from,
        const wish::PacketEnvelope& envelope,
        wish::u32 command_sequence,
        time_point now);

    // -----------------------------------------------------------------------
    // Snapshot dispatch — per-client via ActivityManager::broadcast_snapshots
    // -----------------------------------------------------------------------

    /// Convenience: broadcast snapshots for all running activities.
    /// The callback receives (session_id, data, len) — the caller is
    /// responsible for sending to the actual client address.
    template <typename Fn>
    void broadcast_snapshots(Fn&& fn) {
        activity_mgr_.broadcast_snapshots(std::forward<Fn>(fn));
    }

    // -----------------------------------------------------------------------
    // Tick — advance all activities
    // -----------------------------------------------------------------------

    void tick_all(float dt) { activity_mgr_.tick_all(dt); }

    // -----------------------------------------------------------------------
    // Disconnect / reconnect — stale-identity guard
    // -----------------------------------------------------------------------

    /// Mark a session as disconnected (graceful disconnect).
    /// Does NOT immediately remove — the identity is preserved briefly for
    /// reconnection but CANNOT transfer control to a different identity.
    void mark_disconnected(wish::session::SessionId sid, time_point now);

    /// Attempt to reconnect a session.
    /// Returns true if the session was found in the grace period AND the
    /// reconnecting identity matches the original.
    /// This prevents stale/dead identities from being hijacked.
    bool handle_reconnect(
        wish::session::SessionId sid,
        const std::string& player_id,
        time_point now);

    // -----------------------------------------------------------------------
    // Result reporting
    // -----------------------------------------------------------------------

    /// Report match results for all active sessions in a given activity.
    void report_activity_results(wish::core::ActivityId activity_id);

    /// Report result for a single session.
    void report_session_result(wish::session::SessionId sid);

    // -----------------------------------------------------------------------
    // Service access
    // -----------------------------------------------------------------------

    wish::core::ActivityManager& activity_manager() { return activity_mgr_; }
    const wish::core::ActivityManager& activity_manager() const { return activity_mgr_; }

    /// Build an ErrorEnvelope for protocol mismatch or service failure.
    static wish::ErrorEnvelope make_protocol_error(wish::WishErrorCode code,
                                                    const std::string& incident_id = {});

    static wish::ErrorEnvelope make_service_error(wish::WishErrorCode code,
                                                   const std::string& incident_id = {});

  private:
    // ── Internal session tracking ──────────────────────────────────────

    struct SessionEntry {
        wish::session::SessionId session_id {};
        wish::core::ActivityId activity_id {0};
        std::string player_id;
        bool admitted {false};
        time_point admitted_at {};
        time_point disconnected_at {};   // zero == still connected
        std::string identity_token;       // stable identity for reconnect guard
    };

    struct AddressEntry {
        wish::NetAddress address {};
        wish::session::SessionId session_id {};
        time_point last_seen {};
        bool disconnected {false};
    };

    SessionEntry* find_session_entry(wish::session::SessionId sid);
    const SessionEntry* find_session_entry(wish::session::SessionId sid) const;
    SessionEntry* find_session_entry_by_player(const std::string& player_id);
    AddressEntry* find_address_entry(const wish::NetAddress& addr);
    AddressEntry* find_address_entry_by_session(wish::session::SessionId sid);

    // ── Dependencies (non-owning) ──────────────────────────────────────

    wish::core::ActivityManager& activity_mgr_;
    wish::core::AuthValidator& auth_validator_;
    wish::core::SessionAdmissionService& admission_service_;
    wish::core::MatchResultReporter& result_reporter_;

    // ── State ──────────────────────────────────────────────────────────

    std::vector<SessionEntry> sessions_;
    std::vector<AddressEntry> address_map_;

    // Grace period for reconnection
    clock::duration grace_period_{std::chrono::seconds(10)};
};

}  // namespace ahamkara::game::adapters
