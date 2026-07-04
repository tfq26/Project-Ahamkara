#pragma once

#include <string>

namespace ae {

/**
 * @brief Trim leading and trailing whitespace from a string.
 *
 * Returns a string_view into the original string to avoid allocation.
 */
inline std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/**
 * @brief Parse a "--key=value" CLI argument as a float.
 *
 * If arg starts with "--<key>=", attempts to parse the suffix as float.
 * Returns default_val on parse failure or if the key does not match.
 */
inline float parse_float_arg(const char* arg, const char* key, float default_val) {
    std::string prefix = std::string("--") + key + "=";
    std::string s(arg);
    if (s.starts_with(prefix)) {
        try {
            return std::stof(s.substr(prefix.size()));
        } catch (...) {
            // Logging disabled by default (Debug gated); unblocked once
            // core logging is ready. Callers can also log at their own level.
            return default_val;
        }
    }
    return default_val;
}

/**
 * @brief Parse a "--key" boolean flag from argv.
 *
 * Returns true when the argument matches "--<key>" exactly (no value).
 */
inline bool parse_bool_arg(const char* arg, const char* key) {
    std::string prefix = std::string("--") + key;
    return std::string(arg) == prefix;
}

}  // namespace ae
