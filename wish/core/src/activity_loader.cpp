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

    pos = json.find('"', pos + search.size());
    if (pos == std::string_view::npos) return {};
    auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return {};

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
            json[val_end] == '.' || json[val_end] == '-'))
        ++val_end;

    std::string num(json.substr(val_start, val_end - val_start));
    if (num.empty()) return default_val;
    return std::stof(num);
}

int extract_int(std::string_view json, std::string_view key, int default_val) {
    return static_cast<int>(extract_float(json, key, static_cast<float>(default_val)));
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
