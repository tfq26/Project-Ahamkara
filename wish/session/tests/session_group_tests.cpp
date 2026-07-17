#include "wish/session/session_group.h"
#include "wish/session/session_runtime.h"

#include <cassert>
#include <chrono>

namespace {

void test_add_client_to_group() {
    wish::session::SessionGroup group(1, 8);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30001};
    auto* client = group.add_client(addr, base_time);

    assert(client != nullptr);
    assert(client->address == addr);
    assert(client->identity == "127.0.0.1:30001");
    assert(client->connection_state == wish::session::ClientConnectionState::PendingAdmission);
    assert(client->last_seen == base_time);
    assert(group.client_count() == 1);
    assert(!group.is_full());
}

void test_remove_client_from_group() {
    wish::session::SessionGroup group(2);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30002};
    group.add_client(addr, base_time);
    assert(group.client_count() == 1);

    bool removed = group.remove_client(addr);
    assert(removed);
    assert(group.client_count() == 0);
    assert(group.find_client(addr) == nullptr);
}

void test_remove_nonexistent_client_returns_false() {
    wish::session::SessionGroup group(3);
    const wish::NetAddress addr {"127.0.0.1", 30003};
    const wish::NetAddress missing_addr {"127.0.0.1", 39999};

    group.add_client(addr, wish::session::SessionGroup::clock::time_point {});
    bool removed = group.remove_client(missing_addr);
    assert(!removed);
    assert(group.client_count() == 1);
}

void test_group_full_detection() {
    wish::session::SessionGroup group(4, 2); // max 2 clients
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr_a {"127.0.0.1", 30004};
    const wish::NetAddress addr_b {"127.0.0.1", 30005};
    const wish::NetAddress addr_c {"127.0.0.1", 30006};

    auto* a = group.add_client(addr_a, base_time);
    assert(a != nullptr);
    assert(!group.is_full());
    assert(group.client_count() == 1);

    auto* b = group.add_client(addr_b, base_time + std::chrono::milliseconds(100));
    assert(b != nullptr);
    assert(group.is_full());
    assert(group.client_count() == 2);

    auto* c = group.add_client(addr_c, base_time + std::chrono::milliseconds(200));
    assert(c == nullptr); // group is full, should not add
    assert(group.client_count() == 2);
}

void test_find_client_in_group() {
    wish::session::SessionGroup group(5);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30007};
    group.add_client(addr, base_time);

    auto* found = group.find_client(addr);
    assert(found != nullptr);
    assert(found->address == addr);

    const wish::NetAddress missing_addr {"127.0.0.1", 37777};
    assert(group.find_client(missing_addr) == nullptr);
}

void test_client_timeout_pruning() {
    wish::session::SessionGroup group(6, 4);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr_a {"127.0.0.1", 30008};
    const wish::NetAddress addr_b {"127.0.0.1", 30009};

    group.add_client(addr_a, base_time);
    group.add_client(addr_b, base_time + std::chrono::milliseconds(100));

    // Advance time past the default 10s timeout
    const auto far_future = base_time + std::chrono::seconds(15);
    group.tick(far_future);

    assert(group.client_count() == 0);
    assert(group.find_client(addr_a) == nullptr);
    assert(group.find_client(addr_b) == nullptr);
}

void test_connected_tracking() {
    wish::session::SessionGroup group(7, 4);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30010};
    auto* client = group.add_client(addr, base_time);

    assert(group.connected_count() == 0); // PendingAdmission by default

    client->connection_state = wish::session::ClientConnectionState::Connected;
    assert(group.connected_count() == 1);
}

void test_add_client_twice_updates_last_seen() {
    wish::session::SessionGroup group(8, 4);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30011};
    group.add_client(addr, base_time);
    assert(group.client_count() == 1);

    // Add the same client again later
    const auto later = base_time + std::chrono::seconds(5);
    auto* client = group.add_client(addr, later);
    assert(client != nullptr);
    assert(client->last_seen == later);
    assert(group.client_count() == 1); // still only one entry
}

void test_for_each_client_enumerates_all() {
    wish::session::SessionGroup group(9, 4);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr_a {"127.0.0.1", 30012};
    const wish::NetAddress addr_b {"127.0.0.1", 30013};

    group.add_client(addr_a, base_time);
    group.add_client(addr_b, base_time + std::chrono::milliseconds(50));

    int count = 0;
    group.for_each_client([&](const wish::session::ClientSession&) {
        ++count;
    });
    assert(count == 2);
}

void test_multiple_client_lifecycle_in_one_group() {
    wish::session::SessionGroup group(10, 8);
    const auto base_time = wish::session::SessionGroup::clock::time_point {};

    // Add 5 clients
    for (wish::u16 port = 30100; port < 30105; ++port) {
        const wish::NetAddress addr {"127.0.0.1", port};
        auto* client = group.add_client(addr, base_time);
        assert(client != nullptr);
        assert(client->address.port == port);
    }

    assert(group.client_count() == 5);
    assert(!group.is_full());

    // Remove one client mid-group
    const wish::NetAddress remove_addr {"127.0.0.1", 30102};
    assert(group.remove_client(remove_addr));
    assert(group.client_count() == 4);

    // Verify the removed client is gone and remaining ones are still there
    assert(group.find_client(remove_addr) == nullptr);
    assert(group.find_client({"127.0.0.1", 30100}) != nullptr);
    assert(group.find_client({"127.0.0.1", 30104}) != nullptr);

    // Add a new client to fill the gap
    const wish::NetAddress new_addr {"127.0.0.1", 30105};
    auto* new_client = group.add_client(new_addr, base_time + std::chrono::milliseconds(200));
    assert(new_client != nullptr);
    assert(group.client_count() == 5);
    assert(new_client->address == new_addr);
}

} // namespace

int main() {
    test_add_client_to_group();
    test_remove_client_from_group();
    test_remove_nonexistent_client_returns_false();
    test_group_full_detection();
    test_find_client_in_group();
    test_client_timeout_pruning();
    test_connected_tracking();
    test_add_client_twice_updates_last_seen();
    test_for_each_client_enumerates_all();
    test_multiple_client_lifecycle_in_one_group();
    return 0;
}
