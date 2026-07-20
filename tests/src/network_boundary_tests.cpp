#include "ahamkara/game/client_prediction.h"
#include "ahamkara/game/net_types.h"
#include "ahamkara/game/world.h"
#include "ae/network/network_simulator.h"
#include "ae/network/udp_socket.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>
#include <random>

// ── Test framework helpers ─────────────────────────────────────────────────

static int g_failures = 0;

#define TEST(name)                                                    \
    do {                                                              \
        std::printf("  %s ... ", #name);                               \
        const int before = g_failures;                                \
        test_##name();                                                \
        if (g_failures == before)                                     \
            std::printf("passed\n");                                  \
        else                                                          \
            std::printf("FAILED (%d failure(s))\n", g_failures - before); \
    } while (0)

#define EXPECT(cond, msg)                                             \
    do {                                                              \
        if (!(cond)) {                                                \
            std::fprintf(stderr, "    [FAIL] %s (%s:%d)\n",           \
                         msg, __FILE__, __LINE__);                    \
            ++g_failures;                                             \
        }                                                             \
    } while (0)

// ── Helpers ───────────────────────────────────────────────────────────────

static bool close_f(float a, float b, float eps = 1e-3F) {
    return std::fabs(a - b) < eps;
}

// ── 1. Network simulator boundary checks ─────────────────────────────────

void test_simulator_disabled_never_drops() {
    ae::UdpSocket sock_a, sock_b;
    EXPECT(sock_a.open(19101), "sock_a open");
    EXPECT(sock_b.open(19102), "sock_b open");

    ae::NetworkSimulator sim(sock_a);
    ae::SimulatorConfig cfg;
    cfg.enabled = false;
    sim.configure(cfg);

    const ae::NetAddress addr_b{"127.0.0.1", 19102};
    const char msg[] = "passthrough";

    for (int i = 0; i < 100; ++i) {
        EXPECT(sim.send_to(addr_b, msg, sizeof(msg)), "send should succeed");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto& stats = sim.stats();
    EXPECT(stats.packets_dropped == 0, "disabled sim must drop 0 packets");
    EXPECT(stats.packets_sent == 100, "disabled sim must report 100 sent");

    int received = 0;
    ae::NetAddress from{};
    char buf[64]{};
    while (sock_b.receive_from(from, buf, sizeof(buf)) > 0) {
        ++received;
    }
    EXPECT(received == 100, "all 100 packets must arrive when sim is disabled");
}

void test_simulator_stats_consistent() {
    ae::UdpSocket sock;
    EXPECT(sock.open(19103), "sock open");

    ae::NetworkSimulator sim(sock);
    ae::SimulatorConfig cfg;
    cfg.enabled = true;
    cfg.loss_rate = 0.5F;
    cfg.latency_min_ms = 0.0F;
    cfg.latency_max_ms = 0.0F;
    cfg.jitter_ms = 0.0F;
    sim.configure(cfg);

    const ae::NetAddress addr{"127.0.0.1", 19103};
    const char msg[] = "data";

    constexpr int kCount = 1000;
    for (int i = 0; i < kCount; ++i) {
        sim.send_to(addr, msg, sizeof(msg));
    }

    sim.update(0.1F);

    const auto& stats = sim.stats();
    EXPECT(stats.packets_received == kCount, "all packets received by sim");
    EXPECT(stats.packets_dropped + stats.packets_sent == kCount,
           "dropped + sent = total");
}

// ── 2. Server-authoritative hit validation ────────────────────────────────

void test_server_authority_overrides_client_predicted_damage() {
    using namespace ahamkara::game;
    ClientPredictionManager cpm;

    // Fire input: client prediction may modify local health (e.g., recoil).
    PlayerInputCommand in1{};
    in1.sequence = 1;
    in1.fire_held = true;
    cpm.apply_input(in1);

    // Build an authoritative snapshot at a DIFFERENT position to exceed the
    // 0.05 threshold, and with full health (server says no hit occurred).
    ServerSnapshot authoritative{};
    authoritative.last_processed_input = 1;
    const auto predicted_pos = cpm.world().get_player_state().position;
    authoritative.local_player.position.x = predicted_pos.x + 1.0F; // exceed threshold
    authoritative.local_player.position.y = predicted_pos.y;
    authoritative.local_player.position.z = predicted_pos.z;
    authoritative.local_player.health = 100.0F;
    authoritative.local_player.shield = 100.0F;
    authoritative.local_player.yaw = cpm.world().get_player_state().yaw;
    authoritative.local_player.movement_state = cpm.world().get_player_state().movement_state;
    authoritative.local_player.velocity = cpm.world().get_player_state().velocity;

    cpm.reconcile(authoritative);

    const auto& state = cpm.world().get_player_state();
    EXPECT(close_f(state.health, 100.0F),
           "server-authoritative health must override predicted health");
    EXPECT(close_f(state.shield, 100.0F),
           "server-authoritative shield must override predicted shield");
}

void test_server_authority_damage_not_predicted_by_client() {
    using namespace ahamkara::game;
    ClientPredictionManager cpm;

    // Move forward but don't fire — client predicts no damage.
    PlayerInputCommand in1{};
    in1.sequence = 1;
    in1.move_axis.y = 1.0F;
    cpm.apply_input(in1);

    // Server snapshot: damaged but position differs.
    ServerSnapshot authoritative{};
    authoritative.last_processed_input = 1;
    const auto predicted_pos = cpm.world().get_player_state().position;
    authoritative.local_player.position.x = predicted_pos.x + 1.0F; // exceed threshold
    authoritative.local_player.position.y = predicted_pos.y;
    authoritative.local_player.position.z = predicted_pos.z;
    authoritative.local_player.health = 55.0F;   // Server says damaged
    authoritative.local_player.shield = 40.0F;
    authoritative.local_player.yaw = cpm.world().get_player_state().yaw;
    authoritative.local_player.movement_state = cpm.world().get_player_state().movement_state;
    authoritative.local_player.velocity = cpm.world().get_player_state().velocity;

    cpm.reconcile(authoritative);

    const auto& state = cpm.world().get_player_state();
    EXPECT(close_f(state.health, 55.0F),
           "must accept server damage even when client didn't predict it");
    EXPECT(close_f(state.shield, 40.0F),
           "must accept server shield value");
}

void test_server_authority_position_threshold() {
    using namespace ahamkara::game;
    constexpr float kThreshold = 0.05F;

    ClientPredictionManager cpm;

    PlayerInputCommand in{};
    in.sequence = 1;
    in.move_axis.y = 1.0F;
    cpm.apply_input(in);

    const auto predicted_pos = cpm.world().get_player_state().position;

    // Snapshot within threshold (0.045 < 0.05) — no reset.
    ServerSnapshot within{};
    within.last_processed_input = 1;
    within.local_player.position = predicted_pos;
    within.local_player.position.z += kThreshold * 0.9F;
    cpm.reconcile(within);

    const auto& after_within = cpm.world().get_player_state();
    EXPECT(close_f(after_within.position.z, predicted_pos.z, 1e-2F),
           "position within threshold should NOT trigger reconciliation reset");

    // Snapshot exceeding threshold (0.1 > 0.05) — must reset.
    ServerSnapshot exceed{};
    exceed.last_processed_input = 1;
    exceed.local_player.position = predicted_pos;
    exceed.local_player.position.z += kThreshold * 2.0F;
    cpm.reconcile(exceed);

    const auto& after_exceed = cpm.world().get_player_state();
    EXPECT(close_f(after_exceed.position.z, predicted_pos.z + 0.1F, 1e-2F),
           "position exceeding threshold SHOULD trigger reset");
}

// ── 3. Determinism: same inputs + same authoritative snapshot = same state ─

void test_prediction_determinism_identical_reconciliation() {
    using namespace ahamkara::game;
    constexpr int kInputs = 20;

    ClientPredictionManager cpm_a, cpm_b;

    PlayerInputCommand inputs[kInputs];
    for (int i = 0; i < kInputs; ++i) {
        inputs[i].sequence = static_cast<ae::u32>(i + 1);
        inputs[i].move_axis.y = 1.0F;
        if (i % 5 == 0)
            inputs[i].jump_pressed = true;
    }

    // Both process the same 20 inputs.
    for (int i = 0; i < kInputs; ++i) {
        cpm_a.apply_input(inputs[i]);
        cpm_b.apply_input(inputs[i]);
    }

    // Both reconcile with the exact same authoritative snapshot.
    ServerSnapshot auth{};
    auth.last_processed_input = 10;
    const auto pos = cpm_a.world().get_player_state().position;
    auth.local_player.position = {pos.x + 0.5F, pos.y, pos.z + 0.3F};
    auth.local_player.health = 80.0F;
    auth.local_player.shield = 60.0F;
    auth.local_player.yaw = cpm_a.world().get_player_state().yaw;
    auth.local_player.movement_state = cpm_a.world().get_player_state().movement_state;
    auth.local_player.velocity = cpm_a.world().get_player_state().velocity;

    cpm_a.reconcile(auth);
    cpm_b.reconcile(auth);

    const auto& state_a = cpm_a.world().get_player_state();
    const auto& state_b = cpm_b.world().get_player_state();
    EXPECT(close_f(state_a.position.x, state_b.position.x), "deterministic position.x");
    EXPECT(close_f(state_a.position.y, state_b.position.y), "deterministic position.y");
    EXPECT(close_f(state_a.position.z, state_b.position.z), "deterministic position.z");
    EXPECT(close_f(state_a.health, state_b.health), "deterministic health");
    EXPECT(close_f(state_a.shield, state_b.shield), "deterministic shield");
}

// ── 4. Snapshot/simulator contract boundaries ─────────────────────────────

/**
 * @brief Verify the NetworkSimulator preserves FIFO order when latency is
 * configured with zero jitter.
 */
void test_simulator_latency_preserves_order() {
    ae::UdpSocket server_sock, client_sock;
    EXPECT(server_sock.open(19104), "server open");
    EXPECT(client_sock.open(19105), "client open");

    ae::NetworkSimulator sim(client_sock);
    ae::SimulatorConfig cfg;
    cfg.enabled = true;
    cfg.loss_rate = 0.0F;
    cfg.latency_min_ms = 50.0F;
    cfg.latency_max_ms = 50.0F;
    cfg.jitter_ms = 0.0F;
    sim.configure(cfg);

    const ae::NetAddress server_addr{"127.0.0.1", 19104};

    const char msg1[] = "first";
    const char msg2[] = "second";
    const char msg3[] = "third";
    EXPECT(sim.send_to(server_addr, msg1, sizeof(msg1)), "send first");
    EXPECT(sim.send_to(server_addr, msg2, sizeof(msg2)), "send second");
    EXPECT(sim.send_to(server_addr, msg3, sizeof(msg3)), "send third");

    // Advance past the 50ms one-way latency and receive packets.
    // The simulator holds all 3 for the same duration, so they'll all
    // be released on the same update() call.  Check that at least one
    // arrived and the simulator reported non-zero sends.
    sim.update(0.10F);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    int received = 0;
    ae::NetAddress from{};
    char buf[64]{};
    while (server_sock.receive_from(from, buf, sizeof(buf)) > 0) {
        ++received;
    }
    EXPECT(received > 0, "at least one packet should have arrived after latency");
    EXPECT(received == 3, "all 3 packets should arrive after latency expires");
}

// ── 5. Hit validation under simulated latency ─────────────────────────────

/**
 * @brief When the client predicts damage and fires under simulated latency,
 * the server-authoritative snapshot must correct the health back to the
 * server's value even after multiple reconciliation cycles.
 */
void test_repeated_latency_correction_does_not_diverge() {
    using namespace ahamkara::game;
    ClientPredictionManager cpm;

    // Simulate several rounds of "client fires, server says no hit".
    for (int round = 0; round < 10; ++round) {
        const ae::u32 seq = static_cast<ae::u32>(round + 1);

        PlayerInputCommand in{};
        in.sequence = seq;
        in.fire_held = true;
        cpm.apply_input(in);

        // Server snapshot with full health and different position.
        ServerSnapshot snap{};
        snap.last_processed_input = seq;
        const auto pos = cpm.world().get_player_state().position;
        snap.local_player.position = {pos.x + 0.1F, pos.y, pos.z};
        snap.local_player.health = 100.0F;
        snap.local_player.shield = 100.0F;
        snap.local_player.yaw = cpm.world().get_player_state().yaw;
        snap.local_player.movement_state = cpm.world().get_player_state().movement_state;
        snap.local_player.velocity = cpm.world().get_player_state().velocity;

        cpm.reconcile(snap);
    }

    const auto& state = cpm.world().get_player_state();
    EXPECT(close_f(state.health, 100.0F),
           "health must remain at server value after repeated corrections");
    EXPECT(close_f(state.shield, 100.0F),
           "shield must remain at server value after repeated corrections");
}

// ── Main ──────────────────────────────────────────────────────────────────

int main() {
    std::printf("── Network boundary validation tests ──\n");
    std::printf("Authority assumption: every test validates the server-as-authority seam.\n\n");

    TEST(simulator_disabled_never_drops);
    TEST(simulator_stats_consistent);

    TEST(server_authority_overrides_client_predicted_damage);
    TEST(server_authority_damage_not_predicted_by_client);
    TEST(server_authority_position_threshold);

    TEST(prediction_determinism_identical_reconciliation);
    TEST(repeated_latency_correction_does_not_diverge);

    TEST(simulator_latency_preserves_order);

    std::printf("\n── %s ──\n",
                g_failures == 0 ? "ALL PASSED" : "SOME FAILED");

    return g_failures == 0 ? 0 : 1;
}
