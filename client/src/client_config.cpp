#include "ahamkara/client/client_config.h"

#include "ae/core/log.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace ahamkara::client {
namespace {

/// Strip leading and trailing whitespace from a string_view.
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }

    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }

    return value;
}

/// Parse a single config line and update the matching field in `config`.
/// Returns false when the line is syntactically invalid (key with no value).
void apply_line(std::string_view line, ClientConfig& config) {
    // Strip inline comments (first unquoted # that is preceded by whitespace or at start).
    const auto comment = line.find_first_of('#');
    if (comment != std::string_view::npos && (comment == 0 || line[comment - 1] == ' ' || line[comment - 1] == '\t')) {
        line = line.substr(0, comment);
    }

    line = trim(line);
    if (line.empty()) {
        return;
    }

    const auto delimiter = line.find('=');
    if (delimiter == std::string_view::npos) {
        ae::log_warning("Config line has no '=' separator, skipping.");
        return;
    }

    const std::string_view key   = trim(line.substr(0, delimiter));
    const std::string_view value = trim(line.substr(delimiter + 1));

    if (key.empty() || value.empty()) {
        ae::log_warning("Config line has an empty key or value, skipping.");
        return;
    }

    if (key == "window_width") {
        std::int32_t parsed = 0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc {} && parsed > 0) {
            config.window_width = parsed;
        } else {
            ae::log_warning("Config window_width value is invalid, keeping default.");
        }
        return;
    }

    if (key == "window_height") {
        std::int32_t parsed = 0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc {} && parsed > 0) {
            config.window_height = parsed;
        } else {
            ae::log_warning("Config window_height value is invalid, keeping default.");
        }
        return;
    }

    if (key == "fullscreen") {
        if (value == "true" || value == "1") {
            config.fullscreen = true;
        } else if (value == "false" || value == "0") {
            config.fullscreen = false;
        } else {
            ae::log_warning("Config fullscreen value is invalid, keeping default.");
        }
        return;
    }

    if (key == "gamma") {
        const std::string value_str {value};
        char* end = nullptr;
        float parsed = std::strtof(value_str.c_str(), &end);
        if (end != value_str.c_str() && parsed >= 0.5F && parsed <= 2.0F) {
            config.gamma = parsed;
        } else {
            ae::log_warning("Config gamma value is invalid (expected 0.5–2.0), keeping default.");
        }
        return;
    }

    if (key == "mouse_sensitivity") {
        float parsed = 0.0F;
        // from_chars for float is not universally available, use strtof.
        // We accept "0.0", "1.0", etc.
        const std::string value_str {value};   // null-terminated
        char* end = nullptr;
        parsed = std::strtof(value_str.c_str(), &end);
        if (end != value_str.c_str() && parsed > 0.0F) {
            config.mouse_sensitivity = parsed;
        } else {
            ae::log_warning("Config mouse_sensitivity value is invalid, keeping default.");
        }
        return;
    }

    if (key == "server_ip") {
        config.server_ip = value;  // raw copy, validation happens at connect time
        return;
    }

    ae::log_warning("Config line contains an unknown key, skipping.");
}

}  // namespace

bool ClientConfig::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ae::log_info("No client config file found, using defaults.");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        apply_line(line, *this);
    }

    ae::log_info("Client config loaded from file.");
    return true;
}

}  // namespace ahamkara::client
