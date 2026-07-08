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

        // Delta-compressed player state portion
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
                continue; // NOLINT (intentional early-exit on serialization failure)
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

        // Remote players (none mapped yet — reserved for future use)
        const ae::u8 zero_remote = 0;
        if (!writer.write(zero_remote)) continue;

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

// ---------------------------------------------------------------------------
// Lag-compensated hit validation — server rewind
// ---------------------------------------------------------------------------

HitValidationResult DeathmatchActivity::validate_hit(
    wish::session::SessionId /*firing_player*/,
    ae::u32 client_tick,
    const Vec3& origin,
    const Vec3& forward,
    float base_damage,
    float headshot_multiplier)
{
    // Query the server's history buffer for the tick the client perceived
    // when they fired.  If the tick is outside the retained window, bail.
    HistoricalState hist{};
    if (!history_buffer_.get(client_tick, hist)) {
        return {};
    }

    constexpr float kHitscanRange = 1000.0F;
    constexpr float kDummyRadius = 0.35F;
    constexpr float kDummyHalfHeight = 1.0F;

    Vec3 ray_end {
        origin.x + forward.x * kHitscanRange,
        origin.y + forward.y * kHitscanRange,
        origin.z + forward.z * kHitscanRange
    };

    float closest_t = kHitscanRange;
    bool hit_something = false;
    bool is_headshot = false;
    int hit_dummy_idx = -1;
    Vec3 hit_position = ray_end;

    // Ray direction (normalised).
    Vec3 ray_dir {
        ray_end.x - origin.x,
        ray_end.y - origin.y,
        ray_end.z - origin.z
    };
    float ray_len = std::sqrt(ray_dir.x * ray_dir.x +
                              ray_dir.y * ray_dir.y +
                              ray_dir.z * ray_dir.z);
    if (ray_len > 0.001F) {
        ray_dir.x /= ray_len;
        ray_dir.y /= ray_len;
        ray_dir.z /= ray_len;
    }

    // Test each dummy at its historical position (lag compensation).
    for (int i = 0; i < HistoricalState::kMaxDummies; ++i) {
        if (!hist.dummy_alive[i]) continue;

        const Vec3 d_pos = hist.dummy_positions[i];
        const Vec3 d_bottom = {d_pos.x, d_pos.y - kDummyHalfHeight, d_pos.z};
        const Vec3 d_top    = {d_pos.x, d_pos.y + kDummyHalfHeight, d_pos.z};

        // Ray-vs-infinite-cylinder test (XZ plane).
        Vec3 oc = {origin.x - d_pos.x, origin.y - d_pos.y, origin.z - d_pos.z};
        float a = ray_dir.x * ray_dir.x + ray_dir.z * ray_dir.z;
        if (a < 0.001F) continue;  // Ray is near-vertical — skip cylinder test.
        float b = 2.0F * (oc.x * ray_dir.x + oc.z * ray_dir.z);
        float c = oc.x * oc.x + oc.z * oc.z - kDummyRadius * kDummyRadius;
        float disc = b * b - 4.0F * a * c;

        if (disc >= 0.0F) {
            float sqrt_disc = std::sqrt(disc);
            float t0 = (-b - sqrt_disc) / (2.0F * a);
            float t1 = (-b + sqrt_disc) / (2.0F * a);
            if (t0 > t1) std::swap(t0, t1);

            // Test cylinder body.
            for (float t : {t0, t1}) {
                if (t > 0.001F && t < closest_t) {
                    float hit_y = origin.y + ray_dir.y * t;
                    if (hit_y >= d_bottom.y && hit_y <= d_top.y) {
                        closest_t = t;
                        hit_something = true;
                        hit_dummy_idx = i;
                        hit_position = {origin.x + ray_dir.x * t, hit_y,
                                        origin.z + ray_dir.z * t};
                        is_headshot = (hit_y >= d_top.y - 0.3F);
                    }
                }
            }

            // Hemisphere caps (approximate as full-sphere test for top and
            // bottom centres).
            for (const Vec3& cap_center : {d_top, d_bottom}) {
                Vec3 to_cap = {origin.x - cap_center.x,
                               origin.y - cap_center.y,
                               origin.z - cap_center.z};
                float cap_b = 2.0F * (to_cap.x * ray_dir.x +
                                      to_cap.y * ray_dir.y +
                                      to_cap.z * ray_dir.z);
                float cap_c = to_cap.x * to_cap.x +
                              to_cap.y * to_cap.y +
                              to_cap.z * to_cap.z - kDummyRadius * kDummyRadius;
                float cap_disc = cap_b * cap_b - 4.0F * cap_c;
                if (cap_disc >= 0.0F) {
                    float cap_t = (-cap_b - std::sqrt(cap_disc)) * 0.5F;
                    if (cap_t > 0.001F && cap_t < closest_t) {
                        closest_t = cap_t;
                        hit_something = true;
                        is_headshot = (&cap_center == &d_top);
                        hit_dummy_idx = i;
                        hit_position = {origin.x + ray_dir.x * cap_t,
                                        origin.y + ray_dir.y * cap_t,
                                        origin.z + ray_dir.z * cap_t};
                    }
                }
            }
        }
    }

    if (!hit_something) return {};

    float damage = base_damage;
    if (is_headshot) damage *= headshot_multiplier;

    return {true, hit_dummy_idx, hit_position, damage, is_headshot};
}

}  // namespace ahamkara::game::activities
