#pragma once

#include <array>

namespace ae::render {

/**
 * @brief A plane in Hessian normal form: dot(normal, point) + distance = 0.
 *
 * The normal points inward (toward the visible half-space).
 * A point is inside the frustum if dot(normal, point) + distance >= 0
 * for all six planes.
 */
struct FrustumPlane {
    float nx {0.0F};
    float ny {0.0F};
    float nz {0.0F};
    float d  {0.0F};
};

/**
 * @brief An axis-aligned bounding box.
 */
struct AABB {
    float min_x {0.0F};
    float min_y {0.0F};
    float min_z {0.0F};
    float max_x {0.0F};
    float max_y {0.0F};
    float max_z {0.0F};
};

/**
 * @brief A six-plane view frustum extracted from a model-view-projection matrix.
 *
 * Planes are indexed as: Left=0, Right=1, Bottom=2, Top=3, Near=4, Far=5.
 */
struct Frustum {
    std::array<FrustumPlane, 6> planes;

    static constexpr int kLeft   = 0;
    static constexpr int kRight  = 1;
    static constexpr int kBottom = 2;
    static constexpr int kTop    = 3;
    static constexpr int kNear   = 4;
    static constexpr int kFar    = 5;

    /**
     * @brief Extract frustum planes from a column-major 4x4 MVP matrix (16 floats).
     *
     * The input is in OpenGL column-major order: m[col*4 + row].
     */
    static Frustum from_matrix(const float* mvp);

    /**
     * @brief Test if an AABB is at least partially inside the frustum.
     * @return true if the AABB is visible (intersects or is inside).
     */
    [[nodiscard]] bool intersects_aabb(const AABB& box) const;

    /**
     * @brief Test if a sphere is at least partially inside the frustum.
     * @param cx,cy,cz  Sphere center.
     * @param radius     Sphere radius.
     * @return true if visible.
     */
    [[nodiscard]] bool intersects_sphere(float cx, float cy, float cz, float radius) const;
};

/**
 * @brief Render statistics for profiling and debug overlay.
 */
struct RenderStats {
    int total_dummies {0};
    int culled_dummies {0};
    int drawn_dummies {0};

    int total_decal_count {0};
    int drawn_decal_count {0};

    int total_particle_count {0};
    int drawn_particle_count {0};

    int total_projectiles {0};
    int drawn_projectiles {0};

    int map_cells_visible {0};
    int map_cells_total {0};

    int lod0_count {0};   // full detail
    int lod1_count {0};   // medium detail
    int lod2_count {0};   // low detail

    void reset() {
        total_dummies = 0;
        culled_dummies = 0;
        drawn_dummies = 0;
        total_decal_count = 0;
        drawn_decal_count = 0;
        total_particle_count = 0;
        drawn_particle_count = 0;
        total_projectiles = 0;
        drawn_projectiles = 0;
        map_cells_visible = 0;
        map_cells_total = 0;
        lod0_count = 0;
        lod1_count = 0;
        lod2_count = 0;
    }
};

/** LOD level for skinned meshes. */
enum class LodLevel : int {
    High   = 0,  // full detail
    Medium = 1,  // reduced detail
    Low    = 2,  // minimal
};

/**
 * @brief Select an LOD level based on distance from camera.
 *
 * Thresholds are tuned for the procedural humanoid mesh (~1 unit tall).
 * @param distance_sq  Squared distance from camera to object center.
 * @return appropriate LOD level.
 */
[[nodiscard]] LodLevel select_lod(float distance_sq);

} // namespace ae::render
