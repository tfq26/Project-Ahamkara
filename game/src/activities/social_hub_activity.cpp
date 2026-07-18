#include "ahamkara/game/activities/social_hub_activity.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cstring>

namespace ahamkara::game::activities {

SocialHubActivity::SocialHubActivity() = default;

bool SocialHubActivity::initialize(const wish::core::ActivityConfig& cfg) {
    config_ = cfg;

    ae::log_info(std::string("Social hub '") + std::string(cfg.name)
                 + "' initialized (max_players=" + std::to_string(cfg.max_players)
                 + ", tick_rate=" + std::to_string(cfg.tick_rate) + ")");
    return true;
}

void SocialHubActivity::shutdown() {
    ae::log_info(std::string("Social hub '") + std::string(config_.name) + "' shutting down.");
}

bool SocialHubActivity::admit_player(const wish::core::SessionAdmissionRequest& req) {
    if (slots_.size() >= static_cast<std::size_t>(config_.max_players)) return false;

    Slot s {};
    s.session_id.value = next_sid_.value++;
    s.connected = true;
    s.last_seen = std::chrono::steady_clock::now();

    // Assign spawn position in a circle
    float angle = static_cast<float>(slots_.size()) * 0.8F;
    s.player_state.position = Vec3 {
        std::cos(angle) * 5.0F,
        2.0F,
        std::sin(angle) * 5.0F
    };
    s.player_state.health = 100.0F;
    s.player_state.shield = 0.0F;
    s.player_state.player_id = static_cast<ae::u32>(s.session_id.value);
    s.player_state.network_object_id = 1;

    slots_.push_back(std::move(s));
    return true;
}

void SocialHubActivity::remove_player(wish::session::SessionId sid) {
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
        [sid](const Slot& s) { return s.session_id.value == sid.value; }),
        slots_.end());
}

ae::u32 SocialHubActivity::player_count() const {
    return static_cast<ae::u32>(slots_.size());
}

void SocialHubActivity::tick(float dt) {
    server_tick_++;
    (void)dt;
}

void SocialHubActivity::process_input(wish::session::SessionId sid,
                                      const wish::PacketEnvelope& envelope,
                                      ae::u32 command_sequence) {
    Slot* slot = find_slot(sid);
    if (!slot) return;

    slot->seq_tracker.process_incoming(envelope);
    slot->last_received = command_sequence;
    slot->last_seen = std::chrono::steady_clock::now();
}

ae::usize SocialHubActivity::build_snapshot_bytes(wish::session::SessionId sid,
                                                   std::span<std::byte> buffer) {
    Slot* slot = find_slot(sid);
    if (!slot || !slot->connected) return 0;

    build_current_snapshot();
    current_snapshot_.last_processed_input = slot->last_processed;

    detail::ByteWriter writer(buffer);

    ae::u32 magic = kPacketMagic;
    ae::u16 version = kProtocolVersion;
    ae::u16 pkt_type = static_cast<ae::u16>(PacketType::ServerSnapshot);
    writer.write(magic);
    writer.write(version);
    writer.write(pkt_type);

    auto env = slot->seq_tracker.prepare_outgoing();
    writer.write(env.sequence);
    writer.write(env.ack_sequence);
    writer.write(env.ack_bitfield);

    ae::u16 activity_tag = 3;
    writer.write(activity_tag);

    if (!write_social_snapshot(writer, current_snapshot_)) return 0;
    return writer.bytes_written();
}

void SocialHubActivity::for_each_connected_snapshot(
    void (*fn)(void* ctx, wish::session::SessionId sid,
               const std::byte* data, ae::usize len),
    void* ctx) {
    build_current_snapshot();

    for (auto& slot : slots_) {
        if (!slot.connected) continue;

        current_snapshot_.last_processed_input = slot.last_processed;
        snapshot_buffer_.fill(std::byte{0});

        detail::ByteWriter writer(std::span<std::byte>(snapshot_buffer_.data(), snapshot_buffer_.size()));

        ae::u32 magic = kPacketMagic;
        ae::u16 version = kProtocolVersion;
        ae::u16 pkt_type = static_cast<ae::u16>(PacketType::ServerSnapshot);
        writer.write(magic);
        writer.write(version);
        writer.write(pkt_type);

        auto env = slot.seq_tracker.prepare_outgoing();
        writer.write(env.sequence);
        writer.write(env.ack_sequence);
        writer.write(env.ack_bitfield);

        ae::u16 activity_tag = 3;
        writer.write(activity_tag);

        if (!write_social_snapshot(writer, current_snapshot_)) continue;

        fn(ctx, slot.session_id, snapshot_buffer_.data(), writer.bytes_written());
    }
}

SocialHubActivity::Slot* SocialHubActivity::find_slot(wish::session::SessionId sid) {
    for (auto& s : slots_) {
        if (s.session_id.value == sid.value) return &s;
    }
    return nullptr;
}

void SocialHubActivity::build_current_snapshot() {
    current_snapshot_ = {};
    current_snapshot_.server_tick = server_tick_;

    // Populate nearby players (all connected players visible)
    ae::u8 count = 0;
    for (auto& s : slots_) {
        if (!s.connected || count >= 32) continue;
        NearbyPlayerState& n = current_snapshot_.nearby_players[count];
        n.player_id = static_cast<ae::u32>(s.session_id.value);
        n.position = s.player_state.position;
        n.yaw = s.player_state.yaw;
        n.emote_id = 0;  // idle by default
        n.movement_state = static_cast<ae::u8>(s.player_state.movement_state);
        ++count;
    }
    current_snapshot_.nearby_count = count;
}

}  // namespace ahamkara::game::activities
