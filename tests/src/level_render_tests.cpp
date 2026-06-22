#include "ae/render/level_render.h"

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
    multi.model.meshes.resize(3);
    multi.model.meshes[0].vbo_positions = {11};
    multi.model.meshes[1].vbo_positions = {12};
    multi.model.meshes[2].vbo_positions = {13};
    int draw_calls = 0;
    for (auto& m : multi.model.meshes) {
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
    std::cout << "level_render_tests passed\n";
    return 0;
}
