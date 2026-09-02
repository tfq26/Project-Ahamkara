#include "ahamkara/game/activities/deathmatch_activity.h"
#include "ahamkara/game/rewind.h"
#include "ae/core/log.h"
#include "ahamkara/game/movement.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace ahamkara::game::activities {

DeathmatchActivity::DeathmatchActivity()
    : rewind_validation_(history_buffer_) {
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
    slot.player_index = world_.add_player();
    slot.network_object_id = world_.players()[slot.player_index].network_object_id();
    slot.connected = true;
    slot.last_seen = std::chrono::steady_clock::now();

    Player* player = world_.get_player(slot.player_index);
    if (player) {
        player->set_player_id(slot.session_id.value);
    }

    slots_.push_back(std::move(slot));
    return true;
}

void DeathmatchActivity::remove_player(wish::session::SessionId sid) {
    auto it = std::remove_if(slots_.begin(), slots_.end(),
                             [this, sid](PlayerSlot& s) {
                                 if (s.session_id.value != sid.value)
                                     return false;
                                 world_.remove_player(s.player_index);
                                 s.has_pending_input = false;
                                 s.connected = false;
                                 return true;
                             });
    slots_.erase(it, slots_.end());
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
        slot.clock_tracker.record_server_tick(server_tick_);
        if (!slot.has_pending_input) continue;
        world_.apply_input(slot.player_index, dt, slot.pending_input);
        slot.last_processed_input_sequence = slot.pending_input.sequence;
        slot.has_pending_input = false;
    }
}

void DeathmatchActivity::process_input(wish::session::SessionId sid,
                                       const wish::PacketEnvelope& envelope,
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

    // Also apply anti-cheat checks immediately.
    apply_anti_cheat(*slot, cmd);
}

PlayerSlot* DeathmatchActivity::first_slot() {
    if (slots_.empty()) return nullptr;
    return &slots_.front();
}

void DeathmatchActivity::for_each_connected_snapshot(
    void (*fn)(void* ctx, wish::session::SessionId sid,
               const std::byte* data, ae::usize len),
    void* ctx) {
    for (auto& slot : slots_) {
        if (!slot.connected) continue;

        // Build the snapshot specifically for this recipient.
        build_snapshot_for_slot(slot);

        // Populate remote players from other connected slots.
        current_snapshot_.remote_player_count = 0;
        for (const auto& other : slots_) {
            if (!other.connected)
                continue;
            if (&other == &slot)
                continue; // don't list self
            if (current_snapshot_.remote_player_count >= 4)
                break;

            const Player* other_player = world_.get_player(other.player_index);
            if (!other_player)
                continue;

            auto& rp = current_snapshot_.remote_players[current_snapshot_.remote_player_count++];
            rp.player_id = other_player->player_id();
            rp.network_object_id = other_player->network_object_id();
            rp.position = other_player->state().position;
            rp.yaw = other_player->state().yaw;
            rp.health = other_player->state().health;
        }

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

        // Delta-compressed player state (recipient's local player)
        SnapshotDelta delta = compute_player_delta(
            current_snapshot_.local_player, slot.last_sent_player_state);
        if (!write_snapshot_delta(writer, delta)) continue;
        slot.last_sent_player_state = current_snapshot_.local_player;

        // Projectiles
        if (!writer.write(current_snapshot_.projectile_count)) continue;
        for (ae::u8 pi = 0; pi < current_snapshot_.projectile_count && pi < 8; ++pi) {
            const auto& p = current_snapshot_.projectiles[pi];
            if (!write_vec3(writer, p.position)
                || !write_vec3(writer, p.velocity)
                || !writer.write(p.lifetime_seconds)
                || !writer.write_bool(p.alive)
                || !writer.write(p.client_tick))
                continue;
        }

        // Dummies
        if (!writer.write(current_snapshot_.dummy_count)) continue;
        for (ae::u8 di = 0; di < current_snapshot_.dummy_count && di < 4; ++di) {
            const auto& d = current_snapshot_.dummies[di];
            if (!writer.write(d.dummy_id)
                || !write_vec3(writer, d.position)
                || !writer.write(d.yaw)
                || !writer.write(d.health)
                || !writer.write_bool(d.alive))
                continue;
        }

        // Match state
        if (!writer.write(current_snapshot_.match_phase)
            || !writer.write(current_snapshot_.match_time)
            || !writer.write(current_snapshot_.team_score_red)
            || !writer.write(current_snapshot_.team_score_blue)
            || !writer.write(current_snapshot_.individual_score))
            continue;

        // Remote players (other connected players visible to this recipient)
        if (!writer.write(current_snapshot_.remote_player_count))
            continue;
        for (ae::u8 ri = 0; ri < current_snapshot_.remote_player_count; ++ri) {
            const auto& rp = current_snapshot_.remote_players[ri];
            if (!writer.write(rp.player_id) || !writer.write(rp.network_object_id) || !write_vec3(writer, rp.position) || !writer.write(rp.yaw) || !writer.write(rp.health))
                continue;
        }

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

PlayerSlot* DeathmatchActivity::find_slot_by_address(const wish::NetAddress& addr) {
    for (auto& s : slots_) {
        if (s.address.ip == addr.ip && s.address.port == addr.port) return &s;
    }
    return nullptr;
}

void DeathmatchActivity::build_snapshot_for_slot(PlayerSlot& slot) {
    current_snapshot_ = {};
    current_snapshot_.server_tick = server_tick_;
    current_snapshot_.local_player = world_.get_player_state(slot.player_index);
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
    for (ae::u32 pi = 0; pi < world_.players().size() && pi < HistoricalState::kMaxPlayerPositions; ++pi) {
        const auto& p = world_.players()[pi];
        hist.player_positions[pi] = p.state().position;
        hist.player_alive[pi] = p.is_alive();
    }
    for (int d = 0; d < World::kMaxDummies; ++d) {
        const auto& dummies = world_.get_dummies();
        hist.dummy_positions[d] = dummies[d].position;
        hist.dummy_alive[d] = dummies[d].alive;
    }
    history_buffer_.record(current_snapshot_.server_tick, hist);
}

void DeathmatchActivity::apply_anti_cheat(PlayerSlot& slot, const PlayerInputCommand& cmd) {
    const Player* player = world_.get_player(slot.player_index);
    if (!player)
        return;

    const auto& current_pos = player->state().position;
    float dx = current_pos.x - slot.prev_position.x;
    float dy = current_pos.y - slot.prev_position.y;
    float dz = current_pos.z - slot.prev_position.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    constexpr float kMaxDistPerTick = 0.25F;

    if (dist > kMaxDistPerTick && server_tick_ > 10) {
        ae::log_warning("Player " + std::to_string(player->player_id()) + " possible speed hack: moved " + std::to_string(dist) + " units in one tick (max " + std::to_string(kMaxDistPerTick) + ")");
    }
    slot.prev_position = current_pos;
}

// ---------------------------------------------------------------------------
// Lag-compensated hit validation — server rewind
// ---------------------------------------------------------------------------

HitResult DeathmatchActivity::validate_hit(
    wish::session::SessionId firing_player,
    ae::u32 client_tick,
    float client_rtt,
    const Vec3& origin,
    const Vec3& forward,
    float base_damage,
    float headshot_multiplier) {
    // Find the firing player's slot to access their clock tracker.
    PlayerSlot* slot = find_slot(firing_player);
    if (!slot) {
        HitResult r {};
        r.reject_reason = HitReject::NoHistory;
        return r;
    }

    // Step 1: convert client tick through the per-player clock tracker.
    // The clock tracker clamps to the max rewind window.
    const ae::u32 server_tick = slot->clock_tracker.convert(client_tick, client_rtt);

    // Step 2: validate the rewind bounds.
    HitReject reject = slot->clock_tracker.validate_rewind(client_tick, server_tick);
    if (reject != HitReject::None) {
        HitResult r {};
        r.reject_reason = reject;
        return r;
    }

    // Step 3: delegate to the rewind validation engine (non-mutating query).
    HitResult r = rewind_validation_.validate_hit(
        server_tick, origin, forward, base_damage, headshot_multiplier);

    // Step 4: guard against duplicate validation requests.
    if (r.hit && r.hit_dummy_idx >= 0) {
        // Use a hash of (client_tick, dummy_idx) to check duplicates.
        const ae::u32 dup_key = (client_tick << 16) |
                                (static_cast<ae::u32>(r.hit_dummy_idx) & 0xFFFF);
        static_cast<void>(dup_key); // Reserved for future use.
    }

    return r;
}

}  // namespace ahamkara::game::activities
