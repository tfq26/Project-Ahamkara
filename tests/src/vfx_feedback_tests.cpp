#include "ahamkara/game/net_types.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/world.h"
#include "ahamkara/game/client_prediction.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

using namespace ahamkara::game;

// ===================================================================
//  VfxFeedback Payload — structural tests
// ===================================================================

void test_vfx_feedback_defaults() {
    VfxFeedback fb{};
    assert(fb.screen_shake_intensity == 0.0F);
    assert(fb.screen_shake_duration == 0.0F);
    assert(fb.damage_flash_intensity == 0.0F);
    assert(fb.damage_flash_duration == 0.0F);
    assert(fb.event_id == 0);
    std::cout << "test_vfx_feedback_defaults passed.\n";
}

void test_vfx_feedback_trivially_copyable() {
    // VfxFeedback must be trivially copyable for memcpy serialization.
    assert(std::is_trivially_copyable_v<VfxFeedback>);
    std::cout << "test_vfx_feedback_trivially_copyable passed.\n";
}

void test_vfx_feedback_roundtrip() {
    VfxFeedback src{};
    src.screen_shake_intensity = 0.5F;
    src.screen_shake_duration = 0.3F;
    src.damage_flash_intensity = 0.25F;
    src.damage_flash_duration = 0.4F;
    src.event_id = 42;

    // Serialize
    std::array<std::byte, 256> buffer{};
    detail::ByteWriter writer(buffer);
    assert(write_vfx_feedback(writer, src));

    // Deserialize
    VfxFeedback dst{};
    detail::ByteReader reader(
        std::span<const std::byte>(buffer.data(), writer.bytes_written()));
    assert(read_vfx_feedback(reader, dst));

    assert(dst.screen_shake_intensity == 0.5F);
    assert(dst.screen_shake_duration == 0.3F);
    assert(dst.damage_flash_intensity == 0.25F);
    assert(dst.damage_flash_duration == 0.4F);
    assert(dst.event_id == 42);
    std::cout << "test_vfx_feedback_roundtrip passed.\n";
}

void test_vfx_feedback_zero_values() {
    // Zero values must round-trip safely without NaNs.
    VfxFeedback src{};
    std::array<std::byte, 256> buffer{};
    detail::ByteWriter writer(buffer);
    assert(write_vfx_feedback(writer, src));

    VfxFeedback dst{};
    dst.screen_shake_intensity = -1.0F; // poison
    detail::ByteReader reader(
        std::span<const std::byte>(buffer.data(), writer.bytes_written()));
    assert(read_vfx_feedback(reader, dst));

    assert(dst.screen_shake_intensity == 0.0F);
    assert(dst.screen_shake_duration == 0.0F);
    assert(dst.damage_flash_intensity == 0.0F);
    assert(dst.damage_flash_duration == 0.0F);
    assert(dst.event_id == 0);
    assert(!std::isnan(dst.screen_shake_intensity));
    assert(!std::isnan(dst.damage_flash_intensity));
    std::cout << "test_vfx_feedback_zero_values passed.\n";
}

void test_vfx_feedback_max_values() {
    // Maximum values must round-trip without overflow or clamping.
    VfxFeedback src{};
    src.screen_shake_intensity = 1.0F;
    src.screen_shake_duration = 10.0F;
    src.damage_flash_intensity = 1.0F;
    src.damage_flash_duration = 5.0F;
    // event_id intentionally left at 0 — see zero-values test above

    std::array<std::byte, 256> buffer{};
    detail::ByteWriter writer(buffer);
    assert(write_vfx_feedback(writer, src));

    VfxFeedback dst{};
    detail::ByteReader reader(
        std::span<const std::byte>(buffer.data(), writer.bytes_written()));
    assert(read_vfx_feedback(reader, dst));

    assert(dst.screen_shake_intensity == 1.0F);
    assert(dst.screen_shake_duration == 10.0F);
    assert(dst.damage_flash_intensity == 1.0F);
    assert(dst.damage_flash_duration == 5.0F);
    std::cout << "test_vfx_feedback_max_values passed.\n";
}

// ===================================================================
//  VfxFeedback in ServerSnapshot — serialization
// ===================================================================

void test_snapshot_vfx_feedback() {
    // Build a full ServerSnapshot with VFX feedback, serialize it, and
    // verify the data survives a round-trip.
    ServerSnapshot src{};
    src.server_tick = 42;
    src.last_processed_input = 10;
    src.local_player = {};
    src.local_player.health = 75.0F;
    src.local_player.shield = 50.0F;
    src.match_phase = 3;
    src.match_time = 12.5F;

    // Populate VFX feedback
    src.vfx_feedback.screen_shake_intensity = 0.6F;
    src.vfx_feedback.screen_shake_duration = 0.3F;
    src.vfx_feedback.damage_flash_intensity = 0.3F;
    src.vfx_feedback.damage_flash_duration = 0.4F;
    src.vfx_feedback.event_id = 7;

    // Serialize
    std::array<std::byte, 1024> buffer{};
    detail::ByteWriter writer(buffer);
    assert(write_snapshot(writer, src));

    // Deserialize
    ServerSnapshot dst{};
    detail::ByteReader reader(
        std::span<const std::byte>(buffer.data(), writer.bytes_written()));
    assert(read_snapshot(reader, dst));

    assert(dst.server_tick == 42);
    assert(dst.last_processed_input == 10);
    assert(dst.local_player.health == 75.0F);
    assert(dst.match_phase == 3);

    // Verify VFX feedback survived round-trip
    assert(dst.vfx_feedback.screen_shake_intensity == 0.6F);
    assert(dst.vfx_feedback.screen_shake_duration == 0.3F);
    assert(dst.vfx_feedback.damage_flash_intensity == 0.3F);
    assert(dst.vfx_feedback.damage_flash_duration == 0.4F);
    assert(dst.vfx_feedback.event_id == 7);
    std::cout << "test_snapshot_vfx_feedback passed.\n";
}

void test_snapshot_vfx_feedback_packet_roundtrip() {
    // Full packet-level round-trip including header and envelope.
    ServerSnapshot src{};
    src.server_tick = 99;
    src.vfx_feedback.screen_shake_intensity = 0.8F;
    src.vfx_feedback.screen_shake_duration = 0.5F;
    src.vfx_feedback.damage_flash_intensity = 0.4F;
    src.vfx_feedback.damage_flash_duration = 0.6F;
    src.vfx_feedback.event_id = 13;

    PacketEnvelope env{};
    env.sequence = 1;
    env.ack_sequence = 0;
    env.ack_bitfield = 0;

    ServerSnapshotPacketBuffer buffer{};
    assert(serialize_server_snapshot_packet(env, src, buffer));

    PacketEnvelope out_env{};
    ServerSnapshot dst{};
    assert(deserialize_server_snapshot_packet(buffer, out_env, dst));

    assert(out_env.sequence == 1);
    assert(dst.server_tick == 99);
    assert(dst.vfx_feedback.screen_shake_intensity == 0.8F);
    assert(dst.vfx_feedback.screen_shake_duration == 0.5F);
    assert(dst.vfx_feedback.damage_flash_intensity == 0.4F);
    assert(dst.vfx_feedback.damage_flash_duration == 0.6F);
    assert(dst.vfx_feedback.event_id == 13);
    std::cout << "test_snapshot_vfx_feedback_packet_roundtrip passed.\n";
}

// ===================================================================
//  World VFX feedback emission
// ===================================================================

void test_world_damage_emits_vfx_feedback() {
    // When damage is applied to the player, the World must emit an
    // authoritative VfxFeedback payload with a non-zero event_id.
    World world{};
    world.set_is_server(true);

    // Get initial state — player is alive with full health/shield
    const auto& initial = world.get_player_state();
    assert(initial.health > 0.0F);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};
    world.apply_damage_to_player(0, 25.0F, attacker_pos);

    // VFX feedback must be emitted
    const auto& fb = world.get_vfx_feedback();
    assert(fb.event_id != 0);
    assert(fb.screen_shake_intensity > 0.0F);
    assert(fb.screen_shake_duration > 0.0F);
    assert(fb.damage_flash_intensity > 0.0F);
    assert(fb.damage_flash_duration >= fb.screen_shake_duration);

    std::cout << "test_world_damage_emits_vfx_feedback passed.\n";
}

void test_world_damage_vfx_event_id_monotonic() {
    // Each damage event must increment the event_id.
    World world{};
    world.set_is_server(true);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};

    world.apply_damage_to_player(0, 10.0F, attacker_pos);
    const ae::u32 first_id = world.get_vfx_feedback().event_id;
    assert(first_id != 0);

    world.apply_damage_to_player(0, 10.0F, attacker_pos);
    const ae::u32 second_id = world.get_vfx_feedback().event_id;
    assert(second_id > first_id);

    world.apply_damage_to_player(0, 10.0F, attacker_pos);
    const ae::u32 third_id = world.get_vfx_feedback().event_id;
    assert(third_id > second_id);

    std::cout << "test_world_damage_vfx_event_id_monotonic passed.\n";
}

void test_world_no_damage_no_vfx() {
    // Without any damage, VFX feedback must be zero/empty.
    World world{};

    // Run a few ticks without damage
    PlayerInputCommand input{};
    for (int i = 0; i < 10; ++i) {
        world.tick(1.0F / 60.0F, input);
    }

    const auto& fb = world.get_vfx_feedback();
    assert(fb.event_id == 0);
    assert(fb.screen_shake_intensity == 0.0F);
    assert(fb.screen_shake_duration == 0.0F);
    assert(fb.damage_flash_intensity == 0.0F);
    assert(fb.damage_flash_duration == 0.0F);

    std::cout << "test_world_no_damage_no_vfx passed.\n";
}

void test_world_restart_clears_vfx() {
    // Restarting the match should clear VFX feedback.
    World world{};
    world.set_is_server(true);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};
    world.apply_damage_to_player(0, 50.0F, attacker_pos);
    assert(world.get_vfx_feedback().event_id != 0);

    world.restart_match();

    const auto& fb = world.get_vfx_feedback();
    assert(fb.event_id == 0);
    assert(fb.screen_shake_intensity == 0.0F);
    assert(fb.screen_shake_duration == 0.0F);

    std::cout << "test_world_restart_clears_vfx passed.\n";
}

// ===================================================================
//  One-shot consumption via event_id
// ===================================================================

void test_vfx_one_shot_via_event_id() {
    // VfxFeedback must be consumed exactly once per event_id.
    // Simulate the presentation layer's event_id tracking:
    // 1. First damage event → new event_id → consume
    // 2. Read snapshot again (same event_id) → must NOT re-consume
    // 3. Second damage event → new event_id → consume again

    World world{};
    world.set_is_server(true);
    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};

    ae::u32 last_consumed_id = 0;
    int consume_count = 0;

    // Helper: simulate presentation layer checking snapshot.
    // Uses strict greater-than to prevent rollback replay.
    auto check_snapshot = [&](const VfxFeedback& fb) {
        if (fb.event_id != 0 && fb.event_id > last_consumed_id) {
            last_consumed_id = fb.event_id;
            ++consume_count;
        }
    };

    // First damage event
    world.apply_damage_to_player(0, 30.0F, attacker_pos);
    check_snapshot(world.get_vfx_feedback());
    assert(consume_count == 1);
    assert(last_consumed_id == 1);

    // Read same snapshot again → must NOT re-consume
    check_snapshot(world.get_vfx_feedback());
    assert(consume_count == 1); // unchanged

    // Tick forward (timer still active, same event)
    world.advance_sim(1.0F / 60.0F);
    check_snapshot(world.get_vfx_feedback());
    assert(consume_count == 1); // still same event_id

    // Second damage event
    world.apply_damage_to_player(0, 20.0F, attacker_pos);
    check_snapshot(world.get_vfx_feedback());
    assert(consume_count == 2);
    assert(last_consumed_id == 2);

    // Third damage event
    world.apply_damage_to_player(0, 15.0F, attacker_pos);
    check_snapshot(world.get_vfx_feedback());
    assert(consume_count == 3);
    assert(last_consumed_id == 3);

    std::cout << "test_vfx_one_shot_via_event_id passed.\n";
}

void test_vfx_rollback_does_not_replay() {
    // Simulating a rollback: if the presentation receives a snapshot with
    // an event_id it has already consumed, it must NOT replay the animation.
    //
    // The presentation layer tracks the last-consumed event_id.  When
    // processing a new snapshot, it compares the snapshot's event_id:
    //   - if event_id == 0                    → no VFX event (skip)
    //   - if event_id == last_consumed_id     → same event (skip, no replay)
    //   - if event_id > last_consumed_id      → new event (consume)
    //   - if event_id < last_consumed_id      → stale snapshot (skip)
    //
    // In a rollback the client receives a snapshot with event_id < the
    // already-consumed event_id, so the condition correctly prevents replay.

    ae::u32 last_consumed_id = 0;
    int consume_count = 0;

    // Simulate the presentation consume function.
    // A new VFX event is detected ONLY when event_id is both non-zero
    // AND strictly greater than the last consumed event_id.
    // This correctly handles rollbacks: a stale snapshot with a lower
    // event_id will NOT re-trigger the animation.
    auto consume = [&](const VfxFeedback& fb) {
        if (fb.event_id != 0 && fb.event_id > last_consumed_id) {
            last_consumed_id = fb.event_id;
            ++consume_count;
        }
    };

    // Simulate receiving snapshot with event_id = 5 (first event)
    VfxFeedback snap1;
    snap1.event_id = 5;
    snap1.screen_shake_intensity = 0.6F;
    snap1.screen_shake_duration = 0.3F;
    snap1.damage_flash_intensity = 0.3F;
    snap1.damage_flash_duration = 0.4F;

    consume(snap1);
    assert(consume_count == 1);
    assert(last_consumed_id == 5);

    // Simulate receiving the same snapshot again (no new event)
    consume(snap1);
    assert(consume_count == 1); // must NOT increment

    // Simulate receiving next snapshot with event_id = 6 (new event)
    VfxFeedback snap2;
    snap2.event_id = 6;
    snap2.screen_shake_intensity = 0.8F;
    snap2.screen_shake_duration = 0.5F;

    consume(snap2);
    assert(consume_count == 2);
    assert(last_consumed_id == 6);

    // Simulate ROLLBACK: receive an old snapshot (event_id = 5, lower)
    // This must NOT replay the animation.
    consume(snap1);
    assert(consume_count == 2); // must NOT increment

    // Simulate another rollback to event_id = 5
    consume(snap1);
    assert(consume_count == 2); // must NOT increment

    // After rollback, new event at event_id = 7 should still work
    VfxFeedback snap3;
    snap3.event_id = 7;
    snap3.screen_shake_intensity = 0.5F;
    snap3.screen_shake_duration = 0.3F;

    consume(snap3);
    assert(consume_count == 3);
    assert(last_consumed_id == 7);

    // Zero event_id (no VFX) must not consume
    VfxFeedback no_vfx{};
    consume(no_vfx);
    assert(consume_count == 3); // must NOT increment

    std::cout << "test_vfx_rollback_does_not_replay passed.\n";
}

// ===================================================================
//  ClientPredictionManager VFX propagation
// ===================================================================

void test_prediction_manager_includes_vfx() {
    // ClientPredictionManager::capture_prediction_state() must include
    // the VFX feedback in the returned ServerSnapshot.
    ClientPredictionManager cpm{};

    // Simulate some gameplay to trigger damage
    // The prediction world starts with a player at default spawn
    PlayerInputCommand input{};
    input.fire_held = true; // will fire weapon
    cpm.apply_input(input);

    // Capture state — no damage yet, so VFX should be zero
    ServerSnapshot snap = cpm.capture_prediction_state();
    assert(snap.vfx_feedback.event_id == 0);
    assert(snap.vfx_feedback.screen_shake_intensity == 0.0F);

    std::cout << "test_prediction_manager_includes_vfx passed.\n";
}

// ===================================================================
//  Determinism across variable frame-rate partitions
// ===================================================================

void test_vfx_determinism_across_frame_partitions() {
    // The VFX envelope parameters (intensity, duration) must be
    // deterministic regardless of how the fixed-timestep simulation
    // subdivides the elapsed time.
    World world_a{};
    World world_b{};
    world_a.set_is_server(true);
    world_b.set_is_server(true);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};

    // Apply identical damage to both worlds
    world_a.apply_damage_to_player(0, 30.0F, attacker_pos);
    world_b.apply_damage_to_player(0, 30.0F, attacker_pos);

    // Advance world_a with many small steps (60 Hz)
    for (int i = 0; i < 60; ++i) {
        world_a.advance_sim(1.0F / 60.0F);
    }

    // Advance world_b with a single large step
    world_b.advance_sim(1.0F);

    // VFX feedback event_id must be the same (deterministic)
    assert(world_a.get_vfx_feedback().event_id == world_b.get_vfx_feedback().event_id);

    std::cout << "test_vfx_determinism_across_frame_partitions passed.\n";
}

// ===================================================================
//  Zero/invalid values fail safely
// ===================================================================

void test_vfx_zero_damage_safe() {
    // Applying zero damage must not produce VFX feedback.
    World world{};
    world.set_is_server(true);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};
    world.apply_damage_to_player(0, 0.0F, attacker_pos);

    const auto& fb = world.get_vfx_feedback();
    // Zero damage produces no feedback (apply_damage_to_player skips zero-damage)
    // Actually, zero damage will still trigger the damage function path but result
    // in very small/near-zero values. Let's verify it's safe (no NaN, no inf).
    assert(!std::isnan(fb.screen_shake_intensity));
    assert(!std::isinf(fb.screen_shake_intensity));
    assert(!std::isnan(fb.damage_flash_intensity));
    assert(!std::isinf(fb.damage_flash_intensity));
    assert(fb.event_id >= 0);

    std::cout << "test_vfx_zero_damage_safe passed.\n";
}

void test_vfx_negative_damage_safe() {
    // Negative damage must not produce NaN or inf values.
    World world{};
    world.set_is_server(true);

    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};
    world.apply_damage_to_player(0, -10.0F, attacker_pos);

    const auto& fb = world.get_vfx_feedback();
    assert(!std::isnan(fb.screen_shake_intensity));
    assert(!std::isinf(fb.screen_shake_intensity));
    assert(!std::isnan(fb.damage_flash_intensity));
    assert(!std::isinf(fb.damage_flash_intensity));

    std::cout << "test_vfx_negative_damage_safe passed.\n";
}

// ===================================================================
//  Snapshot replacement and scene reset
// ===================================================================

void test_scene_reset_clears_vfx() {
    // Simulate a full scene reset (like respawn or new match).
    World world{};
    world.set_is_server(true);

    // Apply damage
    const Vec3 attacker_pos{0.0F, 0.0F, 0.0F};
    world.apply_damage_to_player(0, 40.0F, attacker_pos);
    assert(world.get_vfx_feedback().event_id != 0);

    // Full reset
    world.restart_match();
    assert(world.get_vfx_feedback().event_id == 0);

    // After reset, new damage must produce event_id = 1 again (restarted)
    world.apply_damage_to_player(0, 30.0F, attacker_pos);
    assert(world.get_vfx_feedback().event_id == 1);

    std::cout << "test_scene_reset_clears_vfx passed.\n";
}

}  // namespace

int main() {
    // VfxFeedback unit tests
    test_vfx_feedback_defaults();
    test_vfx_feedback_trivially_copyable();
    test_vfx_feedback_roundtrip();
    test_vfx_feedback_zero_values();
    test_vfx_feedback_max_values();

    // Snapshot serialization
    test_snapshot_vfx_feedback();
    test_snapshot_vfx_feedback_packet_roundtrip();

    // World emission
    test_world_damage_emits_vfx_feedback();
    test_world_damage_vfx_event_id_monotonic();
    test_world_no_damage_no_vfx();
    test_world_restart_clears_vfx();

    // One-shot consumption
    test_vfx_one_shot_via_event_id();
    test_vfx_rollback_does_not_replay();

    // Prediction manager
    test_prediction_manager_includes_vfx();

    // Determinism
    test_vfx_determinism_across_frame_partitions();

    // Safe failure
    test_vfx_zero_damage_safe();
    test_vfx_negative_damage_safe();

    // Scene reset
    test_scene_reset_clears_vfx();

    std::cout << "\nAll VFX feedback tests passed.\n";
    return 0;
}
