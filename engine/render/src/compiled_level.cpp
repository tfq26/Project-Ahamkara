#include "ae/render/compiled_level.h"
#include "ae/render/binary_io.h"
#include "ae/core/log.h"

#include <filesystem>
#include <fstream>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {
namespace {

bool write_world_settings(std::ofstream& file, const LevelAsset& level, std::string& error) {
    const std::uint32_t name_length = checked_count(level.name.size(), "name");
    if (!write_value(file, name_length) ||
        !write_bytes(file, level.name.data(), level.name.size()) ||
        !write_value(file, level.sky_color_r) ||
        !write_value(file, level.sky_color_g) ||
        !write_value(file, level.sky_color_b) ||
        !write_value(file, level.ambient_r) ||
        !write_value(file, level.ambient_g) ||
        !write_value(file, level.ambient_b) ||
        !write_value(file, level.gravity) ||
        !write_string(file, level.skybox_material) ||
        !write_string(file, level.ground_material)) {
        error = "Failed to write world settings";
        return false;
    }

    return true;
}

bool read_world_settings(std::ifstream& file, LevelAsset& level, std::string& error) {
    std::uint32_t name_length = 0;
    if (!read_value(file, name_length)) {
        error = "Failed to read name length";
        return false;
    }

    if (name_length > 256) {
        error = "Level name too long";
        return false;
    }

    level.name.resize(name_length);
    if (name_length > 0) {
        if (!read_bytes(file, level.name.data(), name_length)) {
            error = "Failed to read level name";
            return false;
        }
    }

    if (!read_value(file, level.sky_color_r) ||
        !read_value(file, level.sky_color_g) ||
        !read_value(file, level.sky_color_b) ||
        !read_value(file, level.ambient_r) ||
        !read_value(file, level.ambient_g) ||
        !read_value(file, level.ambient_b) ||
        !read_value(file, level.gravity)) {
        error = "Failed to read world settings scalars";
        return false;
    }

    if (!read_string(file, level.skybox_material, error) ||
        !read_string(file, level.ground_material, error)) {
        return false;
    }

    return true;
}

bool write_spawn_points(std::ofstream& file, const LevelAsset& level, std::string& error) {
    const auto count = checked_count(level.spawn_points.size(), "spawn point");
    if (!write_value(file, count)) {
        error = "Failed to write spawn point count";
        return false;
    }

    for (const auto& sp : level.spawn_points) {
        if (!write_value(file, sp.pos_x) ||
            !write_value(file, sp.pos_y) ||
            !write_value(file, sp.pos_z) ||
            !write_value(file, sp.yaw) ||
            !write_value(file, sp.team)) {
            error = "Failed to write spawn point";
            return false;
        }
    }

    return true;
}

bool read_spawn_points(std::ifstream& file, LevelAsset& level, std::string& error) {
    std::uint32_t count = 0;
    if (!read_value(file, count) || !validate_count(count, "spawn point", error, 256)) {
        if (error.empty()) {
            error = "Failed to read spawn point count";
        }
        return false;
    }

    level.spawn_points.resize(count);
    for (auto& sp : level.spawn_points) {
        if (!read_value(file, sp.pos_x) ||
            !read_value(file, sp.pos_y) ||
            !read_value(file, sp.pos_z) ||
            !read_value(file, sp.yaw) ||
            !read_value(file, sp.team)) {
            error = "Failed to read spawn point";
            return false;
        }
    }

    return true;
}

bool write_collision_boxes(std::ofstream& file, const LevelAsset& level, std::string& error) {
    const auto count = checked_count(level.collision_boxes.size(), "collision box");
    if (!write_value(file, count)) {
        error = "Failed to write collision box count";
        return false;
    }

    for (const auto& cb : level.collision_boxes) {
        const std::uint32_t wall_flag = cb.wall ? 1U : 0U;
        const std::uint32_t jump_through_flag = cb.jump_through ? 1U : 0U;
        const std::uint32_t auto_step_flag = cb.auto_step ? 1U : 0U;

        if (!write_value(file, cb.min_x) ||
            !write_value(file, cb.min_z) ||
            !write_value(file, cb.max_x) ||
            !write_value(file, cb.max_z) ||
            !write_value(file, cb.top_y) ||
            !write_value(file, cb.bottom_y) ||
            !write_value(file, wall_flag) ||
            !write_value(file, jump_through_flag) ||
            !write_value(file, auto_step_flag) ||
            !write_value(file, cb.surface_material)) {
            error = "Failed to write collision box";
            return false;
        }
    }

    return true;
}

bool read_collision_boxes(std::ifstream& file, LevelAsset& level, std::string& error) {
    std::uint32_t count = 0;
    if (!read_value(file, count) || !validate_count(count, "collision box", error, 65536)) {
        if (error.empty()) {
            error = "Failed to read collision box count";
        }
        return false;
    }

    level.collision_boxes.resize(count);
    for (auto& cb : level.collision_boxes) {
        std::uint32_t wall_flag = 0;
        std::uint32_t jump_through_flag = 0;
        std::uint32_t auto_step_flag = 0;

        if (!read_value(file, cb.min_x) ||
            !read_value(file, cb.min_z) ||
            !read_value(file, cb.max_x) ||
            !read_value(file, cb.max_z) ||
            !read_value(file, cb.top_y) ||
            !read_value(file, cb.bottom_y) ||
            !read_value(file, wall_flag) ||
            !read_value(file, jump_through_flag) ||
            !read_value(file, auto_step_flag) ||
            !read_value(file, cb.surface_material)) {
            error = "Failed to read collision box";
            return false;
        }

        cb.wall = wall_flag != 0;
        cb.jump_through = jump_through_flag != 0;
        cb.auto_step = auto_step_flag != 0;
    }

    return true;
}

bool write_mesh_instances(std::ofstream& file, const LevelAsset& level, std::string& error) {
    const auto count = checked_count(level.mesh_instances.size(), "mesh instance");
    if (!write_value(file, count)) {
        error = "Failed to write mesh instance count";
        return false;
    }

    for (const auto& mi : level.mesh_instances) {
        if (!write_string(file, mi.mesh_asset_id) ||
            !write_string(file, mi.material_asset_id) ||
            !write_value(file, mi.pos_x) ||
            !write_value(file, mi.pos_y) ||
            !write_value(file, mi.pos_z) ||
            !write_value(file, mi.yaw) ||
            !write_value(file, mi.pitch) ||
            !write_value(file, mi.roll) ||
            !write_value(file, mi.scale_x) ||
            !write_value(file, mi.scale_y) ||
            !write_value(file, mi.scale_z)) {
            error = "Failed to write mesh instance";
            return false;
        }
    }

    return true;
}

bool read_mesh_instances(std::ifstream& file, LevelAsset& level, std::string& error) {
    std::uint32_t count = 0;
    if (!read_value(file, count) || !validate_count(count, "mesh instance", error, 65536)) {
        if (error.empty()) {
            error = "Failed to read mesh instance count";
        }
        return false;
    }

    level.mesh_instances.resize(count);
    for (auto& mi : level.mesh_instances) {
        if (!read_string(file, mi.mesh_asset_id, error) ||
            !read_string(file, mi.material_asset_id, error) ||
            !read_value(file, mi.pos_x) ||
            !read_value(file, mi.pos_y) ||
            !read_value(file, mi.pos_z) ||
            !read_value(file, mi.yaw) ||
            !read_value(file, mi.pitch) ||
            !read_value(file, mi.roll) ||
            !read_value(file, mi.scale_x) ||
            !read_value(file, mi.scale_y) ||
            !read_value(file, mi.scale_z)) {
            if (error.empty()) {
                error = "Failed to read mesh instance";
            }
            return false;
        }
    }

    return true;
}

} // namespace

bool save_compiled_level(const std::string& path, const LevelAsset& level, std::string& error) {
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

    try {
        if (!write_value(file, CompiledLevelFormat::magic) ||
            !write_value(file, CompiledLevelFormat::version) ||
            !write_world_settings(file, level, error) ||
            !write_spawn_points(file, level, error) ||
            !write_collision_boxes(file, level, error) ||
            !write_mesh_instances(file, level, error)) {
            if (error.empty()) {
                error = "Failed to write compiled level";
            }
            return false;
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }

    return true;
}

bool CompiledLevelLoader::load(const std::string& path, LevelAsset& level) {
    error_.clear();
    level = {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open: " + path;
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!read_value(file, magic) || !read_value(file, version)) {
        error_ = "Failed to read compiled level header";
        return false;
    }

    if (magic != CompiledLevelFormat::magic) {
        error_ = "Invalid compiled level magic";
        return false;
    }

    if (version != CompiledLevelFormat::version) {
        error_ = "Unsupported compiled level version";
        return false;
    }

    if (!read_world_settings(file, level, error_) ||
        !read_spawn_points(file, level, error_) ||
        !read_collision_boxes(file, level, error_) ||
        !read_mesh_instances(file, level, error_)) {
        return false;
    }

    return true;
}

} // namespace ae::render
