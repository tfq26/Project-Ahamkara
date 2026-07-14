#pragma once

#include "ae/skeleton/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace ae::animation {

/// Headless animation clip player.
/// Owns neutral skeleton clip/skin data and evaluates poses without a graphics
/// context or ae_render dependency.
class AnimationClipPlayer {
public:
    AnimationClipPlayer() = default;

    /// Install neutral skeleton data (skin + clips). Replaces any previous data.
    bool set_data(ae::skeleton::Skin skin,
                  std::vector<ae::skeleton::AnimationClipData> clips);

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
    [[nodiscard]] const ae::skeleton::Mat4* tick(float dt);

    /// Number of joints in the loaded skeleton.
    [[nodiscard]] int joint_count() const { return joint_count_; }

    /// Current animation time (seconds).
    [[nodiscard]] float current_time() const { return current_time_; }

    /// Currently playing clip name (empty if none).
    [[nodiscard]] std::string_view current_clip() const { return current_clip_name_; }

    /// Is a clip currently playing?
    [[nodiscard]] bool is_playing() const { return active_animation_ != nullptr; }

    [[nodiscard]] bool has_data() const { return joint_count_ > 0 && !clips_.empty(); }

private:
    ae::skeleton::Skin skin_{};
    std::vector<ae::skeleton::AnimationClipData> clips_{};
    std::vector<ae::skeleton::Mat4> joint_matrices_;

    const ae::skeleton::AnimationClipData* active_animation_{nullptr};
    std::string current_clip_name_;
    float current_time_{0.0f};
    float duration_{0.0f};
    bool paused_{false};
    bool loop_{true};
    int joint_count_{0};
};

}  // namespace ae::animation
