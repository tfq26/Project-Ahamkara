#pragma once

#include <string>

namespace ae {

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
