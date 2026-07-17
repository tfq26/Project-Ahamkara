#pragma once

#include "wish/types.h"
#include "wish/log.h"

#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace wish::admin {

struct ServerConfig {
    wish::u16 port {7777};
    wish::u16 admin_port {7778};
    float tick_rate {60.0F};
    int max_players {8};
    float disconnect_timeout_seconds {10.0F};
    float match_duration_seconds {600.0F};
    std::string map_path {"assets/compiled/levels/javelin4.aelevel"};
};

namespace detail {

inline std::string_view cli_value(const char* arg, std::string_view key) {
    const std::string prefix = std::string("--") + std::string(key) + "=";
    const std::string_view value {arg};
    if (value.starts_with(prefix)) {
        return value.substr(prefix.size());
    }
    return {};
}

inline bool parse_u16(std::string_view text, wish::u16& out) {
    try {
        const unsigned long parsed = std::stoul(std::string(text));
        if (parsed == 0 || parsed > std::numeric_limits<wish::u16>::max()) {
            return false;
        }
        out = static_cast<wish::u16>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool parse_int(std::string_view text, int& out) {
    try {
        out = std::stoi(std::string(text));
        return true;
    } catch (...) {
        return false;
    }
}

inline bool parse_float(std::string_view text, float& out) {
    try {
        out = std::stof(std::string(text));
        return true;
    } catch (...) {
        return false;
    }
}

inline void apply_env_u16(const char* name, wish::u16& value) {
    if (const char* raw = std::getenv(name)) {
        wish::u16 parsed {};
        if (parse_u16(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid value for ") + name + ".");
        }
    }
}

inline void apply_env_int(const char* name, int& value) {
    if (const char* raw = std::getenv(name)) {
        int parsed {};
        if (parse_int(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid value for ") + name + ".");
        }
    }
}

inline void apply_env_float(const char* name, float& value) {
    if (const char* raw = std::getenv(name)) {
        float parsed {};
        if (parse_float(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid value for ") + name + ".");
        }
    }
}

inline void apply_cli_u16(const char* arg, std::string_view key, wish::u16& value) {
    const std::string_view raw = cli_value(arg, key);
    if (!raw.empty()) {
        wish::u16 parsed {};
        if (parse_u16(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid CLI value for --") + std::string(key) + ".");
        }
    }
}

inline void apply_cli_int(const char* arg, std::string_view key, int& value) {
    const std::string_view raw = cli_value(arg, key);
    if (!raw.empty()) {
        int parsed {};
        if (parse_int(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid CLI value for --") + std::string(key) + ".");
        }
    }
}

inline void apply_cli_float(const char* arg, std::string_view key, float& value) {
    const std::string_view raw = cli_value(arg, key);
    if (!raw.empty()) {
        float parsed {};
        if (parse_float(raw, parsed)) {
            value = parsed;
        } else {
            wish::log_warning(std::string("Ignoring invalid CLI value for --") + std::string(key) + ".");
        }
    }
}

}  // namespace detail

inline ServerConfig load_server_config(int argc, char** argv) {
    ServerConfig config {};

    detail::apply_env_u16("WISH_SERVER_PORT", config.port);
    detail::apply_env_u16("WISH_SERVER_ADMIN_PORT", config.admin_port);
    detail::apply_env_float("WISH_SERVER_TICK_RATE", config.tick_rate);
    detail::apply_env_int("WISH_SERVER_MAX_PLAYERS", config.max_players);
    detail::apply_env_float("WISH_SERVER_DISCONNECT_TIMEOUT_SEC", config.disconnect_timeout_seconds);
    detail::apply_env_float("WISH_SERVER_MATCH_DURATION_SEC", config.match_duration_seconds);

    for (int i = 1; i < argc; ++i) {
        detail::apply_cli_u16(argv[i], "port", config.port);
        detail::apply_cli_u16(argv[i], "server-port", config.port);
        detail::apply_cli_u16(argv[i], "admin-port", config.admin_port);
        detail::apply_cli_float(argv[i], "tick-rate", config.tick_rate);
        detail::apply_cli_int(argv[i], "max-players", config.max_players);
        detail::apply_cli_float(argv[i], "disconnect-timeout", config.disconnect_timeout_seconds);
        detail::apply_cli_float(argv[i], "match-duration", config.match_duration_seconds);

        const std::string_view map_raw = detail::cli_value(argv[i], "map");
        if (!map_raw.empty()) {
            config.map_path = std::string(map_raw);
        }
    }

    if (config.tick_rate <= 0.0F) {
        wish::log_warning("Ignoring invalid server tick rate; using 60 Hz.");
        config.tick_rate = 60.0F;
    }

    if (config.max_players <= 0) {
        wish::log_warning("Ignoring invalid max player count; using 8.");
        config.max_players = 8;
    }

    if (config.disconnect_timeout_seconds <= 0.0F) {
        wish::log_warning("Ignoring invalid disconnect timeout; using 10 seconds.");
        config.disconnect_timeout_seconds = 10.0F;
    }

    if (config.match_duration_seconds < 0.0F) {
        wish::log_warning("Ignoring invalid match duration; using 600 seconds.");
        config.match_duration_seconds = 600.0F;
    }

    return config;
}

}  // namespace wish::admin
