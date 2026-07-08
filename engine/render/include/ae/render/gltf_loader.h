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
    std::vector<float> uvs;         // interleaved uv (TEXCOORD_0); empty if absent
    std::vector<float> joint_indices; // 4 indices per vertex
    std::vector<float> joint_weights; // 4 weights per vertex
    std::vector<std::uint32_t> indices;
    float color_r {1.0F};           // base color (used when no material)
    float color_g {1.0F};
    float color_b {1.0F};
    bool has_material_color {false};
};

/**
 * @brief A single joint in a skin, with inverse bind matrix.
 *
 * The inverse_bind_matrix is stored as 16 floats in column-major order,
 * matching the glTF 4x4 matrix convention (element [col*4+row]).
 */
struct GltfJoint {
    std::string name;
    int node_index {-1};   // glTF node index in the scene graph
    int parent_index {-1};  // -1 = root joint
    std::vector<float> inverse_bind_matrix; // 16 floats (column-major 4x4)
};

/**
 * @brief A skin associating joints with mesh geometry.
 */
struct GltfSkin {
    std::vector<GltfJoint> joints;
};

/**
 * @brief A channel maps a sampler to a node property for animation.
 */
struct GltfAnimationChannel {
    int node_index {-1};
    std::string path; // "translation", "rotation", "scale"
    int sampler_index {-1};
};

/**
 * @brief A sampler holds keyframe data for an animation property.
 *
 * For translation: output_values has 3 floats per keyframe.
 * For rotation: output_values has 4 floats per keyframe (quaternion).
 * For scale: output_values has 3 floats per keyframe.
 */
struct GltfAnimationSampler {
    std::vector<float> input_times;
    std::vector<float> output_values;
    std::string interpolation; // "LINEAR" or "STEP"
};

/**
 * @brief A named animation consisting of channels and samplers.
 */
struct GltfAnimation {
    std::string name;
    std::vector<GltfAnimationChannel> channels;
    std::vector<GltfAnimationSampler> samplers;
};

/**
 * @brief A complete model made of one or more meshes, with optional
 *        skins and animations loaded from glTF.
 */
struct GltfModel {
    std::vector<GltfMesh> meshes;
    std::vector<GltfSkin> skins;
    std::vector<GltfAnimation> animations;
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
