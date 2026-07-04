#include "ae/render/compiled_material.h"
#include "ae/core/log.h"
#include "ae/render/binary_io.h"

#include <filesystem>
#include <fstream>


#define AE_LOG_CATEGORY "Render"

namespace ae::render {

bool save_compiled_material(const std::string& path, const MaterialAsset& material, std::string& error) {
    error.clear();

    const std::filesystem::path output_path(path);
    if (!output_path.parent_path().empty()) {
        std::error_code create_error;
        std::filesystem::create_directories(output_path.parent_path(), create_error);
        if (create_error) {
            error = "Failed to create output directory: " + create_error.message();
            return false;
        }
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        error = "Failed to create: " + path;
        return false;
    }

    const std::uint32_t double_sided = material.double_sided ? 1U : 0U;
    if (!write_value(file, CompiledMaterialFile::magic) ||
        !write_value(file, CompiledMaterialFile::version) ||
        !write_value(file, material.base_color_r) ||
        !write_value(file, material.base_color_g) ||
        !write_value(file, material.base_color_b) ||
        !write_value(file, material.base_color_a) ||
        !write_value(file, material.metallic) ||
        !write_value(file, material.roughness) ||
        !write_value(file, material.emissive_r) ||
        !write_value(file, material.emissive_g) ||
        !write_value(file, material.emissive_b) ||
        !write_value(file, double_sided) ||
        !write_string(file, material.albedo_texture) ||
        !write_string(file, material.normal_texture) ||
        !write_string(file, material.orm_texture) ||
        !write_string(file, material.emissive_texture)) {
        error = "Failed to write compiled material";
        return false;
    }

    return true;
}

bool CompiledMaterialLoader::load(const std::string& path, MaterialAsset& material) {
    error_.clear();
    material = {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open: " + path;
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t double_sided = 0;
    if (!read_value(file, magic) || !read_value(file, version)) {
        error_ = "Failed to read compiled material header";
        return false;
    }

    if (magic != CompiledMaterialFile::magic) {
        error_ = "Invalid compiled material magic";
        return false;
    }

    if (version != CompiledMaterialFile::version) {
        error_ = "Unsupported compiled material version";
        return false;
    }

    if (!read_value(file, material.base_color_r) ||
        !read_value(file, material.base_color_g) ||
        !read_value(file, material.base_color_b) ||
        !read_value(file, material.base_color_a) ||
        !read_value(file, material.metallic) ||
        !read_value(file, material.roughness) ||
        !read_value(file, material.emissive_r) ||
        !read_value(file, material.emissive_g) ||
        !read_value(file, material.emissive_b) ||
        !read_value(file, double_sided)) {
        error_ = "Failed to read compiled material properties";
        return false;
    }

    material.double_sided = double_sided != 0;
    if (!read_string(file, material.albedo_texture, error_) ||
        !read_string(file, material.normal_texture, error_) ||
        !read_string(file, material.orm_texture, error_) ||
        !read_string(file, material.emissive_texture, error_)) {
        return false;
    }

    return true;
}

} // namespace ae::render
