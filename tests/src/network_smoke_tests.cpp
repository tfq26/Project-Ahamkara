#include "ae/core/time.h"
#include "ae/network/network_clock.h"
#include "ae/network/network_simulator.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/server_history.h"
#include "ae/network/udp_socket.h"
#include "ae/runtime/application.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <thread>

void run_camera_smoke_tests();
void run_local_play_tests();

namespace {

void test_application_lifecycle() {
    ae::Application application(ae::RuntimeMode::Tests);
    assert(!application.is_running());
    assert(application.mode() == ae::RuntimeMode::Tests);

    application.start();
    assert(application.is_running());

    application.shutdown();
    assert(!application.is_running());
}

void test_player_input_packet_round_trip() {
    ahamkara::game::PlayerInputCommand source {};
    source.sequence = 42;
    source.client_tick = 84;
    source.client_time = 1.5F;
    source.move_axis = {0.25F, 1.0F};
    source.look_delta = {1.0F, -0.5F};
    source.jump_pressed = true;
    source.crouch_held = false;
    source.sprint_held = true;
    source.slide_pressed = false;
    source.fire_held = true;
    source.reload_pressed = false;
    source.ability_pressed = true;
    source.interact_pressed = true;

    ahamkara::game::PacketEnvelope out_env {};
    out_env.sequence = 7;
    out_env.ack_sequence = 3;
    out_env.ack_bitfield = 0xAAAAAAAAU;

    ahamkara::game::PlayerInputPacketBuffer buffer {};
    const bool serialized = ahamkara::game::serialize_player_input_packet(out_env, source, buffer);
    assert(serialized);

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::PlayerInputCommand decoded {};
    const bool deserialized = ahamkara::game::deserialize_player_input_packet(buffer, in_env, decoded);
    assert(deserialized);

    assert(in_env.sequence == out_env.sequence);
    assert(in_env.ack_sequence == out_env.ack_sequence);
    assert(in_env.ack_bitfield == out_env.ack_bitfield);
    assert(decoded.sequence == source.sequence);
    assert(decoded.client_tick == source.client_tick);
    assert(decoded.client_time == source.client_time);
    assert(decoded.move_axis.x == source.move_axis.x);
    assert(decoded.move_axis.y == source.move_axis.y);
    assert(decoded.look_delta.x == source.look_delta.x);
    assert(decoded.look_delta.y == source.look_delta.y);
    assert(decoded.jump_pressed == source.jump_pressed);
    assert(decoded.crouch_held == source.crouch_held);
    assert(decoded.sprint_held == source.sprint_held);
    assert(decoded.slide_pressed == source.slide_pressed);
    assert(decoded.fire_held == source.fire_held);
    assert(decoded.reload_pressed == source.reload_pressed);
    assert(decoded.ability_pressed == source.ability_pressed);
    assert(decoded.interact_pressed == source.interact_pressed);
}

void test_snapshot_packet_round_trip() {
    ahamkara::game::ServerSnapshot source {};
    source.server_tick = 7;
    source.last_processed_input = 99;
    source.local_player.network_object_id = 123;
    source.local_player.player_id = 456;
    source.local_player.position = {10.0F, 1.0F, -3.0F};
    source.local_player.velocity = {8.0F, 0.0F, 2.0F};
    source.local_player.yaw = 180.0F;
    source.local_player.movement_state = ahamkara::game::MovementState::Sprinting;
    source.local_player.health = 75.0F;
    source.local_player.shield = 25.0F;

    ahamkara::game::ServerSnapshotPacketBuffer buffer {};
    ahamkara::game::PacketEnvelope out_env {};
    const bool serialized = ahamkara::game::serialize_server_snapshot_packet(out_env, source, buffer);
    assert(serialized);

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::ServerSnapshot decoded {};
    const bool deserialized = ahamkara::game::deserialize_server_snapshot_packet(
        std::span<const std::byte>(buffer.data(), buffer.size()), in_env, decoded);
    assert(deserialized);

    assert(decoded.server_tick == source.server_tick);
    assert(decoded.last_processed_input == source.last_processed_input);
    assert(decoded.local_player.network_object_id == source.local_player.network_object_id);
    assert(decoded.local_player.player_id == source.local_player.player_id);
    assert(decoded.local_player.position.x == source.local_player.position.x);
    assert(decoded.local_player.position.y == source.local_player.position.y);
    assert(decoded.local_player.position.z == source.local_player.position.z);
    assert(decoded.local_player.velocity.x == source.local_player.velocity.x);
    assert(decoded.local_player.velocity.y == source.local_player.velocity.y);
    assert(decoded.local_player.velocity.z == source.local_player.velocity.z);
    assert(decoded.local_player.yaw == source.local_player.yaw);
    assert(decoded.local_player.movement_state == source.local_player.movement_state);
    assert(decoded.local_player.health == source.local_player.health);
    assert(decoded.local_player.shield == source.local_player.shield);
}

void test_client_hello_packet_round_trip() {
    ahamkara::game::ClientHelloPacket source {};
    source.protocol_version = ahamkara::game::kProtocolVersion;
    const char test_token[] = "nakama-test-jwt-token-12345";
    source.auth_token_length = static_cast<ae::u16>(sizeof(test_token) - 1);
    std::memcpy(source.auth_token, test_token, source.auth_token_length);

    ahamkara::game::ClientHelloPacketBuffer buffer {};
    ahamkara::game::PacketEnvelope out_env {};
    out_env.sequence = 11;
    out_env.ack_sequence = 7;
    out_env.ack_bitfield = 0x11223344U;
    const bool serialized = ahamkara::game::serialize_client_hello_packet(out_env, source, buffer);
    assert(serialized);

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::ClientHelloPacket decoded {};
    const bool deserialized = ahamkara::game::deserialize_client_hello_packet(buffer, in_env, decoded);
    assert(deserialized);

    assert(in_env.sequence == out_env.sequence);
    assert(in_env.ack_sequence == out_env.ack_sequence);
    assert(in_env.ack_bitfield == out_env.ack_bitfield);
    assert(decoded.protocol_version == source.protocol_version);
    assert(decoded.auth_token_length == source.auth_token_length);
    assert(std::strncmp(decoded.auth_token, source.auth_token, source.auth_token_length) == 0);
}

void test_server_welcome_packet_round_trip() {
    ahamkara::game::ServerWelcomePacket source {};
    source.protocol_version = ahamkara::game::kProtocolVersion;
    std::snprintf(source.player_id, ahamkara::game::kMaxPlayerIdLength, "%s", "player-42");

    ahamkara::game::ServerWelcomePacketBuffer buffer {};
    ahamkara::game::PacketEnvelope out_env {};
    out_env.sequence = 21;
    out_env.ack_sequence = 13;
    out_env.ack_bitfield = 0xA5A5A5A5U;
    const bool serialized = ahamkara::game::serialize_server_welcome_packet(out_env, source, buffer);
    assert(serialized);

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::ServerWelcomePacket decoded {};
    const bool deserialized = ahamkara::game::deserialize_server_welcome_packet(buffer, in_env, decoded);
    assert(deserialized);

    assert(in_env.sequence == out_env.sequence);
    assert(in_env.ack_sequence == out_env.ack_sequence);
    assert(in_env.ack_bitfield == out_env.ack_bitfield);
    assert(decoded.protocol_version == source.protocol_version);
    assert(std::strncmp(decoded.player_id, source.player_id, ahamkara::game::kMaxPlayerIdLength) == 0);
}

void test_server_reject_packet_round_trip() {
    ahamkara::game::ServerRejectPacket source {};
    source.protocol_version = ahamkara::game::kProtocolVersion;
    source.reason = ahamkara::game::HandshakeRejectReason::ServerBusy;

    ahamkara::game::ServerRejectPacketBuffer buffer {};
    ahamkara::game::PacketEnvelope out_env {};
    out_env.sequence = 31;
    out_env.ack_sequence = 17;
    out_env.ack_bitfield = 0x01020304U;
    const bool serialized = ahamkara::game::serialize_server_reject_packet(out_env, source, buffer);
    assert(serialized);

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::ServerRejectPacket decoded {};
    const bool deserialized = ahamkara::game::deserialize_server_reject_packet(buffer, in_env, decoded);
    assert(deserialized);

    assert(in_env.sequence == out_env.sequence);
    assert(in_env.ack_sequence == out_env.ack_sequence);
    assert(in_env.ack_bitfield == out_env.ack_bitfield);
    assert(decoded.protocol_version == source.protocol_version);
    assert(decoded.reason == source.reason);
}

void test_packet_validation_rejects_corruption() {
    ahamkara::game::PlayerInputCommand source {};
    ahamkara::game::PacketEnvelope out_env {};
    ahamkara::game::PlayerInputPacketBuffer buffer {};
    assert(ahamkara::game::serialize_player_input_packet(out_env, source, buffer));

    buffer[0] = std::byte {0x00};

    ahamkara::game::PacketEnvelope in_env {};
    ahamkara::game::PlayerInputCommand decoded {};
    const bool deserialized = ahamkara::game::deserialize_player_input_packet(buffer, in_env, decoded);
    assert(!deserialized);
}

void test_handshake_rejects_protocol_version_mismatch() {
    ahamkara::game::ClientHelloPacket source {};
    source.protocol_version = ahamkara::game::kProtocolVersion;

    ahamkara::game::ClientHelloPacketBuffer buffer {};
    ahamkara::game::PacketEnvelope envelope {};
    assert(ahamkara::game::serialize_client_hello_packet(envelope, source, buffer));

    const ae::usize protocol_offset = sizeof(ae::u32) + sizeof(ae::u16) + sizeof(ae::u16) + ahamkara::game::kEnvelopeWireSize;
    buffer[protocol_offset] = std::byte {0x00};

    ahamkara::game::PacketEnvelope decoded_env {};
    ahamkara::game::ClientHelloPacket decoded {};
    const bool deserialized = ahamkara::game::deserialize_client_hello_packet(buffer, decoded_env, decoded);
    assert(deserialized);
    assert(!ahamkara::game::is_supported_protocol_version(decoded.protocol_version));
}

void test_shared_movement_simulation() {
    ahamkara::game::ReplicatedPlayerState player_state {};
    ahamkara::game::PlayerInputCommand command {};
    command.move_axis.y = 1.0F;
    command.sprint_held = true;

    ahamkara::game::simulate_player_movement(player_state, command, 1.0F);

    assert(player_state.position.x == 0.0F);
    assert(player_state.position.z == 8.0F);
    assert(player_state.velocity.z == 8.0F);
    assert(player_state.movement_state == ahamkara::game::MovementState::Sprinting);
}

// ── Server history buffer tests ─────────────────────────────────────────────

struct TestState {
    ae::u32 tick {0};
    float x {0.0F};
    float y {0.0F};
};

void test_server_history_basic() {
    ae::ServerHistoryBuffer<TestState, 16> history;

    assert(history.size() == 0);
    assert(history.newest_tick() == 0);
    assert(history.oldest_tick() == 0);
    assert(history.capacity() == 16);
}

void test_server_history_record_and_retrieve() {
    ae::ServerHistoryBuffer<TestState, 16> history;

    history.record(0, TestState{0, 1.0F, 2.0F});
    assert(history.size() == 1);
    assert(history.newest_tick() == 0);

    TestState retrieved {};
    assert(history.get(0, retrieved));
    assert(retrieved.x == 1.0F);
    assert(retrieved.y == 2.0F);

    // Out-of-range query.
    assert(!history.get(1, retrieved));
    assert(!history.get(999, retrieved));
}

void test_server_history_gap_fill() {
    ae::ServerHistoryBuffer<TestState, 16> history;

    history.record(0, TestState{0, 1.0F, 0.0F});
    history.record(5, TestState{5, 6.0F, 0.0F});  // Skips ticks 1-4.

    // Gaps should be filled with the previous value.
    TestState s {};
    assert(history.get(0, s));
    assert(s.x == 1.0F);

    assert(history.get(3, s));
    assert(s.x == 1.0F);  // Filled from tick 0.

    assert(history.get(5, s));
    assert(s.x == 6.0F);

    assert(history.size() == 6);  // Ticks 0-5.
}

void test_server_history_wraparound() {
    ae::ServerHistoryBuffer<ae::u32, 4> history;

    // Fill beyond capacity to force wrap.
    for (ae::u32 t = 0; t < 10; ++t) {
        history.record(t, t);
    }

    assert(history.size() == 4);
    assert(history.newest_tick() == 9);
    assert(history.oldest_tick() == 6);

    ae::u32 val = 0;
    assert(history.get(6, val));
    assert(val == 6);

    assert(history.get(9, val));
    assert(val == 9);

    // Tick 5 fell off.
    assert(!history.get(5, val));
}

void test_server_history_reset() {
    ae::ServerHistoryBuffer<TestState, 8> history;
    history.record(0, TestState{0, 1.0F, 2.0F});
    history.record(1, TestState{1, 3.0F, 4.0F});

    history.reset();
    assert(history.size() == 0);

    TestState s {};
    assert(!history.get(0, s));
}

// ── Network simulator tests ──────────────────────────────────────────────────

void test_simulator_disabled_is_passthrough() {
    ae::UdpSocket server_sock;
    ae::UdpSocket client_sock;

    assert(server_sock.open(18901));
    assert(client_sock.open(18902));

    ae::NetworkSimulator sim(client_sock);
    ae::SimulatorConfig config;
    config.enabled = false;
    sim.configure(config);

    const ae::NetAddress server_addr {"127.0.0.1", 18901};

    const char msg[] = "hello";
    assert(sim.send_to(server_addr, msg, sizeof(msg)));

    // Small sleep to let the packet arrive.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ae::NetAddress from {};
    char recv_buf[64] {};
    const ae::i32 n = server_sock.receive_from(from, recv_buf, sizeof(recv_buf));
    assert(n == static_cast<ae::i32>(sizeof(msg)));
}

void test_simulator_full_loss() {
    ae::UdpSocket sock;
    assert(sock.open(18903));

    ae::NetworkSimulator sim(sock);
    ae::SimulatorConfig config;
    config.enabled = true;
    config.loss_rate = 1.0F;  // Drop everything.
    sim.configure(config);

    const ae::NetAddress addr {"127.0.0.1", 18903};
    const char msg[] = "should_drop";

    // Send a batch; all should be "sent" from caller's perspective but dropped internally.
    for (int i = 0; i < 20; ++i) {
        assert(sim.send_to(addr, msg, sizeof(msg)));
    }

    // Update to flush any queued packets (none should exist since all were dropped).
    sim.update(0.1F);

    const auto& stats = sim.stats();
    assert(stats.packets_received == 20);
    assert(stats.packets_dropped == 20);
    assert(stats.packets_sent == 0);
}

void test_simulator_latency_delays_packets() {
    ae::UdpSocket server_sock;
    ae::UdpSocket client_sock;

    assert(server_sock.open(18905));
    assert(client_sock.open(18906));

    ae::NetworkSimulator sim(client_sock);
    ae::SimulatorConfig config;
    config.enabled = true;
    config.loss_rate = 0.0F;
    config.latency_min_ms = 100.0F;
    config.latency_max_ms = 100.0F;
    config.jitter_ms = 0.0F;
    sim.configure(config);

    const ae::NetAddress server_addr {"127.0.0.1", 18905};
    const char msg[] = "delayed";

    assert(sim.send_to(server_addr, msg, sizeof(msg)));

    // Immediately after send, the packet should still be in the delay queue.
    {
        ae::NetAddress from {};
        char buf[64] {};
        assert(server_sock.receive_from(from, buf, sizeof(buf)) == 0);
    }

    // Advance the simulator past the one-way delay (~50ms).
    sim.update(0.06F);

    // Now it should have been flushed.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ae::NetAddress from {};
    char buf[64] {};
    const ae::i32 n = server_sock.receive_from(from, buf, sizeof(buf));
    assert(n == static_cast<ae::i32>(sizeof(msg)));
}

void test_simulator_stats_counters() {
    ae::UdpSocket sock;
    assert(sock.open(18907));

    ae::NetworkSimulator sim(sock);

    // Disabled mode: stats track packets as "sent" directly.
    ae::SimulatorConfig config;
    config.enabled = false;
    sim.configure(config);

    const ae::NetAddress addr {"127.0.0.1", 18907};
    const char msg[] = "stats_test";

    sim.send_to(addr, msg, sizeof(msg));
    const auto& stats = sim.stats();
    assert(stats.packets_sent >= 1);
    assert(stats.bytes_sent >= sizeof(msg));

    // Reset and verify zeroed.
    sim.reset_stats();
    const auto& zeroed = sim.stats();
    assert(zeroed.packets_received == 0);
    assert(zeroed.packets_dropped == 0);
    assert(zeroed.packets_sent == 0);
}

// ── Network clock tests ─────────────────────────────────────────────────────

void test_network_clock_initial_state() {
    ae::NetworkClock clock;

    // Before any snapshot, offset is zero.
    assert(std::abs(clock.smoothed_offset_seconds()) < 0.001F);
    assert(clock.estimated_rtt_seconds() < 0.001F);
}

void test_network_clock_snapshot_offset() {
    ae::NetworkClock clock;

    // Server tick 60 at 60 Hz = exactly 1.0 second of server time.
    // Local time is 0.5 s, so the server is 0.5 s ahead.
    clock.record_snapshot(60, 60.0F, 0.5);

    assert(std::abs(clock.smoothed_offset_seconds() - 0.5F) < 0.01F);

    // Projection: at local time 1.0 s, estimated server time = 1.0 + 0.5 = 1.5 s.
    const float est = clock.estimate_server_time(1.0, 60.0F);
    assert(std::abs(est - 1.5F) < 0.01F);
}

void test_network_clock_smoothing() {
    ae::NetworkClock clock;

    // First sample: offset = 1.0 s.
    clock.record_snapshot(60, 60.0F, 0.0);
    assert(std::abs(clock.smoothed_offset_seconds() - 1.0F) < 0.01F);

    // Second sample: raw offset = 2.0 s.  EWMA blend (0.9 * 1.0 + 0.1 * 2.0) = 1.1 s.
    clock.record_snapshot(120, 60.0F, 0.0);
    assert(std::abs(clock.smoothed_offset_seconds() - 1.1F) < 0.02F);
}

void test_network_clock_rtt_tracking() {
    ae::NetworkClock clock;

    clock.record_rtt(0.05F);  // 50 ms.
    assert(std::abs(clock.estimated_rtt_seconds() - 0.05F) < 0.001F);

    // Negative RTT should be ignored.
    clock.record_rtt(-0.01F);
    assert(std::abs(clock.estimated_rtt_seconds() - 0.05F) < 0.001F);
}

void test_network_clock_reset() {
    ae::NetworkClock clock;

    clock.record_snapshot(60, 60.0F, 0.5);
    clock.record_rtt(0.03F);

    clock.reset();

    assert(std::abs(clock.smoothed_offset_seconds()) < 0.001F);
    assert(clock.estimated_rtt_seconds() < 0.001F);
}

// ── Sequence tracker tests ───────────────────────────────────────────────────

void test_sequence_tracker_initial_state() {
    ae::SequenceTracker tracker;

    assert(tracker.packets_sent() == 0);
    assert(tracker.packets_received() == 0);
    assert(tracker.estimated_lost() == 0);
}

void test_sequence_tracker_outgoing_increment() {
    ae::SequenceTracker tracker;

    auto env0 = tracker.prepare_outgoing();
    assert(env0.sequence == 0);
    assert(tracker.packets_sent() == 1);

    auto env1 = tracker.prepare_outgoing();
    assert(env1.sequence == 1);
    assert(tracker.packets_sent() == 2);
}

void test_sequence_tracker_incoming_without_gaps() {
    ae::SequenceTracker tracker;

    ahamkara::game::PacketEnvelope e0 {};
    e0.sequence = 0;
    tracker.process_incoming(e0);
    assert(tracker.packets_received() == 1);
    assert(tracker.estimated_lost() == 0);

    ahamkara::game::PacketEnvelope e1 {};
    e1.sequence = 1;
    tracker.process_incoming(e1);
    assert(tracker.packets_received() == 2);
    assert(tracker.estimated_lost() == 0);

    ahamkara::game::PacketEnvelope e2 {};
    e2.sequence = 2;
    tracker.process_incoming(e2);
    assert(tracker.estimated_lost() == 0);
}

void test_sequence_tracker_incoming_with_gaps() {
    ae::SequenceTracker tracker;

    ahamkara::game::PacketEnvelope e0 {};
    e0.sequence = 0;
    tracker.process_incoming(e0);

    // Jump from seq 0 to seq 5: packets 1-4 are lost.
    ahamkara::game::PacketEnvelope e5 {};
    e5.sequence = 5;
    tracker.process_incoming(e5);
    assert(tracker.estimated_lost() == 4);
    assert(tracker.packets_received() == 2);
}

void test_sequence_tracker_out_of_order_fills_gap() {
    ae::SequenceTracker tracker;

    ahamkara::game::PacketEnvelope e0 {};
    e0.sequence = 0;
    tracker.process_incoming(e0);

    ahamkara::game::PacketEnvelope e5 {};
    e5.sequence = 5;
    tracker.process_incoming(e5);
    assert(tracker.estimated_lost() == 4);

    // Late arrival of seq 3 fills one gap.
    ahamkara::game::PacketEnvelope e3 {};
    e3.sequence = 3;
    tracker.process_incoming(e3);
    assert(tracker.estimated_lost() == 3);
    assert(tracker.packets_received() == 3);
}

void test_sequence_tracker_ack_reflected_in_outgoing() {
    ae::SequenceTracker tracker;

    ahamkara::game::PacketEnvelope in {};
    in.sequence = 10;
    tracker.process_incoming(in);

    // Next outgoing should acknowledge seq 10.
    auto out = tracker.prepare_outgoing();
    assert(out.ack_sequence == 10);
}

}  // namespace

int main() {
    test_application_lifecycle();
    test_player_input_packet_round_trip();
    test_snapshot_packet_round_trip();
    test_client_hello_packet_round_trip();
    test_server_welcome_packet_round_trip();
    test_server_reject_packet_round_trip();
    test_packet_validation_rejects_corruption();
    test_handshake_rejects_protocol_version_mismatch();
    test_shared_movement_simulation();

    // Server history buffer.
    test_server_history_basic();
    test_server_history_record_and_retrieve();
    test_server_history_gap_fill();
    test_server_history_wraparound();
    test_server_history_reset();

    // Network simulator.
    test_simulator_disabled_is_passthrough();
    test_simulator_full_loss();
    test_simulator_latency_delays_packets();
    test_simulator_stats_counters();

    // Network clock.
    test_network_clock_initial_state();
    test_network_clock_snapshot_offset();
    test_network_clock_smoothing();
    test_network_clock_rtt_tracking();
    test_network_clock_reset();

    // Sequence tracker.
    test_sequence_tracker_initial_state();
    test_sequence_tracker_outgoing_increment();
    test_sequence_tracker_incoming_without_gaps();
    test_sequence_tracker_incoming_with_gaps();
    test_sequence_tracker_out_of_order_fills_gap();
    test_sequence_tracker_ack_reflected_in_outgoing();

    run_camera_smoke_tests();
    run_local_play_tests();
    return 0;
}
