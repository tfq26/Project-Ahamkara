#include "wish/core/live_content_hooks.h"
#include "wish/log.h"

#include <cstdlib>
#include <string>

namespace wish::core {

// ── Non-template helper functions ─────────────────────────────────────────

std::string live_content_hooks_version() {
    return "1.0.0";
}

float get_modifier_float_param(const ModifierConfig& config,
                                std::string_view key,
                                float default_val) {
    std::string_view sv = find_modifier_param(config, key);
    if (sv.empty()) return default_val;
    // Convert string to float manually to avoid <charconv> dependency.
    std::string tmp(sv);
    char* end = nullptr;
    float result = std::strtof(tmp.c_str(), &end);
    if (end == tmp.c_str()) return default_val;
    return result;
}

int get_modifier_int_param(const ModifierConfig& config,
                            std::string_view key,
                            int default_val) {
    std::string_view sv = find_modifier_param(config, key);
    if (sv.empty()) return default_val;
    std::string tmp(sv);
    char* end = nullptr;
    long result = std::strtol(tmp.c_str(), &end, 10);
    if (end == tmp.c_str()) return default_val;
    return static_cast<int>(result);
}

bool get_modifier_bool_param(const ModifierConfig& config,
                              std::string_view key,
                              bool default_val) {
    std::string_view sv = find_modifier_param(config, key);
    if (sv.empty()) return default_val;
    return sv == "true" || sv == "1" || sv == "yes";
}

} // namespace wish::core
