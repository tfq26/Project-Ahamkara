#include "ae/render/level_render.h"

#include "ae/render/compiled_mesh.h"
#include "ae/render/compiled_texture.h"
#include "ae/render/pbr_renderer.h"

#include <cmath>
#include <filesystem>

namespace ae::render {

namespace {

constexpr float kDegToRad = 3.14159265358979323846F / 180.0F;

void mul3(const float a[9], const float b[9], float out[9]) {
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[row * 3 + col] = a[row * 3 + 0] * b[0 * 3 + col] +
                                 a[row * 3 + 1] * b[1 * 3 + col] +
                                 a[row * 3 + 2] * b[2 * 3 + col];
        }
    }
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

// Resolve an asset id to a readable file path. For this slice, the id is tried
// as a literal path first, then joined under asset_root. Registry-based
// resolution (assets/compiled/asset_registry.tsv) is a documented follow-up.
bool resolve_asset_path(const std::string& asset_id, const std::string& asset_root, std::string& out_path) {
    if (asset_id.empty()) {
        return false;
    }
    if (file_exists(asset_id)) {
        out_path = asset_id;
        return true;
    }
    if (!asset_root.empty()) {
        const std::string joined = asset_root + "/" + asset_id;
        if (file_exists(joined)) {
            out_path = joined;
            return true;
        }
    }
    return false;
}

}  // namespace

void compose_model_matrix(const LevelMeshInstance& mi, float out[16]) {
    const float cy = std::cos(mi.yaw * kDegToRad);
    const float sy = std::sin(mi.yaw * kDegToRad);
    const float cp = std::cos(mi.pitch * kDegToRad);
    const float sp = std::sin(mi.pitch * kDegToRad);
    const float cr = std::cos(mi.roll * kDegToRad);
    const float sr = std::sin(mi.roll * kDegToRad);

    // Row-major basic rotations.
    const float ry[9] = {cy, 0.0F, sy, 0.0F, 1.0F, 0.0F, -sy, 0.0F, cy};
    const float rx[9] = {1.0F, 0.0F, 0.0F, 0.0F, cp, -sp, 0.0F, sp, cp};
    const float rz[9] = {cr, -sr, 0.0F, sr, cr, 0.0F, 0.0F, 0.0F, 1.0F};

    float ryx[9];
    mul3(ry, rx, ryx);
    float rot[9];  // R = Ry * Rx * Rz, row-major: rot[row*3 + col]
    mul3(ryx, rz, rot);

    // Column-major 4x4: out[col*4 + row]. Each rotation column scaled by axis scale.
    out[0] = rot[0 * 3 + 0] * mi.scale_x;
    out[1] = rot[1 * 3 + 0] * mi.scale_x;
    out[2] = rot[2 * 3 + 0] * mi.scale_x;
    out[3] = 0.0F;

    out[4] = rot[0 * 3 + 1] * mi.scale_y;
    out[5] = rot[1 * 3 + 1] * mi.scale_y;
    out[6] = rot[2 * 3 + 1] * mi.scale_y;
    out[7] = 0.0F;

    out[8] = rot[0 * 3 + 2] * mi.scale_z;
    out[9] = rot[1 * 3 + 2] * mi.scale_z;
    out[10] = rot[2 * 3 + 2] * mi.scale_z;
    out[11] = 0.0F;

    out[12] = mi.pos_x;
    out[13] = mi.pos_y;
    out[14] = mi.pos_z;
    out[15] = 1.0F;
}

PbrMaterialParams material_to_pbr_params(const MaterialAsset& material) {
    PbrMaterialParams params;
    params.albedo[0] = material.base_color_r;
    params.albedo[1] = material.base_color_g;
    params.albedo[2] = material.base_color_b;
    params.metallic = material.metallic;
    params.roughness = material.roughness;
    return params;
}

void LevelRenderScene::build(const LevelAsset& level, RenderBackend& backend, const std::string& asset_root) {
    auto load_texture = [&](const std::string& tex_id) -> TextureHandle {
        std::string path;
        if (!resolve_asset_path(tex_id, asset_root, path)) {
            return kInvalidTexture;
        }
        CompiledTextureLoader tex_loader;
        TextureAsset tex;
        if (!tex_loader.load(path, tex) || tex.pixels.empty()) {
            return kInvalidTexture;
        }
        TextureHandle handle = backend.create_texture(
            static_cast<int>(tex.width), static_cast<int>(tex.height), tex.pixels.data());
        if (handle.id != 0) {
            owned_textures_.push_back(handle);
        }
        return handle;
    };

    for (const auto& mi : level.mesh_instances) {
        std::string mesh_path;
        if (!resolve_asset_path(mi.mesh_asset_id, asset_root, mesh_path)) {
            continue;
        }

        CompiledMeshLoader mesh_loader;
        GltfModel model;
        if (!mesh_loader.load(mesh_path, model)) {
            continue;
        }

        LevelRenderInstance instance;
        instance.model = backend.create_gpu_model(model);
        compose_model_matrix(mi, instance.model_matrix);

        if (!mi.material_asset_id.empty()) {
            std::string mat_path;
            CompiledMaterialLoader mat_loader;
            MaterialAsset material;
            if (resolve_asset_path(mi.material_asset_id, asset_root, mat_path) &&
                mat_loader.load(mat_path, material)) {
                const PbrMaterialParams params = material_to_pbr_params(material);
                instance.albedo[0] = params.albedo[0];
                instance.albedo[1] = params.albedo[1];
                instance.albedo[2] = params.albedo[2];
                instance.metallic = params.metallic;
                instance.roughness = params.roughness;
                instance.albedo_map = load_texture(material.albedo_texture);
                instance.normal_map = load_texture(material.normal_texture);
                instance.orm_map = load_texture(material.orm_texture);
            }
        }

        instances_.push_back(std::move(instance));
    }
}

void LevelRenderScene::destroy(RenderBackend& backend) {
    for (auto& instance : instances_) {
        backend.destroy_gpu_model(instance.model);
    }
    for (auto& handle : owned_textures_) {
        if (handle.id != 0) {
            backend.destroy_texture(handle);
        }
    }
    instances_.clear();
    owned_textures_.clear();
}

PbrDrawCall make_level_draw_call(const LevelRenderInstance& instance, GpuMesh& mesh) {
    PbrDrawCall draw_call;
    draw_call.mesh = &mesh;
    draw_call.model_matrix = instance.model_matrix;
    draw_call.albedo[0] = instance.albedo[0];
    draw_call.albedo[1] = instance.albedo[1];
    draw_call.albedo[2] = instance.albedo[2];
    draw_call.metallic = instance.metallic;
    draw_call.roughness = instance.roughness;
    draw_call.albedo_map = instance.albedo_map;
    draw_call.normal_map = instance.normal_map;
    draw_call.orm_map = instance.orm_map;
    return draw_call;
}

void LevelRenderScene::submit(PbrRenderer& pbr) const {
    for (const auto& instance : instances_) {
        for (const auto& mesh : instance.model.meshes) {
            PbrDrawCall draw_call = make_level_draw_call(instance, const_cast<GpuMesh&>(mesh));
            pbr.submit(draw_call);
        }
    }
}

}  // namespace ae::render
