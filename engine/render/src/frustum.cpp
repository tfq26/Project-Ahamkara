#include "ae/render/frustum.h"

#include <algorithm>
#include <cmath>

namespace ae::render {

// ============================================================
// Frustum extraction
// ============================================================

Frustum Frustum::from_matrix(const float* mvp) {
    Frustum f;

    // Extract frustum planes from the combined model-view-projection matrix.
    // Each plane is a linear combination of rows/columns of the 4x4 matrix.
    // Convention: the MVP transforms world-space points to clip space.
    // In OpenGL column-major, mvp[col*4 + row].
    // A point (x,y,z,1) in world space becomes (xc,yc,zc,wc) in clip space.
    // The frustum planes in world space are:
    //   Left:   xc + wc >= 0  →  row3 + row0
    //   Right:  wc - xc >= 0  →  row3 - row0
    //   Bottom: yc + wc >= 0  →  row3 + row1
    //   Top:    wc - yc >= 0  →  row3 - row1
    //   Near:   zc + wc >= 0  →  row3 + row2
    //   Far:    wc - zc >= 0  →  row3 - row2

    // Row i = elements at mvp[0*4+i], mvp[1*4+i], mvp[2*4+i], mvp[3*4+i]

    auto extract_plane = [&](int idx, float sign0, float sign1, float sign2, float sign3) {
        FrustumPlane& p = f.planes[static_cast<std::size_t>(idx)];
        // Plane: sign0*row0 + sign1*row1 + sign2*row2 + sign3*row3
        p.nx = sign0 * mvp[0]  + sign1 * mvp[1]  + sign2 * mvp[2]  + sign3 * mvp[3];
        p.ny = sign0 * mvp[4]  + sign1 * mvp[5]  + sign2 * mvp[6]  + sign3 * mvp[7];
        p.nz = sign0 * mvp[8]  + sign1 * mvp[9]  + sign2 * mvp[10] + sign3 * mvp[11];
        p.d  = sign0 * mvp[12] + sign1 * mvp[13] + sign2 * mvp[14] + sign3 * mvp[15];

        // Normalize the plane equation
        float len = std::sqrt(p.nx * p.nx + p.ny * p.ny + p.nz * p.nz);
        if (len > 0.0F) {
            p.nx /= len;
            p.ny /= len;
            p.nz /= len;
            p.d  /= len;
        }
    };

    // Left:   row3 + row0
    extract_plane(kLeft,   1.0F, 0.0F, 0.0F, 1.0F);
    // Right:  row3 - row0
    extract_plane(kRight, -1.0F, 0.0F, 0.0F, 1.0F);
    // Bottom: row3 + row1
    extract_plane(kBottom, 0.0F, 1.0F, 0.0F, 1.0F);
    // Top:    row3 - row1
    extract_plane(kTop,    0.0F, -1.0F, 0.0F, 1.0F);
    // Near:   row3 + row2
    extract_plane(kNear,   0.0F, 0.0F, 1.0F, 1.0F);
    // Far:    row3 - row2
    extract_plane(kFar,    0.0F, 0.0F, -1.0F, 1.0F);

    return f;
}

// ============================================================
// AABB intersection test
// ============================================================

bool Frustum::intersects_aabb(const AABB& box) const {
    // For each plane, find the "positive vertex" (p-vertex) — the corner of
    // the AABB that is furthest in the direction of the plane normal.
    // If the plane is satisfied for the p-vertex, the AABB is on the
    // visible side. If it fails, the AABB is entirely outside.
    for (const auto& p : planes) {
        // Determine p-vertex: choose max corner for positive normal components,
        // min corner for negative normal components.
        float px = (p.nx >= 0.0F) ? box.max_x : box.min_x;
        float py = (p.ny >= 0.0F) ? box.max_y : box.min_y;
        float pz = (p.nz >= 0.0F) ? box.max_z : box.min_z;

        // Test: dot(normal, p_vertex) + d >= 0 means inside
        if (p.nx * px + p.ny * py + p.nz * pz + p.d < 0.0F) {
            return false; // outside this plane
        }
    }
    return true; // inside or intersecting all planes
}

// ============================================================
// Sphere intersection test
// ============================================================

bool Frustum::intersects_sphere(float cx, float cy, float cz, float radius) const {
    for (const auto& p : planes) {
        float dist = p.nx * cx + p.ny * cy + p.nz * cz + p.d;
        if (dist < -radius) {
            return false;
        }
    }
    return true;
}

// ============================================================
// LOD selection
// ============================================================

LodLevel select_lod(float distance_sq) {
    // Thresholds in squared world units for a ~1-unit-tall humanoid mesh.
    // These scale naturally when the mesh is scaled by player_height.
    // At distance > 30 units → LOD2 (minimal)
    // At distance > 12 units → LOD1 (medium)
    // Closer              → LOD0 (full detail)
    constexpr float kLod2ThresholdSq = 30.0F * 30.0F;  // 900
    constexpr float kLod1ThresholdSq = 12.0F * 12.0F;  // 144

    if (distance_sq > kLod2ThresholdSq) {
        return LodLevel::Low;
    }
    if (distance_sq > kLod1ThresholdSq) {
        return LodLevel::Medium;
    }
    return LodLevel::High;
}

} // namespace ae::render
