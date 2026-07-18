#pragma once

#include "ae/render/compiled_level.h"
#include "ae/render/compiled_material.h"
#include "ae/render/frustum.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/render_backend.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ae::render {

class PbrRenderer;

constexpr int kLodLevelCount = 3;

// ---------------------------------------------------------------------------
// Configurable LOD transition distances
// ---------------------------------------------------------------------------

struct LodSettings {
    // Linear (not squared) distances at which to transition between LOD levels.
    // distances[0]: LOD0 -> LOD1 threshold
    // distances[1]: LOD1 -> LOD2 threshold
    // distances[2]: LOD2 -> impostor threshold (beyond which a billboard is used)
    float distances[kLodLevelCount] = {12.0F, 30.0F, 100.0F};

    // Distance at which to switch to an impostor/billboard when no LOD2 mesh is
    // available or as an alternative to LOD2.  Default matches distances[2].
    float billboard_distance = 100.0F;
};

// ---------------------------------------------------------------------------
// Impostor / billboard support
// ---------------------------------------------------------------------------

// A single billboard impostor rendered in place of a mesh at extreme distance.
struct ImpostorInstance {
    float world_pos[3] = {0.0F, 0.0F, 0.0F};
    TextureHandle billboard_texture = kInvalidTexture;
    float size = 1.0F;
};

// Manages a collection of billboard textures used for impostor rendering.
struct ImpostorAtlas {
    std::vector<TextureHandle> textures;
};

// ---------------------------------------------------------------------------
// Core rendering types
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

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

// Select the best available LOD model for an instance given a camera position
// and optional configurable LOD distances.  Falls back to the nearest
// higher-detail level if the selected LOD has no meshes.  Returns the resolved
// LodLevel that was chosen.
[[nodiscard]] LodLevel resolve_instance_lod(const LevelRenderInstance& instance,
                                            const float* camera_position,
                                            const LodSettings& settings = LodSettings {});

// Compute a 64-bit material identity key from a LevelRenderInstance's PBR
// params.  Used for material-aware secondary sorting.
[[nodiscard]] std::uint64_t compute_material_key(const LevelRenderInstance& instance);

// Collect and sort draw calls from all instances by mesh GPU handle so that
// identical meshes are submitted consecutively (batching), with material
// identity as a secondary sort key.  Returns a sorted vector of (mesh,
// instance) pairs.  Pure / GL-free; unit-tested.
[[nodiscard]] std::vector<LodBatchedCall> batch_level_draw_calls(
    const std::vector<LevelRenderInstance>& instances,
    const float* camera_position,
    const LodSettings& settings = LodSettings {});

// Returns true if, in the given sorted batch list, every group of consecutive
// calls sharing the same mesh also shares the same material identity.  This
// verifies that material-aware secondary sorting is correct.
[[nodiscard]] bool sorted_by_material(const std::vector<LodBatchedCall>& calls);

// Measure batch efficiency as the fraction of consecutive same-mesh runs that
// are longer than one (i.e. batched together).  Returns a value in [0, 1]
// where 1.0 means all calls are perfectly batched (one run per unique mesh).
[[nodiscard]] float batch_efficiency(const std::vector<LodBatchedCall>& calls);

// Determine whether an instance should be rendered as an impostor at the given
// camera position using the provided LOD settings.
[[nodiscard]] bool use_impostor(const LevelRenderInstance& instance,
                                const float* camera_position,
                                const LodSettings& settings = LodSettings {});

// ---------------------------------------------------------------------------
// LevelRenderScene
// ---------------------------------------------------------------------------

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
    // mesh+material-based batching.  camera_position is a 3-float array (xyz).
    // Call between begin_frame and end_frame on the renderer.
    // Accepts optional LodSettings for configurable LOD transition distances.
    void submit(PbrRenderer& pbr, const float* camera_position,
                const LodSettings& settings = LodSettings {}) const;

    [[nodiscard]] bool empty() const { return instances_.empty(); }
    [[nodiscard]] std::size_t instance_count() const { return instances_.size(); }
    [[nodiscard]] const std::vector<LevelRenderInstance>& instances() const { return instances_; }

private:
    std::vector<LevelRenderInstance> instances_;
    std::vector<TextureHandle> owned_textures_;
};

}  // namespace ae::render
