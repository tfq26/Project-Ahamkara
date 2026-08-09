/// @file test_landing_page.cpp
///
/// Integration test for the Wish admin HTTP server landing page endpoint.
///
/// The test starts the real HttpAdminServer on a loopback port, issues an
/// HTTP GET / (and /index.html), and verifies:
///   1. The server responds with HTTP 200 and a text/html body.
///   2. The landing page contains all major sections (Server Status, Match,
///      Players, API Endpoints) and the expected server status values.
///   3. The landing page is XSS-safe: user-controlled fields (game name,
///      player endpoints) are HTML-escaped so no raw unescaped user input
///      reaches the rendered document.
///
/// Acceptance criteria: presence of all major sections and absence of
/// unescaped user input.

#include "wish/admin/admin_server.h"
#include "wish/admin/heartbeat_service.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

int fail(const std::string& msg) {
    std::cerr << "test_landing_page failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void close_socket_handle(SocketHandle s) {
#ifdef _WIN32
    ::closesocket(s);
#else
    ::close(s);
#endif
}

// ── Helpers ──────────────────────────────────────────────────────────────

// Bind a loopback socket to port 0 and read back the OS-assigned port so the
// integration test can start the admin server on a free port.
bool find_free_port(std::uint16_t& out_port) {
    const SocketHandle fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kInvalidSocket) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket_handle(fd);
        return false;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        close_socket_handle(fd);
        return false;
    }

    out_port = ntohs(addr.sin_port);
    close_socket_handle(fd);
    return true;
}

// Send a minimal HTTP/1.1 GET request to 127.0.0.1:port and return the raw
// response bytes (status line, headers, and body).
bool http_get(std::uint16_t port, const std::string& path, std::string& out_response) {
    const SocketHandle fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kInvalidSocket) {
        return false;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    // Small retry loop: the listening socket exists synchronously after
    // start(), but give the accept loop a moment to be ready.
    bool connected = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!connected) {
        close_socket_handle(fd);
        return false;
    }

    const std::string request = "GET " + path + " HTTP/1.1\r\n"
                                                "Host: 127.0.0.1\r\n"
                                                "Connection: close\r\n"
                                                "\r\n";
    const char* cursor = request.data();
    std::size_t remaining = request.size();
    while (remaining > 0) {
#ifdef _WIN32
        const int sent = ::send(fd, cursor, static_cast<int>(remaining), 0);
#else
        const ssize_t sent = ::send(fd, cursor, remaining, 0);
#endif
        if (sent <= 0) {
            close_socket_handle(fd);
            return false;
        }
        cursor += sent;
        remaining -= static_cast<std::size_t>(sent);
    }

    out_response.clear();
    char buffer[4096];
    for (;;) {
#ifdef _WIN32
        const int received = ::recv(fd, buffer, sizeof(buffer), 0);
#else
        const ssize_t received = ::recv(fd, buffer, sizeof(buffer), 0);
#endif
        if (received <= 0) {
            break;
        }
        out_response.append(buffer, static_cast<std::size_t>(received));
    }

    close_socket_handle(fd);
    return true;
}

struct HttpResponse {
    std::string status_line {};
    std::string headers {};
    std::string body {};
};

bool split_response(const std::string& raw, HttpResponse& out) {
    const std::size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return false;
    }
    const std::string head = raw.substr(0, header_end);
    out.body = raw.substr(header_end + 4);

    const std::size_t first_lf = head.find("\r\n");
    out.status_line = first_lf == std::string::npos ? head : head.substr(0, first_lf);
    out.headers = first_lf == std::string::npos ? std::string {} : head.substr(first_lf + 2);
    return true;
}

bool status_is_ok(const std::string& status_line) {
    return status_line.find("200") != std::string::npos && status_line.find("OK") != std::string::npos;
}

bool header_contains(const std::string& headers, const std::string& needle) {
    return headers.find(needle) != std::string::npos;
}

int expect_contains(const std::string& haystack, const std::string& needle, const std::string& label) {
    if (haystack.find(needle) == std::string::npos) {
        return fail(label + ": expected to find \"" + needle + "\"");
    }
    return 0;
}

// Build a ServerStatus with the fields the landing page renders.
wish::admin::ServerStatus make_status(bool include_players = true) {
    wish::admin::ServerStatus status {};
    status.game_name = "Ahamkara Engine";
    status.game_port = 7777;
    status.admin_port = 8888;
    status.tick_rate = 60.0F;
    status.server_tick = 42;
    status.max_players = 16;
    status.disconnect_timeout_seconds = 10.0F;
    status.match_duration_seconds = 900.0F;
    status.uptime_seconds = 3661.0F;
    status.match_elapsed_seconds = 180.0F;
    status.match_remaining_seconds = 720.0F;
    status.match_active = true;

    if (include_players) {
        wish::admin::PlayerStatus p1;
        p1.endpoint = "10.0.0.1:7777";
        p1.seconds_since_seen = 2.5F;
        status.players.push_back(p1);

        wish::admin::PlayerStatus p2;
        p2.endpoint = "10.0.0.2:7777";
        p2.seconds_since_seen = 60.0F;
        status.players.push_back(p2);
    }
    return status;
}

// ── Tests ────────────────────────────────────────────────────────────────

int test_landing_page_served() {
    std::uint16_t port = 0;
    EXPECT(find_free_port(port), "failed to find a free port");

    wish::admin::HeartbeatService heartbeat(std::chrono::seconds(30));
    wish::admin::HttpAdminServer server;
    EXPECT(server.start(port, []() { return make_status(); }, heartbeat), "failed to start admin server");

    std::string raw;
    EXPECT(http_get(port, "/", raw), "HTTP GET / failed");
    server.stop();

    HttpResponse response;
    EXPECT(split_response(raw, response), "could not parse HTTP response");
    EXPECT(status_is_ok(response.status_line), "expected HTTP 200 OK, got: " + response.status_line);
    EXPECT(header_contains(response.headers, "Content-Type: text/html"), "expected text/html content type");

    const std::string& html = response.body;
    int failures = 0;
    failures += expect_contains(html, "<!DOCTYPE html>", "doctype");
    failures += expect_contains(html, "Ahamkara", "project name");
    failures += expect_contains(html, "Server Status", "server status section");
    failures += expect_contains(html, "Match", "match section");
    failures += expect_contains(html, "Players", "players section");
    failures += expect_contains(html, "API Endpoints", "api endpoints section");
    failures += expect_contains(html, "Ahamkara Engine", "game name");
    failures += expect_contains(html, "7777", "game port");
    failures += expect_contains(html, "8888", "admin port");
    failures += expect_contains(html, "60 Hz", "tick rate");
    failures += expect_contains(html, "16", "max players");
    failures += expect_contains(html, "10.0.0.1:7777", "player 1 endpoint");
    failures += expect_contains(html, "10.0.0.2:7777", "player 2 endpoint");
    failures += expect_contains(html, "/health", "health link");
    failures += expect_contains(html, "/match/status", "match status link");
    failures += expect_contains(html, "/players", "players link");
    failures += expect_contains(html, "/metrics", "metrics link");
    failures += expect_contains(html, "/api/v1/servers", "servers link");
    failures += expect_contains(html, "Repository", "repository link");

    if (failures > 0) {
        return fail(std::to_string(failures) + " content checks failed in landing page");
    }
    std::cout << "test_landing_page_served: ok\n";
    return 0;
}

int test_index_html_alias_served() {
    std::uint16_t port = 0;
    EXPECT(find_free_port(port), "failed to find a free port");

    wish::admin::HeartbeatService heartbeat(std::chrono::seconds(30));
    wish::admin::HttpAdminServer server;
    EXPECT(server.start(port, []() { return make_status(); }, heartbeat), "failed to start admin server");

    std::string raw;
    EXPECT(http_get(port, "/index.html", raw), "HTTP GET /index.html failed");
    server.stop();

    HttpResponse response;
    EXPECT(split_response(raw, response), "could not parse HTTP response");
    EXPECT(status_is_ok(response.status_line), "expected HTTP 200 OK for /index.html");
    EXPECT(header_contains(response.headers, "Content-Type: text/html"), "expected text/html content type");
    EXPECT(response.body.find("Server Status") != std::string::npos, "/index.html should render the landing page");

    std::cout << "test_index_html_alias_served: ok\n";
    return 0;
}

int test_unknown_path_returns_404() {
    std::uint16_t port = 0;
    EXPECT(find_free_port(port), "failed to find a free port");

    wish::admin::HeartbeatService heartbeat(std::chrono::seconds(30));
    wish::admin::HttpAdminServer server;
    EXPECT(server.start(port, []() { return make_status(); }, heartbeat), "failed to start admin server");

    std::string raw;
    EXPECT(http_get(port, "/does-not-exist", raw), "HTTP GET for unknown path failed");
    server.stop();

    EXPECT(raw.find("404") != std::string::npos, "expected 404 for unknown path");

    std::cout << "test_unknown_path_returns_404: ok\n";
    return 0;
}

int test_landing_page_xss_safe() {
    // The status provider is created before the server thread starts; the
    // rendered document must never contain the raw, unescaped user input.
    wish::admin::ServerStatus status {};
    status.game_name = "<script>alert('game')</script>";
    status.match_active = false;

    wish::admin::PlayerStatus p1;
    p1.endpoint = "<img src=x onerror=alert('player')>";
    p1.seconds_since_seen = 1.0F;
    status.players.push_back(p1);

    std::uint16_t port = 0;
    EXPECT(find_free_port(port), "failed to find a free port");

    wish::admin::HeartbeatService heartbeat(std::chrono::seconds(30));
    wish::admin::HttpAdminServer server;
    EXPECT(server.start(port, [&]() { return status; }, heartbeat), "failed to start admin server");

    std::string raw;
    EXPECT(http_get(port, "/", raw), "HTTP GET / failed");
    server.stop();

    HttpResponse response;
    EXPECT(split_response(raw, response), "could not parse HTTP response");
    EXPECT(status_is_ok(response.status_line), "expected HTTP 200 OK");

    const std::string& html = response.body;

    // Absence of unescaped user input: the raw payloads must not appear.
    EXPECT(html.find("<script>alert('game')</script>") == std::string::npos,
           "raw <script> payload must be escaped");
    EXPECT(html.find("<img src=x onerror=alert('player')>") == std::string::npos,
           "raw <img> payload must be escaped");

    // The escaped forms must be present.
    int failures = 0;
    failures += expect_contains(html, "&lt;script&gt;alert(&#39;game&#39;)&lt;/script&gt;", "escaped game name");
    failures += expect_contains(html, "&lt;img src=x onerror=alert(&#39;player&#39;)&gt;", "escaped player endpoint");

    if (failures > 0) {
        return fail(std::to_string(failures) + " escaping checks failed in landing page");
    }
    std::cout << "test_landing_page_xss_safe: ok\n";
    return 0;
}

} // namespace

int main() {
#ifdef _WIN32
    WSADATA wsa_data {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    int failures = 0;
    failures += test_landing_page_served();
    failures += test_index_html_alias_served();
    failures += test_unknown_path_returns_404();
    failures += test_landing_page_xss_safe();

#ifdef _WIN32
    WSACleanup();
#endif

    if (failures > 0) {
        std::cerr << failures << " landing page test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All landing page integration tests passed.\n";
    return 0;
}
