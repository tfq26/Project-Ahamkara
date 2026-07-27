#include "ahamkara/game/adapters/wish_session_adapter.h"

#include "ae/core/log.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace ahamkara::game::adapters {

// ===========================================================================
// Construction
// ===========================================================================

WishSessionAdapter::WishSessionAdapter(
    wish::core::ActivityManager& activity_mgr,
    wish::core::AuthValidator& auth_validator,
    wish::core::SessionAdmissionService& admission_service,
    wish::core::MatchResultReporter& result_reporter)
    : activity_mgr_(activity_mgr)
    , auth_validator_(auth_validator)
    , admission_service_(admission_service)
    , result_reporter_(result_reporter) {
}

// ===========================================================================
// Admission
// ===========================================================================

WishSessionAdapter::AdmissionResult WishSessionAdapter::admit_session(
    const std::string& player_id,
    const std::string& auth_token,
    const std::string& remote_endpoint,
    wish::core::ActivityId activity_id,
    time_point now) {

    AdmissionResult result {};

    // 1. Authenticate via the Wish auth validator.
    const wish::core::AuthResult auth = auth_validator_.validate(
        wish::core::AuthRequest{
            .token = auth_token,
            .remote_endpoint = remote_endpoint,
        });

    if (!auth.accepted) {
        result.error = make_service_error(
            wish::WishErrorCode::kAuthRejected,
            /*incident_id=*/{});
        result.error.message_key = "errors.auth.rejected";
        ae::log_warning(std::string("WishSessionAdapter: auth rejected for player=")
                        + player_id + " reason=" + auth.error_message);
        return result;
    }

    // 2. Admit through the session admission service.
    const wish::core::SessionAdmissionRequest admit_req{
        .player_id = auth.player_id,
        .session_id = auth.session_id,
        .remote_endpoint = remote_endpoint,
    };

    const wish::core::SessionAdmissionResult admit =
        admission_service_.admit(admit_req);

    if (!admit.admitted) {
        result.error = make_service_error(
            wish::WishErrorCode::kSessionAdmissionRejected,
            /*incident_id=*/{});
        result.error.message_key = "errors.admission.rejected";
        ae::log_warning(std::string("WishSessionAdapter: admission rejected for player=")
                        + player_id);
        return result;
    }

    // 3. Find the activity and admit the player into it.
    //    Each admission produces a unique session-to-player mapping —
    //    no first-slot shortcut.
    auto* activity = activity_mgr_.get_activity(activity_id);
    if (!activity) {
        result.error = make_service_error(
            wish::WishErrorCode::kActivityUnavailable,
            /*incident_id=*/{});
        result.error.message_key = "errors.activity.unavailable";
        ae::log_warning(std::string("WishSessionAdapter: activity ")
                        + std::to_string(activity_id) + " not found");
        return result;
    }

    const wish::core::SessionAdmissionRequest player_admit_req{
        .player_id = auth.player_id,
        .session_id = auth.session_id,
        .remote_endpoint = remote_endpoint,
    };

    if (!activity->admit_player(player_admit_req)) {
        result.error = make_service_error(
            wish::WishErrorCode::kCapacityExceeded,
            /*incident_id=*/{});
        result.error.message_key = "errors.capacity.exceeded";
        ae::log_warning(std::string("WishSessionAdapter: activity ")
                        + std::to_string(activity_id) + " rejected player="
                        + auth.player_id);
        return result;
    }

    // 4. Create a stable session entry.
    const wish::session::SessionId sid{
        .value = static_cast<std::uint64_t>(sessions_.size() + 1)
    };

    SessionEntry entry{};
    entry.session_id = sid;
    entry.activity_id = activity_id;
    entry.player_id = auth.player_id;
    entry.admitted = true;
    entry.admitted_at = now;
    entry.identity_token = auth_token;  // stable identity for reconnect guard
    sessions_.push_back(std::move(entry));

    // 5. Map the remote endpoint address to this session.
    AddressEntry addr_entry{};
    addr_entry.address = wish::NetAddress{};
    addr_entry.session_id = sid;
    addr_entry.last_seen = now;
    addr_entry.disconnected = false;

    // Parse remote_endpoint to extract address for routing
    // Format: "ip:port"
    const auto colon_pos = remote_endpoint.find(':');
    if (colon_pos != std::string::npos) {
        addr_entry.address.ip = remote_endpoint.substr(0, colon_pos);
        const auto port_str = remote_endpoint.substr(colon_pos + 1);
        try {
            addr_entry.address.port = static_cast<wish::u16>(std::stoi(std::string(port_str)));
        } catch (...) {
            addr_entry.address.port = 0;
        }
    } else {
        addr_entry.address.ip = remote_endpoint;
    }
    address_map_.push_back(std::move(addr_entry));

    // 6. Populate the result.
    result.admitted = true;
    result.activity_id = activity_id;
    result.session_id = sid;

    ae::log_info(std::string("WishSessionAdapter: admitted session ")
                 + std::to_string(sid.value) + " player=" + auth.player_id
                 + " activity=" + std::to_string(activity_id));

    return result;
}

bool WishSessionAdapter::remove_session(wish::session::SessionId sid) {
    // Look up the activity for this session before removing tracking state.
    wish::core::ActivityId activity_id = 0;
    {
        const auto* entry = find_session_entry(sid);
        if (entry) {
            activity_id = entry->activity_id;
        }
    }

    // Remove from address map.
    auto ait = std::remove_if(address_map_.begin(), address_map_.end(),
                               [sid](const AddressEntry& e) {
                                   return e.session_id.value == sid.value;
                               });
    address_map_.erase(ait, address_map_.end());

    // Remove from sessions.
    auto sit = std::remove_if(sessions_.begin(), sessions_.end(),
                               [sid](const SessionEntry& e) {
                                   return e.session_id.value == sid.value;
                               });
    bool found = (sit != sessions_.end());
    sessions_.erase(sit, sessions_.end());

    // Notify the activity to remove the player.
    if (activity_id != 0) {
        auto* act = activity_mgr_.get_activity(activity_id);
        if (act) {
            act->remove_player(sid);
        }
    }

    if (found) {
        ae::log_info(std::string("WishSessionAdapter: removed session ")
                     + std::to_string(sid.value));
    }
    return found;
}

// ===========================================================================
// Identity mapping
// ===========================================================================

WishSessionAdapter::SessionRouting WishSessionAdapter::resolve_routing(
    const wish::NetAddress& address) const {

    SessionRouting routing{};

    // Find by address in address_map_.
    for (const auto& ae : address_map_) {
        if (ae.address.ip == address.ip && ae.address.port == address.port) {
            routing.found = true;
            routing.session_id = ae.session_id;
            break;
        }
    }

    if (!routing.found) {
        return routing;
    }

    // Look up the activity for this session.
    for (const auto& se : sessions_) {
        if (se.session_id.value == routing.session_id.value) {
            routing.activity_id = se.activity_id;
            routing.found = true;
            return routing;
        }
    }

    // Session not found in active sessions (might be disconnected).
    routing.found = false;
    return routing;
}

// ===========================================================================
// Input routing
// ===========================================================================

bool WishSessionAdapter::route_input(
    const wish::NetAddress& from,
    const wish::PacketEnvelope& envelope,
    wish::u32 command_sequence,
    time_point now) {

    // 1. Resolve routing for this address.
    const auto routing = resolve_routing(from);
    if (!routing.found) {
        return false;
    }

    // 2. Get the activity.
    auto* activity = activity_mgr_.get_activity(routing.activity_id);
    if (!activity) {
        return false;
    }

    // 3. Route the input to the activity.
    activity->process_input(routing.session_id, envelope, command_sequence);

    // 4. Update last_seen in the address map.
    auto* addr_entry = find_address_entry(from);
    if (addr_entry) {
        addr_entry->last_seen = now;
    }

    return true;
}

// ===========================================================================
// Disconnect / reconnect
// ===========================================================================

void WishSessionAdapter::mark_disconnected(wish::session::SessionId sid, time_point now) {
    auto* entry = find_session_entry(sid);
    if (!entry) {
        return;
    }

    entry->disconnected_at = now;

    // Mark address as disconnected (preserves identity for grace period).
    auto* addr_entry = find_address_entry_by_session(sid);
    if (addr_entry) {
        addr_entry->disconnected = true;
    }

    // Notify the activity to remove the player slot.
    // The session entry remains in the adapter for reconnect grace period.
    auto* activity = activity_mgr_.get_activity(entry->activity_id);
    if (activity) {
        activity->remove_player(sid);
    }

    ae::log_info(std::string("WishSessionAdapter: session ")
                 + std::to_string(sid.value) + " disconnected (grace period started)");
}

bool WishSessionAdapter::handle_reconnect(
    wish::session::SessionId sid,
    const std::string& player_id,
    time_point now) {

    auto* entry = find_session_entry(sid);
    if (!entry) {
        ae::log_warning(std::string("WishSessionAdapter: reconnect failed — ")
                        + "session " + std::to_string(sid.value) + " not found");
        return false;
    }

    // ── Stale-identity guard ───────────────────────────────────────────
    // The reconnecting identity MUST match the original. This prevents a
    // stale/disconnected session from being hijacked by a different identity.
    if (entry->player_id != player_id) {
        ae::log_warning(std::string("WishSessionAdapter: reconnect rejected — ")
                        + "identity mismatch for session " + std::to_string(sid.value)
                        + " (expected=" + entry->player_id + ", got=" + player_id + ")");
        return false;
    }

    // Check grace period expiry.
    if (entry->disconnected_at != time_point{} &&
        (now - entry->disconnected_at) > grace_period_) {
        ae::log_warning(std::string("WishSessionAdapter: reconnect rejected — ")
                        + "grace period expired for session " + std::to_string(sid.value));
        return false;
    }

    // Clear the disconnected flag — session is active again.
    entry->disconnected_at = time_point{};

    // Update the address mapping.
    auto* addr_entry = find_address_entry_by_session(sid);
    if (addr_entry) {
        addr_entry->disconnected = false;
        addr_entry->last_seen = now;
    }

    ae::log_info(std::string("WishSessionAdapter: session ")
                 + std::to_string(sid.value) + " reconnected successfully");
    return true;
}

// ===========================================================================
// Result reporting
// ===========================================================================

void WishSessionAdapter::report_activity_results(wish::core::ActivityId activity_id) {
    for (const auto& se : sessions_) {
        if (se.activity_id == activity_id && se.admitted) {
            report_session_result(se.session_id);
        }
    }
}

void WishSessionAdapter::report_session_result(wish::session::SessionId sid) {
    const auto* entry = find_session_entry(sid);
    if (!entry || !entry->admitted) {
        return;
    }

    result_reporter_.report_match_result(wish::core::MatchResult{
        .match_id = std::to_string(entry->activity_id),
        .player_id = entry->player_id,
        .completed = true,
        .summary = "match completed",
    });

    ae::log_info(std::string("WishSessionAdapter: reported result for session ")
                 + std::to_string(sid.value));
}

// ===========================================================================
// Error envelope factories
// ===========================================================================

wish::ErrorEnvelope WishSessionAdapter::make_protocol_error(
    wish::WishErrorCode code,
    const std::string& incident_id) {

    wish::ErrorEnvelope env{};
    env.version = wish::kErrorEnvelopeVersion;
    env.error_code = static_cast<std::uint32_t>(code);
    env.incident_id = incident_id.empty() ? "NOC" : incident_id;
    env.retryable = false;

    switch (code) {
    case wish::WishErrorCode::kProtocolVersionMismatch:
        env.message_key = "errors.protocol.version_mismatch";
        break;
    case wish::WishErrorCode::kProtocolError:
        env.message_key = "errors.protocol.error";
        break;
    default:
        env.message_key = "errors.protocol.unknown";
        break;
    }

    return env;
}

wish::ErrorEnvelope WishSessionAdapter::make_service_error(
    wish::WishErrorCode code,
    const std::string& incident_id) {

    wish::ErrorEnvelope env{};
    env.version = wish::kErrorEnvelopeVersion;
    env.error_code = static_cast<std::uint32_t>(code);
    env.incident_id = incident_id.empty() ? "NOC" : incident_id;
    env.retryable = true;
    env.retry_after_seconds = 5;

    switch (code) {
    case wish::WishErrorCode::kBackendUnavailable:
        env.message_key = "errors.backend.unavailable";
        break;
    case wish::WishErrorCode::kBackendTimeout:
        env.message_key = "errors.backend.timeout";
        break;
    case wish::WishErrorCode::kCapacityExceeded:
        env.message_key = "errors.capacity.exceeded";
        break;
    default:
        env.message_key = "errors.service.unknown";
        break;
    }

    return env;
}

// ===========================================================================
// Internal helpers
// ===========================================================================

WishSessionAdapter::SessionEntry* WishSessionAdapter::find_session_entry(
    wish::session::SessionId sid) {
    for (auto& e : sessions_) {
        if (e.session_id.value == sid.value) {
            return &e;
        }
    }
    return nullptr;
}

const WishSessionAdapter::SessionEntry* WishSessionAdapter::find_session_entry(
    wish::session::SessionId sid) const {
    for (const auto& e : sessions_) {
        if (e.session_id.value == sid.value) {
            return &e;
        }
    }
    return nullptr;
}

WishSessionAdapter::SessionEntry* WishSessionAdapter::find_session_entry_by_player(
    const std::string& player_id) {
    for (auto& e : sessions_) {
        if (e.player_id == player_id) {
            return &e;
        }
    }
    return nullptr;
}

WishSessionAdapter::AddressEntry* WishSessionAdapter::find_address_entry(
    const wish::NetAddress& addr) {
    for (auto& ae : address_map_) {
        if (ae.address.ip == addr.ip && ae.address.port == addr.port) {
            return &ae;
        }
    }
    return nullptr;
}

WishSessionAdapter::AddressEntry* WishSessionAdapter::find_address_entry_by_session(
    wish::session::SessionId sid) {
    for (auto& ae : address_map_) {
        if (ae.session_id.value == sid.value) {
            return &ae;
        }
    }
    return nullptr;
}

}  // namespace ahamkara::game::adapters
