#pragma once

#include "ae/render/compiled_level.h"
#include "ae/render/compiled_material.h"
#include "ae/render/frustum.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/render_backend.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ae::render {

class PbrRenderer;

constexpr int kLodLevelCount = 3;

// One renderable mesh instance built from a LevelMeshInstance: GPU geometry
// for up to 3 LOD levels, a column-major world transform, scalar PBR material
// params, and optional material texture handles.
struct LevelRenderInstance {
    GpuModel lod_models[kLodLevelCount] {};
    float model_matrix[16] {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float albedo[3] {1.0F, 1.0F, 1.0F};
    float metallic {0.0F};
    float roughness {1.0F};
    TextureHandle albedo_map {};
    TextureHandle normal_map {};
    TextureHandle orm_map {};
    TextureHandle emissive_map {};
};

// A sorted (mesh, instance) pair produced by batch_level_draw_calls.
struct LodBatchedCall {
    GpuMesh* mesh;
    const LevelRenderInstance* instance;
};

// Scalar PBR parameters derived from a compiled material. Pure / GL-free.
struct PbrMaterialParams {
    float albedo[3] {1.0F, 1.0F, 1.0F};
    float metallic {0.0F};
    float roughness {1.0F};
};

// Build a column-major 4x4 world matrix from a level mesh instance.
// Rotation is applied as Ry(yaw) * Rx(pitch) * Rz(roll); yaw/pitch/roll are in
// degrees (matching the .lvl spawn-yaw convention). Pure / GL-free; unit-tested.
void compose_model_matrix(const LevelMeshInstance& instance, float out_matrix[16]);

// Map a compiled material's scalar fields to PBR draw parameters.
// Pure / GL-free; unit-tested.
[[nodiscard]] PbrMaterialParams material_to_pbr_params(const MaterialAsset& material);

// Assemble a single PBR draw call from a level render instance + one of its GPU
// meshes. Pure / GL-free; unit-tested.
[[nodiscard]] PbrDrawCall make_level_draw_call(const LevelRenderInstance& instance, GpuMesh& mesh);

// Select the best available LOD model for an instance given a camera position.
// Falls back to the nearest higher-detail level if the selected LOD has no
// meshes.  Returns the resolved LodLevel that was chosen.
[[nodiscard]] LodLevel resolve_instance_lod(const LevelRenderInstance& instance,
                                             const float* camera_position);

// Collect and sort draw calls from all instances by mesh GPU handle so that
// identical meshes are submitted consecutively (batching).  Returns a sorted
// vector of (mesh, instance) pairs.  Pure / GL-free; unit-tested.
[[nodiscard]] std::vector<LodBatchedCall> batch_level_draw_calls(
    const std::vector<LevelRenderInstance>& instances,
    const float* camera_position);

// Owns the GPU geometry + textures for a level's static mesh instances and
// submits them to the PbrRenderer each frame.
class LevelRenderScene {
public:
    LevelRenderScene() = default;

    LevelRenderScene(const LevelRenderScene&) = delete;
    LevelRenderScene& operator=(const LevelRenderScene&) = delete;
    LevelRenderScene(LevelRenderScene&&) = delete;
    LevelRenderScene& operator=(LevelRenderScene&&) = delete;

    // Load each mesh instance's compiled mesh (and optional material/textures),
    // upload to the GPU, and store with its world transform.  Tries LOD0
    // (mesh_asset_id), LOD1 (mesh_asset_id + ".lod1"), and LOD2
    // (mesh_asset_id + ".lod2").  Missing LOD files are skipped gracefully
    // (falls back to the nearest higher-detail model at draw time).
    //
    // asset_root is an optional directory prefix used to resolve asset ids that
    // are not already valid file paths. Asset-registry-based resolution is a
    // follow-up; for now ids are tried as literal paths first.
    void build(const LevelAsset& level, RenderBackend& backend, const std::string& asset_root);

    // Release all GPU resources for all LOD levels.  Call before the
    // backend/renderer shuts down.
    void destroy(RenderBackend& backend);

    // Submit all instances to the PBR renderer with LOD selection and
    // mesh-based batching.  camera_position is a 3-float array (xyz).
    // Call between begin_frame and end_frame on the renderer.
    void submit(PbrRenderer& pbr, const float* camera_position) const;

    [[nodiscard]] bool empty() const { return instances_.empty(); }
    [[nodiscard]] std::size_t instance_count() const { return instances_.size(); }
    [[nodiscard]] const std::vector<LevelRenderInstance>& instances() const { return instances_; }

private:
    std::vector<LevelRenderInstance> instances_;
    std::vector<TextureHandle> owned_textures_;
};

}  // namespace ae::render
