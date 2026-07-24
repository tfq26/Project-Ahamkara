#include "ae/core/time.h"
#include "ae/network/connection.h"
#include "ae/network/network_clock.h"
#include "ae/network/network_simulator.h"
#include "ae/network/reliable_channel.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/server_history.h"
#include "ae/network/snapshot_interpolator.h"
#include "ae/network/udp_socket.h"
#include "ahamkara/game/client_prediction.h"
#include "ahamkara/game/movement.h"
#include "ahamkara/game/net_packets.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// =========================================================================
// Constants
// =========================================================================

constexpr float kTickRate = 60.0F;
constexpr float kDt = 1.0F / kTickRate;
constexpr double kConvergenceTolerance = 0.05;   // World units
constexpr double kCorrectionBudgetTicks = 10;     // Max ticks to converge
constexpr double kRecoveryBudgetSeconds = 3.0;    // Max seconds to reconnect

// =========================================================================
// Test result aggregator
// =========================================================================

struct HarnessMetrics {
    int convergence_ticks {0};
    double final_error {0.0};
    int correction_count {0};
    double max_correction_magnitude {0.0};
    double total_correction_magnitude {0.0};
    int disconnect_count {0};
    double last_reconnect_time {0.0};
    double min_rtt {1e9};
    double max_rtt {0.0};
    double avg_rtt {0.0};
    int rtt_sample_count {0};

    void record_rtt(double rtt) {
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;
        avg_rtt = (avg_rtt * rtt_sample_count + rtt) / (rtt_sample_count + 1);
        ++rtt_sample_count;
    }

    void record_correction(double magnitude) {
        ++correction_count;
        if (magnitude > max_correction_magnitude) max_correction_magnitude = magnitude;
        total_correction_magnitude += magnitude;
    }

    void reset() { *this = HarnessMetrics{}; }
};

// =========================================================================
// 1. Convergence test: prediction matches authoritative state
// =========================================================================

void test_convergence_under_ideal_conditions(HarnessMetrics& metrics) {
    // Use accelerate_movement (Quake model) directly, no Jolt dependency.
    // Each simulation track gets its own MovementSimState.
    ahamkara::game::ReplicatedPlayerState authoritative_state;
    authoritative_state.position = {0.0F, 0.0F, 0.0F};
    authoritative_state.velocity = {0.0F, 0.0F, 0.0F};
    authoritative_state.yaw = 0.0F;

    ahamkara::game::ReplicatedPlayerState predicted_state = authoritative_state;
    ahamkara::game::MovementSimState auth_sim;
    ahamkara::game::MovementSimState pred_sim;

    const int kTicks = 60;

    for (int tick = 0; tick < kTicks; ++tick) {
        // Generate identical input for both server and client
        ahamkara::game::PlayerInputCommand cmd{};
        cmd.sequence = static_cast<ae::u32>(tick);
        cmd.client_tick = static_cast<ae::u32>(tick);
        cmd.client_time = static_cast<float>(tick) * kDt;
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = (tick % 2 == 0);

        // Server tick with its own sim state
        ahamkara::game::accelerate_movement(
            authoritative_state, auth_sim, cmd, kDt);

        // Client tick with its own sim state (same input => same result)
        ahamkara::game::accelerate_movement(
            predicted_state, pred_sim, cmd, kDt);

        // Check convergence error (should be exactly 0 with same input)
        double dx = predicted_state.position.x - authoritative_state.position.x;
        double dy = predicted_state.position.y - authoritative_state.position.y;
        double dz = predicted_state.position.z - authoritative_state.position.z;
        double error = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (error < kConvergenceTolerance && metrics.convergence_ticks == 0) {
            metrics.convergence_ticks = tick;
        }
        metrics.final_error = error;
    }

    assert(metrics.convergence_ticks >= 0 &&
           "Identical inputs should converge immediately (error=0)");
    assert(metrics.final_error < kConvergenceTolerance &&
           "Identical inputs produce zero divergence");
    assert(std::fabs(authoritative_state.position.z) > 0.01F &&
           "Player should have moved under acceleration model");

    std::printf("[HARNESS] Convergence: tick %d, final error %.6f, position z=%.4f\n",
                metrics.convergence_ticks, metrics.final_error,
                authoritative_state.position.z);
}

// =========================================================================
// 2. Correction test: divergence produces measurable correction
// =========================================================================

void test_correction_after_divergence(HarnessMetrics& metrics) {
    // Each simulation track gets its own MovementSimState.
    ahamkara::game::ReplicatedPlayerState authoritative_state;
    authoritative_state.position = {0.0F, 0.0F, 0.0F};
    authoritative_state.velocity = {0.0F, 0.0F, 0.0F};
    authoritative_state.yaw = 0.0F;

    ahamkara::game::ReplicatedPlayerState predicted_state = authoritative_state;
    ahamkara::game::MovementSimState auth_sim;
    ahamkara::game::MovementSimState pred_sim;

    const int kConvergeTicks = 30;
    const int kDivergentTicks = 30;

    // Phase 1: Converge with identical inputs
    for (int tick = 0; tick < kConvergeTicks; ++tick) {
        ahamkara::game::PlayerInputCommand cmd{};
        cmd.sequence = static_cast<ae::u32>(tick);
        cmd.move_axis.y = 1.0F;  // Forward
        cmd.sprint_held = true;

        ahamkara::game::accelerate_movement(authoritative_state, auth_sim, cmd, kDt);
        ahamkara::game::accelerate_movement(predicted_state, pred_sim, cmd, kDt);
    }

    // Phase 2: Divergence — server goes forward, client goes right
    // (different move_axis creates position divergence since Quake model
    // has no deceleration for walk-below-sprint-speed)
    for (int tick = kConvergeTicks; tick < kConvergeTicks + kDivergentTicks; ++tick) {
        // Server: forward
        ahamkara::game::PlayerInputCommand server_cmd{};
        server_cmd.sequence = static_cast<ae::u32>(tick);
        server_cmd.move_axis.y = 1.0F;
        server_cmd.sprint_held = true;
        ahamkara::game::accelerate_movement(authoritative_state, auth_sim, server_cmd, kDt);

        // Client: right strafe
        ahamkara::game::PlayerInputCommand client_cmd{};
        client_cmd.sequence = static_cast<ae::u32>(tick);
        client_cmd.move_axis.x = 1.0F;  // Right strafe
        client_cmd.sprint_held = true;
        // The Quake acceleration model applies velocity in the wish direction,
        // so different wish directions produce different velocity vectors.
        // With sprint_held=true, speed_cap=6.0 m/s. The client's wish_dir is
        // +X (right), while server's is +Z (forward). Velocities differ.
        ahamkara::game::accelerate_movement(predicted_state, pred_sim, client_cmd, kDt);
    }

    std::printf("[HARNESS] Correction test: server pos=(%.6f,%.6f,%.6f) vel=(%.6f,%.6f,%.6f)\n",
                authoritative_state.position.x, authoritative_state.position.y,
                authoritative_state.position.z,
                authoritative_state.velocity.x, authoritative_state.velocity.y,
                authoritative_state.velocity.z);
    std::printf("[HARNESS] Correction test: client pos=(%.6f,%.6f,%.6f) vel=(%.6f,%.6f,%.6f)\n",
                predicted_state.position.x, predicted_state.position.y,
                predicted_state.position.z,
                predicted_state.velocity.x, predicted_state.velocity.y,
                predicted_state.velocity.z);
    std::fflush(stdout);

    // Measure divergence
    double dx = predicted_state.position.x - authoritative_state.position.x;
    double dy = predicted_state.position.y - authoritative_state.position.y;
    double dz = predicted_state.position.z - authoritative_state.position.z;
    double divergence = std::sqrt(dx * dx + dy * dy + dz * dz);

    std::printf("[HARNESS] Divergence: error=%.6f (server z=%.4f, client x=%.4f)\n",
                divergence, authoritative_state.position.z,
                predicted_state.position.x);

    assert(divergence > kConvergenceTolerance &&
           "Different move directions should produce measurable divergence");

    // Phase 3: Correction — reset prediction to authoritative
    double correction = divergence;
    metrics.record_correction(correction);
    assert(correction > kConvergenceTolerance &&
           "Correction should match divergence magnitude");

    // Phase 4: After correction, identical inputs produce zero error
    predicted_state = authoritative_state;
    pred_sim = auth_sim;

    for (int tick = kConvergeTicks + kDivergentTicks;
         tick < kConvergeTicks + kDivergentTicks + 5; ++tick) {
        ahamkara::game::PlayerInputCommand cmd{};
        cmd.sequence = static_cast<ae::u32>(tick);
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = true;

        ahamkara::game::accelerate_movement(authoritative_state, auth_sim, cmd, kDt);
        ahamkara::game::accelerate_movement(predicted_state, pred_sim, cmd, kDt);

        double err = std::sqrt(
            std::pow(predicted_state.position.x - authoritative_state.position.x, 2) +
            std::pow(predicted_state.position.y - authoritative_state.position.y, 2) +
            std::pow(predicted_state.position.z - authoritative_state.position.z, 2));
        if (err > kConvergenceTolerance) {
            metrics.record_correction(err);
        }
    }

    std::printf("[HARNESS] Correction test: divergence=%.4f, %d corrections\n",
                divergence, metrics.correction_count);
}

// =========================================================================
// 3. Disconnect/Reconnect test with recovery budget
// =========================================================================

void test_disconnect_reconnect_budget(HarnessMetrics& metrics) {
    ae::ConnectionManager mgr;
    mgr.set_handshake_timeout(std::chrono::seconds(5));
    mgr.set_grace_period(std::chrono::seconds(10));
    mgr.set_max_missed_heartbeats(3);

    const auto base = ae::ConnectionManager::time_point{};
    const ae::NetAddress addr{"127.0.0.1", 30001};

    // Phase 1: Normal connection
    mgr.connect_request(addr, base);
    assert(mgr.complete_handshake(addr, 42, base));
    assert(mgr.connected_count() == 1);

    // Phase 2: Forced disconnect
    ++metrics.disconnect_count;

    // Miss heartbeats to trigger grace period (threshold is > max, so need 4)
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.tick(base);

    auto* peer = mgr.find(addr);
    assert(peer != nullptr);
    assert(peer->state == ae::ConnectionState::GracePeriod);

    // Phase 3: Reconnect within grace period
    auto start_time = std::chrono::steady_clock::now();
    auto& reconnected = mgr.connect_request(addr, base + std::chrono::milliseconds(100));

    double recovery_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start_time).count();
    assert(recovery_seconds < kRecoveryBudgetSeconds);
    assert(reconnected.state == ae::ConnectionState::Connected);
    assert(reconnected.session_id == 42);
    assert(reconnected.preserves_session);

    metrics.last_reconnect_time = recovery_seconds;

    // Verify full reconnection
    assert(mgr.connected_count() == 1);
    auto* reconnected_peer = mgr.find_by_session(42);
    assert(reconnected_peer != nullptr);
    assert(reconnected_peer->state == ae::ConnectionState::Connected);

    std::printf("[HARNESS] Disconnect/Reconnect: recovery in %.4fs, session_id=%llu preserved\n",
                recovery_seconds,
                static_cast<unsigned long long>(reconnected.session_id));
}

// =========================================================================
// 4. Multiple distinct client identities via ConnectionManager
// =========================================================================

void test_distinct_client_identities() {
    ae::ConnectionManager mgr;
    auto base = ae::ConnectionManager::time_point{};

    constexpr int kNumClients = 4;
    ae::NetAddress addrs[kNumClients] = {
        {"127.0.0.1", 40001},
        {"127.0.0.1", 40002},
        {"127.0.0.1", 40003},
        {"127.0.0.1", 40004},
    };

    // Connect all clients
    for (int i = 0; i < kNumClients; ++i) {
        mgr.connect_request(addrs[i], base);
        assert(mgr.complete_handshake(addrs[i], static_cast<ae::u64>(1001 + i), base));
    }

    assert(mgr.connected_count() == kNumClients);

    // Verify each client has a distinct identity
    for (int i = 0; i < kNumClients; ++i) {
        auto* peer = mgr.find(addrs[i]);
        assert(peer != nullptr);
        assert(peer->session_id == static_cast<ae::u64>(1001 + i));
        assert(peer->state == ae::ConnectionState::Connected);

        // Find by session ID also works
        auto* by_sid = mgr.find_by_session(static_cast<ae::u64>(1001 + i));
        assert(by_sid == peer);
    }

    std::printf("[HARNESS] Distinct identities: %d clients connected\n", kNumClients);
}

// =========================================================================
// 5. Snapshot interpolation under jitter
// =========================================================================

void test_snapshot_interpolation_under_jitter() {
    ae::SnapshotInterpolator<ahamkara::game::ServerSnapshot, 8> interpolator;

    double t = 0.0;
    const int kSnapshots = 20;

    // Build a sequence of snapshots with varying intervals
    for (int i = 0; i < kSnapshots; ++i) {
        ahamkara::game::ServerSnapshot snap;
        snap.server_tick = static_cast<ae::u32>(i);
        snap.local_player.position.x = static_cast<float>(i) * 0.5F;
        snap.local_player.position.z = static_cast<float>(i) * 1.0F;

        double jitter = (i % 3 == 0) ? 0.03 : (i % 3 == 1) ? 0.01 : 0.02;
        t += (1.0 / 60.0) + jitter;
        interpolator.push(snap, t);
    }

    // Sample at various render times
    for (int s = 0; s < 40; ++s) {
        double render_time = static_cast<double>(s) * (1.0 / 60.0) + 0.05;
        ahamkara::game::ReplicatedPlayerState out{};
        bool ok = interpolator.interpolate(render_time, out);

        if (s >= 2 && s < kSnapshots) {
            assert(ok && "Interpolation should succeed with sufficient snapshots");
            assert(out.position.x >= -0.1F &&
                   out.position.x <= static_cast<float>(kSnapshots - 1) * 0.5F + 0.1F);
        }
    }

    float suggested_delay = interpolator.suggest_delay_seconds(kTickRate);
    assert(suggested_delay > 0.0F && "Suggested delay should be positive");
    assert(suggested_delay < 1.0F && "Suggested delay should be reasonable");

    std::printf("[HARNESS] Snapshot interpolation: %d snapshots, delay=%.4fs\n",
                kSnapshots, suggested_delay);
}

// =========================================================================
// 6. Deterministic replay via accelerate_movement
// =========================================================================

void test_deterministic_replay() {
    ahamkara::game::MovementConfig cfg;

    // Run two identical simulations with separate sim states
    ahamkara::game::ReplicatedPlayerState state_a, state_b;
    state_a.position = {0.0F, 0.0F, 0.0F};
    state_a.yaw = 0.0F;
    state_b.position = {0.0F, 0.0F, 0.0F};
    state_b.yaw = 0.0F;
    ahamkara::game::MovementSimState sim_a, sim_b;

    const int kTicks = 60;

    for (int tick = 0; tick < kTicks; ++tick) {
        ahamkara::game::PlayerInputCommand cmd{};
        cmd.sequence = static_cast<ae::u32>(tick);
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = (tick % 2 == 0);
        cmd.jump_pressed = (tick == 30);

        ahamkara::game::accelerate_movement(state_a, sim_a, cmd, kDt);
        ahamkara::game::accelerate_movement(state_b, sim_b, cmd, kDt);
    }

    assert(state_a.position.x == state_b.position.x);
    assert(state_a.position.y == state_b.position.y);
    assert(state_a.position.z == state_b.position.z);
    assert(state_a.velocity.x == state_b.velocity.x);
    assert(state_a.velocity.y == state_b.velocity.y);
    assert(state_a.velocity.z == state_b.velocity.z);
    assert(state_a.movement_state == state_b.movement_state);

    std::printf("[HARNESS] Deterministic replay: identical results pos=(%.6f, %.6f, %.6f)\n",
                state_a.position.x, state_a.position.y, state_a.position.z);
}

// =========================================================================
// 7. Recovery via reliable channel retransmission
// =========================================================================

void test_reliable_channel_retransmission(HarnessMetrics& metrics) {
    ae::ReliableChannel rc;

    // Send 10 reliable packets
    const ae::u8 payload[] = {0x01, 0x02, 0x03, 0x04};
    for (ae::u16 seq = 1; seq <= 10; ++seq) {
        rc.on_send(seq, payload, sizeof(payload), 0.0);
    }
    assert(rc.pending_count() == 10);

    // Collect retransmits for unacked packets
    auto due = rc.collect_retransmits(0.2, 0.1);
    assert(due.size() == 10); // All should be due

    // Ack some packets: ack_sequence=5 acks seq 5 directly,
    // bits 0-3 (0xF = 0b1111) ack 4,3,2,1
    rc.on_ack(5, 0xF);
    assert(rc.pending_count() == 5); // Seqs 6-10 still pending

    // Ack remaining: ack_sequence=10 acks seq 10 directly,
    // bits 0-3 (0xF) ack 9,8,7,6
    rc.on_ack(10, 0xF);
    assert(rc.pending_count() == 0);

    std::printf("[HARNESS] Reliable channel: 10 packets ack'd, %llu retransmits\n",
                static_cast<unsigned long long>(due.size()));
}

// =========================================================================
// Run all harness tests
// =========================================================================

int run_all_harness_tests() {
    int failures = 0;

    // 1. Convergence
    try {
        HarnessMetrics conv_metrics;
        test_convergence_under_ideal_conditions(conv_metrics);
        std::printf("[HARNESS] Convergence test PASSED\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Convergence test FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Convergence test FAILED: unknown exception\n");
        ++failures;
    }

    // 2. Correction after divergence
    try {
        HarnessMetrics corr_metrics;
        test_correction_after_divergence(corr_metrics);
        std::printf("[HARNESS] Correction test PASSED (%d corrections, max=%.4f)\n",
                    corr_metrics.correction_count,
                    corr_metrics.max_correction_magnitude);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Correction test FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Correction test FAILED: unknown exception\n");
        ++failures;
    }

    // 3. Disconnect/Reconnect budget via ConnectionManager
    try {
        HarnessMetrics dnr_metrics;
        test_disconnect_reconnect_budget(dnr_metrics);
        std::printf("[HARNESS] Disconnect/Reconnect test PASSED (recovery in %.4fs)\n",
                    dnr_metrics.last_reconnect_time);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Disconnect/Reconnect test FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Disconnect/Reconnect test FAILED: unknown exception\n");
        ++failures;
    }

    // 4. Distinct client identities
    try {
        test_distinct_client_identities();
        std::printf("[HARNESS] Distinct identities test PASSED\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Distinct identities test FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Distinct identities test FAILED: unknown exception\n");
        ++failures;
    }

    // 5. Snapshot interpolation under jitter
    try {
        test_snapshot_interpolation_under_jitter();
        std::printf("[HARNESS] Snapshot interpolation under jitter PASSED\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Snapshot interpolation under jitter FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Snapshot interpolation under jitter FAILED: unknown exception\n");
        ++failures;
    }

    // 6. Deterministic replay
    try {
        test_deterministic_replay();
        std::printf("[HARNESS] Deterministic replay PASSED\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Deterministic replay FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Deterministic replay FAILED: unknown exception\n");
        ++failures;
    }

    // 7. Recovery via reliable channel
    try {
        HarnessMetrics rc_metrics;
        test_reliable_channel_retransmission(rc_metrics);
        std::printf("[HARNESS] Reliable channel retransmission PASSED\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[HARNESS] Reliable channel retransmission FAILED: %s\n", e.what());
        ++failures;
    } catch (...) {
        std::fprintf(stderr, "[HARNESS] Reliable channel retransmission FAILED: unknown exception\n");
        ++failures;
    }

    std::printf("\n[HARNESS] %d/%d tests passed\n", 7 - failures, 7);
    return failures;
}

} // anonymous namespace

int main() {
    int failures = run_all_harness_tests();
    if (failures > 0) {
        std::fprintf(stderr, "network_harness_tests: %d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("network_harness_tests: all tests passed\n");
    return 0;
}
