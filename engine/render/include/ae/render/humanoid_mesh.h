#pragma once

#include "ae/render/gltf_loader.h"

namespace ae::render {

/**
 * @brief LOD level specifier for mesh generation.
 */
enum class HumanoidLod {
    High   = 0,  // Full detail: 7 parts (head, torso, hips, 2 arms, 2 legs, 2 feet)
    Medium = 1,  // Medium: 4 parts (head+torso, hips+legs, 2 arms)
    Low    = 2   // Minimal: single capsule-like box
};

/**
 * @brief Generates a simple procedural humanoid mesh.
 *
 * Produces a low-poly humanoid figure consisting of:
 *   - Torso (tapered box)
 *   - Head (small box)
 *   - Two arms (thin boxes)
 *   - Two legs (thin boxes)
 *
 * The mesh is centered at origin with feet at y=0.
 * Total height is roughly 1.0 units (scale by player_height in the renderer).
 *
 * This serves as a built-in placeholder that requires no external asset files.
 *
 * @param lod  Detail level for the generated mesh.
 */
[[nodiscard]] GltfModel generate_humanoid_mesh(HumanoidLod lod = HumanoidLod::High);

}  // namespace ae::render
