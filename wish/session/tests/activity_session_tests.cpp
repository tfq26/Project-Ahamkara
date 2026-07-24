#include "wish/session/activity_session.h"
#include "wish/session/session_runtime.h"

#include <cassert>
#include <chrono>
#include <iostream>

namespace {

using clock = wish::session::ActivitySession::clock;
using time_point = clock::time_point;

// ── Helpers ────────────────────────────────────────────────────────────────

int fail(const std::string& msg) {
    std::cerr << "activity_session_tests FAILED: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// Minimal activity stub for testing launch/active lifecycle.
// Does not depend on game types.
struct TestActivity : wish::core::IActivityBase {
    bool initialized = false;
    bool shutdown_called = false;
    int tick_count = 0;
    wish::u32 players = 0;
    wish::core::ActivityId act_id = 42;
    wish::core::ActivityCategory cat = wish::core::ActivityCategory::PvP;
    std::string_view name = "test_activity";

    bool initialize(const wish::core::ActivityConfig& cfg) override {
        initialized = true;
        act_id = cfg.id;
        cat = cfg.category;
        name = cfg.name;
        return true;
    }
    void shutdown() override { shutdown_called = true; }

    bool admit_player(const wish::core::SessionAdmissionRequest&) override {
        ++players;
        return true;
    }
    void remove_player(wish::session::SessionId) override {
        if (players > 0) --players;
    }
    wish::u32 player_count() const override { return players; }

    void tick(float) override { ++tick_count; }

    void process_input(wish::session::SessionId,
                       const wish::PacketEnvelope&,
                       wish::u32) override {}

    wish::usize build_snapshot_bytes(wish::session::SessionId,
                                     std::span<std::byte>) override { return 0; }

    wish::core::ActivityId activity_id() const override { return act_id; }
    wish::core::ActivityCategory category() const override { return cat; }
    std::string_view activity_name() const override { return name; }

    void for_each_connected_snapshot(
        void (*)(void*, wish::session::SessionId,
                 const std::byte*, wish::usize),
        void*) override {}
};

// ── Tests ──────────────────────────────────────────────────────────────────

int test_initial_state() {
    wish::session::ActivitySession session(1);
    EXPECT(session.id() == 1, "id should match constructor");
    EXPECT(session.state() == wish::session::ActivitySessionState::Idle,
           "initial state should be Idle");
    EXPECT(session.client_count() == 0, "should have no clients initially");
    EXPECT(session.activity() == nullptr, "no activity bound initially");
    EXPECT(session.category() == wish::core::ActivityCategory::PvP,
           "default category should be PvP");
    EXPECT(session.config().max_players == 8, "default max players should be 8");

    std::cout << "test_initial_state: ok\n";
    return 0;
}

int test_custom_config() {
    wish::session::ActivitySessionConfig cfg;
    cfg.category = wish::core::ActivityCategory::PvE;
    cfg.max_players = 4;
    cfg.lobby_timeout = std::chrono::seconds(60);
    cfg.allow_spectators = true;

    wish::session::ActivitySession session(2, cfg);
    EXPECT(session.id() == 2, "id should match");
    EXPECT(session.category() == wish::core::ActivityCategory::PvE,
           "category should be PvE");
    EXPECT(session.config().max_players == 4, "max_players should be 4");
    EXPECT(session.config().lobby_timeout == std::chrono::seconds(60),
           "lobby timeout should be 60s");
    EXPECT(session.config().allow_spectators == true,
           "spectators should be allowed");

    std::cout << "test_custom_config: ok\n";
    return 0;
}

int test_lobby_transition() {
    wish::session::ActivitySession session(3);
    const time_point now{};

    bool started = session.start_lobby(now);
    EXPECT(started, "start_lobby should succeed from Idle");
    EXPECT(session.state() == wish::session::ActivitySessionState::Lobby,
           "state should be Lobby after start_lobby");

    // Starting lobby again should fail
    bool retry = session.start_lobby(now + std::chrono::seconds(1));
    EXPECT(!retry, "start_lobby should fail if already in Lobby");

    std::cout << "test_lobby_transition: ok\n";
    return 0;
}

int test_launch_transition() {
    wish::session::ActivitySession session(4);

    // Launch without lobby should fail
    TestActivity activity;
    bool direct_launch = session.launch(time_point{}, &activity);
    EXPECT(!direct_launch, "launch should fail from Idle");

    // Proper flow: lobby -> launch
    session.start_lobby(time_point{});
    bool launched = session.launch(time_point{}, &activity);
    EXPECT(launched, "launch should succeed from Lobby");
    EXPECT(session.state() == wish::session::ActivitySessionState::Active,
           "state should be Active after launch");
    EXPECT(session.activity() == &activity, "activity pointer should match");

    std::cout << "test_launch_transition: ok\n";
    return 0;
}

int test_launch_with_null_activity() {
    wish::session::ActivitySession session(5);
    session.start_lobby(time_point{});

    bool launched = session.launch(time_point{}, nullptr);
    EXPECT(!launched, "launch with null activity should fail");
    EXPECT(session.state() == wish::session::ActivitySessionState::Lobby,
           "state should remain Lobby after failed launch");

    std::cout << "test_launch_with_null_activity: ok\n";
    return 0;
}

int test_complete_transition() {
    wish::session::ActivitySession session(6);
    TestActivity activity;

    session.start_lobby(time_point{});
    session.launch(time_point{}, &activity);

    bool completed = session.complete();
    EXPECT(completed, "complete should succeed from Active");
    EXPECT(session.state() == wish::session::ActivitySessionState::Completed,
           "state should be Completed");
    EXPECT(session.activity() == nullptr,
           "activity should be unbound after completion");

    // Double complete should fail
    bool retry = session.complete();
    EXPECT(!retry, "complete should fail from Completed");

    std::cout << "test_complete_transition: ok\n";
    return 0;
}

int test_cancel_from_lobby() {
    wish::session::ActivitySession session(7);

    session.start_lobby(time_point{});

    bool cancelled = session.cancel();
    EXPECT(cancelled, "cancel should succeed from Lobby");
    EXPECT(session.state() == wish::session::ActivitySessionState::Cancelled,
           "state should be Cancelled");

    std::cout << "test_cancel_from_lobby: ok\n";
    return 0;
}

int test_cancel_from_active() {
    wish::session::ActivitySession session(8);
    TestActivity activity;

    session.start_lobby(time_point{});
    session.launch(time_point{}, &activity);

    bool cancelled = session.cancel();
    EXPECT(cancelled, "cancel should succeed from Active");
    EXPECT(session.state() == wish::session::ActivitySessionState::Cancelled,
           "state should be Cancelled");
    EXPECT(session.activity() == nullptr,
           "activity should be unbound after cancel");

    // Cancel again should fail
    bool retry = session.cancel();
    EXPECT(!retry, "cancel should fail from Cancelled");

    std::cout << "test_cancel_from_active: ok\n";
    return 0;
}

int test_add_client_during_lobby() {
    wish::session::ActivitySession session(9);
    const time_point now{};
    const wish::NetAddress addr{"127.0.0.1", 30001};

    // Adding client from Idle should fail
    auto* client_idle = session.add_client(addr, now);
    EXPECT(client_idle == nullptr, "add_client should fail from Idle");

    // Start lobby and add client
    session.start_lobby(now);
    auto* client = session.add_client(addr, now);
    EXPECT(client != nullptr, "add_client should succeed in Lobby");
    EXPECT(client->address == addr, "client address should match");
    EXPECT(session.client_count() == 1, "should have 1 client");
    EXPECT(session.connected_count() == 0, "client starts as PendingAdmission");
    EXPECT(!session.is_full(), "should not be full");

    std::cout << "test_add_client_during_lobby: ok\n";
    return 0;
}

int test_add_client_to_full_session() {
    wish::session::ActivitySessionConfig cfg;
    cfg.max_players = 2;

    wish::session::ActivitySession session(10, cfg);
    const time_point now{};
    session.start_lobby(now);

    const wish::NetAddress addr_a{"127.0.0.1", 30001};
    const wish::NetAddress addr_b{"127.0.0.1", 30002};
    const wish::NetAddress addr_c{"127.0.0.1", 30003};

    EXPECT(session.add_client(addr_a, now) != nullptr, "first client ok");
    EXPECT(session.add_client(addr_b, now) != nullptr, "second client ok");
    EXPECT(session.add_client(addr_c, now) == nullptr, "third client should be rejected");
    EXPECT(session.is_full(), "session should be full");
    EXPECT(session.client_count() == 2, "should have 2 clients");

    std::cout << "test_add_client_to_full_session: ok\n";
    return 0;
}

int test_remove_client() {
    wish::session::ActivitySession session(11);
    const time_point now{};
    const wish::NetAddress addr{"127.0.0.1", 30001};

    session.start_lobby(now);
    (void)session.add_client(addr, now);

    bool removed = session.remove_client(addr);
    EXPECT(removed, "remove_client should succeed");
    EXPECT(session.client_count() == 0, "should have 0 clients after removal");
    EXPECT(session.find_client(addr) == nullptr, "client should not be found");

    // Remove again should fail
    bool removed_again = session.remove_client(addr);
    EXPECT(!removed_again, "remove_client should fail for removed client");

    std::cout << "test_remove_client: ok\n";
    return 0;
}

int test_tick_forwards_to_activity() {
    wish::session::ActivitySession session(12);
    TestActivity activity;

    session.start_lobby(time_point{});
    session.launch(time_point{}, &activity);

    EXPECT(activity.tick_count == 0, "no ticks yet");
    session.tick(1.0f, time_point{} + std::chrono::seconds(1));
    EXPECT(activity.tick_count == 1, "tick should forward to activity");
    session.tick(1.0f, time_point{} + std::chrono::seconds(2));
    EXPECT(activity.tick_count == 2, "second tick should forward");

    std::cout << "test_tick_forwards_to_activity: ok\n";
    return 0;
}

int test_tick_prunes_timeouts() {
    wish::session::ActivitySession session(13);
    const time_point base{};
    const wish::NetAddress addr{"127.0.0.1", 30001};

    session.start_lobby(base);
    (void)session.add_client(addr, base);
    EXPECT(session.client_count() == 1, "should have 1 client");

    // Tick far in the future to trigger timeout pruning
    const auto far_future = base + std::chrono::seconds(30);
    session.tick(0.0f, far_future);

    // SessionGroup default timeout is 10s, so the client should be pruned
    // Note: pruning removes the client but the session remains in Lobby
    EXPECT(session.client_count() == 0, "client should be pruned after timeout");

    std::cout << "test_tick_prunes_timeouts: ok\n";
    return 0;
}

int test_state_name() {
    using S = wish::session::ActivitySessionState;
    EXPECT(wish::session::activity_session_state_name(S::Idle) == "Idle", "Idle name");
    EXPECT(wish::session::activity_session_state_name(S::Lobby) == "Lobby", "Lobby name");
    EXPECT(wish::session::activity_session_state_name(S::Active) == "Active", "Active name");
    EXPECT(wish::session::activity_session_state_name(S::Completed) == "Completed", "Completed name");
    EXPECT(wish::session::activity_session_state_name(S::Cancelled) == "Cancelled", "Cancelled name");
    EXPECT(wish::session::activity_session_state_name(static_cast<S>(99)) == "Unknown", "Unknown name");

    std::cout << "test_state_name: ok\n";
    return 0;
}

int test_time_in_state() {
    wish::session::ActivitySession session(14);
    const time_point base{};

    // Idle state, time_in_state should be zero
    EXPECT(session.time_in_state(base) == clock::duration::zero(), "time in Idle should be 0");

    session.start_lobby(base);
    const auto later = base + std::chrono::seconds(5);
    auto elapsed = session.time_in_state(later);
    EXPECT(elapsed == std::chrono::seconds(5), "time in Lobby should be 5s");

    std::cout << "test_time_in_state: ok\n";
    return 0;
}

int test_multiple_clients_lifecycle() {
    wish::session::ActivitySessionConfig cfg;
    cfg.category = wish::core::ActivityCategory::Social;
    cfg.max_players = 4;

    wish::session::ActivitySession session(15, cfg);
    const time_point base{};

    EXPECT(session.category() == wish::core::ActivityCategory::Social,
           "category should be Social");

    // Start lobby and add 3 clients
    session.start_lobby(base);
    for (wish::u16 port = 30001; port <= 30003; ++port) {
        auto* c = session.add_client(wish::NetAddress{"127.0.0.1", port}, base);
        EXPECT(c != nullptr, "client addition should succeed");
    }
    EXPECT(session.client_count() == 3, "should have 3 clients");

    // Remove one
    session.remove_client(wish::NetAddress{"127.0.0.1", 30002});
    EXPECT(session.client_count() == 2, "should have 2 clients after removal");

    // Launch activity
    TestActivity activity;
    bool launched = session.launch(base, &activity);
    EXPECT(launched, "launch should succeed");
    EXPECT(session.state() == wish::session::ActivitySessionState::Active,
           "state should be Active");

    // Tick the activity
    session.tick(1.0f, base + std::chrono::seconds(1));
    EXPECT(activity.tick_count == 1, "activity should have been ticked");

    // Complete
    session.complete();
    EXPECT(session.state() == wish::session::ActivitySessionState::Completed,
           "state should be Completed");

    std::cout << "test_multiple_clients_lifecycle: ok\n";
    return 0;
}

}  // namespace

int main() {
    int failures = 0;

    failures += test_initial_state();
    failures += test_custom_config();
    failures += test_lobby_transition();
    failures += test_launch_transition();
    failures += test_launch_with_null_activity();
    failures += test_complete_transition();
    failures += test_cancel_from_lobby();
    failures += test_cancel_from_active();
    failures += test_add_client_during_lobby();
    failures += test_add_client_to_full_session();
    failures += test_remove_client();
    failures += test_tick_forwards_to_activity();
    failures += test_tick_prunes_timeouts();
    failures += test_state_name();
    failures += test_time_in_state();
    failures += test_multiple_clients_lifecycle();

    if (failures > 0) {
        std::cerr << failures << " activity_session test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All activity_session tests passed.\n";
    return 0;
}
