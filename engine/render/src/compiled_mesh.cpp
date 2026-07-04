#include "ae/render/compiled_mesh.h"
#include "ae/render/binary_io.h"
#include "ae/core/log.h"

#include <filesystem>
#include <fstream>
#include <string_view>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {
namespace {

bool write_meshes(std::ofstream& file, const GltfModel& model, std::string& error) {
    const auto mesh_count = checked_count(model.meshes.size(), "mesh");
    if (!write_value(file, mesh_count)) {
        error = "Failed to write mesh count";
        return false;
    }

    for (const auto& mesh : model.meshes) {
        const auto position_count = checked_count(mesh.positions.size(), "position");
        const auto normal_count = checked_count(mesh.normals.size(), "normal");
        const auto joint_index_count = checked_count(mesh.joint_indices.size(), "joint index");
        const auto joint_weight_count = checked_count(mesh.joint_weights.size(), "joint weight");
        const auto index_count = checked_count(mesh.indices.size(), "index");
        const auto uv_count = checked_count(mesh.uvs.size(), "uv");
        const std::uint32_t has_material_color = mesh.has_material_color ? 1U : 0U;

        if (!write_value(file, position_count) ||
            !write_value(file, normal_count) ||
            !write_value(file, joint_index_count) ||
            !write_value(file, joint_weight_count) ||
            !write_value(file, index_count) ||
            !write_value(file, mesh.color_r) ||
            !write_value(file, mesh.color_g) ||
            !write_value(file, mesh.color_b) ||
            !write_value(file, has_material_color) ||
            !write_vector(file, mesh.positions) ||
            !write_vector(file, mesh.normals) ||
            !write_vector(file, mesh.joint_indices) ||
            !write_vector(file, mesh.joint_weights) ||
            !write_vector(file, mesh.indices) ||
            !write_value(file, uv_count) ||
            !write_vector(file, mesh.uvs)) {
            error = "Failed to write mesh data";
            return false;
        }
    }

    return true;
}

bool read_meshes(std::ifstream& file, GltfModel& model, std::uint32_t version, std::string& error) {
    std::uint32_t mesh_count = 0;
    if (!read_value(file, mesh_count) || !validate_count(mesh_count, "mesh", error)) {
        if (error.empty()) {
            error = "Failed to read mesh count";
        }
        return false;
    }

    model.meshes.resize(mesh_count);
    for (auto& mesh : model.meshes) {
        std::uint32_t position_count = 0;
        std::uint32_t normal_count = 0;
        std::uint32_t joint_index_count = 0;
        std::uint32_t joint_weight_count = 0;
        std::uint32_t index_count = 0;
        std::uint32_t has_material_color = 0;

        if (!read_value(file, position_count) ||
            !read_value(file, normal_count) ||
            !read_value(file, joint_index_count) ||
            !read_value(file, joint_weight_count) ||
            !read_value(file, index_count) ||
            !read_value(file, mesh.color_r) ||
            !read_value(file, mesh.color_g) ||
            !read_value(file, mesh.color_b) ||
            !read_value(file, has_material_color)) {
            error = "Failed to read mesh header";
            return false;
        }

        mesh.has_material_color = has_material_color != 0;

        if (!read_vector(file, mesh.positions, position_count, "position", error) ||
            !read_vector(file, mesh.normals, normal_count, "normal", error) ||
            !read_vector(file, mesh.joint_indices, joint_index_count, "joint index", error) ||
            !read_vector(file, mesh.joint_weights, joint_weight_count, "joint weight", error) ||
            !read_vector(file, mesh.indices, index_count, "index", error)) {
            return false;
        }

        if (version >= 2) {
            std::uint32_t uv_count = 0;
            if (!read_value(file, uv_count) ||
                !read_vector(file, mesh.uvs, uv_count, "uv", error)) {
                if (error.empty()) {
                    error = "Failed to read mesh uvs";
                }
                return false;
            }
        }
    }

    return true;
}

bool write_skins(std::ofstream& file, const GltfModel& model, std::string& error) {
    const auto skin_count = checked_count(model.skins.size(), "skin");
    if (!write_value(file, skin_count)) {
        error = "Failed to write skin count";
        return false;
    }

    for (const auto& skin : model.skins) {
        const auto joint_count = checked_count(skin.joints.size(), "joint");
        if (!write_value(file, joint_count)) {
            error = "Failed to write joint count";
            return false;
        }

        for (const auto& joint : skin.joints) {
            const auto matrix_count = checked_count(joint.inverse_bind_matrix.size(), "inverse bind matrix");
            if (!write_string(file, joint.name) ||
                !write_value(file, joint.node_index) ||
                !write_value(file, joint.parent_index) ||
                !write_value(file, matrix_count) ||
                !write_vector(file, joint.inverse_bind_matrix)) {
                error = "Failed to write joint data";
                return false;
            }
        }
    }

    return true;
}

bool read_skins(std::ifstream& file, GltfModel& model, std::string& error) {
    std::uint32_t skin_count = 0;
    if (!read_value(file, skin_count) || !validate_count(skin_count, "skin", error)) {
        if (error.empty()) {
            error = "Failed to read skin count";
        }
        return false;
    }

    model.skins.resize(skin_count);
    for (auto& skin : model.skins) {
        std::uint32_t joint_count = 0;
        if (!read_value(file, joint_count) || !validate_count(joint_count, "joint", error)) {
            if (error.empty()) {
                error = "Failed to read joint count";
            }
            return false;
        }

        skin.joints.resize(joint_count);
        for (auto& joint : skin.joints) {
            std::uint32_t matrix_count = 0;
            if (!read_string(file, joint.name, error) ||
                !read_value(file, joint.node_index) ||
                !read_value(file, joint.parent_index) ||
                !read_value(file, matrix_count) ||
                !read_vector(file, joint.inverse_bind_matrix, matrix_count, "inverse bind matrix", error)) {
                if (error.empty()) {
                    error = "Failed to read joint data";
                }
                return false;
            }
        }
    }

    return true;
}

bool write_animations(std::ofstream& file, const GltfModel& model, std::string& error) {
    const auto animation_count = checked_count(model.animations.size(), "animation");
    if (!write_value(file, animation_count)) {
        error = "Failed to write animation count";
        return false;
    }

    for (const auto& animation : model.animations) {
        const auto channel_count = checked_count(animation.channels.size(), "animation channel");
        const auto sampler_count = checked_count(animation.samplers.size(), "animation sampler");
        if (!write_string(file, animation.name) ||
            !write_value(file, channel_count) ||
            !write_value(file, sampler_count)) {
            error = "Failed to write animation header";
            return false;
        }

        for (const auto& channel : animation.channels) {
            if (!write_value(file, channel.node_index) ||
                !write_string(file, channel.path) ||
                !write_value(file, channel.sampler_index)) {
                error = "Failed to write animation channel";
                return false;
            }
        }

        for (const auto& sampler : animation.samplers) {
            const auto input_count = checked_count(sampler.input_times.size(), "animation input");
            const auto output_count = checked_count(sampler.output_values.size(), "animation output");
            if (!write_string(file, sampler.interpolation) ||
                !write_value(file, input_count) ||
                !write_vector(file, sampler.input_times) ||
                !write_value(file, output_count) ||
                !write_vector(file, sampler.output_values)) {
                error = "Failed to write animation sampler";
                return false;
            }
        }
    }

    return true;
}

bool read_animations(std::ifstream& file, GltfModel& model, std::string& error) {
    std::uint32_t animation_count = 0;
    if (!read_value(file, animation_count) || !validate_count(animation_count, "animation", error)) {
        if (error.empty()) {
            error = "Failed to read animation count";
        }
        return false;
    }

    model.animations.resize(animation_count);
    for (auto& animation : model.animations) {
        std::uint32_t channel_count = 0;
        std::uint32_t sampler_count = 0;
        if (!read_string(file, animation.name, error) ||
            !read_value(file, channel_count) ||
            !read_value(file, sampler_count) ||
            !validate_count(channel_count, "animation channel", error) ||
            !validate_count(sampler_count, "animation sampler", error)) {
            if (error.empty()) {
                error = "Failed to read animation header";
            }
            return false;
        }

        animation.channels.resize(channel_count);
        animation.samplers.resize(sampler_count);

        for (auto& channel : animation.channels) {
            if (!read_value(file, channel.node_index) ||
                !read_string(file, channel.path, error) ||
                !read_value(file, channel.sampler_index)) {
                if (error.empty()) {
                    error = "Failed to read animation channel";
                }
                return false;
            }
        }

        for (auto& sampler : animation.samplers) {
            std::uint32_t input_count = 0;
            std::uint32_t output_count = 0;
            if (!read_string(file, sampler.interpolation, error) ||
                !read_value(file, input_count) ||
                !read_vector(file, sampler.input_times, input_count, "animation input", error) ||
                !read_value(file, output_count) ||
                !read_vector(file, sampler.output_values, output_count, "animation output", error)) {
                if (error.empty()) {
                    error = "Failed to read animation sampler";
                }
                return false;
            }
        }
    }

    return true;
}

} // namespace

bool save_compiled_mesh(const std::string& path, const GltfModel& model, std::string& error) {
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
        if (!write_value(file, CompiledMeshFormat::magic) ||
            !write_value(file, CompiledMeshFormat::version) ||
            !write_meshes(file, model, error) ||
            !write_skins(file, model, error) ||
            !write_animations(file, model, error)) {
            if (error.empty()) {
                error = "Failed to write compiled mesh";
            }
            return false;
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }

    return true;
}

bool CompiledMeshLoader::load(const std::string& path, GltfModel& model) {
    error_.clear();
    model = {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open: " + path;
        log_error_cat(AE_LOG_CATEGORY, "CompiledMeshLoader: " + error_);
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!read_value(file, magic) || !read_value(file, version)) {
        error_ = "Failed to read compiled mesh header";
        log_error_cat(AE_LOG_CATEGORY, "CompiledMeshLoader: " + error_);
        return false;
    }

    if (magic != CompiledMeshFormat::magic) {
        error_ = "Invalid compiled mesh magic";
        log_error_cat(AE_LOG_CATEGORY, "CompiledMeshLoader: " + error_);
        return false;
    }

    if (version == 0 || version > CompiledMeshFormat::version) {
        error_ = "Unsupported compiled mesh version";
        log_error_cat(AE_LOG_CATEGORY, "CompiledMeshLoader: " + error_);
        return false;
    }

    if (!read_meshes(file, model, version, error_) ||
        !read_skins(file, model, error_) ||
        !read_animations(file, model, error_)) {
        log_error_cat(AE_LOG_CATEGORY, "CompiledMeshLoader: " + error_);
        return false;
    }

    log_debug_cat(AE_LOG_CATEGORY, "Compiled mesh loaded: " + path);
    return true;
}

} // namespace ae::render
