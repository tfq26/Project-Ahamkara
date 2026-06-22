#pragma once

#include "ae/render/compiled_level.h"
#include "ae/render/compiled_material.h"
#include "ae/render/pbr_renderer.h"
#include "ae/render/render_backend.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ae::render {

class PbrRenderer;

// One renderable mesh instance built from a LevelMeshInstance: GPU geometry,
// a column-major world transform, scalar PBR material params, and optional
// (currently inert — see note below) material texture handles.
struct LevelRenderInstance {
    GpuModel model {};
    float model_matrix[16] {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float albedo[3] {1.0F, 1.0F, 1.0F};
    float metallic {0.0F};
    float roughness {1.0F};
    TextureHandle albedo_map {};
    TextureHandle normal_map {};
    TextureHandle orm_map {};
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

// Owns the GPU geometry + textures for a level's static mesh instances and
// submits them to the PbrRenderer each frame.
//
// NOTE: this slice wires the texture-loading path but the PBR shader currently
// samples textures at a constant UV (no UVs are plumbed yet), so textured
// output is inert. Scalar albedo/metallic/roughness render correctly. UV
// plumbing is a separate follow-up task.
class LevelRenderScene {
public:
    LevelRenderScene() = default;

    LevelRenderScene(const LevelRenderScene&) = delete;
    LevelRenderScene& operator=(const LevelRenderScene&) = delete;
    LevelRenderScene(LevelRenderScene&&) = delete;
    LevelRenderScene& operator=(LevelRenderScene&&) = delete;

    // Load each mesh instance's compiled mesh (and optional material/textures),
    // upload to the GPU, and store with its world transform. Missing assets are
    // skipped gracefully (the frame is never failed).
    //
    // asset_root is an optional directory prefix used to resolve asset ids that
    // are not already valid file paths. Asset-registry-based resolution is a
    // follow-up; for now ids are tried as literal paths first.
    void build(const LevelAsset& level, RenderBackend& backend, const std::string& asset_root);

    // Release all GPU resources. Call before the backend/renderer shuts down.
    void destroy(RenderBackend& backend);

    // Submit all instances to the PBR renderer (call between begin_frame and
    // end_frame on the renderer).
    void submit(PbrRenderer& pbr) const;

    [[nodiscard]] bool empty() const { return instances_.empty(); }
    [[nodiscard]] std::size_t instance_count() const { return instances_.size(); }
    [[nodiscard]] const std::vector<LevelRenderInstance>& instances() const { return instances_; }

private:
    std::vector<LevelRenderInstance> instances_;
    std::vector<TextureHandle> owned_textures_;
};

}  // namespace ae::render
