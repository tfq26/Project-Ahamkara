#include "wish/session/activity_router.h"
#include "wish/session/session_runtime.h"

#include <cassert>
#include <chrono>
#include <iostream>

namespace {

using clock = wish::session::ActivityRouter::clock;
using time_point = clock::time_point;

// ── Helpers ────────────────────────────────────────────────────────────────

int fail(const std::string& msg) {
    std::cerr << "activity_router_tests FAILED: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_router_initial_state() {
    wish::session::ActivityRouter router;
    EXPECT(router.session_count() == 0, "should start empty");
    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Idle) == 0,
           "no idle sessions");
    EXPECT(router.count_by_category(wish::core::ActivityCategory::PvP) == 0,
           "no PvP sessions");
    EXPECT(router.sessions().empty(), "sessions map should be empty");

    std::cout << "test_router_initial_state: ok\n";
    return 0;
}

int test_create_session_explicit() {
    wish::session::ActivityRouter router;
    const time_point now{};

    wish::session::ActivitySessionConfig cfg;
    cfg.category = wish::core::ActivityCategory::PvP;
    cfg.max_players = 4;

    auto* session = router.create_session(100, cfg);
    EXPECT(session != nullptr, "create_session should succeed");
    EXPECT(session->id() == 100, "session id should be 100");
    EXPECT(session->category() == wish::core::ActivityCategory::PvP,
           "category should be PvP");
    EXPECT(session->config().max_players == 4, "max_players should be 4");
    EXPECT(session->state() == wish::session::ActivitySessionState::Idle,
           "session should start in Idle");
    EXPECT(router.session_count() == 1, "router should have 1 session");

    // Creating the same id again should fail
    auto* dup = router.create_session(100, cfg);
    EXPECT(dup == nullptr, "duplicate id should return nullptr");
    EXPECT(router.session_count() == 1, "count should still be 1");

    std::cout << "test_create_session_explicit: ok\n";
    return 0;
}

int test_create_session_by_category() {
    wish::session::ActivityRouter router;

    auto* session = router.create_session(200, wish::core::ActivityCategory::PvE, 6);
    EXPECT(session != nullptr, "create_session by category should succeed");
    EXPECT(session->category() == wish::core::ActivityCategory::PvE,
           "category should be PvE");
    EXPECT(session->config().max_players == 6, "max_players should be 6");

    std::cout << "test_create_session_by_category: ok\n";
    return 0;
}

int test_find_session_by_id() {
    wish::session::ActivityRouter router;

    (void)router.create_session(10, wish::core::ActivityCategory::PvP);
    (void)router.create_session(20, wish::core::ActivityCategory::PvE);

    auto* found = router.find_session(10);
    EXPECT(found != nullptr, "find_session(10) should succeed");
    EXPECT(found->id() == 10, "id should be 10");

    auto* not_found = router.find_session(99);
    EXPECT(not_found == nullptr, "find_session(99) should return nullptr");

    // Const lookup
    const auto& const_router = router;
    auto* cfound = const_router.find_session(20);
    EXPECT(cfound != nullptr, "const find_session(20) should succeed");

    std::cout << "test_find_session_by_id: ok\n";
    return 0;
}

int test_find_sessions_by_category() {
    wish::session::ActivityRouter router;

    (void)router.create_session(1, wish::core::ActivityCategory::PvP);
    (void)router.create_session(2, wish::core::ActivityCategory::PvE);
    (void)router.create_session(3, wish::core::ActivityCategory::PvP);
    (void)router.create_session(4, wish::core::ActivityCategory::Social);

    auto pvp_sessions = router.find_sessions_by_category(wish::core::ActivityCategory::PvP);
    EXPECT(pvp_sessions.size() == 2, "should find 2 PvP sessions");

    auto pve_sessions = router.find_sessions_by_category(wish::core::ActivityCategory::PvE);
    EXPECT(pve_sessions.size() == 1, "should find 1 PvE session");

    auto social_sessions = router.find_sessions_by_category(wish::core::ActivityCategory::Social);
    EXPECT(social_sessions.size() == 1, "should find 1 Social session");

    auto custom_sessions = router.find_sessions_by_category(wish::core::ActivityCategory::Custom);
    EXPECT(custom_sessions.empty(), "should find 0 Custom sessions");

    std::cout << "test_find_sessions_by_category: ok\n";
    return 0;
}

int test_find_sessions_by_state() {
    wish::session::ActivityRouter router;
    const time_point now{};

    auto* s1 = router.create_session(1, wish::core::ActivityCategory::PvP);
    auto* s2 = router.create_session(2, wish::core::ActivityCategory::PvE);
    s2->start_lobby(now);

    auto idle_sessions = router.find_sessions_by_state(wish::session::ActivitySessionState::Idle);
    EXPECT(idle_sessions.size() == 1, "should find 1 Idle session");
    EXPECT(idle_sessions[0]->id() == 1, "Idle session id should be 1");

    auto lobby_sessions = router.find_sessions_by_state(wish::session::ActivitySessionState::Lobby);
    EXPECT(lobby_sessions.size() == 1, "should find 1 Lobby session");
    EXPECT(lobby_sessions[0]->id() == 2, "Lobby session id should be 2");

    std::cout << "test_find_sessions_by_state: ok\n";
    return 0;
}

int test_find_available_lobby() {
    wish::session::ActivityRouter router;
    const time_point now{};

    // No lobbies yet
    auto* none = router.find_available_lobby(wish::core::ActivityCategory::PvP);
    EXPECT(none == nullptr, "no lobbies available initially");

    // Create a session and start lobby
    auto* s1 = router.create_session(1, wish::core::ActivityCategory::PvP, 2);
    s1->start_lobby(now);

    auto* available = router.find_available_lobby(wish::core::ActivityCategory::PvP);
    EXPECT(available != nullptr, "should find available PvP lobby");
    EXPECT(available->id() == 1, "should find session 1");

    // Fill the session
    (void)s1->add_client({"127.0.0.1", 30001}, now);
    (void)s1->add_client({"127.0.0.1", 30002}, now);
    
    auto* full = router.find_available_lobby(wish::core::ActivityCategory::PvP);
    EXPECT(full == nullptr, "no available lobby when session is full");

    // Different category should not match
    auto* pve = router.find_available_lobby(wish::core::ActivityCategory::PvE);
    EXPECT(pve == nullptr, "no PvE lobby available");

    std::cout << "test_find_available_lobby: ok\n";
    return 0;
}

int test_route_client_creates_session() {
    wish::session::ActivityRouter router;
    const time_point now{};
    const wish::NetAddress addr{"127.0.0.1", 30001};

    wish::session::ActivitySession* routed = nullptr;
    bool result = router.route_client(addr, wish::core::ActivityCategory::PvP, now, &routed);

    EXPECT(result, "route_client should succeed");
    EXPECT(routed != nullptr, "out_session should be set");
    EXPECT(routed->category() == wish::core::ActivityCategory::PvP,
           "routed session category should be PvP");
    EXPECT(routed->state() == wish::session::ActivitySessionState::Lobby,
           "routed session should be in Lobby");
    EXPECT(routed->client_count() == 1, "routed session should have 1 client");
    EXPECT(router.session_count() == 1, "router should have 1 session");

    std::cout << "test_route_client_creates_session: ok\n";
    return 0;
}

int test_route_client_reuses_lobby() {
    wish::session::ActivityRouter router;
    const time_point now{};

    // Route first client (creates a session)
    wish::session::ActivitySession* first = nullptr;
    router.route_client({"127.0.0.1", 30001}, wish::core::ActivityCategory::PvE, now, &first);
    EXPECT(first != nullptr, "first client routed");

    // Route second client (should reuse the same lobby)
    wish::session::ActivitySession* second = nullptr;
    router.route_client({"127.0.0.1", 30002}, wish::core::ActivityCategory::PvE, now, &second);
    EXPECT(second != nullptr, "second client routed");

    // Both should be in the same session (same id)
    EXPECT(first->id() == second->id(),
           "both clients should be in the same session");
    EXPECT(first->client_count() == 2, "session should have 2 clients");
    EXPECT(router.session_count() == 1, "router should still have 1 session");

    std::cout << "test_route_client_reuses_lobby: ok\n";
    return 0;
}

int test_route_client_different_categories_create_separate_sessions() {
    wish::session::ActivityRouter router;
    const time_point now{};

    wish::session::ActivitySession* pvp_session = nullptr;
    wish::session::ActivitySession* pve_session = nullptr;

    router.route_client({"127.0.0.1", 30001}, wish::core::ActivityCategory::PvP, now, &pvp_session);
    router.route_client({"127.0.0.1", 30002}, wish::core::ActivityCategory::PvE, now, &pve_session);

    EXPECT(pvp_session != nullptr, "PvP session should exist");
    EXPECT(pve_session != nullptr, "PvE session should exist");
    EXPECT(pvp_session->id() != pve_session->id(),
           "different categories should create different sessions");
    EXPECT(pvp_session->category() == wish::core::ActivityCategory::PvP,
           "first session should be PvP");
    EXPECT(pve_session->category() == wish::core::ActivityCategory::PvE,
           "second session should be PvE");
    EXPECT(router.session_count() == 2, "router should have 2 sessions");

    std::cout << "test_route_client_different_categories_create_separate_sessions: ok\n";
    return 0;
}

int test_remove_session() {
    wish::session::ActivityRouter router;

    (void)router.create_session(1, wish::core::ActivityCategory::PvP);
    EXPECT(router.session_count() == 1, "should have 1 session");

    bool removed = router.remove_session(1);
    EXPECT(removed, "remove_session should succeed");
    EXPECT(router.session_count() == 0, "should have 0 sessions after removal");
    EXPECT(router.find_session(1) == nullptr, "session should not be found");

    // Remove again should fail
    bool removed_again = router.remove_session(1);
    EXPECT(!removed_again, "remove_session should fail for already removed session");

    std::cout << "test_remove_session: ok\n";
    return 0;
}

int test_count_by_state() {
    wish::session::ActivityRouter router;
    const time_point now{};

    auto* s1 = router.create_session(1, wish::core::ActivityCategory::PvP);
    auto* s2 = router.create_session(2, wish::core::ActivityCategory::PvE);

    s1->start_lobby(now);
    // s2 stays Idle

    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Idle) == 1,
           "1 Idle session");
    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Lobby) == 1,
           "1 Lobby session");
    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Active) == 0,
           "0 Active sessions");
    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Completed) == 0,
           "0 Completed sessions");
    EXPECT(router.count_by_state(wish::session::ActivitySessionState::Cancelled) == 0,
           "0 Cancelled sessions");

    std::cout << "test_count_by_state: ok\n";
    return 0;
}

int test_count_by_category() {
    wish::session::ActivityRouter router;

    (void)router.create_session(1, wish::core::ActivityCategory::PvP);
    (void)router.create_session(2, wish::core::ActivityCategory::PvE);
    (void)router.create_session(3, wish::core::ActivityCategory::PvP);
    (void)router.create_session(4, wish::core::ActivityCategory::Social);
    (void)router.create_session(5, wish::core::ActivityCategory::PvEvP);

    EXPECT(router.count_by_category(wish::core::ActivityCategory::PvP) == 2,
           "2 PvP sessions");
    EXPECT(router.count_by_category(wish::core::ActivityCategory::PvE) == 1,
           "1 PvE session");
    EXPECT(router.count_by_category(wish::core::ActivityCategory::Social) == 1,
           "1 Social session");
    EXPECT(router.count_by_category(wish::core::ActivityCategory::PvEvP) == 1,
           "1 PvEvP session");
    EXPECT(router.count_by_category(wish::core::ActivityCategory::Custom) == 0,
           "0 Custom sessions");

    std::cout << "test_count_by_category: ok\n";
    return 0;
}

int test_tick_all_auto_removes_completed() {
    wish::session::ActivityRouter router;
    const time_point now{};

    auto* s1 = router.create_session(1, wish::core::ActivityCategory::PvP);
    auto* s2 = router.create_session(2, wish::core::ActivityCategory::PvE);

    // Complete s1
    s1->start_lobby(now);
    // Need an activity to launch
    // (We use a simple approach: cancel from lobby to test completion removal)
    s1->cancel();

    EXPECT(router.session_count() == 2, "should have 2 sessions before tick");

    router.tick_all(0.0f, now);

    EXPECT(router.session_count() == 1, "should have 1 session after tick (completed removed)");
    EXPECT(router.find_session(2) != nullptr, "s2 should still exist");

    std::cout << "test_tick_all_auto_removes_completed: ok\n";
    return 0;
}

int test_clear_router() {
    wish::session::ActivityRouter router;

    (void)router.create_session(1, wish::core::ActivityCategory::PvP);
    (void)router.create_session(2, wish::core::ActivityCategory::PvE);
    (void)router.create_session(3, wish::core::ActivityCategory::Social);

    EXPECT(router.session_count() == 3, "should have 3 sessions");

    router.clear();
    EXPECT(router.session_count() == 0, "should have 0 sessions after clear");

    std::cout << "test_clear_router: ok\n";
    return 0;
}

int test_multi_category_routing() {
    wish::session::ActivityRouter router;
    const time_point now{};

    // Route clients for different categories
    wish::session::ActivitySession* social = nullptr;
    wish::session::ActivitySession* pvp = nullptr;
    wish::session::ActivitySession* pve = nullptr;

    router.route_client({"127.0.0.1", 10001}, wish::core::ActivityCategory::Social, now, &social);
    router.route_client({"127.0.0.1", 10002}, wish::core::ActivityCategory::PvP, now, &pvp);
    router.route_client({"127.0.0.1", 10003}, wish::core::ActivityCategory::PvE, now, &pve);

    EXPECT(social->category() == wish::core::ActivityCategory::Social, "Social session");
    EXPECT(pvp->category() == wish::core::ActivityCategory::PvP, "PvP session");
    EXPECT(pve->category() == wish::core::ActivityCategory::PvE, "PvE session");
    EXPECT(router.session_count() == 3, "should have 3 sessions total");

    // Route more clients to existing lobbies
    wish::session::ActivitySession* social2 = nullptr;
    router.route_client({"127.0.0.1", 10004}, wish::core::ActivityCategory::Social, now, &social2);

    EXPECT(social->id() == social2->id(), "social clients should be in same session");
    EXPECT(social->client_count() == 2, "social session should have 2 clients");
    EXPECT(router.session_count() == 3, "should still have 3 sessions");

    std::cout << "test_multi_category_routing: ok\n";
    return 0;
}

}  // namespace

int main() {
    int failures = 0;

    failures += test_router_initial_state();
    failures += test_create_session_explicit();
    failures += test_create_session_by_category();
    failures += test_find_session_by_id();
    failures += test_find_sessions_by_category();
    failures += test_find_sessions_by_state();
    failures += test_find_available_lobby();
    failures += test_route_client_creates_session();
    failures += test_route_client_reuses_lobby();
    failures += test_route_client_different_categories_create_separate_sessions();
    failures += test_remove_session();
    failures += test_count_by_state();
    failures += test_count_by_category();
    failures += test_tick_all_auto_removes_completed();
    failures += test_clear_router();
    failures += test_multi_category_routing();

    if (failures > 0) {
        std::cerr << failures << " activity_router test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All activity_router tests passed.\n";
    return 0;
}
