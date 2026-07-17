#include "wish/admin/heartbeat_service.h"
#include "wish/types.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

int fail(const std::string& msg) {
    std::cerr << "heartbeat_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_register_heartbeat() {
    wish::admin::HeartbeatService svc(std::chrono::seconds(30));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);

    const auto servers = svc.get_servers();
    EXPECT(servers.size() == 1, "should have 1 server");
    EXPECT(servers[0].id == "server-1", "server id should match");
    EXPECT(servers[0].address == "127.0.0.1", "address should match");
    EXPECT(servers[0].port == 7777, "port should match");
    EXPECT(servers[0].alive, "server should be alive");

    std::cout << "test_register_heartbeat: ok\n";
    return 0;
}

int test_check_server_alive() {
    wish::admin::HeartbeatService svc(std::chrono::seconds(30));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);
    EXPECT(svc.is_alive("server-1"), "server-1 should be alive");
    EXPECT(!svc.is_alive("unknown"), "unknown server should not be alive");

    std::cout << "test_check_server_alive: ok\n";
    return 0;
}

int test_server_timeout_detection() {
    wish::admin::HeartbeatService svc(std::chrono::milliseconds(10));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);
    EXPECT(svc.is_alive("server-1"), "server should be alive initially");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(!svc.is_alive("server-1"), "server should be dead after timeout");

    std::cout << "test_server_timeout_detection: ok\n";
    return 0;
}

int test_prune_dead_servers() {
    wish::admin::HeartbeatService svc(std::chrono::milliseconds(10));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);
    svc.report_heartbeat("server-2", "127.0.0.2", 7778);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT(svc.get_servers().size() == 2, "should have 2 servers before prune");

    svc.prune_dead_servers();

    const auto after = svc.get_servers();
    EXPECT(after.size() == 0, "should have 0 servers after prune");

    std::cout << "test_prune_dead_servers: ok\n";
    return 0;
}

int test_prune_keeps_alive_servers() {
    wish::admin::HeartbeatService svc(std::chrono::seconds(30));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);
    svc.report_heartbeat("server-2", "127.0.0.2", 7778);

    svc.prune_dead_servers();

    const auto after = svc.get_servers();
    EXPECT(after.size() == 2, "should have 2 servers after prune (both alive)");
    EXPECT(after[0].alive, "server-1 should still be alive");
    EXPECT(after[1].alive, "server-2 should still be alive");

    std::cout << "test_prune_keeps_alive_servers: ok\n";
    return 0;
}

int test_update_existing_heartbeat() {
    wish::admin::HeartbeatService svc(std::chrono::seconds(30));

    svc.report_heartbeat("server-1", "127.0.0.1", 7777);
    svc.report_heartbeat("server-1", "10.0.0.1", 8888);

    const auto servers = svc.get_servers();
    EXPECT(servers.size() == 1, "should still have 1 server");
    EXPECT(servers[0].address == "10.0.0.1", "address should be updated");
    EXPECT(servers[0].port == 8888, "port should be updated");

    std::cout << "test_update_existing_heartbeat: ok\n";
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_register_heartbeat();
    failures += test_check_server_alive();
    failures += test_server_timeout_detection();
    failures += test_prune_dead_servers();
    failures += test_prune_keeps_alive_servers();
    failures += test_update_existing_heartbeat();

    if (failures > 0) {
        std::cerr << failures << " heartbeat test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All heartbeat tests passed.\n";
    return 0;
}
