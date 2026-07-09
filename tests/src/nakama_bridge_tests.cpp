#include "wish/integrations/nakama/nakama_bridge.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
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

#ifdef _WIN32
using SocketHandle = SOCKET;
#define SOCKET_INVALID INVALID_SOCKET
#define CLOSE_SOCKET(s) closesocket(s)
#define SOCKET_IS_VALID(s) ((s) != INVALID_SOCKET)
#else
using SocketHandle = int;
#define SOCKET_INVALID (-1)
#define CLOSE_SOCKET(s) ::close(s)
#define SOCKET_IS_VALID(s) ((s) >= 0)
#endif

struct ScopedSocket {
    SocketHandle fd {SOCKET_INVALID};

    ScopedSocket() = default;
    explicit ScopedSocket(SocketHandle value)
        : fd(value) {
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept
        : fd(other.fd) {
        other.fd = SOCKET_INVALID;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            if (SOCKET_IS_VALID(fd)) {
                CLOSE_SOCKET(fd);
            }
            fd = other.fd;
            other.fd = SOCKET_INVALID;
        }
        return *this;
    }

    ~ScopedSocket() {
        if (SOCKET_IS_VALID(fd)) {
            CLOSE_SOCKET(fd);
        }
    }
};

struct FakeHttpServerResult {
    unsigned short port {0};
    std::thread thread {};
};

FakeHttpServerResult spawn_fake_nakama_server(std::string response, std::string* captured_request) {
    SocketHandle raw_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ScopedSocket listen_socket(raw_sock);
    assert(SOCKET_IS_VALID(listen_socket.fd));

    int reuse = 1;
    assert(::setsockopt(listen_socket.fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == 0);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    assert(::bind(listen_socket.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    assert(::listen(listen_socket.fd, 1) == 0);

#ifdef _WIN32
    int address_length = sizeof(address);
#else
    socklen_t address_length = sizeof(address);
#endif
    assert(::getsockname(listen_socket.fd, reinterpret_cast<sockaddr*>(&address), &address_length) == 0);

    FakeHttpServerResult result {};
    result.port = ntohs(address.sin_port);
    result.thread = std::thread(
        [socket = std::move(listen_socket), response = std::move(response), captured_request]() mutable {
            sockaddr_in client_address {};
#ifdef _WIN32
            int client_length = sizeof(client_address);
#else
            socklen_t client_length = sizeof(client_address);
#endif
            const SocketHandle client_fd = ::accept(socket.fd, reinterpret_cast<sockaddr*>(&client_address), &client_length);
            assert(SOCKET_IS_VALID(client_fd));

            ScopedSocket client_socket(client_fd);
            char buffer[2048] {};
            std::string request;
            while (true) {
#ifdef _WIN32
                const int received = ::recv(client_socket.fd, buffer, sizeof(buffer), 0);
#else
                const ssize_t received = ::recv(client_socket.fd, buffer, sizeof(buffer), 0);
#endif
                if (received <= 0) {
                    break;
                }
                request.append(buffer, static_cast<std::size_t>(received));
                if (request.find("\r\n\r\n") != std::string::npos) {
                    break;
                }
            }

            if (captured_request) {
                *captured_request = request;
            }

            const char* payload = response.data();
            std::size_t remaining = response.size();
            while (remaining > 0) {
#ifdef _WIN32
                const int sent = ::send(client_socket.fd, payload, static_cast<int>(remaining), 0);
#else
                const ssize_t sent = ::send(client_socket.fd, payload, remaining, 0);
#endif
                assert(sent > 0);
                payload += sent;
                remaining -= static_cast<std::size_t>(sent);
            }
        });

    return result;
}

void test_bridge_settings_defaults() {
    char arg0[] = "bridge-test";
    char* argv[] = {arg0, nullptr};
    const auto settings = wish::integrations::nakama::load_bridge_settings(1, argv);
    assert(!settings.enabled);
    assert(settings.host == "127.0.0.1");
    assert(settings.port == 7350);
    assert(settings.account_path == "/v2/account");
    std::cout << "test_bridge_settings_defaults passed.\n";
}

void test_bridge_settings_parse_cli_url() {
    char arg0[] = "bridge-test";
    char arg1[] = "--nakama-url=http://nakama.local:9123/v2/account";
    char* argv[] = {arg0, arg1, nullptr};
    const auto settings = wish::integrations::nakama::load_bridge_settings(2, argv);
    assert(settings.enabled);
    assert(!settings.use_tls);
    assert(settings.host == "nakama.local");
    assert(settings.port == 9123);
    assert(settings.account_path == "/v2/account");
    std::cout << "test_bridge_settings_parse_cli_url passed.\n";
}

void test_validator_rejects_missing_token() {
    wish::integrations::nakama::BridgeSettings settings {};
    settings.enabled = true;
    wish::integrations::nakama::NakamaHttpAuthValidator validator(settings);
    const auto result = validator.validate({.token = "", .remote_endpoint = "127.0.0.1:7777"});
    assert(!result.accepted);
    assert(result.error_message.find("missing Nakama session token") != std::string::npos);
    std::cout << "test_validator_rejects_missing_token passed.\n";
}

void test_validator_accepts_account_lookup() {
    std::string captured_request;
    auto fake_server = spawn_fake_nakama_server(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 49\r\n"
        "\r\n"
        "{\"user\":{\"id\":\"player-123\",\"username\":\"tester\"}}",
        &captured_request);

    wish::integrations::nakama::BridgeSettings settings {};
    settings.enabled = true;
    settings.host = "127.0.0.1";
    settings.port = fake_server.port;
    settings.account_path = "/v2/account";
    settings.timeout_ms = 1500;

    wish::integrations::nakama::NakamaHttpAuthValidator validator(settings);
    const auto result = validator.validate(
        {.token = "nakama-session-token", .remote_endpoint = "127.0.0.1:7777"});
    fake_server.thread.join();

    assert(result.accepted);
    assert(result.player_id == "player-123");
    assert(result.session_id == "nakama-session-token");
    assert(captured_request.find("GET /v2/account HTTP/1.1") != std::string::npos);
    assert(captured_request.find("Authorization: Bearer nakama-session-token") != std::string::npos);
    std::cout << "test_validator_accepts_account_lookup passed.\n";
}

void test_validator_rejects_http_401() {
    auto fake_server = spawn_fake_nakama_server(
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 24\r\n"
        "\r\n"
        "{\"message\":\"unauthorized\"}",
        nullptr);

    wish::integrations::nakama::BridgeSettings settings {};
    settings.enabled = true;
    settings.host = "127.0.0.1";
    settings.port = fake_server.port;

    wish::integrations::nakama::NakamaHttpAuthValidator validator(settings);
    const auto result = validator.validate({.token = "bad-token", .remote_endpoint = "127.0.0.1:7777"});
    fake_server.thread.join();

    assert(!result.accepted);
    assert(result.error_message.find("HTTP 401") != std::string::npos);
    std::cout << "test_validator_rejects_http_401 passed.\n";
}

}  // namespace

int main() {
    test_bridge_settings_defaults();
    test_bridge_settings_parse_cli_url();
    test_validator_rejects_missing_token();
    test_validator_accepts_account_lookup();
    test_validator_rejects_http_401();
    std::cout << "All Nakama bridge tests passed.\n";
    return 0;
}
