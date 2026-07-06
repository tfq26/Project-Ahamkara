#include "wish/admin/admin_server.h"

#include "ae/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wish::admin {
namespace {

constexpr int kRequestBufferSize = 4096;

// Platform abstraction helpers
#ifdef _WIN32
#define SOCKET_EINTR WSAEINTR
#define SOCKET_EBADF WSAENOTSOCK
inline int socket_errno() { return WSAGetLastError(); }
#else
#define SOCKET_EINTR EINTR
#define SOCKET_EBADF EBADF
inline int socket_errno() { return errno; }
#endif

std::string seconds_to_json(float seconds) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << seconds;
    return stream.str();
}

bool write_all(HttpAdminServer::SocketHandle fd, const std::string& data) {
    const char* ptr = data.data();
    std::size_t remaining = data.size();
    while (remaining > 0) {
#ifdef _WIN32
        const int sent = ::send(fd, ptr, static_cast<int>(remaining), 0);
#else
        const ssize_t sent = ::send(fd, ptr, remaining, 0);
#endif
        if (sent < 0) {
            if (socket_errno() == SOCKET_EINTR) {
                continue;
            }
            return false;
        }
        ptr += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

}  // namespace

HttpAdminServer::~HttpAdminServer() {
    stop();
}

bool HttpAdminServer::start(ae::u16 port, StatusProvider provider) {
    if (running_) {
        return true;
    }

    port_ = port;
    status_provider_ = std::move(provider);

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!socket_is_valid(listen_fd_)) {
        ae::log_error("Failed to create admin HTTP socket.");
        return false;
    }

    const int reuse = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) < 0) {
        ae::log_error("Failed to enable SO_REUSEADDR for admin HTTP socket.");
        close_socket(listen_fd_);
        listen_fd_ = socket_invalid();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ae::log_error("Failed to bind admin HTTP socket.");
        close_socket(listen_fd_);
        listen_fd_ = socket_invalid();
        return false;
    }

    if (::listen(listen_fd_, 16) < 0) {
        ae::log_error("Failed to listen on admin HTTP socket.");
        close_socket(listen_fd_);
        listen_fd_ = socket_invalid();
        return false;
    }

    running_ = true;
    thread_ = std::thread(&HttpAdminServer::serve, this);
    return true;
}

void HttpAdminServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (socket_is_valid(listen_fd_)) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        close_socket(listen_fd_);
        listen_fd_ = socket_invalid();
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

bool HttpAdminServer::is_running() const {
    return running_;
}

void HttpAdminServer::serve() {
    while (running_) {
        fd_set read_fds {};
        FD_ZERO(&read_fds);
        FD_SET(listen_fd_, &read_fds);

        timeval timeout {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;

        const int ready = ::select(static_cast<int>(listen_fd_ + 1), &read_fds, nullptr, nullptr, &timeout);
        if (!running_) {
            break;
        }

        if (ready < 0) {
            if (socket_errno() == SOCKET_EINTR) {
                continue;
            }
            if (socket_errno() != SOCKET_EBADF) {
                ae::log_error("Admin HTTP select loop failed.");
            }
            break;
        }

        if (ready == 0 || !FD_ISSET(listen_fd_, &read_fds)) {
            continue;
        }

        sockaddr_in client_address {};
#ifdef _WIN32
        int client_length = sizeof(client_address);
#else
        socklen_t client_length = sizeof(client_address);
#endif
        const SocketHandle client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (!socket_is_valid(client_fd)) {
            if (socket_errno() != SOCKET_EINTR && socket_errno() != SOCKET_EBADF) {
                ae::log_warning("Admin HTTP accept failed.");
            }
            continue;
        }

        handle_client(static_cast<int>(client_fd));
        close_socket(client_fd);
    }
}

void HttpAdminServer::handle_client(int client_fd) const {
    char buffer[kRequestBufferSize + 1] {};
#ifdef _WIN32
    const int received = ::recv(client_fd, buffer, kRequestBufferSize, 0);
#else
    const ssize_t received = ::recv(client_fd, buffer, kRequestBufferSize, 0);
#endif
    if (received <= 0) {
        return;
    }

    buffer[received] = '\0';
    const std::string_view request(buffer);
    const std::size_t line_end = request.find("\r\n");
    const std::string_view request_line = line_end == std::string_view::npos ? request : request.substr(0, line_end);
    const std::string method = parse_method(request_line);
    const std::string path = parse_path(request_line);
    const ServerStatus status = status_provider_ ? status_provider_() : ServerStatus {};

    std::string body;
    int code = 200;
    std::string_view text = "OK";

    if (method != "GET") {
        code = 405;
        text = "Method Not Allowed";
        body = "{\"error\":\"GET only\"}";
    } else if (path == "/health") {
        body = render_health(status);
    } else if (path == "/match/status") {
        body = render_match_status(status);
    } else if (path == "/players") {
        body = render_players(status);
    } else {
        code = 404;
        text = "Not Found";
        body = "{\"error\":\"unknown endpoint\"}";
    }

    const std::string response = make_response(code, text, std::move(body));
    (void)write_all(client_fd, response);
}

std::string HttpAdminServer::make_response(int status_code, std::string_view status_text, std::string body) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n";
    stream << "Content-Type: application/json\r\n";
    stream << "Content-Length: " << body.size() << "\r\n";
    stream << "Connection: close\r\n";
    stream << "Cache-Control: no-store\r\n";
    stream << "\r\n";
    stream << body;
    return stream.str();
}

std::string HttpAdminServer::make_json_response(std::string body) {
    return make_response(200, "OK", std::move(body));
}

std::string HttpAdminServer::make_error_response(int status_code, std::string_view status_text, std::string body) {
    return make_response(status_code, status_text, std::move(body));
}

std::string HttpAdminServer::escape_json(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream hex;
                    hex << "\\u";
                    hex.width(4);
                    hex.fill('0');
                    hex << std::hex << static_cast<int>(static_cast<unsigned char>(c));
                    escaped += hex.str();
                } else {
                    escaped += c;
                }
                break;
        }
    }
    return escaped;
}

std::string HttpAdminServer::render_health(const ServerStatus& status) {
    std::ostringstream stream;
    stream << '{'
           << "\"status\":\"ok\","
           << "\"service\":\"" << escape_json(status.game_name) << "\","
           << "\"game_port\":" << status.game_port << ','
           << "\"admin_port\":" << status.admin_port << ','
           << "\"tick_rate\":" << seconds_to_json(status.tick_rate)
           << '}';
    return stream.str();
}

std::string HttpAdminServer::render_match_status(const ServerStatus& status) {
    std::ostringstream stream;
    stream << '{'
           << "\"status\":\"ok\","
           << "\"server_tick\":" << status.server_tick << ','
           << "\"uptime_seconds\":" << seconds_to_json(status.uptime_seconds) << ','
           << "\"match\":{"
           << "\"active\":" << (status.match_active ? "true" : "false") << ','
           << "\"elapsed_seconds\":" << seconds_to_json(status.match_elapsed_seconds) << ','
           << "\"duration_seconds\":" << seconds_to_json(status.match_duration_seconds) << ',';
    if (status.match_remaining_seconds.has_value()) {
        stream << "\"remaining_seconds\":" << seconds_to_json(*status.match_remaining_seconds);
    } else {
        stream << "\"remaining_seconds\":null";
    }
    stream << "}}";
    return stream.str();
}

std::string HttpAdminServer::render_players(const ServerStatus& status) {
    std::ostringstream stream;
    stream << '{'
           << "\"status\":\"ok\","
           << "\"connected\":" << status.players.size() << ','
           << "\"max_players\":" << status.max_players << ','
           << "\"disconnect_timeout_seconds\":" << seconds_to_json(status.disconnect_timeout_seconds) << ','
           << "\"players\":[";
    for (std::size_t i = 0; i < status.players.size(); ++i) {
        if (i > 0) {
            stream << ',';
        }
        const auto& player = status.players[i];
        stream << '{'
               << "\"endpoint\":\"" << escape_json(player.endpoint) << "\","
               << "\"seconds_since_seen\":" << seconds_to_json(player.seconds_since_seen)
               << '}';
    }
    stream << "]}";
    return stream.str();
}

std::string HttpAdminServer::status_code_text(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default: return "Error";
    }
}

std::string HttpAdminServer::parse_path(std::string_view request_line) {
    const std::size_t first_space = request_line.find(' ');
    if (first_space == std::string_view::npos) {
        return "/";
    }

    const std::size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos) {
        return "/";
    }

    std::string_view path = request_line.substr(first_space + 1, second_space - first_space - 1);
    const std::size_t query_pos = path.find('?');
    if (query_pos != std::string_view::npos) {
        path = path.substr(0, query_pos);
    }

    return std::string(path);
}

std::string HttpAdminServer::parse_method(std::string_view request_line) {
    const std::size_t space = request_line.find(' ');
    if (space == std::string_view::npos) {
        return {};
    }

    return std::string(request_line.substr(0, space));
}

}  // namespace wish::admin
