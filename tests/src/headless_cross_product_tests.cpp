#include "ae/core/time.h"
#include "ae/network/connection.h"
#include "ae/network/reliable_channel.h"
#include "ae/network/snapshot_interpolator.h"
#include "ahamkara/game/client_prediction.h"
#include "ahamkara/game/game_module.h"
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
constexpr int kMaxPlayers = 4;

// =========================================================================
// Test configuration – exercises product boundaries:
//   Ahamkara (engine/network)  →  Wish (session/identity)
//   Ahamkara (engine/network)  →  Flashback (game/world)
// =========================================================================

struct ProductBoundaryTestConfig {
    const char* name;
    const char* description;
    bool (*test_fn)();
};

// =========================================================================
// 1. Ahamkara Engine Boundary: ConnectionManager with distinct identities
// =========================================================================

bool test_connection_manager_distinct_identities() {
    ae::ConnectionManager mgr;

    auto base = ae::ConnectionManager::time_point{};
    const ae::NetAddress addr1{"127.0.0.1", 20001};
    const ae::NetAddress addr2{"127.0.0.1", 20002};

    // Two distinct peers connect
    auto& peer1 = mgr.connect_request(addr1, base);
    auto& peer2 = mgr.connect_request(addr2, base);

    assert(peer1.state == ae::ConnectionState::Handshaking);
    assert(peer2.state == ae::ConnectionState::Handshaking);
    assert(peer1.address.port == 20001);
    assert(peer2.address.port == 20002);

    // Complete handshakes with distinct session IDs
    assert(mgr.complete_handshake(addr1, 1001, base));
    assert(mgr.complete_handshake(addr2, 1002, base));

    assert(mgr.connected_count() == 2);

    // Verify distinct session IDs
    auto* found1 = mgr.find(addr1);
    auto* found2 = mgr.find(addr2);
    assert(found1 != nullptr && found2 != nullptr);
    assert(found1->session_id == 1001);
    assert(found2->session_id == 1002);
    assert(found1 != found2);

    // Find by session ID
    auto* by_sid1 = mgr.find_by_session(1001);
    auto* by_sid2 = mgr.find_by_session(1002);
    assert(by_sid1 != nullptr && by_sid2 != nullptr);
    assert(by_sid1->address.port == addr1.port);
    assert(by_sid2->address.port == addr2.port);

    std::printf("[CROSS-PRODUCT] ConnectionManager distinct identities: 2 peers, sessions %llu and %llu\n",
                static_cast<unsigned long long>(found1->session_id),
                static_cast<unsigned long long>(found2->session_id));
    return true;
}

// =========================================================================
// 2. Wish Boundary: ReliableChannel per-session tracking
// =========================================================================

bool test_reliable_channel_per_session() {
    // Simulate two sessions each with their own reliable channel
    ae::ReliableChannel channel_a;
    ae::ReliableChannel channel_b;

    const ae::u8 payload[] = {0x01, 0x02, 0x03, 0x04};

    // Session A sends packets
    channel_a.on_send(1, payload, sizeof(payload), 0.0);
    channel_a.on_send(2, payload, sizeof(payload), 0.0);
    assert(channel_a.pending_count() == 2);

    // Session B sends packets (independent tracking)
    channel_b.on_send(1, payload, sizeof(payload), 0.0);
    channel_b.on_send(2, payload, sizeof(payload), 0.0);
    channel_b.on_send(3, payload, sizeof(payload), 0.0);
    assert(channel_b.pending_count() == 3);

    // ACK only affects the correct channel
    channel_a.on_ack(2, 0x1u); // Ack seq 1 and 2 for session A
    assert(channel_a.pending_count() == 0);
    assert(channel_b.pending_count() == 3); // B unaffected

    // Retransmit check per channel
    auto due_b = channel_b.collect_retransmits(0.2, 0.1);
    assert(due_b.size() == 3); // All B packets are due

    // A should have nothing pending
    auto due_a = channel_a.collect_retransmits(0.2, 0.1);
    assert(due_a.empty());

    std::printf("[CROSS-PRODUCT] ReliableChannel per-session: A=%zu pending, B=%zu pending (after ack)\n",
                channel_a.pending_count(), channel_b.pending_count());
    return true;
}

// =========================================================================
// 3. Flashback Boundary: World with multiple player identities
// =========================================================================

bool test_world_multiple_player_identities() {
    ahamkara::game::World world;
    world.set_is_client(false);

    // Add multiple players with distinct identities
    ae::u32 p1 = world.add_player();
    ae::u32 p2 = world.add_player();
    ae::u32 p3 = world.add_player();

    assert(p1 == 1 || p1 == 0); // Either 0-indexed or 1-indexed
    assert(p2 != p1);
    assert(p3 != p2);

    assert(world.player_count() >= 3);

    // Verify each player slot is accessible and has valid state
    auto* player1 = world.get_player(p1);
    auto* player2 = world.get_player(p2);
    auto* player3 = world.get_player(p3);

    assert(player1 != nullptr);
    assert(player2 != nullptr);
    assert(player3 != nullptr);
    assert(player1 != player2 && "Players should be distinct objects");
    assert(player2 != player3 && "Players should be distinct objects");

    // Each player starts with default spawn health
    assert(player1->state().health > 0.0F);
    assert(player2->state().health > 0.0F);
    assert(player3->state().health > 0.0F);

    // Set distinct positions via player state (simulating server authority)
    player1->state().position = {0.0F, 0.0F, 0.0F};
    player2->state().position = {5.0F, 0.0F, 0.0F};
    player3->state().position = {10.0F, 0.0F, 0.0F};

    // Verify each identity can be queried via get_player and has correct data
    auto* q1 = world.get_player(p1);
    auto* q2 = world.get_player(p2);
    auto* q3 = world.get_player(p3);
    assert(q1->state().position.x == 0.0F);
    assert(q2->state().position.x == 5.0F);
    assert(q3->state().position.x == 10.0F);

    // Remove a player and verify count decreases
    std::printf("[CROSS-PRODUCT] Before remove: count=%u p1=%u p2=%u p3=%u\n",
                static_cast<unsigned>(world.player_count()),
                static_cast<unsigned>(p1), static_cast<unsigned>(p2), static_cast<unsigned>(p3));
    world.remove_player(p1);
    std::printf("[CROSS-PRODUCT] After remove: count=%u\n",
                static_cast<unsigned>(world.player_count()));
    assert(world.player_count() < 4 && "Player count should decrease after removal");

    std::printf("[CROSS-PRODUCT] World multiple player identities: %u active, identities preserved after removal\n",
                static_cast<unsigned>(world.player_count()));
    return true;
}

// =========================================================================
// 4. ClientPredictionManager with distinct player identity
// =========================================================================

bool test_client_prediction_distinct_identity() {
    ahamkara::game::ClientPredictionManager pred;
    assert(pred.pending_count() == 0);
    assert(pred.last_acknowledged() == 0);

    // Simulate a sequence of inputs that a distinct identity would produce
    const int kInputs = 30;
    ae::u32 last_processed = 0;

    for (int i = 0; i < kInputs; ++i) {
        ahamkara::game::PlayerInputCommand cmd{};
        cmd.sequence = static_cast<ae::u32>(i);
        cmd.client_tick = static_cast<ae::u32>(i);
        cmd.client_time = static_cast<float>(i) * kDt;
        cmd.move_axis.y = 1.0F;
        cmd.sprint_held = (i % 2 == 0);
        cmd.weapon_slot = 1;

        pred.apply_input(cmd);
    }

    assert(pred.pending_count() == kInputs);
    assert(pred.prediction_tick() == static_cast<ae::u32>(kInputs));

    // Create an authoritative snapshot to reconcile against
    ahamkara::game::ServerSnapshot auth_snap;
    auth_snap.server_tick = static_cast<ae::u32>(kInputs);
    auth_snap.last_processed_input = static_cast<ae::u32>(kInputs - 5);
    auth_snap.local_player = pred.world().get_player_state();

    // Reconcile
    pred.reconcile(auth_snap);

    // After reconciliation, acknowledged inputs should be cleared
    assert(pred.pending_count() == 4); // Inputs 26-29 unprocessed (4 remaining)
    assert(pred.last_acknowledged() == static_cast<ae::u32>(kInputs - 5)); // last_processed = 25

    // Verify prediction state exists (player is alive)
    const auto& state = pred.world().get_player_state();
    assert(state.health > 0.0F && "Predicted player should have health");

    std::printf("[CROSS-PRODUCT] ClientPrediction distinct identity: %d pending after reconcile\n",
                pred.pending_count());
    return true;
}

// =========================================================================
// 5. SnapshotInterpolator with distinct player identity
// =========================================================================

bool test_snapshot_interpolator_distinct_identity() {
    ae::SnapshotInterpolator<ahamkara::game::ServerSnapshot, 8> interpolator;

    // Simulate a distinct player identity receiving snapshots
    double t = 0.0;
    const double dt = 1.0 / 60.0;

    for (int i = 0; i < 10; ++i) {
        ahamkara::game::ServerSnapshot snap;
        snap.server_tick = static_cast<ae::u32>(i);
        snap.local_player.player_id = 1;
        snap.local_player.network_object_id = 42;
        snap.local_player.position.x = static_cast<float>(i) * 0.1F;
        snap.local_player.position.z = static_cast<float>(i) * 0.5F;
        snap.local_player.velocity.z = 6.0F;

        t += dt;
        interpolator.push(snap, t);
    }

    // Verify the interpolator preserves the player identity
    ahamkara::game::ReplicatedPlayerState out{};
    bool ok = interpolator.interpolate(t * 0.5, out);
    assert(ok && "Interpolation should succeed");

    // The player_id and network_object_id should be preserved from snapshots
    assert(out.player_id == 1 && "Player identity should be preserved");
    assert(out.network_object_id == 42 && "Network object identity should be preserved");

    // Position should be interpolated between bracketing snapshots
    assert(out.position.z > 0.0F && "Interpolated position should have forward movement");
    assert(out.position.z < 5.0F && "Interpolated position should be bounded");

    std::printf("[CROSS-PRODUCT] SnapshotInterpolator distinct identity: player_id=%u, obj_id=%u\n",
                out.player_id, out.network_object_id);
    return true;
}

// =========================================================================
// 6. Game module boundary: game module config/hooks
// =========================================================================

bool test_game_module_boundary() {
    // Verify game module provides required product boundary hooks
    const char* name = ahamkara::game::game_name();
    assert(name != nullptr && "Game module should provide a name");
    assert(std::strlen(name) > 0 && "Game module name should not be empty");

    // Verify config variables are registered without crashing
    ahamkara::game::register_game_config();

    // Verify movement tuning functions return sensible defaults
    float walk_speed = ahamkara::game::cfg_walk_speed();
    float sprint_speed = ahamkara::game::cfg_sprint_speed();
    float jump_speed = ahamkara::game::cfg_jump_speed();
    float gravity = ahamkara::game::cfg_gravity();

    assert(walk_speed > 0.0F && "Walk speed should be positive");
    assert(sprint_speed > walk_speed && "Sprint speed should exceed walk speed");
    assert(jump_speed > 0.0F && "Jump speed should be positive");
    assert(gravity > 0.0F && "Gravity should be positive");

    std::printf("[CROSS-PRODUCT] Game module boundary: %s, walk=%.1f, sprint=%.1f, jump=%.1f, gravity=%.1f\n",
                name, walk_speed, sprint_speed, jump_speed, gravity);
    return true;
}

// =========================================================================
// 7. Connection lifecycle boundary: disconnect and grace period
// =========================================================================

bool test_connection_lifecycle_boundary() {
    ae::ConnectionManager mgr;
    auto base = ae::ConnectionManager::time_point{};

    const ae::NetAddress addr{"127.0.0.1", 30001};

    // Connect
    mgr.connect_request(addr, base);
    assert(mgr.complete_handshake(addr, 42, base));
    assert(mgr.connected_count() == 1);

    // Grace period (threshold is > max, so need max+1 = 4 misses)
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.tick(base);

    auto* peer = mgr.find(addr);
    assert(peer != nullptr);
    assert(peer->state == ae::ConnectionState::GracePeriod);

    // Reconnect restores identity
    auto& reconnected = mgr.connect_request(addr, base + std::chrono::milliseconds(100));
    assert(reconnected.state == ae::ConnectionState::Connected);
    assert(reconnected.session_id == 42);
    assert(reconnected.preserves_session);

    // Disconnect
    mgr.disconnect(addr);
    mgr.tick(base + std::chrono::milliseconds(200));
    assert(mgr.find(addr) == nullptr); // Removed after tick

    std::printf("[CROSS-PRODUCT] Connection lifecycle boundary: connect→grace→reconnect→disconnect\n");
    return true;
}

// =========================================================================
// 8. Seat/player slot identity through World
// =========================================================================

bool test_player_seat_identity() {
    ahamkara::game::World world;
    world.set_is_client(false);

    // Add players and verify they get sequential, stable identities
    ae::u32 seats[kMaxPlayers];
    for (int i = 0; i < kMaxPlayers; ++i) {
        seats[i] = world.add_player();
    }

    // World starts with 1 player from constructor, so total is 1 + kMaxPlayers
    const ae::u32 expected_count = 1 + static_cast<ae::u32>(kMaxPlayers);
    assert(world.player_count() == expected_count);

    // Verify each seat has a unique and valid identity
    for (int i = 0; i < kMaxPlayers; ++i) {
        auto* player = world.get_player(seats[i]);
        assert(player != nullptr && "Each seat should map to a valid player");

        // Each player should have a unique identifier
        for (int j = i + 1; j < kMaxPlayers; ++j) {
            auto* other = world.get_player(seats[j]);
            assert(other != nullptr);

            // The player objects should be at different addresses
            assert(player != other);
        }
    }

    // Remove a player and verify the slot is reclaimed
    world.remove_player(seats[1]);
    assert(world.player_count() == expected_count - 1);

    std::printf("[CROSS-PRODUCT] Player seat identity: %d seats, %u active after removal\n",
                kMaxPlayers, static_cast<unsigned>(world.player_count()));
    return true;
}

// =========================================================================
// 9. Serialization boundary: packets preserve identity across wire
// =========================================================================

bool test_serialization_preserves_identity() {
    // Verify that player identity fields survive serialization round-trips

    // Test: ClientHello with distinct auth token
    {
        ahamkara::game::ClientHelloPacket source{};
        const char* token = "distinct-player-identity-token-abc123";
        source.auth_token_length = static_cast<ae::u16>(std::strlen(token));
        std::memcpy(source.auth_token, token, source.auth_token_length);

        ahamkara::game::ClientHelloPacketBuffer buffer{};
        ahamkara::game::PacketEnvelope env{};
        env.sequence = 1;

        assert(ahamkara::game::serialize_client_hello_packet(env, source, buffer));

        ahamkara::game::PacketEnvelope decoded_env{};
        ahamkara::game::ClientHelloPacket decoded{};
        assert(ahamkara::game::deserialize_client_hello_packet(buffer, decoded_env, decoded));

        assert(decoded.auth_token_length == source.auth_token_length);
        assert(std::memcmp(decoded.auth_token, source.auth_token, source.auth_token_length) == 0);
    }

    // Test: ServerWelcome with distinct player ID
    {
        ahamkara::game::ServerWelcomePacket source{};
        std::snprintf(source.player_id, ahamkara::game::kMaxPlayerIdLength, "%s", "player-identity-999");

        ahamkara::game::ServerWelcomePacketBuffer buffer{};
        ahamkara::game::PacketEnvelope env{};
        env.sequence = 2;

        assert(ahamkara::game::serialize_server_welcome_packet(env, source, buffer));

        ahamkara::game::PacketEnvelope decoded_env{};
        ahamkara::game::ServerWelcomePacket decoded{};
        assert(ahamkara::game::deserialize_server_welcome_packet(buffer, decoded_env, decoded));

        assert(std::strncmp(decoded.player_id, "player-identity-999", ahamkara::game::kMaxPlayerIdLength) == 0);
    }

    std::printf("[CROSS-PRODUCT] Serialization preserves identity: ClientHello + ServerWelcome\n");
    return true;
}

// =========================================================================
// Run all cross-product smoke tests
// =========================================================================

const ProductBoundaryTestConfig kAllTests[] = {
    {"ConnectionManager", "Ahamkara engine: distinct peer identities in ConnectionManager",
     test_connection_manager_distinct_identities},
    {"ReliableChannel", "Wish boundary: per-session reliable channel tracking",
     test_reliable_channel_per_session},
    {"World", "Flashback boundary: multiple player identities in World",
     test_world_multiple_player_identities},
    {"ClientPrediction", "Ahamkara→Flashback: ClientPredictionManager with distinct identity",
     test_client_prediction_distinct_identity},
    {"SnapshotInterpolator", "Ahamkara engine: SnapshotInterpolator preserves player identity",
     test_snapshot_interpolator_distinct_identity},
    {"GameModule", "Flashback boundary: game module config/hooks",
     test_game_module_boundary},
    {"ConnectionLifecycle", "Ahamkara engine: connect→grace→reconnect→disconnect lifecycle",
     test_connection_lifecycle_boundary},
    {"PlayerSeats", "Flashback boundary: player seat identity through World",
     test_player_seat_identity},
    {"Serialization", "Ahamkara↔Flashback: packet serialization preserves identity",
     test_serialization_preserves_identity},
};

constexpr int kNumTests = sizeof(kAllTests) / sizeof(kAllTests[0]);

int run_all_cross_product_tests() {
    int passed = 0;
    int failed = 0;

    std::printf("\n=== Headless Cross-Product Smoke Tests ===\n\n");

    for (int i = 0; i < kNumTests; ++i) {
        std::printf("[%d/%d] %s: %s ... ",
                    i + 1, kNumTests,
                    kAllTests[i].name,
                    kAllTests[i].description);

        try {
            bool ok = kAllTests[i].test_fn();
            if (ok) {
                std::printf("PASSED\n");
                ++passed;
            } else {
                std::printf("FAILED (returned false)\n");
                ++failed;
            }
        } catch (const std::exception& e) {
            std::printf("FAILED (exception: %s)\n", e.what());
            ++failed;
        } catch (...) {
            std::printf("FAILED (unknown exception)\n");
            ++failed;
        }
    }

    std::printf("\n=== Results: %d/%d passed, %d failed ===\n\n",
                passed, kNumTests, failed);

    return failed;
}

} // anonymous namespace

int main() {
    int failures = run_all_cross_product_tests();
    if (failures > 0) {
        std::fprintf(stderr, "headless_cross_product_tests: %d test(s) FAILED\n", failures);
        return 1;
    }
    std::printf("headless_cross_product_tests: all tests passed\n");
    return 0;
}
