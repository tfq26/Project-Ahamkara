#include "ae/render/reflection_probe.h"

#include <cmath>
#include <iostream>
#include <cstring>

namespace {

bool near_eq(float lhs, float rhs, float eps = 0.0001F) {
    return std::fabs(lhs - rhs) < eps;
}

int fail(const std::string& message) {
    std::cerr << "ibl_tests failed: " << message << '\n';
    return 1;
}

// ── Reflection probe creation / insertion / query tests ─────────────────────

int test_probe_creation_and_insert() {
    ae::render::ReflectionProbeManager mgr;

    // Initially empty
    if (mgr.count() != 0)
        return fail("expected 0 probes initially");

    ae::render::ReflectionProbe p1;
    p1.position[0] = 10.0F;
    p1.position[1] = 5.0F;
    p1.position[2] = -3.0F;
    p1.influence_radius = 20.0F;
    p1.intensity = 1.0F;
    p1.cubemap = {42};

    std::size_t idx1 = mgr.add_probe(p1);
    if (idx1 != 0)
        return fail("first probe should get index 0");
    if (mgr.count() != 1)
        return fail("expected 1 probe after insert");

    ae::render::ReflectionProbe p2;
    p2.position[0] = -5.0F;
    p2.position[1] = 0.0F;
    p2.position[2] = 10.0F;
    p2.influence_radius = 30.0F;
    p2.intensity = 0.5F;

    std::size_t idx2 = mgr.add_probe(p2);
    if (idx2 != 1)
        return fail("second probe should get index 1");
    if (mgr.count() != 2)
        return fail("expected 2 probes after second insert");

    // Verify probe data
    const auto& r1 = mgr.get_probe(0);
    if (!near_eq(r1.position[0], 10.0F) || !near_eq(r1.position[1], 5.0F) ||
        !near_eq(r1.position[2], -3.0F)) {
        return fail("probe 0 position mismatch");
    }
    if (!near_eq(r1.influence_radius, 20.0F))
        return fail("probe 0 radius mismatch");
    if (r1.cubemap.id != 42U)
        return fail("probe 0 cubemap handle mismatch");

    const auto& r2 = mgr.get_probe(1);
    if (!near_eq(r2.position[0], -5.0F) || !near_eq(r2.position[1], 0.0F) ||
        !near_eq(r2.position[2], 10.0F)) {
        return fail("probe 1 position mismatch");
    }
    if (!near_eq(r2.intensity, 0.5F))
        return fail("probe 1 intensity mismatch");

    return 0;
}

int test_probe_find_near() {
    ae::render::ReflectionProbeManager mgr;

    ae::render::ReflectionProbe p;
    p.position[0] = 0.0F;
    p.position[1] = 0.0F;
    p.position[2] = 0.0F;
    p.influence_radius = 10.0F;
    mgr.add_probe(p);

    // Inside AABB
    float pos1[3] = {5.0F, 3.0F, -4.0F};
    auto res1 = mgr.find_probes_near(pos1);
    if (res1.size() != 1 || res1[0] != 0)
        return fail("position inside probe AABB should find it");

    // On the surface (within radius)
    float pos2[3] = {10.0F, 0.0F, 0.0F};
    auto res2 = mgr.find_probes_near(pos2);
    if (res2.size() != 1 || res2[0] != 0)
        return fail("position on AABB surface should find it");

    // Outside AABB
    float pos3[3] = {10.1F, 0.0F, 0.0F};
    auto res3 = mgr.find_probes_near(pos3);
    if (!res3.empty())
        return fail("position outside AABB should not find the probe");

    return 0;
}

int test_probe_aabb_overlap() {
    ae::render::ReflectionProbeManager mgr;

    // Probe centered at (0,0,0) with radius 10 → AABB [-10,10] in each axis
    ae::render::ReflectionProbe p;
    p.position[0] = 0.0F;
    p.position[1] = 0.0F;
    p.position[2] = 0.0F;
    p.influence_radius = 10.0F;
    mgr.add_probe(p);

    // Just inside on each axis
    float test_pts[][3] = {
        {9.999F, 0.0F, 0.0F},
        {0.0F, 9.999F, 0.0F},
        {0.0F, 0.0F, 9.999F},
        {-9.999F, 0.0F, 0.0F},
        {0.0F, -9.999F, 0.0F},
        {0.0F, 0.0F, -9.999F},
        {7.0F, 4.0F, -5.0F},
    };
    for (const auto& pt : test_pts) {
        auto r = mgr.find_probes_near(pt);
        if (r.size() != 1)
            return fail("point inside AABB should be found");
    }

    // Just outside
    float out_pts[][3] = {
        {10.001F, 0.0F, 0.0F},
        {0.0F, 10.001F, 0.0F},
        {0.0F, 0.0F, 10.001F},
        {100.0F, 0.0F, 0.0F},
    };
    for (const auto& pt : out_pts) {
        auto r = mgr.find_probes_near(pt);
        if (!r.empty())
            return fail("point outside AABB should not be found");
    }

    return 0;
}

int test_probe_clear() {
    ae::render::ReflectionProbeManager mgr;

    ae::render::ReflectionProbe p;
    p.position[0] = 1.0F;
    p.position[1] = 2.0F;
    p.position[2] = 3.0F;
    p.influence_radius = 5.0F;

    mgr.add_probe(p);
    mgr.add_probe(p);
    mgr.add_probe(p);
    if (mgr.count() != 3)
        return fail("expected 3 probes before clear");

    mgr.clear();
    if (mgr.count() != 0)
        return fail("expected 0 probes after clear");

    // Query after clear should be empty
    float pos[3] = {1.0F, 2.0F, 3.0F};
    auto res = mgr.find_probes_near(pos);
    if (!res.empty())
        return fail("query after clear should be empty");

    return 0;
}

int test_probe_remove() {
    ae::render::ReflectionProbeManager mgr;

    ae::render::ReflectionProbe p;
    p.position[0] = 0.0F;
    p.position[1] = 0.0F;
    p.position[2] = 0.0F;
    p.influence_radius = 10.0F;

    mgr.add_probe(p);
    mgr.add_probe(p);
    mgr.add_probe(p);
    if (mgr.count() != 3)
        return fail("expected 3 probes");

    mgr.remove_probe(1);
    if (mgr.count() != 2)
        return fail("expected 2 probes after remove");

    // Remaining probes should be indices 0 and 1 (was 0 and 2)
    const auto& r0 = mgr.get_probe(0);
    if (!near_eq(r0.position[0], 0.0F))
        return fail("first remaining probe wrong");
    const auto& r1 = mgr.get_probe(1);
    if (!near_eq(r1.position[0], 0.0F))
        return fail("second remaining probe wrong");

    return 0;
}

} // namespace

int main() {
    if (int rc = test_probe_creation_and_insert(); rc != 0)
        return rc;
    if (int rc = test_probe_find_near(); rc != 0)
        return rc;
    if (int rc = test_probe_aabb_overlap(); rc != 0)
        return rc;
    if (int rc = test_probe_clear(); rc != 0)
        return rc;
    if (int rc = test_probe_remove(); rc != 0)
        return rc;

    std::cout << "ibl_tests passed\n";
    return 0;
}
