#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "wish/types.h"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace wish::admin {

struct PlayerStatus {
    std::string endpoint {};
    float seconds_since_seen {0.0F};
};

struct ServerStatus {
    std::string game_name {"Wish Engine"};
    wish::u16 game_port {0};
    wish::u16 admin_port {0};
    float tick_rate {0.0F};
    wish::u32 server_tick {0};
    int max_players {0};
    float disconnect_timeout_seconds {0.0F};
    float match_duration_seconds {0.0F};
    float uptime_seconds {0.0F};
    float match_elapsed_seconds {0.0F};
    std::optional<float> match_remaining_seconds {};
    bool match_active {true};
    std::vector<PlayerStatus> players {};
};

class HttpAdminServer {
public:
    using StatusProvider = std::function<ServerStatus()>;

    HttpAdminServer() = default;
    HttpAdminServer(const HttpAdminServer&) = delete;
    HttpAdminServer& operator=(const HttpAdminServer&) = delete;
    HttpAdminServer(HttpAdminServer&&) = delete;
    HttpAdminServer& operator=(HttpAdminServer&&) = delete;
    ~HttpAdminServer();

    bool start(wish::u16 port, StatusProvider provider);
    void stop();

    [[nodiscard]] bool is_running() const;

    void serve();
    void handle_client(int client_fd) const;

#ifdef _WIN32
    using SocketHandle = unsigned long long;
#else
    using SocketHandle = int;
#endif

private:

    static void close_socket(SocketHandle s) {
#ifdef _WIN32
        closesocket(s);
#else
        ::close(s);
#endif
    }

    static std::string make_response(int status_code, std::string_view status_text, std::string body);
    static std::string make_json_response(std::string body);
    static std::string make_error_response(int status_code, std::string_view status_text, std::string body);
    static std::string escape_json(std::string_view text);
    static std::string render_health(const ServerStatus& status);
    static std::string render_match_status(const ServerStatus& status);
    static std::string render_players(const ServerStatus& status);
    static std::string status_code_text(int status_code);
    static std::string parse_path(std::string_view request_line);
    static std::string parse_method(std::string_view request_line);

    wish::u16 port_ {0};
    SocketHandle listen_fd_ {socket_invalid()};

    static SocketHandle socket_invalid() {
#ifdef _WIN32
        return INVALID_SOCKET;
#else
        return -1;
#endif
    }

    static bool socket_is_valid(SocketHandle s) {
#ifdef _WIN32
        return s != INVALID_SOCKET;
#else
        return s >= 0;
#endif
    }
    StatusProvider status_provider_ {};
    std::thread thread_ {};
    bool running_ {false};
};

}  // namespace wish::admin
