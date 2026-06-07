#include "ae/core/config.h"

#include "ae/core/log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace ae {

// ============================================================
// ConfigRegistry
// ============================================================

ConfigRegistry& ConfigRegistry::instance() {
    static ConfigRegistry registry;
    return registry;
}

void ConfigRegistry::register_var(std::string_view key,
                                   std::function<void(std::string_view)> reload_fn,
                                   std::function<std::string()> serialize_fn) {
    vars_[std::string(key)] = {std::move(reload_fn), std::move(serialize_fn)};
}

int ConfigRegistry::reload_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        log_warning_cat("Config", "Could not open config file: " + path);
        return 0;
    }

    int updated = 0;
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        };
        trim(line);

        // Skip comments and blank lines
        if (line.empty() || line[0] == '#') continue;

        // Parse key=value
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trim(key);
        trim(value);

        auto it = vars_.find(key);
        if (it != vars_.end() && it->second.reload) {
            it->second.reload(value);
            ++updated;
        }
    }

    if (updated > 0) {
        log_info_cat("Config", std::to_string(updated) + " config variable(s) reloaded from " + path);
    }

    return updated;
}

bool ConfigRegistry::save_to_file(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        log_warning_cat("Config", "Could not write config file: " + path);
        return false;
    }

    // Collect sorted keys for deterministic output
    std::vector<std::string> keys;
    for (const auto& [key, entry] : vars_) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());

    file << "# Ahamkara engine config — auto-generated\n";
    for (const auto& key : keys) {
        auto it = vars_.find(key);
        if (it != vars_.end() && it->second.serialize) {
            file << key << "=" << it->second.serialize() << "\n";
        }
    }

    return true;
}

int ConfigRegistry::poll_reload(const std::string& path) {
    std::error_code error;
    const auto write_time = std::filesystem::last_write_time(path, error);
    if (error) {
        return 0;
    }

    if (has_last_write_time_ && write_time <= last_write_time_) {
        return 0;
    }

    last_write_time_ = write_time;
    has_last_write_time_ = true;
    return reload_from_file(path);
}

}  // namespace ae
