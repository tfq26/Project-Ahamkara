#pragma once

#include "ae/render/compiled_mesh.h"
#include "ae/render/gltf_loader.h"
#include "ae/render/skeletal_animation.h"

#include <string>
#include <string_view>
#include <vector>

namespace ae::animation {

/// Simple animation clip player.  Loads a compiled .aemesh (which contains
/// mesh + skeleton + animation data), then plays named clips on demand.
/// Outputs joint matrices suitable for the GPU skinning pipeline.
class AnimationClipPlayer {
public:
    AnimationClipPlayer() = default;

    /// Load a compiled mesh (must contain skin + animation data).
    bool load(const std::string& path);

    /// Start playing a named animation clip.
    void play(std::string_view name, bool loop = true);

    /// Stop playback.
    void stop();

    /// Pause/resume.
    void pause(bool p);
    [[nodiscard]] bool is_paused() const { return paused_; }

    /// Advance time and evaluate the current clip. Call once per frame.
    /// Returns joint matrices as Mat4 array (size = joint_count).
    /// Returns nullptr if no clip is active or no data loaded.
    [[nodiscard]] const ae::render::Mat4* tick(float dt);

    /// Number of joints in the loaded skeleton.
    [[nodiscard]] int joint_count() const { return joint_count_; }

    /// Current animation time (seconds).
    [[nodiscard]] float current_time() const { return current_time_; }

    /// Currently playing clip name (empty if none).
    [[nodiscard]] std::string_view current_clip() const { return current_clip_name_; }

    /// Is a clip currently playing?
    [[nodiscard]] bool is_playing() const { return active_animation_ != nullptr; }

private:
    ae::render::GltfModel model_;
    std::vector<ae::render::Mat4> joint_matrices_;

    const ae::render::GltfAnimation* active_animation_{nullptr};
    std::string current_clip_name_;
    float current_time_{0.0f};
    float duration_{0.0f};
    bool paused_{false};
    int joint_count_{0};
};

}  // namespace ae::animation
