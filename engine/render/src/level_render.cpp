#include "ae/render/level_render.h"
#include "ae/core/log.h"

#include "ae/render/compiled_mesh.h"
#include "ae/render/compiled_texture.h"
#include "ae/core/log.h"
#include "ae/render/frustum.h"
#include "ae/render/pbr_renderer.h"
#include "ae/core/log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>


#define AE_LOG_CATEGORY "Render"

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

// Try to load a single LOD mesh for an instance.
// Returns true and sets `out_model` on success; `out_model` is left untouched on failure.
bool try_load_lod(RenderBackend& backend, const std::string& mesh_path, GpuModel& out_model) {
    CompiledMeshLoader loader;
    GltfModel model;
    if (!loader.load(mesh_path, model)) {
        return false;
    }
    out_model = backend.create_gpu_model(model);
    return true;
}

// Load a LOD variant by asset id.
bool load_lod_variant(const std::string& asset_id, const std::string& asset_root,
                      RenderBackend& backend,
                      GpuModel& out_model) {
    std::string resolved;
    if (!resolve_asset_path(asset_id, asset_root, resolved)) {
        return false;
    }
    return try_load_lod(backend, resolved, out_model);
}

const char* kLodSuffixes[kLodLevelCount] = {"", ".lod1", ".lod2"};

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
        // Resolve LOD0 path first (required).
        std::string lod0_path;
        if (!resolve_asset_path(mi.mesh_asset_id, asset_root, lod0_path)) {
            continue;
        }

        LevelRenderInstance instance;
        compose_model_matrix(mi, instance.model_matrix);

        // Load LOD0 (mandatory).
        try_load_lod(backend, lod0_path, instance.lod_models[0]);

        // Load LOD1 (optional).
        const std::string lod1_id = mi.mesh_asset_id + kLodSuffixes[1];
        load_lod_variant(lod1_id, asset_root, backend, instance.lod_models[1]);

        // Load LOD2 (optional).
        const std::string lod2_id = mi.mesh_asset_id + kLodSuffixes[2];
        load_lod_variant(lod2_id, asset_root, backend, instance.lod_models[2]);

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
                instance.emissive_map = load_texture(material.emissive_texture);
            }
        }

        instances_.push_back(std::move(instance));
    }
}

void LevelRenderScene::destroy(RenderBackend& backend) {
    for (auto& instance : instances_) {
        for (int i = 0; i < kLodLevelCount; ++i) {
            backend.destroy_gpu_model(instance.lod_models[i]);
        }
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
    draw_call.emissive_color[0] = 0.0F;
    draw_call.emissive_color[1] = 0.0F;
    draw_call.emissive_color[2] = 0.0F;
    draw_call.emissive_intensity = 0.0F;
    draw_call.emissive_map = instance.emissive_map;
    return draw_call;
}

LodLevel resolve_instance_lod(const LevelRenderInstance& instance,
                               const float* camera_position) {
    // Compute squared distance from camera to instance center.
    const float dx = instance.model_matrix[12] - camera_position[0];
    const float dy = instance.model_matrix[13] - camera_position[1];
    const float dz = instance.model_matrix[14] - camera_position[2];
    const float dist_sq = dx * dx + dy * dy + dz * dz;

    LodLevel lod = select_lod(dist_sq);

    // Fall back to a higher-detail LOD if the selected model has no meshes.
    // LOD0 is always expected to have meshes (or the instance wouldn't exist).
    while (static_cast<int>(lod) > 0 &&
           instance.lod_models[static_cast<int>(lod)].meshes.empty()) {
        lod = static_cast<LodLevel>(static_cast<int>(lod) - 1);
    }
    return lod;
}

std::vector<LodBatchedCall> batch_level_draw_calls(
    const std::vector<LevelRenderInstance>& instances,
    const float* camera_position) {

    struct DrawCallSortEntry {
        const GpuMesh* mesh;
        const LevelRenderInstance* instance;
        std::uint64_t sort_key;
    };
    std::vector<DrawCallSortEntry> entries;

    for (const auto& instance : instances) {
        const LodLevel lod = resolve_instance_lod(instance, camera_position);
        const GpuModel& model = instance.lod_models[static_cast<int>(lod)];

        for (const auto& mesh : model.meshes) {
            // Sort key combines VBO and IBO handles so identical meshes group.
            const std::uint64_t key =
                (static_cast<std::uint64_t>(mesh.vbo_positions.id) << 32) |
                static_cast<std::uint64_t>(mesh.ibo_indices.id);
            entries.push_back({&mesh, &instance, key});
        }
    }

    // Sort by mesh identity so same-mesh draw calls are consecutive.
    std::sort(entries.begin(), entries.end(),
              [](const DrawCallSortEntry& a, const DrawCallSortEntry& b) {
                  return a.sort_key < b.sort_key;
              });

    std::vector<LodBatchedCall> result;
    result.reserve(entries.size());
    for (auto& e : entries) {
        result.push_back({const_cast<GpuMesh*>(e.mesh), e.instance});
    }
    return result;
}

void LevelRenderScene::submit(PbrRenderer& pbr, const float* camera_position) const {
    std::vector<LodBatchedCall> calls = batch_level_draw_calls(instances_, camera_position);
    for (const auto& call : calls) {
        PbrDrawCall draw_call = make_level_draw_call(*call.instance, *call.mesh);
        pbr.submit(draw_call);
    }
}

}  // namespace ae::render
