#include "wish/admin/admin_server.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& msg) {
    std::cerr << "info_page_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

int expect_contains(const std::string& haystack, const std::string& needle, const std::string& label) {
    if (haystack.find(needle) == std::string::npos) {
        return fail(label + ": expected to find \"" + needle + "\"");
    }
    return 0;
}

int expect_not_contains(const std::string& haystack, const std::string& needle, const std::string& label) {
    if (haystack.find(needle) != std::string::npos) {
        return fail(label + ": expected NOT to find \"" + needle + "\"");
    }
    return 0;
}

int test_render_full_page() {
    wish::admin::ServerStatus status {};
    status.game_name = "Ahamkara Engine";
    status.game_port = 7777;
    status.admin_port = 8888;
    status.tick_rate = 60.0F;
    status.server_tick = 42;
    status.max_players = 16;
    status.uptime_seconds = 3661.0F;
    status.match_active = true;
    status.match_elapsed_seconds = 180.0F;
    status.match_duration_seconds = 900.0F;
    status.match_remaining_seconds = 720.0F;
    status.disconnect_timeout_seconds = 10.0F;

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    int failures = 0;
    failures += expect_contains(html, "<!DOCTYPE html>", "doctype");
    failures += expect_contains(html, "Ahamkara", "project name");
    failures += expect_contains(html, "Ahamkara Engine", "game name");
    failures += expect_contains(html, "7777", "game port");
    failures += expect_contains(html, "8888", "admin port");
    failures += expect_contains(html, "60 Hz", "tick rate");
    failures += expect_contains(html, "42", "server tick");
    failures += expect_contains(html, "16", "max players");
    failures += expect_contains(html, "1h 1m 1s", "uptime formatted");
    failures += expect_contains(html, "/health", "health link");
    failures += expect_contains(html, "/players", "players link");
    failures += expect_contains(html, "/metrics", "metrics link");
    failures += expect_contains(html, "Repository", "repo link");
    failures += expect_contains(html, "No players connected", "empty players msg");

    if (failures > 0) {
        return fail(std::to_string(failures) + " checks failed in render_full_page");
    }
    std::cout << "test_render_full_page: ok\n";
    return 0;
}

int test_render_with_players() {
    wish::admin::ServerStatus status {};
    status.game_name = "Test";
    status.match_active = false;

    wish::admin::PlayerStatus p1;
    p1.endpoint = "10.0.0.1:7777";
    p1.seconds_since_seen = 2.5F;
    status.players.push_back(p1);

    wish::admin::PlayerStatus p2;
    p2.endpoint = "10.0.0.2:7777";
    p2.seconds_since_seen = 60.0F;
    status.players.push_back(p2);

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    int failures = 0;
    failures += expect_contains(html, "10.0.0.1:7777", "player 1 endpoint");
    failures += expect_contains(html, "10.0.0.2:7777", "player 2 endpoint");
    failures += expect_contains(html, "2 connected", "player count in header");
    // "No players connected" should NOT appear
    if (html.find("No players connected") != std::string::npos) {
        failures += fail("should not show 'no players' when players exist");
    }

    if (failures > 0) {
        return fail(std::to_string(failures) + " checks failed in render_with_players");
    }
    std::cout << "test_render_with_players: ok\n";
    return 0;
}

int test_render_without_match_remaining() {
    wish::admin::ServerStatus status {};
    status.game_name = "Test";
    status.match_remaining_seconds = {};

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    // Should not crash; remaining time section simply omitted
    if (html.empty()) {
        return fail("HTML should not be empty");
    }

    std::cout << "test_render_without_match_remaining: ok\n";
    return 0;
}

int test_escapes_xss_in_game_name() {
    wish::admin::ServerStatus status {};
    status.game_name = "<script>alert('xss')</script>";

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    int failures = 0;
    failures += expect_contains(html, "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;", "escaped game name");
    failures += expect_not_contains(html, "<script>", "raw script tag must not appear");
    return failures;
}

int test_escapes_xss_in_player_endpoint() {
    wish::admin::ServerStatus status {};

    wish::admin::PlayerStatus p1;
    p1.endpoint = "\"><script>alert(1)</script>";
    p1.seconds_since_seen = 1.0F;
    status.players.push_back(p1);

    const std::string html = wish::admin::HttpAdminServer::render_info_page(status);

    int failures = 0;
    failures += expect_contains(html, "&quot;&gt;&lt;script&gt;alert(1)&lt;/script&gt;", "escaped endpoint");
    failures += expect_not_contains(html, "<script>", "raw script tag must not appear");
    return failures;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_render_full_page();
    failures += test_render_with_players();
    failures += test_render_without_match_remaining();
    failures += test_escapes_xss_in_game_name();
    failures += test_escapes_xss_in_player_endpoint();

    if (failures > 0) {
        std::cerr << failures << " info page test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All info page tests passed.\n";
    return 0;
}
