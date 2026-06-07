#include "asset_importer_material.h"
#include "ae/render/compiled_material.h"

#include <fstream>
#include <iostream>
#include <string>

namespace asset_importer {

bool load_material_source(const std::filesystem::path& path, ae::render::MaterialAsset& material, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Failed to open material source";
        return false;
    }

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

        const auto equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            error = "Expected key=value on line " + std::to_string(line_number);
            return false;
        }

        const auto key = trim(line.substr(0, equals_pos));
        const auto value = trim(line.substr(equals_pos + 1));
        if (key.empty()) {
            error = "Empty material key on line " + std::to_string(line_number);
            return false;
        }

        const auto tokens = split_tokens(value);
        if (key == "base_color") {
            if (tokens.size() != 4 ||
                !parse_float_token(tokens[0], material.base_color_r) ||
                !parse_float_token(tokens[1], material.base_color_g) ||
                !parse_float_token(tokens[2], material.base_color_b) ||
                !parse_float_token(tokens[3], material.base_color_a)) {
                error = "base_color expects four floats";
                return false;
            }
        } else if (key == "metallic") {
            if (tokens.size() != 1 || !parse_float_token(tokens[0], material.metallic)) {
                error = "metallic expects one float";
                return false;
            }
        } else if (key == "roughness") {
            if (tokens.size() != 1 || !parse_float_token(tokens[0], material.roughness)) {
                error = "roughness expects one float";
                return false;
            }
        } else if (key == "emissive_color") {
            if (tokens.size() != 3 ||
                !parse_float_token(tokens[0], material.emissive_r) ||
                !parse_float_token(tokens[1], material.emissive_g) ||
                !parse_float_token(tokens[2], material.emissive_b)) {
                error = "emissive_color expects three floats";
                return false;
            }
        } else if (key == "double_sided") {
            if (tokens.size() != 1 || !parse_bool_token(tokens[0], material.double_sided)) {
                error = "double_sided expects a boolean";
                return false;
            }
        } else if (key == "albedo_texture") {
            material.albedo_texture = value;
        } else if (key == "normal_texture") {
            material.normal_texture = value;
        } else if (key == "orm_texture") {
            material.orm_texture = value;
        } else if (key == "emissive_texture") {
            material.emissive_texture = value;
        } else {
            error = "Unknown material key '" + key + "'";
            return false;
        }
    }

    return true;
}

bool compile_material(const ImportEntry& entry) {
    ae::render::MaterialAsset material;
    std::string error;
    if (!load_material_source(entry.source, material, error)) {
        std::cerr << "Material import failed for " << entry.source << ": " << error << '\n';
        return false;
    }

    if (!ae::render::save_compiled_material(entry.output.string(), material, error)) {
        std::cerr << "Failed to compile " << entry.source << " -> " << entry.output << ": " << error << '\n';
        return false;
    }

    std::cout << "material " << entry.source << " -> " << entry.output << '\n';
    return true;
}

} // namespace asset_importer
