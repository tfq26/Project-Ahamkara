#pragma once

#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "wish/core/activity.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <span>
#include <string>

namespace ahamkara::game::activities {

/// Per-player state visible to nearby players in a social space.
struct NearbyPlayerState {
    ae::u32 player_id {0};
    Vec3    position {};
    float   yaw {0.0F};
    ae::u8  emote_id {0};
    ae::u8  movement_state {0};
};

/// Snapshot payload for social hubs — no combat data.
struct SocialSnapshot {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    ReplicatedPlayerState local_player {};
    ae::u8 nearby_count {0};
    NearbyPlayerState nearby_players[32] {};
    ae::u8 chat_message_count {0};
};

inline bool write_social_snapshot(detail::ByteWriter& writer, const SocialSnapshot& snap) {
    if (!writer.write(snap.server_tick)
        || !writer.write(snap.last_processed_input)
        || !write_player_state(writer, snap.local_player)
        || !writer.write(snap.nearby_count))
        return false;

    for (ae::u8 i = 0; i < snap.nearby_count && i < 32; ++i) {
        const auto& n = snap.nearby_players[i];
        if (!writer.write(n.player_id)
            || !write_vec3(writer, n.position)
            || !writer.write(n.yaw)
            || !writer.write(n.emote_id)
            || !writer.write(n.movement_state))
            return false;
    }

    return writer.write(snap.chat_message_count);
}

/// Social Hub — non-combat gathering space.
/// Players can move, emote, and chat. No health, weapons, or scoring.
class SocialHubActivity : public wish::core::IActivityBase {
public:
    SocialHubActivity();
    ~SocialHubActivity() override = default;

    bool initialize(const wish::core::ActivityConfig& cfg) override;
    void shutdown() override;

    bool admit_player(const wish::core::SessionAdmissionRequest& req) override;
    void remove_player(wish::session::SessionId sid) override;
    ae::u32 player_count() const override;

    void tick(float dt) override;
    void process_input(wish::session::SessionId sid,
                       const ae::PacketEnvelope& envelope,
                       ae::u32 command_sequence) override;

    ae::usize build_snapshot_bytes(wish::session::SessionId sid,
                                   std::span<std::byte> buffer) override;

    bool is_complete() const override { return false; }  // social spaces never end

    wish::core::ActivityId activity_id() const override { return config_.id; }
    wish::core::ActivityCategory category() const override {
        return wish::core::ActivityCategory::Social;
    }
    std::string_view activity_name() const override { return config_.name; }

    void for_each_connected_snapshot(
        void (*fn)(void* ctx, wish::session::SessionId sid,
                   const std::byte* data, ae::usize len),
        void* ctx) override;

private:
    struct Slot {
        ae::NetAddress address {};
        wish::session::SessionId session_id {};
        ae::SequenceTracker seq_tracker {};
        ae::u32 last_processed {0};
        ae::u32 last_received {0};
        ReplicatedPlayerState player_state {};
        bool connected {false};
        std::chrono::steady_clock::time_point last_seen {};
    };

    Slot* find_slot(wish::session::SessionId sid);
    void build_current_snapshot();

    wish::core::ActivityConfig config_;
    std::deque<Slot> slots_;
    wish::session::SessionId next_sid_ {1};
    ae::u32 server_tick_ {0};

    SocialSnapshot current_snapshot_;
    std::array<std::byte, 4096> snapshot_buffer_ {};
};

}  // namespace ahamkara::game::activities
