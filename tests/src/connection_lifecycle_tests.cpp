#include "ae/network/connection.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

using clock = ae::ConnectionManager::clock;
using time_point = clock::time_point;

// ── Helpers ──────────────────────────────────────────────────────────────────

int fail(const std::string& msg) {
    std::cerr << "connection_lifecycle_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

void expect_state(const ae::PeerConnection* peer, ae::ConnectionState expected) {
    if (!peer) {
        fail("peer is null");
        return;
    }
    if (peer->state != expected) {
        std::cerr << "  expected state " << ae::connection_state_name(expected)
                  << " but got " << ae::connection_state_name(peer->state) << '\n';
    }
}

// ── Tests ────────────────────────────────────────────────────────────────────

int test_new_peer_starts_handshaking() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10001};

    auto& peer = mgr.connect_request(addr, base);
    EXPECT(peer.state == ae::ConnectionState::Handshaking, "new peer should be Handshaking");
    EXPECT(mgr.count() == 1, "should have 1 peer");
    EXPECT(mgr.count_by_state(ae::ConnectionState::Handshaking) == 1, "1 handshaking");
    return 0;
}

int test_complete_handshake_transitions_to_connected() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10002};

    mgr.connect_request(addr, base);
    EXPECT(mgr.complete_handshake(addr, 42, base), "handshake should succeed");
    EXPECT(mgr.count_by_state(ae::ConnectionState::Connected) == 1, "1 connected");
    EXPECT(mgr.connected_count() == 1, "connected_count == 1");

    auto* peer = mgr.find(addr);
    EXPECT(peer != nullptr, "peer should exist");
    expect_state(peer, ae::ConnectionState::Connected);
    EXPECT(peer->session_id == 42, "session_id should be 42");
    return 0;
}

int test_handshake_from_wrong_state_fails() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10003};

    // No peer at all.
    EXPECT(!mgr.complete_handshake(addr, 1, base), "handshake on unknown addr should fail");

    // Peer already connected.
    mgr.connect_request(addr, base);
    mgr.complete_handshake(addr, 1, base);
    EXPECT(!mgr.complete_handshake(addr, 2, base), "handshake on connected peer should fail");
    return 0;
}

int test_disconnect_transitions_to_disconnecting() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10004};

    mgr.connect_request(addr, base);
    mgr.complete_handshake(addr, 1, base);

    EXPECT(mgr.disconnect(addr), "disconnect should succeed");
    auto* peer = mgr.find(addr);
    EXPECT(peer != nullptr, "peer should still exist before tick");
    expect_state(peer, ae::ConnectionState::Disconnecting);
    return 0;
}

int test_disconnect_from_wrong_state_fails() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10005};

    // Not connected.
    EXPECT(!mgr.disconnect(addr), "disconnect on unknown addr should fail");

    // Handshaking only.
    mgr.connect_request(addr, base);
    EXPECT(!mgr.disconnect(addr), "disconnect on handshaking peer should fail");
    return 0;
}

int test_tick_removes_handshake_timeout() {
    ae::ConnectionManager mgr;
    mgr.set_handshake_timeout(std::chrono::milliseconds(100));
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10006};

    mgr.connect_request(addr, base);
    EXPECT(mgr.count() == 1, "peer exists");

    // Tick before timeout: peer should remain.
    mgr.tick(base + std::chrono::milliseconds(50));
    EXPECT(mgr.count() == 1, "peer should still exist before timeout");

    // Tick after timeout: peer should be removed.
    mgr.tick(base + std::chrono::milliseconds(200));
    EXPECT(mgr.count() == 0, "peer should be removed after handshake timeout");
    return 0;
}

int test_grace_period_and_reconnect() {
    ae::ConnectionManager mgr;
    mgr.set_handshake_timeout(std::chrono::seconds(5));
    mgr.set_grace_period(std::chrono::seconds(10));
    mgr.set_max_missed_heartbeats(2);

    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10007};

    mgr.connect_request(addr, base);
    mgr.complete_handshake(addr, 1, base);
    EXPECT(mgr.connected_count() == 1, "1 connected");

    // Simulate missed heartbeats → GracePeriod.
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr); // exceeds max_missed_heartbeats (2)
    mgr.tick(base + std::chrono::milliseconds(100));

    EXPECT(mgr.count() == 1, "peer should still exist in grace period");
    EXPECT(mgr.count_by_state(ae::ConnectionState::GracePeriod) == 1, "1 in grace period");
    EXPECT(mgr.connected_count() == 0, "0 connected");

    // Reconnect: client sends connect_request (handles GracePeriod → Connected).
    auto& reconnected = mgr.connect_request(addr, base + std::chrono::seconds(1));
    EXPECT(reconnected.state == ae::ConnectionState::Connected,
           "reconnect should restore Connected state");
    EXPECT(reconnected.preserves_session, "reconnect should set preserves_session flag");
    EXPECT(reconnected.session_id == 1, "session_id should be preserved");
    return 0;
}

int test_grace_expiry_removes_peer() {
    ae::ConnectionManager mgr;
    mgr.set_handshake_timeout(std::chrono::seconds(1));
    mgr.set_grace_period(std::chrono::milliseconds(100));
    mgr.set_max_missed_heartbeats(1);

    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10008};

    mgr.connect_request(addr, base);
    mgr.complete_handshake(addr, 1, base);

    // Force into grace period.
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);
    mgr.tick(base);

    EXPECT(mgr.count_by_state(ae::ConnectionState::GracePeriod) == 1, "in grace period");

    // Tick past grace expiry.
    mgr.tick(base + std::chrono::seconds(5));
    EXPECT(mgr.count() == 0, "peer should be removed after grace period expiry");
    return 0;
}

int test_for_each_state() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};

    mgr.connect_request(ae::NetAddress {"127.0.0.1", 20001}, base);
    mgr.complete_handshake(ae::NetAddress {"127.0.0.1", 20001}, 1, base);

    mgr.connect_request(ae::NetAddress {"127.0.0.1", 20002}, base);
    mgr.complete_handshake(ae::NetAddress {"127.0.0.1", 20002}, 2, base);

    mgr.connect_request(ae::NetAddress {"127.0.0.1", 20003}, base);

    EXPECT(mgr.count_by_state(ae::ConnectionState::Connected) == 2, "2 connected");
    EXPECT(mgr.count_by_state(ae::ConnectionState::Handshaking) == 1, "1 handshaking");

    int connected_count = 0;
    mgr.for_each_state(ae::ConnectionState::Connected, [&](ae::PeerConnection& peer) {
        (void)peer;
        ++connected_count;
    });
    EXPECT(connected_count == 2, "for_each_state should iterate 2 connected peers");
    return 0;
}

int test_touch_resets_missed_heartbeats() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10009};

    mgr.connect_request(addr, base);
    mgr.complete_handshake(addr, 1, base);

    // Mark some heartbeats as missed.
    mgr.mark_missed_heartbeat(addr);
    mgr.mark_missed_heartbeat(addr);

    auto* peer = mgr.find(addr);
    EXPECT(peer->missed_heartbeats == 2, "2 missed heartbeats");

    // Touch resets missed_heartbeats for connected peers.
    mgr.touch(addr, base + std::chrono::milliseconds(50));
    EXPECT(peer->missed_heartbeats == 0, "touch should reset missed heartbeats");
    return 0;
}

int test_remove_peer() {
    ae::ConnectionManager mgr;
    const auto base = time_point {};
    const ae::NetAddress addr {"127.0.0.1", 10010};

    mgr.connect_request(addr, base);
    EXPECT(mgr.count() == 1, "1 peer");

    EXPECT(mgr.remove(addr), "remove should succeed");
    EXPECT(mgr.count() == 0, "0 peers after remove");
    EXPECT(!mgr.remove(addr), "second remove should fail");
    return 0;
}

} // namespace

int main() {
    if (int rc = test_new_peer_starts_handshaking(); rc != 0)
        return rc;
    if (int rc = test_complete_handshake_transitions_to_connected(); rc != 0)
        return rc;
    if (int rc = test_handshake_from_wrong_state_fails(); rc != 0)
        return rc;
    if (int rc = test_disconnect_transitions_to_disconnecting(); rc != 0)
        return rc;
    if (int rc = test_disconnect_from_wrong_state_fails(); rc != 0)
        return rc;
    if (int rc = test_tick_removes_handshake_timeout(); rc != 0)
        return rc;
    if (int rc = test_grace_period_and_reconnect(); rc != 0)
        return rc;
    if (int rc = test_grace_expiry_removes_peer(); rc != 0)
        return rc;
    if (int rc = test_for_each_state(); rc != 0)
        return rc;
    if (int rc = test_touch_resets_missed_heartbeats(); rc != 0)
        return rc;
    if (int rc = test_remove_peer(); rc != 0)
        return rc;
    std::cout << "connection_lifecycle_tests passed\n";
    return 0;
}
