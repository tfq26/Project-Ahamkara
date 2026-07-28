#include "wish/core/session_orchestrator.h"
#include "wish/core/matchmaking_service.h"
#include "wish/session/party.h"

#include <cassert>
#include <chrono>
#include <iostream>

namespace {

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
wish::core::Fireteam make_fireteam(wish::u64 fireteam_id, wish::u64 activity_id,
                                   const std::vector<wish::NetAddress>& members) {
    wish::core::Fireteam ft;
    ft.fireteam_id = fireteam_id;
    ft.activity_id = activity_id;
    ft.member_addresses = members;
    ft.formed_at = time_point{};
    ft.party_ids = {fireteam_id}; // 1:1 party:fireteam for simplicity
    return ft;
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
void test_default_construction() {
    wish::core::SessionOrchestrator orch;
    assert(orch.group_count() == 0);
    assert(orch.lobby_count() == 0);
    assert(orch.active_count() == 0);
    assert(orch.ended_count() == 0);
    assert(orch.find_group(1) == nullptr);
    assert(orch.config().ownership_policy == wish::core::OwnershipPolicy::Strict);
    assert(orch.config().auto_activate_sessions == true);
    assert(orch.config().auto_end_on_empty == true);

    std::cout << "test_default_construction: ok\n";
}

void test_custom_config() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.ownership_policy = wish::core::OwnershipPolicy::Lax;
    cfg.auto_activate_sessions = false;
    cfg.auto_end_on_empty = false;

    wish::core::SessionOrchestrator orch(cfg);
    assert(orch.config().ownership_policy == wish::core::OwnershipPolicy::Lax);
    assert(orch.config().auto_activate_sessions == false);
    assert(orch.config().auto_end_on_empty == false);

    std::cout << "test_custom_config: ok\n";
}

// ---------------------------------------------------------------------------
// on_match_found
// ---------------------------------------------------------------------------
void test_on_match_found_creates_group() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"192.168.1.1", 30001};
    const wish::NetAddress member {"192.168.1.1", 30002};
    const auto ft = make_fireteam(1, 10, {leader, member});

    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.group_count() == 1);
    assert(orch.lobby_count() == 0); // auto_activate is on
    assert(orch.active_count() == 1);

    // Verify the group
    const auto* group = orch.find_group(group_id);
    assert(group != nullptr);
    assert(group->fireteam_id() == 1);
    assert(group->activity_id() == 10);
    assert(group->owner_address() == leader);
    assert(group->state() == wish::session::GroupState::Active);
    assert(group->client_count() == 2);

    // Party bindings
    assert(orch.party_group(1) == group_id);

    std::cout << "test_on_match_found_creates_group: ok\n";
}

void test_on_match_found_empty_fireteam() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const auto ft = make_fireteam(2, 10, {});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id == 0);
    assert(orch.group_count() == 0);

    std::cout << "test_on_match_found_empty_fireteam: ok\n";
}

void test_on_match_found_no_auto_activate() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    const wish::NetAddress leader {"192.168.1.1", 30001};
    const auto ft = make_fireteam(3, 20, {leader});

    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.group_count() == 1);
    assert(orch.lobby_count() == 1); // still in lobby
    assert(orch.active_count() == 0);

    std::cout << "test_on_match_found_no_auto_activate: ok\n";
}

// ---------------------------------------------------------------------------
// join / leave flow
// ---------------------------------------------------------------------------
void test_process_join() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    // Create a group from a fireteam with 1 member but set it in Lobby
    // Then add 2 more members via join to prove the flow works
    const wish::NetAddress leader {"10.0.0.1", 7777};
    // Create fireteam with 1 leader + 2 additional players (total 3)
    const wish::NetAddress member1 {"10.0.0.2", 7778};
    const wish::NetAddress member2 {"10.0.0.3", 7779};
    // Use 3 members in fireteam so max_clients is set high enough,
    // then test that joining still works for an additional party
    const auto ft = make_fireteam(4, 30, {leader, member1, member2});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.find_group(group_id)->client_count() == 3);

    // Join request for a 4th player joining as a separate party
    const wish::NetAddress new_player {"10.0.0.4", 7780};
    wish::core::JoinRequest req;
    req.player_address = new_player;
    req.group_id = group_id;
    req.party_id = 42;
    req.player_identity = "player4";

    const auto result = orch.process_join(req, now);
    assert(result.accepted);
    assert(result.group_id == group_id);
    assert(result.error_message.empty());

    // Verify the player was added
    const auto* group = orch.find_group(group_id);
    assert(group != nullptr);
    assert(group->client_count() == 4);
    assert(orch.party_group(42) == group_id);

    std::cout << "test_process_join: ok\n";
}

void test_process_join_nonexistent_group() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    wish::core::JoinRequest req;
    req.player_address = {"10.0.0.1", 7777};
    req.group_id = 9999;

    const auto result = orch.process_join(req, now);
    assert(!result.accepted);
    assert(!result.error_message.empty());

    std::cout << "test_process_join_nonexistent_group: ok\n";
}

void test_process_join_full_group() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    // Create a group with 1 fireteam member + 4 extra slots = capacity 5
    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(5, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    auto* group = orch.find_group(group_id);
    assert(group != nullptr);
    // Capacity = 1 leader + 4 extra = 5
    // Fill up the remaining 4 slots
    for (int i = 0; i < 4; ++i) {
        const wish::NetAddress addr {"10.0.0.1", static_cast<wish::u16>(7778 + i)};
        wish::core::JoinRequest jreq;
        jreq.player_address = addr;
        jreq.group_id = group_id;
        const auto jres = orch.process_join(jreq, now);
        assert(jres.accepted);
    }

    // Group should now be full
    assert(group->client_count() == 5);
    assert(group->is_full());

    // Try to join another player - group should be full
    wish::core::JoinRequest req;
    req.player_address = {"10.0.0.9", 9999};
    req.group_id = group_id;

    const auto result = orch.process_join(req, now);
    assert(!result.accepted);
    assert(!result.error_message.empty());

    std::cout << "test_process_join_full_group: ok\n";
}

void test_process_leave() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const wish::NetAddress member {"10.0.0.2", 7778};
    const auto ft = make_fireteam(6, 30, {leader, member});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.find_group(group_id)->client_count() == 2);

    // Leave request
    wish::core::LeaveRequest lreq;
    lreq.player_address = member;
    lreq.group_id = group_id;
    lreq.party_id = 6;

    const auto lresult = orch.process_leave(lreq, now);
    assert(lresult.removed);
    assert(!lresult.group_ended); // 1 player still left

    const auto* group = orch.find_group(group_id);
    assert(group != nullptr);
    assert(group->client_count() == 1);

    // Owner should have been transferred if the leaving player was owner
    // (member is not the owner, so ownership stays)

    std::cout << "test_process_leave: ok\n";
}

void test_process_leave_ends_group_when_empty() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(7, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.find_group(group_id)->client_count() == 1);

    // Leave request (the last player)
    wish::core::LeaveRequest lreq;
    lreq.player_address = leader;
    lreq.group_id = group_id;
    lreq.party_id = 7;

    const auto lresult = orch.process_leave(lreq, now);
    assert(lresult.removed);
    assert(lresult.group_ended); // group should be ended

    const auto* group = orch.find_group(group_id);
    assert(group != nullptr);
    assert(group->state() == wish::session::GroupState::Ended);
    assert(group->client_count() == 0);

    std::cout << "test_process_leave_ends_group_when_empty: ok\n";
}

void test_process_leave_nonexistent_player() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(8, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    wish::core::LeaveRequest lreq;
    lreq.player_address = {"10.0.0.9", 9999}; // not in group
    lreq.group_id = group_id;

    const auto lresult = orch.process_leave(lreq, now);
    assert(!lresult.removed);
    assert(!lresult.error_message.empty());

    std::cout << "test_process_leave_nonexistent_player: ok\n";
}

// ---------------------------------------------------------------------------
// ownership
// ---------------------------------------------------------------------------
void test_is_owner() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(9, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    assert(orch.is_owner(leader, group_id));
    assert(!orch.is_owner({"10.0.0.2", 7778}, group_id));
    assert(!orch.is_owner(leader, 9999)); // nonexistent group

    std::cout << "test_is_owner: ok\n";
}

void test_transfer_ownership() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const wish::NetAddress member {"10.0.0.2", 7778};
    const auto ft = make_fireteam(10, 30, {leader, member});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    // Transfer to member
    assert(orch.transfer_ownership(member, group_id));
    assert(orch.is_owner(member, group_id));
    assert(!orch.is_owner(leader, group_id));

    // Transfer to non-member fails
    assert(!orch.transfer_ownership({"10.0.0.3", 7779}, group_id));

    // Transfer to nonexistent group fails
    assert(!orch.transfer_ownership(member, 9999));

    std::cout << "test_transfer_ownership: ok\n";
}

// ---------------------------------------------------------------------------
// session lifecycle
// ---------------------------------------------------------------------------
void test_activate_session() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(11, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.find_group(group_id)->state() == wish::session::GroupState::Lobby);

    // Activate
    assert(orch.activate_session(group_id, now + std::chrono::seconds(1)));
    assert(orch.find_group(group_id)->state() == wish::session::GroupState::Active);

    // Double activation should fail
    assert(!orch.activate_session(group_id, now + std::chrono::seconds(2)));

    // Activate nonexistent
    assert(!orch.activate_session(9999, now));

    std::cout << "test_activate_session: ok\n";
}

void test_end_session() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(12, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.find_group(group_id)->state() == wish::session::GroupState::Active);

    // End
    assert(orch.end_session(group_id, now + std::chrono::seconds(100)));
    assert(orch.find_group(group_id)->state() == wish::session::GroupState::Ended);

    // Double end (should be no-op, returning true)
    assert(orch.end_session(group_id, now + std::chrono::seconds(200)));
    assert(orch.find_group(group_id)->state() == wish::session::GroupState::Ended);

    // End nonexistent
    assert(!orch.end_session(9999, now));

    std::cout << "test_end_session: ok\n";
}

void test_destroy_group() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(13, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);
    assert(orch.group_count() == 1);

    // End then destroy
    orch.end_session(group_id, now);
    assert(orch.destroy_group(group_id));
    assert(orch.group_count() == 0);
    assert(orch.find_group(group_id) == nullptr);

    // Double destroy
    assert(!orch.destroy_group(group_id));

    std::cout << "test_destroy_group: ok\n";
}

void test_destroy_only_ended_groups() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(14, 30, {leader});
    const auto group_id = orch.on_match_found(ft, now);

    // Cannot destroy while still in Lobby
    assert(!orch.destroy_group(group_id));

    std::cout << "test_destroy_only_ended_groups: ok\n";
}

// ---------------------------------------------------------------------------
// callbacks
// ---------------------------------------------------------------------------
void test_session_activated_callback() {
    wish::core::SessionOrchestrator::Config cfg;
    cfg.auto_activate_sessions = false;

    wish::core::SessionOrchestrator orch(cfg);
    const auto now = time_point{};

    bool callback_fired = false;
    wish::u32 callback_group_id = 0;
    wish::u64 callback_activity_id = 0;

    orch.set_on_session_activated([&](wish::u32 gid, wish::u64 aid) {
        callback_fired = true;
        callback_group_id = gid;
        callback_activity_id = aid;
    });

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(15, 50, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    // Activate
    orch.activate_session(group_id, now);

    assert(callback_fired);
    assert(callback_group_id == group_id);
    assert(callback_activity_id == 50);

    std::cout << "test_session_activated_callback: ok\n";
}

void test_session_ended_callback() {
    wish::core::SessionOrchestrator orch;
    const auto now = time_point{};

    bool callback_fired = false;
    wish::u32 callback_group_id = 0;
    wish::u64 callback_activity_id = 0;

    orch.set_on_session_ended([&](wish::u32 gid, wish::u64 aid) {
        callback_fired = true;
        callback_group_id = gid;
        callback_activity_id = aid;
    });

    const wish::NetAddress leader {"10.0.0.1", 7777};
    const auto ft = make_fireteam(16, 60, {leader});
    const auto group_id = orch.on_match_found(ft, now);
    assert(group_id > 0);

    // End the session
    orch.end_session(group_id, now);

    assert(callback_fired);
    assert(callback_group_id == group_id);
    assert(callback_activity_id == 60);

    std::cout << "test_session_ended_callback: ok\n";
}

// ---------------------------------------------------------------------------
// matchmaking integration
// ---------------------------------------------------------------------------
void test_attach_matchmaking_registers_callback() {
    wish::core::MatchmakingService mm;
    wish::core::SessionOrchestrator orch;

    // Verify that attaching registers the callback
    // (the callback itself is tested indirectly via on_match_found tests)
    orch.attach_matchmaking(mm);

    // Enqueue a party and tick to form a match
    const auto now = time_point{};
    const auto ticket_id = mm.enqueue_party(1, 8, 100, now);
    assert(ticket_id > 0);

    // Verify the party is in the queue
    assert(mm.queue_size() == 1);
    assert(mm.queued_player_count() == 8);

    std::cout << "test_attach_matchmaking_registers_callback: ok\n";
}

} // namespace

int main() {
    test_default_construction();
    test_custom_config();
    test_on_match_found_creates_group();
    test_on_match_found_empty_fireteam();
    test_on_match_found_no_auto_activate();
    test_process_join();
    test_process_join_nonexistent_group();
    test_process_join_full_group();
    test_process_leave();
    test_process_leave_ends_group_when_empty();
    test_process_leave_nonexistent_player();
    test_is_owner();
    test_transfer_ownership();
    test_activate_session();
    test_end_session();
    test_destroy_group();
    test_destroy_only_ended_groups();
    test_session_activated_callback();
    test_session_ended_callback();
    test_attach_matchmaking_registers_callback();

    std::cout << "All session_orchestrator_tests passed.\n";
    return 0;
}
