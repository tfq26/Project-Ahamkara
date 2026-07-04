#include "ae/animation/clip_player.h"

#include "ae/core/log.h"
#include "ae/render/compiled_mesh.h"
#include "ae/render/skeletal_animation.h"

#include <algorithm>
#include <cmath>
#include <string>

#define AE_LOG_CATEGORY "Animation"

namespace ae::animation {

bool AnimationClipPlayer::load(const std::string& path) {
    ae::render::CompiledMeshLoader loader;
    ae::render::GltfModel model;
    if (!loader.load(path, model)) {
        ae::log_warning_cat(AE_LOG_CATEGORY, "Failed to load animation model: " + path + " — " + loader.last_error());
        return false;
    }

    model_ = std::move(model);
    joint_count_ = 0;

    // Determine joint count from the primary skin
    if (!model_.skins.empty()) {
        const auto& skin = model_.skins[0];
        joint_count_ = static_cast<int>(skin.joints.size());
        joint_matrices_.resize(static_cast<std::size_t>(joint_count_));
        for (auto& m : joint_matrices_) m = ae::render::Mat4::identity();
    }

    ae::log_info_cat(AE_LOG_CATEGORY, "Loaded animation model: " + path + " (" +
                      std::to_string(model_.meshes.size()) + " meshes, " +
                      std::to_string(model_.skins.size()) + " skins, " +
                      std::to_string(model_.animations.size()) + " animations, " +
                      std::to_string(joint_count_) + " joints)");

    for (const auto& anim : model_.animations) {
        ae::log_debug_cat(AE_LOG_CATEGORY, "  clip: " + anim.name + " (" +
                          std::to_string(anim.channels.size()) + " channels, " +
                          std::to_string(anim.samplers.size()) + " samplers)");
    }

    return true;
}

void AnimationClipPlayer::play(std::string_view name, bool loop) {
    active_animation_ = nullptr;
    current_clip_name_ = {};
    current_time_ = 0.0f;
    duration_ = 0.0f;

    if (model_.skins.empty()) return;

    for (const auto& anim : model_.animations) {
        if (anim.name == name) {
            active_animation_ = &anim;
            current_clip_name_ = anim.name;

            // Compute animation duration from the longest sampler
            for (const auto& sampler : anim.samplers) {
                if (!sampler.input_times.empty()) {
                    float last = sampler.input_times.back();
                    if (last > duration_) duration_ = last;
                }
            }
            break;
        }
    }

    if (active_animation_ && loop && duration_ > 0.0f) {
        // Looping is handled in tick()
    }

    (void)loop; // loop is always true for now
    paused_ = false;
}

void AnimationClipPlayer::stop() {
    active_animation_ = nullptr;
    current_clip_name_ = {};
    current_time_ = 0.0f;
}

void AnimationClipPlayer::pause(bool p) {
    paused_ = p;
}

const ae::render::Mat4* AnimationClipPlayer::tick(float dt) {
    if (!active_animation_ || model_.skins.empty() || joint_count_ <= 0) return nullptr;
    if (paused_) return joint_matrices_.data();

    const auto& skin = model_.skins[0];
    current_time_ += dt;

    // Loop if we've passed the duration
    if (duration_ > 0.0f && current_time_ >= duration_) {
        current_time_ = std::fmod(current_time_, duration_);
    }

    ae::render::evaluate_animation(*active_animation_, skin, current_time_, joint_matrices_);
    return joint_matrices_.data();
}

}  // namespace ae::animation
