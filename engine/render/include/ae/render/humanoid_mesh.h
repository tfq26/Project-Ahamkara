#pragma once

#include "ae/render/gltf_loader.h"

namespace ae::render {

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
 */
[[nodiscard]] GltfModel generate_humanoid_mesh();

}  // namespace ae::render
