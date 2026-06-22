#pragma once

#include "ae/core/types.h"
#include "ae/network/packet_envelope.h"
#include "wish/core/session_services.h"
#include "wish/session/session_model.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace wish::core {

using ActivityId = ae::u32;

enum class ActivityCategory : ae::u8 {
    PvP        = 0,
    PvE        = 1,
    PvEvP      = 2,
    Social     = 3,
    Custom     = 4
};

constexpr std::string_view activity_category_name(ActivityCategory cat) {
    switch (cat) {
        case ActivityCategory::PvP:    return "PvP";
        case ActivityCategory::PvE:    return "PvE";
        case ActivityCategory::PvEvP:  return "PvEvP";
        case ActivityCategory::Social: return "Social";
        case ActivityCategory::Custom: return "Custom";
    }
    return "Unknown";
}

struct ActivityConfig {
    ActivityId       id {0};
    std::string_view name {};
    ActivityCategory category {ActivityCategory::PvP};
    ae::u32          max_players {8};
    float            tick_rate {60.0F};
    std::string_view map_path {};
    bool             allow_spectators {false};
};

/// Non-templated base so ActivityManager can store any activity type.
/// Snapshot serialization is delegated to the activity itself via
/// build_snapshot_bytes(), keeping the wire format activity-controlled.
struct IActivityBase {
    virtual ~IActivityBase() = default;

    virtual bool       initialize(const ActivityConfig& cfg) = 0;
    virtual void       shutdown() = 0;

    virtual bool       admit_player(const SessionAdmissionRequest& req) = 0;
    virtual void       remove_player(session::SessionId sid) = 0;
    virtual ae::u32    player_count() const = 0;

    virtual void       tick(float dt) = 0;

    virtual void       process_input(session::SessionId sid,
                                     const ae::PacketEnvelope& envelope,
                                     ae::u32 command_sequence) = 0;

    /// Build a per-client snapshot into the provided buffer.
    /// The buffer receives the complete wire packet (header + envelope + activity payload).
    /// Returns bytes written (0 on failure).
    virtual ae::usize  build_snapshot_bytes(session::SessionId sid,
                                            std::span<std::byte> buffer) = 0;

    virtual bool       is_complete() const { return false; }

    virtual ActivityId       activity_id() const = 0;
    virtual ActivityCategory category() const = 0;
    virtual std::string_view activity_name() const = 0;

    /// Call fn(ctx, sid, data, len) for each connected client that needs
    /// a snapshot sent. data points into an internal buffer — the caller
    /// must copy/send before the next call to this method.
    virtual void       for_each_connected_snapshot(
                           void (*fn)(void* ctx, session::SessionId sid,
                                      const std::byte* data, ae::usize len),
                           void* ctx) = 0;
};

}  // namespace wish::core
