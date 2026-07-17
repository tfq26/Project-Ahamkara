#include "asset_importer_level.h"
#include "ae/render/compiled_level.h"
#include "ae/core/log.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define AE_LOG_CATEGORY "Tools"

namespace asset_importer {
namespace {

enum class Section {
    None,
    Spawn,
    Collision,
    Mesh,
};

Section parse_section_header(const std::string& line) {
    if (line == "[spawn]") return Section::Spawn;
    if (line == "[collision]") return Section::Collision;
    if (line == "[mesh]") return Section::Mesh;
    return Section::None;
}

bool parse_float3(const std::string& value, float& a, float& b, float& c, std::string& error, const std::string& key) {
    const auto tokens = split_tokens(value);
    if (tokens.size() < 3 ||
        !parse_float_token(tokens[0], a) ||
        !parse_float_token(tokens[1], b) ||
        !parse_float_token(tokens[2], c)) {
        error = key + " expects three floats";
        return false;
    }
    return true;
}

float parse_named_float(const std::vector<std::string>& tokens, const std::string& name, float default_value) {
    for (const auto& token : tokens) {
        if (token.starts_with(name + "=")) {
            const auto value = token.substr(name.size() + 1);
            float result = default_value;
            if (parse_float_token(value, result)) {
                return result;
            }
        }
    }
    return default_value;
}

bool parse_named_bool(const std::vector<std::string>& tokens, const std::string& name, bool default_value) {
    for (const auto& token : tokens) {
        if (token.starts_with(name + "=")) {
            const auto value = token.substr(name.size() + 1);
            bool result = default_value;
            if (parse_bool_token(value, result)) {
                return result;
            }
        }
    }
    return default_value;
}

std::string parse_named_string(const std::vector<std::string>& tokens, const std::string& name) {
    for (const auto& token : tokens) {
        if (token.starts_with(name + "=")) {
            return token.substr(name.size() + 1);
        }
    }
    return {};
}

} // namespace

bool load_level_source(const std::filesystem::path& path, ae::render::LevelAsset& level, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Failed to open level source";
        return false;
    }

    Section section = Section::None;
    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;

        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        // Check for section headers
        if (line[0] == '[') {
            const auto new_section = parse_section_header(line);
            if (new_section == Section::None) {
                error = "Unknown section header on line " + std::to_string(line_number);
                return false;
            }
            section = new_section;
            continue;
        }

        // Section-specific parsing
        if (section == Section::None) {
            // Top-level key=value settings
            const auto equals_pos = line.find('=');
            if (equals_pos == std::string::npos) {
                error = "Expected key=value on line " + std::to_string(line_number);
                return false;
            }

            const auto key = trim(line.substr(0, equals_pos));
            const auto value = trim(line.substr(equals_pos + 1));
            if (key.empty()) {
                error = "Empty key on line " + std::to_string(line_number);
                return false;
            }

            if (key == "name") {
                level.name = value;
            } else if (key == "sky_color") {
                if (!parse_float3(value, level.sky_color_r, level.sky_color_g, level.sky_color_b, error, "sky_color")) {
                    return false;
                }
            } else if (key == "ambient") {
                if (!parse_float3(value, level.ambient_r, level.ambient_g, level.ambient_b, error, "ambient")) {
                    return false;
                }
            } else if (key == "gravity") {
                if (!parse_float_token(value, level.gravity)) {
                    error = "gravity expects a float";
                    return false;
                }
            } else if (key == "skybox_material") {
                level.skybox_material = value;
            } else if (key == "ground_material") {
                level.ground_material = value;
            } else {
                error = "Unknown level key '" + key + "'";
                return false;
            }
        } else if (section == Section::Spawn) {
            const auto tokens = split_tokens(line);
            if (tokens.size() < 4) {
                error = "Spawn point needs pos_x pos_y pos_z yaw on line " + std::to_string(line_number);
                return false;
            }

            ae::render::LevelSpawnPoint sp;
            if (!parse_float_token(tokens[0], sp.pos_x) ||
                !parse_float_token(tokens[1], sp.pos_y) ||
                !parse_float_token(tokens[2], sp.pos_z) ||
                !parse_float_token(tokens[3], sp.yaw)) {
                error = "Invalid spawn point values on line " + std::to_string(line_number);
                return false;
            }

            sp.team = static_cast<std::uint32_t>(parse_named_float(tokens, "team", 0.0F));
            level.spawn_points.push_back(sp);
        } else if (section == Section::Collision) {
            const auto tokens = split_tokens(line);
            if (tokens.size() < 6) {
                error = "Collision box needs min_x min_z max_x max_z top_y bottom_y on line " + std::to_string(line_number);
                return false;
            }

            ae::render::LevelCollisionBox cb;
            if (!parse_float_token(tokens[0], cb.min_x) ||
                !parse_float_token(tokens[1], cb.min_z) ||
                !parse_float_token(tokens[2], cb.max_x) ||
                !parse_float_token(tokens[3], cb.max_z) ||
                !parse_float_token(tokens[4], cb.top_y) ||
                !parse_float_token(tokens[5], cb.bottom_y)) {
                error = "Invalid collision box values on line " + std::to_string(line_number);
                return false;
            }

            cb.wall = parse_named_bool(tokens, "wall", false);
            cb.jump_through = parse_named_bool(tokens, "jump_through", false);
            cb.auto_step = parse_named_bool(tokens, "auto_step", true);
            cb.surface_material = static_cast<std::uint32_t>(parse_named_float(tokens, "surface", 0.0F));
            level.collision_boxes.push_back(cb);
        } else if (section == Section::Mesh) {
            const auto tokens = split_tokens(line);
            const auto mesh_id = parse_named_string(tokens, "mesh_id");
            if (mesh_id.empty()) {
                error = "Mesh instance needs mesh_id on line " + std::to_string(line_number);
                return false;
            }

            ae::render::LevelMeshInstance mi;
            mi.mesh_asset_id = mesh_id;
            mi.material_asset_id = parse_named_string(tokens, "material_id");

            const auto pos_idx = [&]() -> int {
                for (int i = 0; i < static_cast<int>(tokens.size()); ++i) {
                    if (tokens[i] == "pos=" || tokens[i].starts_with("pos=")) return i;
                }
                return -1;
            }();
            if (pos_idx >= 0) {
                const auto& val = tokens[pos_idx].find('=') != std::string::npos
                    ? tokens[pos_idx].substr(tokens[pos_idx].find('=') + 1)
                    : (pos_idx + 3 < static_cast<int>(tokens.size()) ? tokens[pos_idx + 1] : "");
                std::vector<std::string> pos_tokens;
                if (val.empty()) {
                    pos_tokens = {tokens[pos_idx + 1], tokens[pos_idx + 2], tokens[pos_idx + 3]};
                } else {
                    pos_tokens = split_tokens(val);
                }
                if (pos_tokens.size() >= 3) {
                    parse_float_token(pos_tokens[0], mi.pos_x);
                    parse_float_token(pos_tokens[1], mi.pos_y);
                    parse_float_token(pos_tokens[2], mi.pos_z);
                }
            }

            mi.yaw = parse_named_float(tokens, "yaw", 0.0F);
            mi.pitch = parse_named_float(tokens, "pitch", 0.0F);
            mi.roll = parse_named_float(tokens, "roll", 0.0F);
            mi.scale_x = parse_named_float(tokens, "scale_x", 1.0F);
            mi.scale_y = parse_named_float(tokens, "scale_y", 1.0F);
            mi.scale_z = parse_named_float(tokens, "scale_z", 1.0F);
            level.mesh_instances.push_back(mi);
        }
    }

    return true;
}

bool compile_level(const ImportEntry& entry) {
    ae::render::LevelAsset level;
    std::string error;
    if (!load_level_source(entry.source, level, error)) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Level import failed for " + entry.source.string() + ": " + error);
        return false;
    }

    if (!ae::render::save_compiled_level(entry.output.string(), level, error)) {
        ae::log_error_cat(AE_LOG_CATEGORY, "Failed to compile " + entry.source.string() + " -> " + entry.output.string() + ": " + error);
        return false;
    }

    ae::log_info_cat(AE_LOG_CATEGORY, "level " + entry.source.string() + " -> " + entry.output.string());
    return true;
}

} // namespace asset_importer
