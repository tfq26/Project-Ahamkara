#include "wish/core/activity_loader.h"
#include "wish/log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace wish::core {

namespace {

// Minimal JSON parser helpers — only what activity configs need.
std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

std::string_view extract_string(std::string_view json, std::string_view key) {
    std::string search = "\"" + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos) return {};

    // Find colon AFTER the key (not after some later quote)
    auto after_key = pos + search.size();
    auto colon = json.find(':', after_key);
    if (colon == std::string_view::npos) return {};

    // Find the opening quote of the string value
    auto val_start = json.find('"', colon + 1);
    if (val_start == std::string_view::npos) return {};
    auto val_end = json.find('"', val_start + 1);
    if (val_end == std::string_view::npos) return {};

    return json.substr(val_start + 1, val_end - val_start - 1);
}

float extract_float(std::string_view json, std::string_view key, float default_val) {
    std::string search = "\"" + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos) return default_val;

    auto colon = json.find(':', pos + search.size());
    if (colon == std::string_view::npos) return default_val;

    auto val_start = colon + 1;
    while (val_start < json.size() && std::isspace(static_cast<unsigned char>(json[val_start])))
        ++val_start;
    if (val_start >= json.size()) return default_val;

    auto val_end = val_start;
    while (val_end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[val_end])) ||
            json[val_end] == '.' || json[val_end] == '-' || json[val_end] == '+'))
        ++val_end;

    std::string num(json.substr(val_start, val_end - val_start));
    if (num.empty()) return default_val;
    return std::stof(num);
}

int extract_int(std::string_view json, std::string_view key, int default_val) {
    return static_cast<int>(extract_float(json, key, static_cast<float>(default_val)));
}

bool extract_bool(std::string_view json, std::string_view key, bool default_val) {
    std::string search = "\"" + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos) return default_val;

    auto colon = json.find(':', pos + search.size());
    if (colon == std::string_view::npos) return default_val;

    auto val_start = colon + 1;
    while (val_start < json.size() && std::isspace(static_cast<unsigned char>(json[val_start])))
        ++val_start;

    auto remaining = json.substr(val_start);
    if (remaining.starts_with("true"))  return true;
    if (remaining.starts_with("false")) return false;
    return default_val;
}

/// Extract a sub-object (delimited by braces) for a given key.
std::string_view extract_object(std::string_view json, std::string_view key) {
    std::string search = "\"" + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos) return {};

    auto colon = json.find(':', pos + search.size());
    if (colon == std::string_view::npos) return {};

    auto obj_start = json.find('{', colon + 1);
    if (obj_start == std::string_view::npos) return {};

    // Track brace depth to find matching closing brace.
    int depth = 1;
    auto obj_end = obj_start + 1;
    while (depth > 0 && obj_end < json.size()) {
        if (json[obj_end] == '{') ++depth;
        else if (json[obj_end] == '}') --depth;
        ++obj_end;
    }
    if (depth != 0) return {};

    return json.substr(obj_start, obj_end - obj_start);
}

/// Extract an array of objects for a given key.
/// Returns pairs of (start, length) for each object found inside the array.
struct ObjectSpan { std::size_t start; std::size_t length; };

std::vector<ObjectSpan> extract_object_array(std::string_view json, std::string_view key) {
    std::vector<ObjectSpan> result;
    std::string search = "\"" + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos) return result;

    auto colon = json.find(':', pos + search.size());
    if (colon == std::string_view::npos) return result;

    auto arr_start = json.find('[', colon + 1);
    if (arr_start == std::string_view::npos) return result;

    // Walk through the array finding top-level objects.
    auto scan = arr_start;
    while (scan < json.size()) {
        auto obj_start = json.find('{', scan);
        if (obj_start == std::string_view::npos || obj_start >= json.size()) break;

        int depth = 1;
        auto obj_end = obj_start + 1;
        while (depth > 0 && obj_end < json.size()) {
            if (json[obj_end] == '{') ++depth;
            else if (json[obj_end] == '}') --depth;
            ++obj_end;
        }
        if (depth != 0) break;

        result.push_back({obj_start, obj_end - obj_start});
        scan = obj_end;

        // Check for end of array.
        auto after = trim(json.substr(scan));
        if (after.empty() || after.front() == ']') break;
    }

    return result;
}

/// Parse a single modifier config from a JSON object string.
bool parse_modifier(std::string_view json, ModifierConfig& mod) {
    mod = {};

    std::string_view type_str = extract_string(json, "type");
    if (type_str.empty()) {
        wish::log_warning("ModifierLoader: missing 'type' field in modifier config.");
        return false;
    }
    mod.type = parse_modifier_type(type_str);
    if (mod.type == ModifierType::None) {
        wish::log_warning("ModifierLoader: unknown modifier type '" + std::string(type_str) + "'.");
        return false;
    }

    mod.name = extract_string(json, "name");
    if (mod.name.empty()) {
        mod.name = type_str; // fall back to type name
    }

    mod.active = extract_bool(json, "active", true);
    mod.duration = extract_float(json, "duration", 0.0F);
    mod.remaining_time = mod.duration;
    mod.rotation_order = static_cast<u32>(extract_int(json, "rotation_order", 0));

    // Parse params sub-object.
    std::string_view params_obj = extract_object(json, "params");
    if (!params_obj.empty()) {
        // Extract key-value pairs from the params object.
        auto scan = params_obj.data();
        auto end = scan + params_obj.size();

        auto find_quoted = [](const char* start, const char* end) -> std::pair<const char*, const char*> {
            auto open = std::find(start, end, '"');
            if (open == end) return {nullptr, nullptr};
            auto close = std::find(open + 1, end, '"');
            if (close == end) return {nullptr, nullptr};
            return {open + 1, close};
        };

        auto p = scan;
        while (p < end) {
            auto key_range = find_quoted(p, end);
            if (!key_range.first) break;
            std::string_view key(key_range.first, key_range.second - key_range.first);

            // Find colon after key
            auto colon = std::find(key_range.second, end, ':');
            if (colon == end) break;

            // Find value
            auto val_start = colon + 1;
            while (val_start < end && std::isspace(static_cast<unsigned char>(*val_start)))
                ++val_start;

            if (val_start >= end) break;

            std::string_view value;
            if (*val_start == '"') {
                // Quoted string value
                auto val_open = val_start + 1;
                auto val_close = std::find(val_open, end, '"');
                if (val_close == end) break;
                value = std::string_view(val_open, val_close - val_open);
                p = val_close + 1;
            } else {
                // Unquoted value (number, bool) — find next comma or }
                auto val_end = val_start;
                while (val_end < end && *val_end != ',' && *val_end != '}' && *val_end != ']') {
                    if (!std::isspace(static_cast<unsigned char>(*val_end))) ++val_end;
                    else break;
                }
                // Trim trailing whitespace
                auto trimmed_end = val_end;
                while (trimmed_end > val_start && std::isspace(static_cast<unsigned char>(*(trimmed_end - 1))))
                    --trimmed_end;
                value = std::string_view(val_start, trimmed_end - val_start);
                p = val_end;
            }

            if (!key.empty()) {
                mod.params.push_back({key, value});
            }

            // Skip to next key
            if (p < end && *p == ',') ++p;
        }
    }

    return true;
}

/// Parse modifiers array from an activity JSON config.
void parse_modifiers(std::string_view json, ActivityConfig& cfg) {
    auto modifier_objects = extract_object_array(json, "modifiers");
    for (const auto& span : modifier_objects) {
        std::string_view obj_json = json.substr(span.start, span.length);
        ModifierConfig mod {};
        if (parse_modifier(obj_json, mod)) {
            cfg.modifiers.push_back(std::move(mod));
        }
    }

    cfg.modifier_rotation_enabled = extract_bool(json, "modifier_rotation_enabled", false);
}

ActivityCategory parse_category(std::string_view cat) {
    if (cat == "PvP")    return ActivityCategory::PvP;
    if (cat == "PvE")    return ActivityCategory::PvE;
    if (cat == "PvEvP")  return ActivityCategory::PvEvP;
    if (cat == "Social") return ActivityCategory::Social;
    if (cat == "Custom") return ActivityCategory::Custom;
    return ActivityCategory::PvP;
}

} // anonymous namespace

bool ActivityLoader::parse_one(std::string_view json, ActivityConfig& cfg) {
    std::string_view name = extract_string(json, "name");
    if (name.empty()) {
        wish::log_warning("ActivityLoader: missing 'name' field in activity config.");
        return false;
    }

    cfg = {};
    cfg.id          = static_cast<ActivityId>(extract_int(json, "id", 0));
    cfg.name        = name;
    cfg.category    = parse_category(extract_string(json, "category"));
    cfg.max_players = static_cast<wish::u32>(extract_int(json, "max_players", 8));
    cfg.tick_rate   = extract_float(json, "tick_rate", 60.0F);
    cfg.map_path    = extract_string(json, "map");

    // Parse modifiers
    parse_modifiers(json, cfg);

    if (cfg.id == 0) {
        wish::log_warning("ActivityLoader: activity '" + std::string(cfg.name) + "' has id=0, skipping.");
        return false;
    }

    return true;
}

wish::u32 ActivityLoader::parse_many(std::string_view json,
                                     std::vector<ActivityConfig>& out_configs) {
    wish::u32 count = 0;

    // Find individual objects in the array
    auto pos = json.find('{');
    while (pos != std::string_view::npos) {
        auto end = json.find('}', pos);
        if (end == std::string_view::npos) break;

        ActivityConfig cfg {};
        std::string_view obj = json.substr(pos, end - pos + 1);
        if (parse_one(obj, cfg)) {
            out_configs.push_back(cfg);
            ++count;
        }

        pos = json.find('{', end + 1);
    }

    return count;
}

wish::u32 ActivityLoader::load_directory(std::string_view path,
                                         std::vector<ActivityConfig>& out_configs) {
    // For now this is a placeholder — real implementation would use
    // std::filesystem::directory_iterator (C++17) to scan *.json files.
    // The server can call this at startup to auto-discover activity configs.
    (void)path;
    (void)out_configs;
    wish::log_info("ActivityLoader::load_directory is a stub — use register_template() programmatically.");
    return 0;
}

}  // namespace wish::core
