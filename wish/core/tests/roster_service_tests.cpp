#include "wish/core/roster_service.h"
#include "wish/integrations/mock_identity_services.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& msg) {
    std::cerr << "roster_service_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_empty_roster() {
    wish::integrations::NoopRosterService svc;

    auto result = svc.get_roster("player-1");
    EXPECT(result.ok, "get_roster should succeed");
    EXPECT(result.entries.empty(), "new roster should be empty");

    std::cout << "test_empty_roster: ok\n";
    return 0;
}

int test_add_roster_entry() {
    wish::integrations::NoopRosterService svc;

    auto result = svc.add_entry("player-1", "player-2");
    EXPECT(result.ok, "add_entry should succeed");
    EXPECT(result.entries.size() == 1, "roster should have 1 entry");

    if (result.entries.size() > 0) {
        EXPECT(result.entries[0].player_id == "player-2", "entry should be player-2");
        EXPECT(result.entries[0].is_friend, "entry should be marked as friend");
    }

    std::cout << "test_add_roster_entry: ok\n";
    return 0;
}

int test_add_duplicate_entry() {
    wish::integrations::NoopRosterService svc;

    (void)svc.add_entry("player-1", "player-2");
    auto result = svc.add_entry("player-1", "player-2");
    EXPECT(!result.ok, "duplicate add should fail");
    EXPECT(!result.error_message.empty(), "error message should be present");

    std::cout << "test_add_duplicate_entry: ok\n";
    return 0;
}

int test_remove_roster_entry() {
    wish::integrations::NoopRosterService svc;

    (void)svc.add_entry("player-1", "player-2");
    auto result = svc.remove_entry("player-1", "player-2");
    EXPECT(result.ok, "remove should succeed");
    EXPECT(result.entries.empty(), "roster should be empty after removal");

    std::cout << "test_remove_roster_entry: ok\n";
    return 0;
}

int test_remove_nonexistent_entry() {
    wish::integrations::NoopRosterService svc;

    auto result = svc.remove_entry("player-1", "nonexistent");
    EXPECT(!result.ok, "removing nonexistent entry should fail");

    std::cout << "test_remove_nonexistent_entry: ok\n";
    return 0;
}

int test_block_and_unblock_player() {
    wish::integrations::NoopRosterService svc;

    // Add and then block
    (void)svc.add_entry("player-1", "player-2");
    auto block_result = svc.block_player("player-1", "player-2");
    EXPECT(block_result.ok, "block should succeed");
    EXPECT(svc.is_blocked("player-1", "player-2"), "player-2 should be blocked");

    // Adding a blocked player should fail
    auto add_result = svc.add_entry("player-1", "player-2");
    EXPECT(!add_result.ok, "adding blocked player should fail");

    // Unblock
    auto unblock_result = svc.unblock_player("player-1", "player-2");
    EXPECT(unblock_result.ok, "unblock should succeed");
    EXPECT(!svc.is_blocked("player-1", "player-2"), "player-2 should no longer be blocked");

    // Now add should work again
    add_result = svc.add_entry("player-1", "player-2");
    EXPECT(add_result.ok, "adding after unblock should succeed");

    std::cout << "test_block_and_unblock_player: ok\n";
    return 0;
}

int test_unblock_not_blocked() {
    wish::integrations::NoopRosterService svc;

    auto result = svc.unblock_player("player-1", "player-2");
    EXPECT(!result.ok, "unblocking non-blocked player should fail");

    std::cout << "test_unblock_not_blocked: ok\n";
    return 0;
}

int test_get_online_roster() {
    wish::integrations::NoopRosterService svc;

    (void)svc.add_entry("player-1", "player-2");
    (void)svc.add_entry("player-1", "player-3");

    // In the noop implementation, all added entries are Online
    auto online = svc.get_online_roster("player-1");
    EXPECT(online.ok, "get_online_roster should succeed");
    EXPECT(online.entries.size() == 2, "both entries should be online");

    std::cout << "test_get_online_roster: ok\n";
    return 0;
}

int test_roster_independence() {
    wish::integrations::NoopRosterService svc;

    (void)svc.add_entry("player-1", "friend-a");
    (void)svc.add_entry("player-2", "friend-b");

    auto roster1 = svc.get_roster("player-1");
    auto roster2 = svc.get_roster("player-2");

    EXPECT(roster1.entries.size() == 1, "player-1 should have 1 entry");
    EXPECT(roster2.entries.size() == 1, "player-2 should have 1 entry");
    EXPECT(roster1.entries[0].player_id == "friend-a", "player-1's friend should be friend-a");
    EXPECT(roster2.entries[0].player_id == "friend-b", "player-2's friend should be friend-b");

    std::cout << "test_roster_independence: ok\n";
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_empty_roster();
    failures += test_add_roster_entry();
    failures += test_add_duplicate_entry();
    failures += test_remove_roster_entry();
    failures += test_remove_nonexistent_entry();
    failures += test_block_and_unblock_player();
    failures += test_unblock_not_blocked();
    failures += test_get_online_roster();
    failures += test_roster_independence();

    if (failures > 0) {
        std::cerr << failures << " roster service test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All roster service tests passed.\n";
    return 0;
}
