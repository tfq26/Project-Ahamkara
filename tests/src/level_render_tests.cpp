#include "ae/render/level_render.h"
#include "ae/render/spatial_partition.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool near_eq(float lhs, float rhs, float eps = 0.0001F) {
    return std::fabs(lhs - rhs) < eps;
}

int fail(const std::string& message) {
    std::cerr << "level_render_tests failed: " << message << '\n';
    return 1;
}

int test_identity_transform() {
    ae::render::LevelMeshInstance mi;
    mi.pos_x = 5.0F;
    mi.pos_y = 6.0F;
    mi.pos_z = 7.0F;
    mi.yaw = 0.0F;
    mi.pitch = 0.0F;
    mi.roll = 0.0F;
    mi.scale_x = 2.0F;
    mi.scale_y = 3.0F;
    mi.scale_z = 4.0F;

    float m[16];
    ae::render::compose_model_matrix(mi, m);

    if (!near_eq(m[0], 2.0F) || !near_eq(m[5], 3.0F) || !near_eq(m[10], 4.0F)) {
        return fail("scale not on diagonal");
    }
    if (!near_eq(m[12], 5.0F) || !near_eq(m[13], 6.0F) || !near_eq(m[14], 7.0F)) {
        return fail("translation not in last column");
    }
    if (!near_eq(m[1], 0.0F) || !near_eq(m[2], 0.0F) || !near_eq(m[4], 0.0F) ||
        !near_eq(m[6], 0.0F) || !near_eq(m[8], 0.0F) || !near_eq(m[9], 0.0F)) {
        return fail("unexpected off-diagonal rotation for identity orientation");
    }
    if (!near_eq(m[3], 0.0F) || !near_eq(m[7], 0.0F) || !near_eq(m[11], 0.0F) || !near_eq(m[15], 1.0F)) {
        return fail("homogeneous row incorrect");
    }
    return 0;
}

int test_yaw_90() {
    ae::render::LevelMeshInstance mi;
    mi.yaw = 90.0F;
    mi.scale_x = 1.0F;
    mi.scale_y = 1.0F;
    mi.scale_z = 1.0F;

    float m[16];
    ae::render::compose_model_matrix(mi, m);

    // Column 0 (local +X) should map to world (0, 0, -1).
    if (!near_eq(m[0], 0.0F) || !near_eq(m[1], 0.0F) || !near_eq(m[2], -1.0F)) {
        return fail("yaw 90 did not rotate +X to (0,0,-1)");
    }
    // Column 2 (local +Z) should map to world (1, 0, 0).
    if (!near_eq(m[8], 1.0F) || !near_eq(m[9], 0.0F) || !near_eq(m[10], 0.0F)) {
        return fail("yaw 90 did not rotate +Z to (1,0,0)");
    }
    if (!near_eq(m[5], 1.0F)) {
        return fail("yaw 90 disturbed the Y axis");
    }
    return 0;
}

int test_material_mapping() {
    ae::render::MaterialAsset material;
    material.base_color_r = 0.2F;
    material.base_color_g = 0.4F;
    material.base_color_b = 0.6F;
    material.metallic = 0.3F;
    material.roughness = 0.7F;

    const ae::render::PbrMaterialParams params = ae::render::material_to_pbr_params(material);
    if (!near_eq(params.albedo[0], 0.2F) || !near_eq(params.albedo[1], 0.4F) ||
        !near_eq(params.albedo[2], 0.6F)) {
        return fail("albedo not mapped from base color");
    }
    if (!near_eq(params.metallic, 0.3F) || !near_eq(params.roughness, 0.7F)) {
        return fail("metallic/roughness not mapped");
    }
    return 0;
}

int test_draw_call_assembly() {
    ae::render::LevelRenderInstance instance;
    instance.albedo[0] = 0.1F;
    instance.albedo[1] = 0.2F;
    instance.albedo[2] = 0.3F;
    instance.metallic = 0.4F;
    instance.roughness = 0.6F;
    instance.albedo_map = {7};
    instance.orm_map = {9};
    instance.model_matrix[12] = 5.0F;

    ae::render::GpuMesh mesh;
    mesh.vbo_positions = {3};
    mesh.index_count = 36;

    const ae::render::PbrDrawCall dc = ae::render::make_level_draw_call(instance, mesh);
    if (dc.mesh != &mesh) {
        return fail("draw call mesh pointer mismatch");
    }
    if (dc.model_matrix != instance.model_matrix) {
        return fail("draw call model matrix pointer mismatch");
    }
    if (!near_eq(dc.albedo[0], 0.1F) || !near_eq(dc.albedo[1], 0.2F) || !near_eq(dc.albedo[2], 0.3F)) {
        return fail("draw call albedo mismatch");
    }
    if (!near_eq(dc.metallic, 0.4F) || !near_eq(dc.roughness, 0.6F)) {
        return fail("draw call metallic/roughness mismatch");
    }
    if (dc.albedo_map.id != 7U || dc.orm_map.id != 9U) {
        return fail("draw call texture handles mismatch");
    }

    // A model with N meshes assembles into N draw calls (one per mesh).
    ae::render::LevelRenderInstance multi;
    multi.lod_models[0].meshes.resize(3);
    multi.lod_models[0].meshes[0].vbo_positions = {11};
    multi.lod_models[0].meshes[1].vbo_positions = {12};
    multi.lod_models[0].meshes[2].vbo_positions = {13};
    int draw_calls = 0;
    for (auto& m : multi.lod_models[0].meshes) {
        const ae::render::PbrDrawCall d = ae::render::make_level_draw_call(multi, m);
        if (d.mesh != &m) {
            return fail("multi-mesh draw call mesh pointer mismatch");
        }
        ++draw_calls;
    }
    if (draw_calls != 3) {
        return fail("expected 3 draw calls for a 3-mesh model");
    }
    return 0;
}

// --- Spatial partition tests ---

int test_spatial_grid_construction() {
    ae::render::SpatialGrid grid(10, 8, 2.0F, 0.0F, 0.0F);
    if (grid.cols() != 10) return fail("expected 10 cols");
    if (grid.rows() != 8) return fail("expected 8 rows");
    if (!near_eq(grid.cell_size(), 2.0F)) return fail("cell size mismatch");
    if (grid.empty()) return fail("10x8 grid should not be empty");
    if (grid.cell_count() != 80) return fail("expected 80 cells");
    return 0;
}

int test_spatial_grid_insert_query() {
    ae::render::SpatialGrid grid(10, 10, 1.0F, 0.0F, 0.0F);
    grid.insert(42, {2.0F, -1.0F, 2.0F, 4.0F, 1.0F, 4.0F});
    if (grid.total_ids() != 4) return fail("2x2 box should touch 4 cells");
    // Query the overlapping AABB
    std::vector<std::uint64_t> results;
    grid.query_aabb({2.5F, -1.0F, 2.5F, 3.5F, 1.0F, 3.5F}, results);
    if (results.empty()) return fail("query should find ID 42");
    bool found = false;
    for (auto id : results) { if (id == 42) { found = true; break; } }
    if (!found) return fail("query should contain ID 42");
    return 0;
}

int test_spatial_grid_remove() {
    ae::render::SpatialGrid grid(10, 10, 1.0F, 0.0F, 0.0F);
    ae::render::AABB aabb = {2.0F, -1.0F, 2.0F, 4.0F, 1.0F, 4.0F};
    grid.insert(42, aabb);
    if (grid.total_ids() != 4) return fail("should have 4 IDs after insert");
    grid.remove(42, aabb);
    if (grid.total_ids() != 0) return fail("should have 0 IDs after remove");
    return 0;
}

int test_spatial_grid_empty_query() {
    ae::render::SpatialGrid grid(5, 5, 1.0F, 0.0F, 0.0F);
    std::vector<std::uint64_t> results;
    grid.query_aabb({0.0F, -1.0F, 0.0F, 5.0F, 1.0F, 5.0F}, results);
    if (!results.empty()) return fail("empty grid should return no results");
    if (grid.total_ids() != 0) return fail("total_ids should be 0");
    return 0;
}

int test_spatial_grid_world_to_cell() {
    ae::render::SpatialGrid grid(10, 10, 2.0F, -10.0F, -10.0F);
    if (grid.world_to_cx(-9.0F) != 0) return fail("(-9,?) → cell col 0");
    if (grid.world_to_cx(9.0F) != 9) return fail("(9,?) → cell col 9");
    if (grid.world_to_cy(-9.0F) != 0) return fail("(?,-9) → cell row 0");
    if (grid.world_to_cy(9.0F) != 9) return fail("(?,9) → cell row 9");
    return 0;
}

int test_spatial_grid_default_empty() {
    ae::render::SpatialGrid grid;
    // Default grid has empty cells_ (0 cells).
    if (grid.cell_count() != 0) return fail("default grid should have 0 cells");
    if (!grid.empty()) return fail("default grid should be empty");
    if (grid.total_ids() != 0) return fail("default grid should have 0 IDs");
    return 0;
}

}  // namespace

int main() {
    if (int rc = test_identity_transform(); rc != 0) {
        return rc;
    }
    if (int rc = test_draw_call_assembly(); rc != 0) {
        return rc;
    }
    if (int rc = test_yaw_90(); rc != 0) {
        return rc;
    }
    if (int rc = test_material_mapping(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_construction(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_insert_query(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_remove(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_empty_query(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_world_to_cell(); rc != 0) {
        return rc;
    }
    if (int rc = test_spatial_grid_default_empty(); rc != 0) {
        return rc;
    }
    std::cout << "level_render_tests passed\n";
    return 0;
}
