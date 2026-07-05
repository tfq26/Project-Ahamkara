#include "ahamkara/game/activities/deathmatch_activity.h"
#include "ae/core/log.h"
#include "ahamkara/game/movement.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace ahamkara::game::activities {

DeathmatchActivity::DeathmatchActivity() {
    world_.set_is_client(false);
}

bool DeathmatchActivity::load_map(const std::string& path) {
    ae::render::CompiledLevelLoader loader;
    ae::render::LevelAsset asset;
    if (!loader.load(path, asset)) {
        return false;
    }
    ae::log_info(std::string("Deathmatch activity loading map: ") + asset.name +
                 " (" + std::to_string(asset.collision_boxes.size()) + " colliders, " +
                 std::to_string(asset.spawn_points.size()) + " spawns)");
    world_.load_colliders_from_level(asset);
    return true;
}

bool DeathmatchActivity::initialize(const wish::core::ActivityConfig& cfg) {
    config_ = cfg;

    game_rules_.type         = GameModeType::Deathmatch;
    game_rules_.score_limit  = 50;
    game_rules_.max_players  = static_cast<int>(cfg.max_players);
    game_rules_.respawn_time = 3.0F;
    game_rules_.auto_start   = true;
    game_rules_.time_limit_minutes = 10.0F;

    dm_state_.reset();

    ae::log_info(std::string("Deathmatch activity '") + std::string(cfg.name)
                 + "' initialized (max_players=" + std::to_string(cfg.max_players)
                 + ", tick_rate=" + std::to_string(cfg.tick_rate) + ")");

    if (!cfg.map_path.empty() && cfg.map_path != "default") {
        if (!load_map(std::string(cfg.map_path))) {
            ae::log_warning(std::string("Deathmatch activity using fallback map (") + std::string(cfg.map_path) + " failed to load)");
        }
    }
    return true;
}

void DeathmatchActivity::shutdown() {
    ae::log_info(std::string("Deathmatch activity '") + std::string(config_.name) + "' shutting down.");
}

bool DeathmatchActivity::admit_player(const wish::core::SessionAdmissionRequest& req) {
    if (slots_.size() >= static_cast<std::size_t>(config_.max_players)) {
        return false;
    }

    PlayerSlot slot {};
    slot.session_id.value = next_session_id_.value++;
    slot.connected = true;
    slot.last_seen = std::chrono::steady_clock::now();
    slots_.push_back(std::move(slot));
    return true;
}

void DeathmatchActivity::remove_player(wish::session::SessionId sid) {
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
        [sid](const PlayerSlot& s) { return s.session_id.value == sid.value; }),
        slots_.end());
}

ae::u32 DeathmatchActivity::player_count() const {
    return static_cast<ae::u32>(slots_.size());
}

void DeathmatchActivity::tick(float dt) {
    server_tick_++;
    dm_state_.tick(dt, game_rules_);

    // Server-authoritative sim step: advances physics, projectiles,
    // dummies, match time, particles/decals, history, syncs.
    world_.advance_sim(dt);

    // Apply each connected slot's most recent buffered input.
    for (auto& slot : slots_) {
        if (!slot.connected) continue;
        if (!slot.has_pending_input) continue;
        world_.apply_input(dt, slot.pending_input);
        slot.last_processed_input_sequence = slot.pending_input.sequence;
        slot.has_pending_input = false;
    }
}

void DeathmatchActivity::process_input(wish::session::SessionId sid,
                                       const ae::PacketEnvelope& envelope,
                                       ae::u32 command_sequence) {
    PlayerSlot* slot = find_slot(sid);
    if (!slot) return;

    slot->sequence_tracker.process_incoming(envelope);
    slot->last_received_input_sequence = command_sequence;
    slot->last_seen = std::chrono::steady_clock::now();
}

void DeathmatchActivity::simulate_input(wish::session::SessionId sid,
                                         float /*delta_seconds*/,
                                         const PlayerInputCommand& cmd) {
    PlayerSlot* slot = find_slot(sid);
    if (!slot) return;

    // Buffer the input for the next server tick. The world is NOT
    // advanced here — tick() owns the authoritative sim step.
    slot->pending_input = cmd;
    slot->has_pending_input = true;

    // Also apply anti-cheat checks immediately (they're stateless).
    apply_anti_cheat(cmd);
}

PlayerSlot* DeathmatchActivity::first_slot() {
    if (slots_.empty()) return nullptr;
    return &slots_.front();
}

void DeathmatchActivity::for_each_connected_snapshot(
    void (*fn)(void* ctx, wish::session::SessionId sid,
               const std::byte* data, ae::usize len),
    void* ctx) {
    // Build the base snapshot state once
    PlayerSlot dummy_slot {};
    PlayerSlot& base_slot = slots_.empty() ? dummy_slot : slots_.front();
    build_snapshot_for_slot(base_slot);

    for (auto& slot : slots_) {
        if (!slot.connected) continue;

        current_snapshot_.last_processed_input = slot.last_processed_input_sequence;
        snapshot_buffer_.fill(std::byte{0});
        detail::ByteWriter writer(std::span<std::byte>(snapshot_buffer_.data(), snapshot_buffer_.size()));

        // Header: magic + version + packet type
        ae::u32 magic = kPacketMagic;
        ae::u16 version = kProtocolVersion;
        ae::u16 pkt_type = static_cast<ae::u16>(PacketType::ServerSnapshot);
        writer.write(magic);
        writer.write(version);
        writer.write(pkt_type);

        // Envelope (per-client sequence/ack state)
        auto env = slot.sequence_tracker.prepare_outgoing();
        writer.write(env.sequence);
        writer.write(env.ack_sequence);
        writer.write(env.ack_bitfield);

        // Activity tag
        ae::u16 activity_tag = 1;
        writer.write(activity_tag);

        if (!write_deathmatch_snapshot(writer, current_snapshot_)) continue;

        fn(ctx, slot.session_id, snapshot_buffer_.data(), writer.bytes_written());
    }
}

ae::usize DeathmatchActivity::build_snapshot_bytes(wish::session::SessionId sid,
                                                    std::span<std::byte> buffer) {
    PlayerSlot* slot = find_slot(sid);
    if (!slot || !slot->connected) return 0;

    build_snapshot_for_slot(*slot);
    current_snapshot_.last_processed_input = slot->last_processed_input_sequence;

    detail::ByteWriter writer(buffer);

    ae::u32 magic = kPacketMagic;
    ae::u16 version = kProtocolVersion;
    ae::u16 pkt_type = static_cast<ae::u16>(PacketType::ServerSnapshot);
    writer.write(magic);
    writer.write(version);
    writer.write(pkt_type);

    auto env = slot->sequence_tracker.prepare_outgoing();
    writer.write(env.sequence);
    writer.write(env.ack_sequence);
    writer.write(env.ack_bitfield);

    ae::u16 activity_tag = 1;
    writer.write(activity_tag);

    if (!write_deathmatch_snapshot(writer, current_snapshot_)) return 0;
    return writer.bytes_written();
}

bool DeathmatchActivity::is_complete() const {
    return dm_state_.is_match_over();
}

PlayerSlot* DeathmatchActivity::find_slot(wish::session::SessionId sid) {
    for (auto& s : slots_) {
        if (s.session_id.value == sid.value) return &s;
    }
    return nullptr;
}

PlayerSlot* DeathmatchActivity::find_slot_by_address(const ae::NetAddress& addr) {
    for (auto& s : slots_) {
        if (s.address.ip == addr.ip && s.address.port == addr.port) return &s;
    }
    return nullptr;
}

void DeathmatchActivity::build_snapshot_for_slot(PlayerSlot& slot) {
    current_snapshot_ = {};
    current_snapshot_.server_tick = server_tick_;
    current_snapshot_.local_player = world_.get_player_state();
    current_snapshot_.last_processed_input = slot.last_processed_input_sequence;

    const auto* world_projectiles = world_.get_projectiles();
    int world_proj_count = world_.get_projectile_count();
    current_snapshot_.projectile_count = static_cast<ae::u8>(std::min(world_proj_count, 8));
    for (int i = 0; i < static_cast<int>(current_snapshot_.projectile_count); ++i) {
        current_snapshot_.projectiles[i] = world_projectiles[i];
    }

    const auto* world_dummies = world_.get_dummies();
    int world_dummy_count = world_.get_dummy_count();
    current_snapshot_.dummy_count = static_cast<ae::u8>(std::min(world_dummy_count, 4));
    static bool prev_dummy_alive[4] = {true, true, true, false};
    for (int i = 0; i < static_cast<int>(current_snapshot_.dummy_count); ++i) {
        current_snapshot_.dummies[i] = world_dummies[i];
        if (prev_dummy_alive[i] && !world_dummies[i].alive) {
            dm_state_.on_kill(Team::Red, 1);
        }
        prev_dummy_alive[i] = world_dummies[i].alive;
    }

    current_snapshot_.match_phase = static_cast<ae::u8>(dm_state_.match.phase);
    current_snapshot_.match_time = dm_state_.match.match_time;
    current_snapshot_.team_score_red = dm_state_.match.team_score_red;
    current_snapshot_.team_score_blue = dm_state_.match.team_score_blue;
    current_snapshot_.individual_score = 0;

    // History record
    HistoricalState hist {};
    hist.tick = current_snapshot_.server_tick;
    hist.player_position = current_snapshot_.local_player.position;
    for (int d = 0; d < World::kMaxDummies; ++d) {
        const auto& dummies = world_.get_dummies();
        hist.dummy_positions[d] = dummies[d].position;
        hist.dummy_alive[d] = dummies[d].alive;
    }
    history_buffer_.record(current_snapshot_.server_tick, hist);
}

void DeathmatchActivity::apply_anti_cheat(const PlayerInputCommand& cmd) {
    const auto& current_pos = world_.get_player_state().position;
    float dx = current_pos.x - prev_player_position_.x;
    float dy = current_pos.y - prev_player_position_.y;
    float dz = current_pos.z - prev_player_position_.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    constexpr float kMaxDistPerTick = 0.25F;

    if (dist > kMaxDistPerTick && server_tick_ > 10) {
        ae::log_warning("Possible speed hack: moved " + std::to_string(dist)
                        + " units in one tick (max " + std::to_string(kMaxDistPerTick) + ")");
    }
    prev_player_position_ = current_pos;
}

}  // namespace ahamkara::game::activities
