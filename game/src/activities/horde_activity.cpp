#include "ahamkara/game/activities/horde_activity.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ahamkara::game::activities {

HordeActivity::HordeActivity() = default;

bool HordeActivity::initialize(const wish::core::ActivityConfig& cfg) {
    config_ = cfg;
    objective_health_ = max_objective_health_;
    wave_number_ = 1;
    wave_timer_ = 0.0F;
    enemy_count_ = 0;

    ae::log_info(std::string("Horde activity '") + std::string(cfg.name)
                 + "' initialized (max_players=" + std::to_string(cfg.max_players)
                 + ", tick_rate=" + std::to_string(cfg.tick_rate) + ")");
    return true;
}

void HordeActivity::shutdown() {
    ae::log_info(std::string("Horde activity '") + std::string(config_.name) + "' shutting down.");
}

bool HordeActivity::admit_player(const wish::core::SessionAdmissionRequest& req) {
    if (slots_.size() >= static_cast<std::size_t>(config_.max_players)) return false;
    Slot s {};
    s.session_id.value = next_sid_.value++;
    s.connected = true;
    s.last_seen = std::chrono::steady_clock::now();
    slots_.push_back(std::move(s));
    return true;
}

void HordeActivity::remove_player(wish::session::SessionId sid) {
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
        [sid](const Slot& s) { return s.session_id.value == sid.value; }),
        slots_.end());
}

ae::u32 HordeActivity::player_count() const {
    return static_cast<ae::u32>(slots_.size());
}

void HordeActivity::tick(float dt) {
    server_tick_++;
    wave_timer_ += dt;

    // Spawn new wave when timer expires
    if (wave_timer_ >= time_between_waves_) {
        spawn_wave();
        wave_timer_ = 0.0F;
        time_between_waves_ = std::max(10.0F, time_between_waves_ - 1.0F);  // escalate
    }

    tick_enemies(dt);
}

void HordeActivity::process_input(wish::session::SessionId sid,
                                   const ae::PacketEnvelope& envelope,
                                   ae::u32 command_sequence) {
    Slot* slot = find_slot(sid);
    if (!slot) return;
    slot->seq_tracker.process_incoming(envelope);
    slot->last_received = command_sequence;
    slot->last_seen = std::chrono::steady_clock::now();
}

ae::usize HordeActivity::build_snapshot_bytes(wish::session::SessionId sid,
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

    ae::u16 activity_tag = 2;
    writer.write(activity_tag);

    if (!write_horde_snapshot(writer, current_snapshot_)) return 0;
    return writer.bytes_written();
}

bool HordeActivity::is_complete() const {
    return objective_health_ <= 0.0F;
}

void HordeActivity::for_each_connected_snapshot(
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

        ae::u16 activity_tag = 2;
        writer.write(activity_tag);

        if (!write_horde_snapshot(writer, current_snapshot_)) continue;

        fn(ctx, slot.session_id, snapshot_buffer_.data(), writer.bytes_written());
    }
}

HordeActivity::Slot* HordeActivity::find_slot(wish::session::SessionId sid) {
    for (auto& s : slots_) {
        if (s.session_id.value == sid.value) return &s;
    }
    return nullptr;
}

void HordeActivity::spawn_wave() {
    ae::u8 count = static_cast<ae::u8>(3 + wave_number_ * 2);
    if (count > 16) count = 16;

    for (ae::u8 i = 0; i < count; ++i) {
        EnemyState e {};
        e.enemy_id = wave_number_ * 100 + i;
        e.position = Vec3 {
            (static_cast<float>(i) - count * 0.5F) * 2.0F,
            2.0F,
            -15.0F + static_cast<float>(wave_number_) * 0.5F
        };
        e.health = 100.0F + wave_number_ * 25.0F;
        e.alive = true;
        e.enemy_type = (i % 3 == 0) ? 1U : (i % 3 == 1) ? 2U : 0U;  // mix of types
        enemies_[i] = e;
    }
    enemy_count_ = count;

    ae::log_info("Horde wave " + std::to_string(wave_number_)
                 + " spawned " + std::to_string(count) + " enemies.");
    wave_number_++;
}

void HordeActivity::tick_enemies(float dt) {
    for (ae::u8 i = 0; i < enemy_count_; ++i) {
        if (!enemies_[i].alive) continue;

        // Simple AI: move toward objective (origin)
        Vec3& pos = enemies_[i].position;
        float dx = -pos.x;
        float dz = -pos.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 0.1F) {
            float speed = enemies_[i].enemy_type == 2 ? 4.0F : 2.0F;  // flyers faster
            pos.x += (dx / dist) * speed * dt;
            pos.z += (dz / dist) * speed * dt;
        } else {
            // Enemy reached objective — deal damage
            objective_health_ -= 5.0F * dt;
            if (objective_health_ < 0.0F) objective_health_ = 0.0F;
        }
    }
}

void HordeActivity::build_current_snapshot() {
    current_snapshot_ = {};
    current_snapshot_.server_tick = server_tick_;

    // Player state (first connected player)
    for (auto& s : slots_) {
        if (s.connected) {
            current_snapshot_.local_player.player_id = static_cast<ae::u32>(s.session_id.value);
            current_snapshot_.local_player.network_object_id = 1;
            break;
        }
    }

    current_snapshot_.enemy_count = enemy_count_;
    for (ae::u8 i = 0; i < enemy_count_ && i < 16; ++i) {
        current_snapshot_.enemies[i] = enemies_[i];
    }

    current_snapshot_.wave_number = wave_number_;
    current_snapshot_.wave_timer = time_between_waves_ - wave_timer_;
    if (current_snapshot_.wave_timer < 0.0F) current_snapshot_.wave_timer = 0.0F;
    current_snapshot_.objective_health = objective_health_;
    current_snapshot_.team_score = team_score_;

    current_snapshot_.last_processed_input = 0;
}

}  // namespace ahamkara::game::activities
