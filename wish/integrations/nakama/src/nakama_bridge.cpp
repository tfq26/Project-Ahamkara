#define WISH_LOG_CATEGORY "NakamaBridge"

#include "wish/types.h"
#include "wish/log.h"
#include "wish/integrations/nakama/nakama_bridge.h"

#include "wish/integrations/nakama/mock_session_services.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
using SSizeT = int;
#define CLOSE_SOCKET(s) closesocket(s)
#define SOCKET_ERRNO WSAGetLastError()
#define SOCKET_ERRMSG(e) (std::to_string(e))
#define SOCKET_IS_VALID(s) ((s) != INVALID_SOCKET)
#define SOCKET_INVALID INVALID_SOCKET
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
using SSizeT = ssize_t;
#define CLOSE_SOCKET(s) ::close(s)
#define SOCKET_ERRNO errno
#define SOCKET_ERRMSG(e) std::strerror(e)
#define SOCKET_IS_VALID(s) ((s) >= 0)
#define SOCKET_INVALID (-1)
#endif

namespace wish::integrations::nakama {

namespace {

bool parse_bool_text(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

void apply_env_bool(const char* name, bool& value) {
    if (const char* raw = std::getenv(name)) {
        value = parse_bool_text(wish::trim(raw));
    }
}

void apply_env_int(const char* name, int& value) {
    if (const char* raw = std::getenv(name)) {
        try {
            value = std::stoi(std::string(wish::trim(raw)));
        } catch (...) {
        }
    }
}

void apply_env_port(const char* name, unsigned short& value) {
    if (const char* raw = std::getenv(name)) {
        try {
            const int parsed = std::stoi(std::string(wish::trim(raw)));
            if (parsed > 0 && parsed <= 65535) {
                value = static_cast<unsigned short>(parsed);
            }
        } catch (...) {
        }
    }
}

void apply_env_string(const char* name, std::string& value) {
    if (const char* raw = std::getenv(name)) {
        value = std::string(wish::trim(raw));
    }
}

std::string_view cli_value(const char* arg, std::string_view key) {
    const std::string prefix = std::string("--") + std::string(key) + "=";
    const std::string_view text(arg);
    if (text.starts_with(prefix)) {
        return text.substr(prefix.size());
    }
    return {};
}

void apply_cli_bool(const char* arg, const char* key, bool& value) {
    const std::string flag = std::string("--") + key;
    if (std::string_view(arg) == flag) {
        value = true;
    }
}

void apply_cli_string(const char* arg, std::string_view key, std::string& value) {
    const std::string_view raw = cli_value(arg, key);
    if (!raw.empty()) {
        value = std::string(raw);
    }
}

void apply_cli_int(const char* arg, std::string_view key, int& value) {
    const std::string_view raw = cli_value(arg, key);
    if (raw.empty()) {
        return;
    }
    try {
        value = std::stoi(std::string(raw));
    } catch (...) {
    }
}

void apply_cli_port(const char* arg, std::string_view key, unsigned short& value) {
    const std::string_view raw = cli_value(arg, key);
    if (raw.empty()) {
        return;
    }
    try {
        const int parsed = std::stoi(std::string(raw));
        if (parsed > 0 && parsed <= 65535) {
            value = static_cast<unsigned short>(parsed);
        }
    } catch (...) {
    }
}

void parse_url_into_settings(std::string_view url, BridgeSettings& settings) {
    const auto trimmed = wish::trim(url);
    if (trimmed.empty()) {
        return;
    }

    std::string_view rest = trimmed;
    if (rest.starts_with("http://")) {
        settings.use_tls = false;
        rest.remove_prefix(7);
    } else if (rest.starts_with("https://")) {
        settings.use_tls = true;
        rest.remove_prefix(8);
    }

    const auto slash = rest.find('/');
    const std::string_view host_port = slash == std::string_view::npos ? rest : rest.substr(0, slash);
    if (slash != std::string_view::npos) {
        settings.account_path = std::string(rest.substr(slash));
    }

    const auto colon = host_port.rfind(':');
    if (colon != std::string_view::npos && colon + 1 < host_port.size()) {
        settings.host = std::string(host_port.substr(0, colon));
        try {
            const int parsed_port = std::stoi(std::string(host_port.substr(colon + 1)));
            if (parsed_port > 0 && parsed_port <= 65535) {
                settings.port = static_cast<unsigned short>(parsed_port);
            }
        } catch (...) {
        }
    } else if (!host_port.empty()) {
        settings.host = std::string(host_port);
    }
}

std::string build_http_request(const BridgeSettings& settings, std::string_view token) {
    std::ostringstream request;
    request << "GET " << settings.account_path << " HTTP/1.1\r\n"
            << "Host: " << settings.host << ':' << settings.port << "\r\n"
            << "Authorization: Bearer " << token << "\r\n"
            << "Accept: application/json\r\n"
            << "Connection: close\r\n\r\n";
    return request.str();
}

std::string extract_http_body(std::string_view response) {
    const std::size_t split = response.find("\r\n\r\n");
    if (split == std::string_view::npos) {
        return {};
    }
    return std::string(response.substr(split + 4));
}

int parse_http_status(std::string_view response) {
    const std::size_t eol = response.find("\r\n");
    const std::string_view status_line = eol == std::string_view::npos ? response : response.substr(0, eol);
    const std::size_t first_space = status_line.find(' ');
    if (first_space == std::string_view::npos) {
        return 0;
    }
    const std::size_t second_space = status_line.find(' ', first_space + 1);
    const std::string_view code_text = status_line.substr(
        first_space + 1,
        second_space == std::string_view::npos ? std::string_view::npos : second_space - first_space - 1);
    try {
        return std::stoi(std::string(code_text));
    } catch (...) {
        return 0;
    }
}

std::string extract_json_string(std::string_view json, std::string_view key) {
    const std::string search = "\"" + std::string(key) + "\"";
    const std::size_t key_pos = json.find(search);
    if (key_pos == std::string_view::npos) {
        return {};
    }

    const std::size_t colon_pos = json.find(':', key_pos + search.size());
    if (colon_pos == std::string_view::npos) {
        return {};
    }

    std::size_t value_start = json.find('"', colon_pos + 1);
    if (value_start == std::string_view::npos) {
        return {};
    }
    ++value_start;

    std::string value;
    bool escaping = false;
    for (std::size_t i = value_start; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaping) {
            value.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }

    return {};
}

std::string extract_user_id(std::string_view body) {
    const std::string user_key = "\"user\"";
    const std::size_t user_pos = body.find(user_key);
    if (user_pos != std::string_view::npos) {
        const std::size_t object_start = body.find('{', user_pos + user_key.size());
        if (object_start != std::string_view::npos) {
            int depth = 0;
            bool in_string = false;
            bool escaping = false;
            for (std::size_t i = object_start; i < body.size(); ++i) {
                const char ch = body[i];
                if (in_string) {
                    if (escaping) {
                        escaping = false;
                    } else if (ch == '\\') {
                        escaping = true;
                    } else if (ch == '"') {
                        in_string = false;
                    }
                    continue;
                }

                if (ch == '"') {
                    in_string = true;
                } else if (ch == '{') {
                    ++depth;
                } else if (ch == '}') {
                    --depth;
                    if (depth == 0) {
                        const std::string nested = std::string(body.substr(object_start, i - object_start + 1));
                        if (const std::string id = extract_json_string(nested, "id"); !id.empty()) {
                            return id;
                        }
                        break;
                    }
                }
            }
        }
    }

    return extract_json_string(body, "id");
}

wish::core::AuthResult reject_result(std::string message) {
    wish::core::AuthResult result {};
    result.accepted = false;
    result.error_message = std::move(message);
    return result;
}

wish::core::AuthResult perform_account_lookup(const BridgeSettings& settings, const wish::core::AuthRequest& request) {
    if (request.token.empty()) {
        wish::log_error_cat(WISH_LOG_CATEGORY, "Auth request missing session token");
        return reject_result("missing Nakama session token");
    }

    if (settings.use_tls) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "https Nakama URLs are not yet supported by the built-in Wish bridge");
        return reject_result("https Nakama URLs are not yet supported by the built-in Wish bridge");
    }

    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* resolved = nullptr;
    const std::string port_text = std::to_string(settings.port);
    if (const int rc = ::getaddrinfo(settings.host.c_str(), port_text.c_str(), &hints, &resolved); rc != 0) {
        const std::string err = std::string("failed to resolve Nakama host: ") + ::gai_strerror(rc);
        wish::log_error_cat(WISH_LOG_CATEGORY, err);
        return reject_result(err);
    }

    const std::unique_ptr<addrinfo, void (*)(addrinfo*)> resolved_guard(resolved, ::freeaddrinfo);
#if defined(_WIN32)
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
#endif
    SocketHandle sock = SOCKET_INVALID;
    for (addrinfo* current = resolved; current != nullptr; current = current->ai_next) {
        sock = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (!SOCKET_IS_VALID(sock)) {
            continue;
        }

        timeval timeout {};
        timeout.tv_sec = settings.timeout_ms / 1000;
        timeout.tv_usec = (settings.timeout_ms % 1000) * 1000;
        ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), static_cast<int>(sizeof(timeout)));
        ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), static_cast<int>(sizeof(timeout)));

        if (::connect(sock, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }

        CLOSE_SOCKET(sock);
        sock = SOCKET_INVALID;
    }

    if (!SOCKET_IS_VALID(sock)) {
        wish::log_error_cat(WISH_LOG_CATEGORY, "Failed to connect to Nakama at " + settings.host + ":" + std::to_string(settings.port));
        return reject_result("failed to connect to Nakama");
    }

    const std::string request_text = build_http_request(settings, request.token);
    std::size_t total_sent = 0;
    while (total_sent < request_text.size()) {
        const SSizeT sent = ::send(
            sock, request_text.data() + static_cast<std::ptrdiff_t>(total_sent),
            static_cast<int>(request_text.size() - total_sent), 0);
        if (sent <= 0) {
            const std::string error_message = SOCKET_ERRMSG(SOCKET_ERRNO);
            wish::log_error_cat(WISH_LOG_CATEGORY, "Failed to send Nakama validation request: " + error_message);
            CLOSE_SOCKET(sock);
            return reject_result("failed to send Nakama validation request: " + error_message);
        }
        total_sent += static_cast<std::size_t>(sent);
    }
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Sent " + std::to_string(total_sent) + " bytes to Nakama");

    std::string response;
    std::array<char, 2048> buffer {};
    while (true) {
        const SSizeT received = ::recv(sock, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received == 0) {
            break;
        }
        if (received < 0) {
            const std::string error_message = SOCKET_ERRMSG(SOCKET_ERRNO);
            wish::log_error_cat(WISH_LOG_CATEGORY, "Failed to read Nakama validation response: " + error_message);
            CLOSE_SOCKET(sock);
            return reject_result("failed to read Nakama validation response: " + error_message);
        }
        response.append(buffer.data(), static_cast<std::size_t>(received));
    }
    CLOSE_SOCKET(sock);

    wish::log_trace_cat(WISH_LOG_CATEGORY, "Received " + std::to_string(response.size()) + " bytes from Nakama");

    const int status_code = parse_http_status(response);
    if (status_code != 200) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Nakama validation rejected session token (HTTP " + std::to_string(status_code) + ")");
        std::ostringstream message;
        message << "Nakama validation rejected session token (HTTP " << status_code << ")";
        return reject_result(message.str());
    }

    const std::string body = extract_http_body(response);
    const std::string player_id = extract_user_id(body);
    if (player_id.empty()) {
        return reject_result("Nakama account lookup succeeded but no player id was returned");
    }

    wish::log_info_cat(WISH_LOG_CATEGORY, "Nakama authentication succeeded for player " + player_id);

    wish::core::AuthResult result {};
    result.accepted = true;
    result.player_id = player_id;
    result.session_id = request.token;
    return result;
}

}  // namespace

bool is_enabled(const BridgeSettings& settings) noexcept {
    const bool enabled = settings.enabled;
    wish::log_debug_cat(WISH_LOG_CATEGORY, "Bridge " + std::string(enabled ? "is enabled" : "is disabled"));
    return enabled;
}

BridgeSettings load_bridge_settings(int argc, char** argv) {
    BridgeSettings settings {};

    apply_env_bool("WISH_NAKAMA_ENABLED", settings.enabled);
    apply_env_bool("WISH_NAKAMA_TLS", settings.use_tls);
    apply_env_string("WISH_NAKAMA_HOST", settings.host);
    apply_env_port("WISH_NAKAMA_PORT", settings.port);
    apply_env_string("WISH_NAKAMA_ACCOUNT_PATH", settings.account_path);
    apply_env_int("WISH_NAKAMA_TIMEOUT_MS", settings.timeout_ms);
    if (const char* raw_url = std::getenv("WISH_NAKAMA_URL")) {
        parse_url_into_settings(raw_url, settings);
    }

    for (int i = 1; i < argc; ++i) {
        apply_cli_bool(argv[i], "nakama", settings.enabled);
        apply_cli_string(argv[i], "nakama-host", settings.host);
        apply_cli_port(argv[i], "nakama-port", settings.port);
        apply_cli_string(argv[i], "nakama-account-path", settings.account_path);
        apply_cli_int(argv[i], "nakama-timeout-ms", settings.timeout_ms);
        if (const std::string_view url = cli_value(argv[i], "nakama-url"); !url.empty()) {
            parse_url_into_settings(url, settings);
            settings.enabled = true;
        }
    }

    if (settings.account_path.empty() || settings.account_path.front() != '/') {
        settings.account_path = "/" + settings.account_path;
    }
    if (settings.timeout_ms <= 0) {
        settings.timeout_ms = 1500;
    }

    wish::log_info_cat(WISH_LOG_CATEGORY, "Bridge settings loaded: enabled=" +
                       std::string(settings.enabled ? "true" : "false") +
                       ", host=" + settings.host + ":" + std::to_string(settings.port) +
                       ", path=" + settings.account_path +
                       ", timeout=" + std::to_string(settings.timeout_ms) + "ms");

    return settings;
}

std::string describe_bridge(const BridgeSettings& settings) {
    std::ostringstream description;
    description << (settings.use_tls ? "https://" : "http://")
                << settings.host << ':' << settings.port << settings.account_path
                << " (timeout=" << settings.timeout_ms << "ms)";
    const std::string desc = description.str();
    wish::log_debug_cat(WISH_LOG_CATEGORY, "Bridge description: " + desc);
    return desc;
}

NakamaHttpAuthValidator::NakamaHttpAuthValidator(BridgeSettings settings)
    : settings_(std::move(settings)) {
}

wish::core::AuthResult NakamaHttpAuthValidator::validate(const wish::core::AuthRequest& request) const {
    return perform_account_lookup(settings_, request);
}

std::unique_ptr<wish::core::AuthValidator> make_auth_validator(const BridgeSettings& settings) {
    if (is_enabled(settings)) {
        wish::log_info_cat(WISH_LOG_CATEGORY, "Creating NakamaHttpAuthValidator");
        return std::make_unique<NakamaHttpAuthValidator>(settings);
    }
    wish::log_info_cat(WISH_LOG_CATEGORY, "Creating NoopAuthValidator (Nakama bridge disabled)");
    return std::make_unique<NoopAuthValidator>();
}

}  // namespace wish::integrations::nakama
