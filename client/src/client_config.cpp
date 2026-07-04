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

    // --- Audio keys ---
    auto parse_audio_float = [&](float& target, float min_val, float max_val) {
        const std::string value_str {value};
        char* end = nullptr;
        float parsed = std::strtof(value_str.c_str(), &end);
        if (end != value_str.c_str() && parsed >= min_val && parsed <= max_val) {
            target = parsed;
        } else {
            ae::log_warning("Config audio value is out of range, keeping default.");
        }
    };

    if (key == "audio_enabled") {
        if (value == "true" || value == "1") {
            config.audio.enabled = true;
        } else if (value == "false" || value == "0") {
            config.audio.enabled = false;
        } else {
            ae::log_warning("Config audio_enabled value is invalid, keeping default.");
        }
        return;
    }
    if (key == "audio_master_volume")   { parse_audio_float(config.audio.master_volume,   0.0F, 1.0F); return; }
    if (key == "audio_sfx_volume")      { parse_audio_float(config.audio.sfx_volume,      0.0F, 1.0F); return; }
    if (key == "audio_weapon_volume")   { parse_audio_float(config.audio.weapon_volume,   0.0F, 1.0F); return; }
    if (key == "audio_ui_volume")       { parse_audio_float(config.audio.ui_volume,       0.0F, 1.0F); return; }
    if (key == "audio_music_volume")    { parse_audio_float(config.audio.music_volume,    0.0F, 1.0F); return; }
    if (key == "audio_ambient_volume")  { parse_audio_float(config.audio.ambient_volume,  0.0F, 1.0F); return; }

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

bool ClientConfig::save_to_file(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        ae::log_warning("Cannot write client config to " + path);
        return false;
    }
    file << "# Ahamkara client config — auto-generated\n";
    file << "window_width=" << window_width << "\n";
    file << "window_height=" << window_height << "\n";
    file << "fullscreen=" << (fullscreen ? "true" : "false") << "\n";
    file << "gamma=" << gamma << "\n";
    file << "mouse_sensitivity=" << mouse_sensitivity << "\n";
    file << "server_ip=" << server_ip << "\n";
    file << "audio_enabled=" << (audio.enabled ? "true" : "false") << "\n";
    file << "audio_master_volume=" << audio.master_volume << "\n";
    file << "audio_sfx_volume=" << audio.sfx_volume << "\n";
    file << "audio_weapon_volume=" << audio.weapon_volume << "\n";
    file << "audio_ui_volume=" << audio.ui_volume << "\n";
    file << "audio_music_volume=" << audio.music_volume << "\n";
    file << "audio_ambient_volume=" << audio.ambient_volume << "\n";
    return true;
}

}  // namespace ahamkara::client
