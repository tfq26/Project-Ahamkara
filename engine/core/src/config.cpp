#include "ae/core/config.h"

#include "ae/core/cli_utils.h"
#include "ae/core/log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#define AE_LOG_CATEGORY "Config"

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
    log_debug_cat(AE_LOG_CATEGORY, "Registered config var: " + std::string(key));
}

int ConfigRegistry::reload_from_file(const std::string& path) {
    log_info_cat(AE_LOG_CATEGORY, "Reloading config from " + path);
    std::ifstream file(path);
    if (!file.is_open()) {
        log_warning_cat(AE_LOG_CATEGORY, "Could not open config file: " + path);
        log_warning_cat(AE_LOG_CATEGORY, "Previous valid configuration preserved.");
        return 0;
    }

    // Take a snapshot so we can roll back on partial failure.
    take_snapshot();

    int updated = 0;
    int unknown = 0;
    std::string line;
    while (std::getline(file, line)) {
        const std::string_view trimmed = ae::trim(line);

        if (trimmed.empty() || trimmed[0] == '#') continue;

        const auto eq = trimmed.find('=');
        if (eq == std::string_view::npos) continue;

        const std::string_view key = ae::trim(trimmed.substr(0, eq));
        const std::string_view value = ae::trim(trimmed.substr(eq + 1));

        auto it = vars_.find(std::string(key));
        if (it != vars_.end() && it->second.reload) {
            it->second.reload(value);
            ++updated;
            log_debug_cat(AE_LOG_CATEGORY, "Config var " + std::string(key) + " = " + std::string(value));
        } else {
            ++unknown;
        }
    }

    if (updated == 0 && unknown > 0) {
        // No known vars were updated — the file may have only unknown keys.
        // This is suspicious; restore snapshot to avoid accidental mutation.
        log_warning_cat(AE_LOG_CATEGORY,
            std::to_string(unknown) + " unknown key(s) found and no known vars updated.");
        log_warning_cat(AE_LOG_CATEGORY, "Restoring previous valid configuration.");
        restore_snapshot();
    } else if (updated > 0) {
        log_info_cat(AE_LOG_CATEGORY, std::to_string(updated) + " config variable(s) reloaded from " + path);
    }

    if (unknown > 0) {
        log_debug_cat(AE_LOG_CATEGORY, std::to_string(unknown) + " unknown key(s) in " + path);
    }

    return updated;
}

void ConfigRegistry::take_snapshot() {
    snapshot_.clear();
    for (const auto& [key, entry] : vars_) {
        if (entry.serialize) {
            snapshot_[key] = entry.serialize();
        }
    }
}

void ConfigRegistry::restore_snapshot() {
    for (const auto& [key, value] : snapshot_) {
        auto it = vars_.find(key);
        if (it != vars_.end() && it->second.reload) {
            it->second.reload(value);
            log_debug_cat(AE_LOG_CATEGORY, "Rolled back config var " + key + " to snapshot value");
        }
    }
    snapshot_.clear();
}

bool ConfigRegistry::save_to_file(const std::string& path) const {
    log_info_cat(AE_LOG_CATEGORY, "Saving config to " + path);
    std::ofstream file(path);
    if (!file.is_open()) {
        log_warning_cat(AE_LOG_CATEGORY, "Could not write config file: " + path);
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

    log_info_cat(AE_LOG_CATEGORY, "Saved " + std::to_string(keys.size()) + " config variable(s) to " + path);
    return true;
}

std::string ConfigRegistry::get_value(std::string_view key) const {
    auto it = vars_.find(std::string(key));
    if (it != vars_.end() && it->second.serialize) {
        return it->second.serialize();
    }
    return {};
}

bool ConfigRegistry::set_value(std::string_view key, std::string_view value) {
    auto it = vars_.find(std::string(key));
    if (it != vars_.end() && it->second.reload) {
        it->second.reload(value);
        log_debug_cat(AE_LOG_CATEGORY, "cvar set via console: " + std::string(key) + " = " + std::string(value));
        return true;
    }
    return false;
}

std::vector<std::string> ConfigRegistry::all_keys() const {
    std::vector<std::string> keys;
    keys.reserve(vars_.size());
    for (const auto& [key, _] : vars_) {
        keys.push_back(key);
    }
    return keys;
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

    log_info_cat(AE_LOG_CATEGORY, "Config file change detected: " + path);
    last_write_time_ = write_time;
    has_last_write_time_ = true;
    return reload_from_file(path);
}

}  // namespace ae
