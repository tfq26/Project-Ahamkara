#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae {

/**
 * @brief A single configuration variable with optional change callback.
 *
 * ConfigVar<T> wraps a typed value and can fire a callback whenever
 * the value changes — whether through set() or a hot-reload from disk.
 *
 * Usage:
 * @code
 *   ConfigVar<float> player_speed("game.player_speed", 5.5F);
 *   player_speed.on_change([](float old, float val) {
 *       log_info_cat("Config", "player_speed: " + std::to_string(old) + " -> " + std::to_string(val));
 *   });
 *   float speed = player_speed.get();
 * @endcode
 */
template <typename T>
class ConfigVar {
public:
    using ChangeCallback = std::function<void(const T& old_value, const T& new_value)>;

    ConfigVar(std::string_view key, T default_value)
        : key_(key), value_(std::move(default_value)) {}

    [[nodiscard]] const std::string& key() const { return key_; }
    [[nodiscard]] const T& get() const { return value_; }
    [[nodiscard]] const T& default_value() const { return default_; }

    /** Set value programmatically. Fires change callback. */
    void set(T new_value) {
        if (new_value == value_) return;
        T old = value_;
        value_ = std::move(new_value);
        fire_change(old, value_);
    }

    /** Register a callback for value changes. */
    void on_change(ChangeCallback cb) {
        callbacks_.push_back(std::move(cb));
    }

    /** Called by the config system when hot-reloading from disk. */
    void reload_from_string(std::string_view text) {
        T parsed{};
        if (parse_value(text, parsed)) {
            set(std::move(parsed));
        }
    }

    /** Serialize current value to string (for writing config files). */
    [[nodiscard]] std::string to_string() const {
        return serialize_value(value_);
    }

private:
    void fire_change(const T& old_val, const T& new_val) {
        for (auto& cb : callbacks_) {
            if (cb) cb(old_val, new_val);
        }
    }

    // Default parse/serialize for common types. Specialize for custom types.
    static bool parse_value(std::string_view text, T& out);
    static std::string serialize_value(const T& value);

    std::string key_;
    T value_;
    T default_{};
    std::vector<ChangeCallback> callbacks_;
};

// ============================================================
// Default parse/serialize for common types
// ============================================================

template <> inline bool ConfigVar<float>::parse_value(std::string_view text, float& out) {
    out = std::stof(std::string(text));
    return true;
}
template <> inline std::string ConfigVar<float>::serialize_value(const float& v) {
    return std::to_string(v);
}

template <> inline bool ConfigVar<int>::parse_value(std::string_view text, int& out) {
    out = std::stoi(std::string(text));
    return true;
}
template <> inline std::string ConfigVar<int>::serialize_value(const int& v) {
    return std::to_string(v);
}

template <> inline bool ConfigVar<bool>::parse_value(std::string_view text, bool& out) {
    std::string s(text);
    out = (s == "true" || s == "1" || s == "yes");
    return true;
}
template <> inline std::string ConfigVar<bool>::serialize_value(const bool& v) {
    return v ? "true" : "false";
}

template <> inline bool ConfigVar<std::string>::parse_value(std::string_view text, std::string& out) {
    out = std::string(text);
    return true;
}
template <> inline std::string ConfigVar<std::string>::serialize_value(const std::string& v) {
    return v;
}

/**
 * @brief Central registry for all config variables.
 *
 * Owns all ConfigVar instances. Provides:
 *   - Registration (config vars auto-register on construction)
 *   - Hot-reload from a key=value text file
 *   - Save current values to file
 *   - Polling interface for file-watch based reload
 */
class ConfigRegistry {
public:
    static ConfigRegistry& instance();

    /** Register a config var. Called automatically by ConfigVar constructor. */
    void register_var(std::string_view key,
                      std::function<void(std::string_view)> reload_fn,
                      std::function<std::string()> serialize_fn);

    /**
     * @brief Reload config from a key=value file.
     *
     * Format: one key=value per line. Lines starting with # are comments.
     * Blank lines are ignored.
     *
     * @param path      File path to read.
     * @return Number of variables updated.
     */
    int reload_from_file(const std::string& path);

    /**
     * @brief Save current config values to a file.
     * @param path  File path to write.
     * @return true on success.
     */
    bool save_to_file(const std::string& path) const;

    /**
     * @brief Poll for config file changes and reload if modified.
     *
     * Call once per frame (or less frequently). Uses file modification
     * timestamp to detect changes.
     *
     * @param path  File to watch.
     * @return Number of variables updated (0 if unchanged).
     */
    int poll_reload(const std::string& path);

    // --- Console/cvar integration ---

    /// Return the serialized value of a registered cvar, or empty string if not found.
    [[nodiscard]] std::string get_value(std::string_view key) const;

    /// Set a cvar value from a string (via its reload function).
    /// Returns true if the key was found and the value was applied.
    bool set_value(std::string_view key, std::string_view value);

    /// List all registered cvar keys.
    [[nodiscard]] std::vector<std::string> all_keys() const;

    /// Number of registered cvars.
    [[nodiscard]] std::size_t count() const { return vars_.size(); }

private:
    ConfigRegistry() = default;

    struct Entry {
        std::function<void(std::string_view)> reload;
        std::function<std::string()> serialize;
    };
    std::unordered_map<std::string, Entry> vars_;

    // For poll-based reload: track last observed write time.
    bool has_last_write_time_ {false};
    std::filesystem::file_time_type last_write_time_ {};
};

}  // namespace ae
