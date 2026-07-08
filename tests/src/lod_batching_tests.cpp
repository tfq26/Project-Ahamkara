#include "ae/render/level_render.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool near_eq(float lhs, float rhs, float eps = 0.0001F) {
    return std::fabs(lhs - rhs) < eps;
}

int fail(const std::string& message) {
    std::cerr << "lod_batching_tests failed: " << message << '\n';
    return 1;
}

// ---------------------------------------------------------------------------
// resolve_instance_lod
// ---------------------------------------------------------------------------

int test_lod_near_camera() {
    // At distance 0, should select LOD0 (High)
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    // Instance at origin
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    float cam[3] = {0.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::High) {
        return fail("near camera should select LOD0 (High)");
    }
    return 0;
}

int test_lod_medium_distance() {
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    // Camera at (15, 0, 0) => dist_sq = 225 > 144 (LOD1 threshold)
    float cam[3] = {15.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::Medium) {
        return fail("distance 15 should select LOD1 (Medium)");
    }
    return 0;
}

int test_lod_far_distance() {
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    // Camera at (40, 0, 0) => dist_sq = 1600 > 900 (LOD2 threshold)
    float cam[3] = {40.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::Low) {
        return fail("distance 40 should select LOD2 (Low)");
    }
    return 0;
}

int test_lod_fallback_when_lod2_missing() {
    // Only LOD0 and LOD1 have meshes; LOD2 is empty.
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    // lod_models[2] is default-constructed with empty meshes
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    float cam[3] = {40.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::Medium) {
        return fail("should fall back to LOD1 when LOD2 is missing");
    }
    return 0;
}

int test_lod_fallback_when_all_missing() {
    // Only LOD0 has meshes
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    float cam[3] = {40.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::High) {
        return fail("should fall back all the way to LOD0 when higher LODs are missing");
    }
    return 0;
}

int test_lod_y_axis_separation() {
    // Distance on Y axis alone should also trigger LOD switching.
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 50.0F;  // 50 units above camera
    instance.model_matrix[14] = 0.0F;

    float cam[3] = {0.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam);
    if (lod != ae::render::LodLevel::Low) {
        return fail("50 units vertical distance should select LOD2");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// batch_level_draw_calls
// ---------------------------------------------------------------------------

int test_batch_sort_by_mesh() {
    // Three instances, all at the same position, with different meshes in LOD0.
    // The batching function should sort them by mesh GPU handles (vbo_positions
    // combined with ibo_indices), so draw calls for the same mesh are consecutive.
    ae::render::LevelRenderInstance a, b, c;
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);
    c.lod_models[0].meshes.resize(1);

    // Give each instance a unique mesh handle.
    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices  = {10};
    b.lod_models[0].meshes[0].vbo_positions = {2};
    b.lod_models[0].meshes[0].ibo_indices  = {20};
    c.lod_models[0].meshes[0].vbo_positions = {1};
    c.lod_models[0].meshes[0].ibo_indices  = {10};

    // Position at origin, camera at origin -> all LOD0
    a.model_matrix[12] = 0.0F; a.model_matrix[13] = 0.0F; a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F; b.model_matrix[13] = 0.0F; b.model_matrix[14] = 0.0F;
    c.model_matrix[12] = 0.0F; c.model_matrix[13] = 0.0F; c.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(a));
    instances.push_back(std::move(b));
    instances.push_back(std::move(c));

    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);

    if (calls.size() != 3) {
        return fail("expected 3 batched draw calls for 3 instances");
    }

    // The sort should group same-mesh calls together.  a and c share mesh
    // (vbo=1, ibo=10), so they should appear consecutively in the output.
    // Check that calls[0].mesh matches calls[2].mesh (both a and c's mesh)
    // or they appear consecutively somewhere in the list.
    bool found_run = false;
    for (std::size_t i = 1; i < calls.size(); ++i) {
        if (calls[i].mesh->vbo_positions.id == calls[i-1].mesh->vbo_positions.id &&
            calls[i].mesh->ibo_indices.id == calls[i-1].mesh->ibo_indices.id) {
            found_run = true;
            break;
        }
    }
    if (!found_run) {
        return fail("same-mesh calls should be consecutive after batching sort");
    }
    return 0;
}

int test_batch_lod_selection() {
    // Instance far from camera should use LOD2 with fewer meshes.
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(3);  // 3 meshes at LOD0
    instance.lod_models[1].meshes.resize(2);  // 2 meshes at LOD1
    instance.lod_models[2].meshes.resize(1);  // 1 mesh at LOD2
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(instance));

    // Camera at 10 units -> LOD0 (dist_sq = 100 < 144)
    float cam_near[3] = {10.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> near_calls =
        ae::render::batch_level_draw_calls(instances, cam_near);
    if (near_calls.size() != 3) {
        return fail("near camera should produce 3 draw calls (LOD0)");
    }

    // Move camera to 20 units -> LOD1 (dist_sq = 400 > 144, < 900)
    float cam_mid[3] = {20.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> mid_calls =
        ae::render::batch_level_draw_calls(instances, cam_mid);
    if (mid_calls.size() != 2) {
        return fail("medium distance should produce 2 draw calls (LOD1)");
    }

    // Move camera to 40 units -> LOD2 (dist_sq = 1600 > 900)
    float cam_far[3] = {40.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> far_calls =
        ae::render::batch_level_draw_calls(instances, cam_far);
    if (far_calls.size() != 1) {
        return fail("far distance should produce 1 draw call (LOD2)");
    }
    return 0;
}

}  // namespace

int main() {
    if (int rc = test_lod_near_camera(); rc != 0) return rc;
    if (int rc = test_lod_medium_distance(); rc != 0) return rc;
    if (int rc = test_lod_far_distance(); rc != 0) return rc;
    if (int rc = test_lod_fallback_when_lod2_missing(); rc != 0) return rc;
    if (int rc = test_lod_fallback_when_all_missing(); rc != 0) return rc;
    if (int rc = test_lod_y_axis_separation(); rc != 0) return rc;
    if (int rc = test_batch_sort_by_mesh(); rc != 0) return rc;
    if (int rc = test_batch_lod_selection(); rc != 0) return rc;
    std::cout << "lod_batching_tests passed\n";
    return 0;
}
