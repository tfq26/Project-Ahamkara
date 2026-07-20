#include "ae/render/level_render.h"

#include <cmath>
#include <iostream>
#include <cstring>
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
// batch_level_draw_calls (existing)
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

// ---------------------------------------------------------------------------
// Configurable LOD distances (new)
// ---------------------------------------------------------------------------

int test_lod_configurable_distances() {
    // Use custom LOD settings with different transition distances.
    ae::render::LodSettings settings;
    settings.distances[0] = 20.0F;  // LOD0->LOD1 at 20 units
    settings.distances[1] = 50.0F;  // LOD1->LOD2 at 50 units
    settings.distances[2] = 200.0F; // LOD2->impostor at 200 units

    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    // At 15 units -> still LOD0 with custom thresholds (20 units)
    float cam_near[3] = {15.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam_near, settings);
    if (lod != ae::render::LodLevel::High) {
        return fail("15 units with custom threshold 20 should select LOD0");
    }

    // At 30 units -> LOD1 (between 20 and 50)
    float cam_mid[3] = {30.0F, 0.0F, 0.0F};
    lod = ae::render::resolve_instance_lod(instance, cam_mid, settings);
    if (lod != ae::render::LodLevel::Medium) {
        return fail("30 units with thresholds 20/50 should select LOD1");
    }

    // At 60 units -> LOD2 (beyond 50)
    float cam_far[3] = {60.0F, 0.0F, 0.0F};
    lod = ae::render::resolve_instance_lod(instance, cam_far, settings);
    if (lod != ae::render::LodLevel::Low) {
        return fail("60 units with threshold 50 should select LOD2");
    }

    return 0;
}

int test_lod_wide_range_same_lod() {
    // When distances[0] is very large, instance at moderate distance stays LOD0.
    ae::render::LodSettings settings;
    settings.distances[0] = 500.0F;
    settings.distances[1] = 1000.0F;

    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    float cam[3] = {100.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam, settings);
    if (lod != ae::render::LodLevel::High) {
        return fail("100 units with 500 threshold should stay LOD0");
    }
    return 0;
}

int test_lod_zero_distance() {
    // When distances[0] is 0, any positive distance triggers LOD1.
    ae::render::LodSettings settings;
    settings.distances[0] = 0.0F;
    settings.distances[1] = 10.0F;

    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1);
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    // At distance 1, already past 0 threshold.
    float cam[3] = {1.0F, 0.0F, 0.0F};
    ae::render::LodLevel lod = ae::render::resolve_instance_lod(instance, cam, settings);
    if (lod != ae::render::LodLevel::Medium) {
        return fail("1 unit with 0 threshold should select LOD1");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Impostor selection (new)
// ---------------------------------------------------------------------------

int test_impostor_beyond_billboard_distance() {
    // Instance beyond billboard distance with no LOD2 mesh -> impostor.
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    // No LOD2 meshes
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    ae::render::LodSettings settings;
    settings.billboard_distance = 50.0F;

    // Camera at 60 units -> beyond billboard distance, no LOD2 -> impostor
    float cam_far[3] = {60.0F, 0.0F, 0.0F};
    bool impostor = ae::render::use_impostor(instance, cam_far, settings);
    if (!impostor) {
        return fail("beyond billboard distance with no LOD2 should use impostor");
    }

    // Camera at 30 units -> within billboard distance, no impostor
    float cam_near[3] = {30.0F, 0.0F, 0.0F};
    impostor = ae::render::use_impostor(instance, cam_near, settings);
    if (impostor) {
        return fail("within billboard distance should not use impostor");
    }
    return 0;
}

int test_impostor_not_used_when_lod2_exists() {
    // Instance beyond billboard distance BUT has LOD2 mesh -> no impostor.
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    instance.lod_models[1].meshes.resize(1);
    instance.lod_models[2].meshes.resize(1); // LOD2 exists
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    ae::render::LodSettings settings;
    settings.billboard_distance = 50.0F;
    settings.distances[2] = 100.0F;

    float cam[3] = {60.0F, 0.0F, 0.0F};
    bool impostor = ae::render::use_impostor(instance, cam, settings);
    if (impostor) {
        return fail("beyond billboard distance but LOD2 exists should NOT use impostor");
    }
    return 0;
}

int test_impostor_batched_calls_excluded() {
    // When instances are beyond impostor distance, batch_level_draw_calls
    // should exclude them from the output.
    ae::render::LevelRenderInstance a, b;
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);
    // b has no LOD2 mesh and is far away
    a.lod_models[2].meshes.resize(1);
    // b.lod_models[2] stays empty

    a.model_matrix[12] = 0.0F;
    a.model_matrix[13] = 0.0F;
    a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F;
    b.model_matrix[13] = 0.0F;
    b.model_matrix[14] = 0.0F;

    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};
    b.lod_models[0].meshes[0].vbo_positions = {2};
    b.lod_models[0].meshes[0].ibo_indices = {20};

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(a));
    instances.push_back(std::move(b));

    ae::render::LodSettings settings;
    settings.billboard_distance = 50.0F;

    // Camera at 60 units -> b has no LOD2 -> impostor, but a has LOD2
    float cam[3] = {60.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam, settings);

    // a should still produce draws (it has LOD2), b should be excluded (impostor)
    if (calls.size() != 1) {
        return fail("expected 1 draw call (instance a with LOD2), b excluded as impostor");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Material-aware sorting (new)
// ---------------------------------------------------------------------------

int test_material_aware_sorting() {
    // Three instances with mesh mix:
    // a: mesh M1, material A
    // b: mesh M1, material B
    // c: mesh M2, material A
    // After material-aware sorting: M1 entries together (a,b), then M2 (c).
    // Within M1, material A then B (by material key).
    ae::render::LevelRenderInstance a, b, c;

    // a and b share mesh M1, c uses mesh M2
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);
    c.lod_models[0].meshes.resize(1);

    // Mesh M1: (vbo=1, ibo=10); Mesh M2: (vbo=2, ibo=20)
    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};
    b.lod_models[0].meshes[0].vbo_positions = {1};
    b.lod_models[0].meshes[0].ibo_indices = {10};
    c.lod_models[0].meshes[0].vbo_positions = {2};
    c.lod_models[0].meshes[0].ibo_indices = {20};

    // Material A: (albedo=1,0,0, metallic=0, roughness=1, no textures)
    a.albedo[0] = 1.0F;
    a.albedo[1] = 0.0F;
    a.albedo[2] = 0.0F;
    a.metallic = 0.0F;
    a.roughness = 1.0F;
    // Material B: (albedo=0,1,0, metallic=0.5, roughness=0.5, no textures)
    b.albedo[0] = 0.0F;
    b.albedo[1] = 1.0F;
    b.albedo[2] = 0.0F;
    b.metallic = 0.5F;
    b.roughness = 0.5F;
    // Material A again for c
    c.albedo[0] = 1.0F;
    c.albedo[1] = 0.0F;
    c.albedo[2] = 0.0F;
    c.metallic = 0.0F;
    c.roughness = 1.0F;

    a.model_matrix[12] = 0.0F;
    a.model_matrix[13] = 0.0F;
    a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F;
    b.model_matrix[13] = 0.0F;
    b.model_matrix[14] = 0.0F;
    c.model_matrix[12] = 0.0F;
    c.model_matrix[13] = 0.0F;
    c.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(a));
    instances.push_back(std::move(b));
    instances.push_back(std::move(c));

    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);

    if (calls.size() != 3) {
        return fail("expected 3 draw calls");
    }

    // First two calls should be mesh M1 (a and b)
    if (calls[0].mesh->vbo_positions.id != 1 || calls[0].mesh->ibo_indices.id != 10) {
        return fail("first call should be mesh M1");
    }
    if (calls[1].mesh->vbo_positions.id != 1 || calls[1].mesh->ibo_indices.id != 10) {
        return fail("second call should be mesh M1");
    }
    // Third call should be mesh M2
    if (calls[2].mesh->vbo_positions.id != 2 || calls[2].mesh->ibo_indices.id != 20) {
        return fail("third call should be mesh M2");
    }

    // Verify material-aware sorting via sorted_by_material
    if (!ae::render::sorted_by_material(calls)) {
        return fail("sorted_by_material should return true for correct material-aware sort");
    }
    return 0;
}

int test_sorted_by_material_false_when_interleaved() {
    // Manually create a bad ordering where same-mesh calls have different
    // materials interleaved.  sorted_by_material should return false.
    ae::render::LevelRenderInstance a, b;
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);

    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};
    b.lod_models[0].meshes[0].vbo_positions = {1};
    b.lod_models[0].meshes[0].ibo_indices = {10};

    // Different materials
    a.albedo[0] = 1.0F;
    b.albedo[0] = 0.0F;

    a.model_matrix[12] = 0.0F;
    a.model_matrix[13] = 0.0F;
    a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F;
    b.model_matrix[13] = 0.0F;
    b.model_matrix[14] = 0.0F;

    // Manually build a sorted list that IS sorted by mesh but NOT by material
    ae::render::LodBatchedCall call_a = {&a.lod_models[0].meshes[0], &a};
    ae::render::LodBatchedCall call_b = {&b.lod_models[0].meshes[0], &b};

    std::vector<ae::render::LodBatchedCall> calls;
    calls.push_back(call_a);
    calls.push_back(call_b);

    bool result = ae::render::sorted_by_material(calls);
    // a and b have different materials but same mesh -> should return false
    if (result) {
        return fail("sorted_by_material should return false when materials differ within mesh group");
    }
    return 0;
}

int test_sorted_by_material_single_entry() {
    // A single entry should trivially pass sorted_by_material.
    ae::render::LevelRenderInstance a;
    a.lod_models[0].meshes.resize(1);
    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};

    ae::render::LodBatchedCall call = {&a.lod_models[0].meshes[0], &a};
    std::vector<ae::render::LodBatchedCall> calls = {call};

    if (!ae::render::sorted_by_material(calls)) {
        return fail("sorted_by_material should return true for a single entry");
    }
    return 0;
}

int test_sorted_by_material_empty() {
    std::vector<ae::render::LodBatchedCall> calls;
    if (!ae::render::sorted_by_material(calls)) {
        return fail("sorted_by_material should return true for empty list");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Batch efficiency (new)
// ---------------------------------------------------------------------------

int test_batch_efficiency_perfect() {
    // When all same-mesh calls are consecutive -> efficiency = 1.0
    ae::render::LevelRenderInstance a, b, c;
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);
    c.lod_models[0].meshes.resize(1);

    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};
    b.lod_models[0].meshes[0].vbo_positions = {1};
    b.lod_models[0].meshes[0].ibo_indices = {10};
    c.lod_models[0].meshes[0].vbo_positions = {2};
    c.lod_models[0].meshes[0].ibo_indices = {20};

    a.model_matrix[12] = 0.0F;
    a.model_matrix[13] = 0.0F;
    a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F;
    b.model_matrix[13] = 0.0F;
    b.model_matrix[14] = 0.0F;
    c.model_matrix[12] = 0.0F;
    c.model_matrix[13] = 0.0F;
    c.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(a));
    instances.push_back(std::move(b));
    instances.push_back(std::move(c));

    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);

    // 2 runs: M1 (a+b), M2 (c). Both are batched runs (length > 1 for M1, =1 for M2).
    // batched_runs=1 (M1), run_count=2 -> efficiency = 0.5
    // Actually M2 has length 1 so it's not counted in batched_runs.
    // Efficiency = batched_runs / run_count = 1/2 = 0.5
    float efficiency = ae::render::batch_efficiency(calls);
    if (!near_eq(efficiency, 0.5F)) {
        return fail("expected efficiency 0.5 for 1 batched run out of 2 total runs");
    }
    return 0;
}

int test_batch_efficiency_no_batching() {
    // When all meshes are unique -> no batching -> efficiency = 0
    ae::render::LevelRenderInstance a, b;
    a.lod_models[0].meshes.resize(1);
    b.lod_models[0].meshes.resize(1);

    a.lod_models[0].meshes[0].vbo_positions = {1};
    a.lod_models[0].meshes[0].ibo_indices = {10};
    b.lod_models[0].meshes[0].vbo_positions = {2};
    b.lod_models[0].meshes[0].ibo_indices = {20};

    a.model_matrix[12] = 0.0F;
    a.model_matrix[13] = 0.0F;
    a.model_matrix[14] = 0.0F;
    b.model_matrix[12] = 0.0F;
    b.model_matrix[13] = 0.0F;
    b.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(a));
    instances.push_back(std::move(b));

    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);

    float efficiency = ae::render::batch_efficiency(calls);
    if (!near_eq(efficiency, 0.0F)) {
        return fail("expected efficiency 0 for all-unique meshes");
    }
    return 0;
}

int test_batch_efficiency_empty() {
    std::vector<ae::render::LodBatchedCall> calls;
    float efficiency = ae::render::batch_efficiency(calls);
    if (!near_eq(efficiency, 1.0F)) {
        return fail("expected efficiency 1 for empty list");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Empty / missing instances (new)
// ---------------------------------------------------------------------------

int test_empty_instances_list() {
    std::vector<ae::render::LevelRenderInstance> instances;
    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);
    if (!calls.empty()) {
        return fail("empty instances should produce empty batched calls");
    }
    return 0;
}

int test_missing_mesh_handles_in_lod() {
    // Instance with buffers that have id=0 (invalid handles) should still
    // produce draw calls (they sort by key 0).
    ae::render::LevelRenderInstance instance;
    instance.lod_models[0].meshes.resize(1);
    // vbo_positions and ibo_indices default to id=0
    instance.model_matrix[12] = 0.0F;
    instance.model_matrix[13] = 0.0F;
    instance.model_matrix[14] = 0.0F;

    std::vector<ae::render::LevelRenderInstance> instances;
    instances.push_back(std::move(instance));

    float cam[3] = {0.0F, 0.0F, 0.0F};
    std::vector<ae::render::LodBatchedCall> calls =
        ae::render::batch_level_draw_calls(instances, cam);

    if (calls.size() != 1) {
        return fail("instance with id=0 handles should still produce 1 draw call");
    }
    return 0;
}

int test_compute_material_key_consistency() {
    // Identical instances should produce identical material keys.
    ae::render::LevelRenderInstance a, b;
    a.albedo[0] = 0.2F;
    a.albedo[1] = 0.4F;
    a.albedo[2] = 0.6F;
    a.metallic = 0.3F;
    a.roughness = 0.7F;
    a.albedo_map = {42};
    a.normal_map = {43};
    a.orm_map = {44};
    a.emissive_map = {45};

    b.albedo[0] = 0.2F;
    b.albedo[1] = 0.4F;
    b.albedo[2] = 0.6F;
    b.metallic = 0.3F;
    b.roughness = 0.7F;
    b.albedo_map = {42};
    b.normal_map = {43};
    b.orm_map = {44};
    b.emissive_map = {45};

    std::uint64_t key_a = ae::render::compute_material_key(a);
    std::uint64_t key_b = ae::render::compute_material_key(b);
    if (key_a != key_b) {
        return fail("identical instances should produce identical material keys");
    }
    return 0;
}

}  // namespace

int main() {
    // Existing LOD tests
    if (int rc = test_lod_near_camera(); rc != 0) return rc;
    if (int rc = test_lod_medium_distance(); rc != 0) return rc;
    if (int rc = test_lod_far_distance(); rc != 0) return rc;
    if (int rc = test_lod_fallback_when_lod2_missing(); rc != 0) return rc;
    if (int rc = test_lod_fallback_when_all_missing(); rc != 0) return rc;
    if (int rc = test_lod_y_axis_separation(); rc != 0) return rc;

    // Existing batch tests
    if (int rc = test_batch_sort_by_mesh(); rc != 0) return rc;
    if (int rc = test_batch_lod_selection(); rc != 0) return rc;

    // Configurable LOD distances
    if (int rc = test_lod_configurable_distances(); rc != 0)
        return rc;
    if (int rc = test_lod_wide_range_same_lod(); rc != 0)
        return rc;
    if (int rc = test_lod_zero_distance(); rc != 0)
        return rc;

    // Impostor selection
    if (int rc = test_impostor_beyond_billboard_distance(); rc != 0)
        return rc;
    if (int rc = test_impostor_not_used_when_lod2_exists(); rc != 0)
        return rc;
    if (int rc = test_impostor_batched_calls_excluded(); rc != 0)
        return rc;

    // Material-aware sorting
    if (int rc = test_material_aware_sorting(); rc != 0)
        return rc;
    if (int rc = test_sorted_by_material_false_when_interleaved(); rc != 0)
        return rc;
    if (int rc = test_sorted_by_material_single_entry(); rc != 0)
        return rc;
    if (int rc = test_sorted_by_material_empty(); rc != 0)
        return rc;

    // Batch efficiency
    if (int rc = test_batch_efficiency_perfect(); rc != 0)
        return rc;
    if (int rc = test_batch_efficiency_no_batching(); rc != 0)
        return rc;
    if (int rc = test_batch_efficiency_empty(); rc != 0)
        return rc;

    // Empty / missing instances
    if (int rc = test_empty_instances_list(); rc != 0)
        return rc;
    if (int rc = test_missing_mesh_handles_in_lod(); rc != 0)
        return rc;
    if (int rc = test_compute_material_key_consistency(); rc != 0)
        return rc;

    std::cout << "lod_batching_tests passed\n";
    return 0;
}
