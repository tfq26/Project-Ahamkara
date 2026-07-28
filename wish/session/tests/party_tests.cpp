#include "wish/session/party.h"

#include <cassert>
#include <chrono>

namespace {

using clock = wish::session::Party::clock;
using time_point = clock::time_point;

void test_create_party() {
    const wish::NetAddress leader_addr {"192.168.1.1", 30001};
    const auto now = time_point {};
    wish::session::Party party(1, leader_addr, now, 8);

    assert(party.party_id() == 1);
    assert(party.member_count() == 1);
    assert(!party.empty());
    assert(!party.is_full());
    assert(party.leader() == leader_addr);
    assert(party.is_leader(leader_addr));
    assert(!party.in_matchmaking());
}

void test_create_party_default_max() {
    const wish::NetAddress leader_addr {"192.168.1.1", 30001};
    const auto now = time_point {};
    wish::session::Party party(2, leader_addr, now); // default max = 8

    assert(party.max_members() == 8);
    assert(party.member_count() == 1);
}

void test_add_member() {
    wish::session::Party party(10, {"192.168.1.1", 30001}, time_point {}, 4);

    auto* m = party.add_member({"192.168.1.1", 30002}, time_point {});
    assert(m != nullptr);
    assert(m->address.port == 30002);
    assert(m->identity == "192.168.1.1:30002");
    assert(m->state == wish::session::PartyMemberState::Online);
    assert(!m->is_ready);
    assert(party.member_count() == 2);
}

void test_add_duplicate_member_returns_null() {
    wish::session::Party party(11, {"192.168.1.1", 30001}, time_point {}, 4);

    auto* m1 = party.add_member({"192.168.1.1", 30002}, time_point {});
    assert(m1 != nullptr);
    assert(party.member_count() == 2);

    auto* m2 = party.add_member({"192.168.1.1", 30002}, time_point {});
    assert(m2 == nullptr);
    assert(party.member_count() == 2);
}

void test_add_member_when_full() {
    wish::session::Party party(12, {"192.168.1.1", 30001}, time_point {}, 2);

    // Party already has leader (1 member), max is 2, so only 1 more slot
    auto* m1 = party.add_member({"192.168.1.1", 30002}, time_point {});
    assert(m1 != nullptr);
    assert(party.is_full());

    auto* m2 = party.add_member({"192.168.1.1", 30003}, time_point {});
    assert(m2 == nullptr);
    assert(party.member_count() == 2);
}

void test_remove_member() {
    wish::session::Party party(13, {"192.168.1.1", 30001}, time_point {}, 4);
    party.add_member({"192.168.1.1", 30002}, time_point {});
    party.add_member({"192.168.1.1", 30003}, time_point {});

    assert(party.member_count() == 3);

    bool removed = party.remove_member({"192.168.1.1", 30002});
    assert(removed);
    assert(party.member_count() == 2);
    assert(party.find_member({"192.168.1.1", 30002}) == nullptr);
}

void test_remove_nonexistent_member() {
    wish::session::Party party(14, {"192.168.1.1", 30001}, time_point {}, 4);

    bool removed = party.remove_member({"192.168.1.1", 39999});
    assert(!removed);
    assert(party.member_count() == 1);
}

void test_leader_transferred_on_removal() {
    wish::session::Party party(15, {"192.168.1.1", 30001}, time_point {}, 4);
    party.add_member({"192.168.1.1", 30002}, time_point {});
    party.add_member({"192.168.1.1", 30003}, time_point {});

    assert(party.is_leader({"192.168.1.1", 30001}));

    // Remove the leader
    party.remove_member({"192.168.1.1", 30001});
    assert(party.member_count() == 2);

    // Leadership should transfer to the first remaining member
    assert(party.is_leader({"192.168.1.1", 30002}));
}

void test_transfer_leadership() {
    wish::session::Party party(16, {"192.168.1.1", 30001}, time_point {}, 4);
    party.add_member({"192.168.1.1", 30002}, time_point {});

    assert(party.is_leader({"192.168.1.1", 30001}));

    bool transferred = party.transfer_leadership({"192.168.1.1", 30002});
    assert(transferred);
    assert(party.is_leader({"192.168.1.1", 30002}));
    assert(!party.is_leader({"192.168.1.1", 30001}));
}

void test_transfer_leadership_to_nonexistent_fails() {
    wish::session::Party party(17, {"192.168.1.1", 30001}, time_point {}, 4);

    bool transferred = party.transfer_leadership({"192.168.1.1", 39999});
    assert(!transferred);
    assert(party.is_leader({"192.168.1.1", 30001}));
}

void test_ready_state() {
    wish::session::Party party(18, {"192.168.1.1", 30001}, time_point {}, 4);
    party.add_member({"192.168.1.1", 30002}, time_point {});

    assert(!party.all_ready());
    assert(!party.is_ready({"192.168.1.1", 30002}));

    party.set_member_ready({"192.168.1.1", 30002}, true);
    assert(party.is_ready({"192.168.1.1", 30002}));

    // Leader is not ready yet
    assert(!party.all_ready());

    party.set_member_ready({"192.168.1.1", 30001}, true);
    assert(party.all_ready());

    // Unready
    party.set_member_ready({"192.168.1.1", 30002}, false);
    assert(!party.all_ready());
}

void test_ready_state_nonexistent_member() {
    wish::session::Party party(19, {"192.168.1.1", 30001}, time_point {}, 4);

    bool result = party.set_member_ready({"192.168.1.1", 39999}, true);
    assert(!result);
}

void test_empty_party_all_ready() {
    // An empty party is trivially all-ready
    // We can't create an empty party normally, but after removing the leader:
    wish::session::Party party(20, {"192.168.1.1", 30001}, time_point {}, 4);
    party.remove_member({"192.168.1.1", 30001});
    assert(party.empty());
    assert(party.all_ready());
}

void test_matchmaking_flag() {
    wish::session::Party party(21, {"192.168.1.1", 30001}, time_point {}, 4);
    party.add_member({"192.168.1.1", 30002}, time_point {});

    assert(!party.in_matchmaking());

    party.set_in_matchmaking(true);
    assert(party.in_matchmaking());

    // All members should be in InMatchmaking state
    party.for_each_member([](const wish::session::PartyMember& m) {
        assert(m.state == wish::session::PartyMemberState::InMatchmaking);
    });

    party.set_in_matchmaking(false);
    assert(!party.in_matchmaking());
}

void test_find_member() {
    wish::session::Party party(22, {"192.168.1.1", 30001}, time_point {}, 8);
    party.add_member({"192.168.1.1", 30002}, time_point {});

    auto* found = party.find_member({"192.168.1.1", 30002});
    assert(found != nullptr);
    assert(found->address.port == 30002);

    auto* not_found = party.find_member({"192.168.1.1", 39999});
    assert(not_found == nullptr);
}

void test_set_max_members() {
    wish::session::Party party(23, {"192.168.1.1", 30001}, time_point {}, 4);
    assert(party.max_members() == 4);

    party.set_max_members(8);
    assert(party.max_members() == 8);
}

void test_for_each_member() {
    wish::session::Party party(24, {"192.168.1.1", 30001}, time_point {}, 8);
    party.add_member({"192.168.1.1", 30002}, time_point {});
    party.add_member({"192.168.1.1", 30003}, time_point {});

    int count = 0;
    party.for_each_member([&count](const wish::session::PartyMember&) {
        ++count;
    });
    assert(count == 3);
}

void test_multiple_member_lifecycle() {
    wish::session::Party party(25, {"192.168.1.1", 30001}, time_point {}, 8);

    // Add 5 members
    for (wish::u16 port = 30002; port < 30007; ++port) {
        auto* m = party.add_member({"192.168.1.1", port}, time_point {});
        assert(m != nullptr);
    }

    assert(party.member_count() == 6); // 5 new + 1 leader
    assert(!party.is_full());

    // Remove one member mid-group
    assert(party.remove_member({"192.168.1.1", 30004}));
    assert(party.member_count() == 5);

    // Verify remaining members
    assert(party.find_member({"192.168.1.1", 30004}) == nullptr);
    assert(party.find_member({"192.168.1.1", 30002}) != nullptr);
    assert(party.find_member({"192.168.1.1", 30006}) != nullptr);

    // Add another to fill the gap
    auto* new_m = party.add_member({"192.168.1.1", 30007}, time_point {});
    assert(new_m != nullptr);
    assert(party.member_count() == 6);
}

} // namespace

int main() {
    test_create_party();
    test_create_party_default_max();
    test_add_member();
    test_add_duplicate_member_returns_null();
    test_add_member_when_full();
    test_remove_member();
    test_remove_nonexistent_member();
    test_leader_transferred_on_removal();
    test_transfer_leadership();
    test_transfer_leadership_to_nonexistent_fails();
    test_ready_state();
    test_ready_state_nonexistent_member();
    test_empty_party_all_ready();
    test_matchmaking_flag();
    test_find_member();
    test_set_max_members();
    test_for_each_member();
    test_multiple_member_lifecycle();

    return 0;
}
