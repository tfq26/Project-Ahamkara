#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ae::render {

/**
 * @brief A single mesh primitive loaded from a glTF file or generated procedurally.
 *
 * Positions and normals are interleaved as [x,y,z, x,y,z, ...].
 * Indices are triangle indices (3 per triangle).
 */
struct GltfMesh {
    std::vector<float> positions;   // interleaved xyz
    std::vector<float> normals;     // interleaved xyz
    std::vector<std::uint32_t> indices;
    float color_r {1.0F};           // base color (used when no material)
    float color_g {1.0F};
    float color_b {1.0F};
    bool has_material_color {false};
};

/**
 * @brief A complete model made of one or more meshes.
 */
struct GltfModel {
    std::vector<GltfMesh> meshes;
};

/**
 * @brief Loads a glTF 2.0 model from a JSON file + optional external .bin buffer.
 *
 * This is a minimal loader supporting:
 *   - glTF 2.0 JSON with external .bin buffers
 *   - Mesh primitives with POSITION, NORMAL attributes
 *   - Triangle indices
 *   - Single node/scene hierarchy (flattened)
 *
 * It does NOT support: animations, skinning, materials, textures,
 * multiple scenes, sparse accessors, or embedded base64 buffers.
 */
class GltfLoader {
public:
    GltfLoader() = default;

    /**
     * @brief Load a glTF model from file.
     * @param gltf_path Path to the .gltf JSON file.
     * @param model Output model data.
     * @return true on success.
     */
    [[nodiscard]] bool load(const std::string& gltf_path, GltfModel& model);

    /**
     * @brief Load a glTF model from an in-memory JSON string.
     *        All buffer URIs are resolved relative to base_path.
     */
    [[nodiscard]] bool load_from_string(const std::string& json, const std::string& base_path, GltfModel& model);

    /**
     * @brief Get the last error message.
     */
    [[nodiscard]] const std::string& last_error() const { return error_; }

private:
    std::string error_;
};

}  // namespace ae::render
