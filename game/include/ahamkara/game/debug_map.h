#pragma once

#include "ahamkara/game/movement.h"

#include <array>
#include <cstddef>

namespace ahamkara::game {

/**
 * @brief Axis-aligned box collider used for the debug obstacle course.
 */
struct ColliderBox {
    float min_x {};
    float min_z {};
    float max_x {};
    float max_z {};
    float top_y {};
    float bottom_y {0.0F};
    bool wall {false};
    bool jump_through {false};
    bool auto_step {true};
    SurfaceMaterial surface_material {SurfaceMaterial::Default};
};

/**
 * @brief Javelin-4 inspired collider set.
 *
 * Matches the visual geometry in the debug renderer:
 * central platform with ramps, connector bridges, spawn areas,
 * cover blocks, heavy alcoves, side routes, and boundary walls.
 */
inline constexpr std::size_t kDebugMapColliderCount = 42;

inline constexpr std::array<ColliderBox, kDebugMapColliderCount> kDebugMapColliders = {{
    // --- Central platform (4x4, 1.5m, requires jumping onto) ---
    {-4.0F, -4.0F, 4.0F, 4.0F, 1.5F, 0.0F, false, false, false},

    // --- Central pillar (wall, blocks movement through center) ---
    {-0.8F, -0.8F, 0.8F, 0.8F, 3.5F, 0.0F, true, false, false},

    // --- Ramps: auto_step so you can walk up them ---
    // North ramp (Z=4.0, X=-1.5 to 1.5)
    {-1.5F, 4.0F, 1.5F, 4.3F, 0.75F, 0.0F, false, false, true},
    {-1.5F, 4.0F, 1.5F, 4.6F, 1.5F, 0.0F, false, false, true},
    // South ramp
    {-1.5F, -4.6F, 1.5F, -4.0F, 0.75F, 0.0F, false, false, true},
    {-1.5F, -4.3F, 1.5F, -4.0F, 1.5F, 0.0F, false, false, true},
    // East ramp
    {4.0F, -1.5F, 4.6F, 1.5F, 0.75F, 0.0F, false, false, true},
    {4.0F, -1.5F, 4.3F, 1.5F, 1.5F, 0.0F, false, false, true},
    // West ramp
    {-4.6F, -1.5F, -4.0F, 1.5F, 0.75F, 0.0F, false, false, true},
    {-4.3F, -1.5F, -4.0F, 1.5F, 1.5F, 0.0F, false, false, true},

    // --- Platform cover pillars (walls) ---
    {2.0F, 0.6F, 3.2F, 1.4F, 2.2F, 0.0F, true, false, false},
    {-3.2F, 0.6F, -2.0F, 1.4F, 2.2F, 0.0F, true, false, false},
    {2.0F, -1.4F, 3.2F, -0.6F, 2.2F, 0.0F, true, false, false},
    {-3.2F, -1.4F, -2.0F, -0.6F, 2.2F, 0.0F, true, false, false},

    // --- Connector bridges (jump-through — can jump up onto them) ---
    // NE bridge: from (5,5) to (9,9)
    {5.0F, 5.0F, 9.0F, 9.0F, 1.15F, 0.0F, false, true, false},
    // NW bridge: from (-9,5) to (-5,9)
    {-9.0F, 5.0F, -5.0F, 9.0F, 1.15F, 0.0F, false, true, false},
    // SE bridge: from (5,-9) to (9,-5)
    {5.0F, -9.0F, 9.0F, -5.0F, 1.15F, 0.0F, false, true, false},
    // SW bridge: from (-9,-9) to (-5,-5)
    {-9.0F, -9.0F, -5.0F, -5.0F, 1.15F, 0.0F, false, true, false},

    // --- Spawn areas ---
    {-13.0F, -3.0F, -10.0F, 3.0F, 0.3F, 0.0F, false, false, true},
    {10.0F, -3.0F, 13.0F, 3.0F, 0.3F, 0.0F, false, false, true},
    // Spawn cover (walls)
    {-13.0F, 0.0F, -11.0F, 1.5F, 1.5F, 0.0F, true, false, false},
    {-13.0F, -1.5F, -11.0F, 0.0F, 1.5F, 0.0F, true, false, false},
    {11.0F, 0.0F, 13.0F, 1.5F, 1.5F, 0.0F, true, false, false},
    {11.0F, -1.5F, 13.0F, 0.0F, 1.5F, 0.0F, true, false, false},

    // --- Heavy alcoves ---
    {-2.0F, 8.0F, 2.0F, 9.5F, 0.3F, 0.0F, false, false, true},
    {-2.0F, -9.5F, 2.0F, -8.0F, 0.3F, 0.0F, false, false, true},

    // --- Side route platforms ---
    {5.0F, 6.0F, 7.0F, 8.0F, 1.0F, 0.0F, false, true, false},
    {-7.0F, 6.0F, -5.0F, 8.0F, 1.0F, 0.0F, false, true, false},
    {5.0F, -8.0F, 7.0F, -6.0F, 1.0F, 0.0F, false, true, false},
    {-7.0F, -8.0F, -5.0F, -6.0F, 1.0F, 0.0F, false, true, false},

    // --- Scattered cover blocks on outer ring (walls) ---
    {5.5F, 2.5F, 6.5F, 3.5F, 0.9F, 0.0F, true, false, false},
    {-6.5F, 2.5F, -5.5F, 3.5F, 0.9F, 0.0F, true, false, false},
    {5.5F, -3.5F, 6.5F, -2.5F, 0.9F, 0.0F, true, false, false},
    {-6.5F, -3.5F, -5.5F, -2.5F, 0.9F, 0.0F, true, false, false},
    {2.5F, 5.5F, 3.5F, 6.5F, 0.9F, 0.0F, true, false, false},
    {-3.5F, 5.5F, -2.5F, 6.5F, 0.9F, 0.0F, true, false, false},
    {2.5F, -6.5F, 3.5F, -5.5F, 0.9F, 0.0F, true, false, false},
    {-3.5F, -6.5F, -2.5F, -5.5F, 0.9F, 0.0F, true, false, false},

    // --- Low boundary walls (solid outermost layer) ---
    {-14.0F, -14.2F, 14.0F, -13.8F, 0.4F, 0.0F, true, false, false},
    {-14.0F, 13.8F, 14.0F, 14.2F, 0.4F, 0.0F, true, false, false},
    {-14.2F, -14.0F, -13.8F, 14.0F, 0.4F, 0.0F, true, false, false},
    {13.8F, -14.0F, 14.2F, 14.0F, 0.4F, 0.0F, true, false, false},
}};

}  // namespace ahamkara::game
