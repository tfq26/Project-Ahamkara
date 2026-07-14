#include "ae/animation/clip_player.h"

#include "ae/core/log.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ae::animation {

namespace {
float clip_duration_seconds(const ae::skeleton::AnimationClipData& clip) {
    float duration = 0.0F;
    for (const auto& sampler : clip.samplers) {
        if (!sampler.input_times.empty()) {
            duration = std::max(duration, sampler.input_times.back());
        }
    }
    return duration;
}
} // namespace

bool AnimationClipPlayer::set_data(ae::skeleton::Skin skin,
                                   std::vector<ae::skeleton::AnimationClipData> clips) {
    stop();
    skin_ = std::move(skin);
    clips_ = std::move(clips);
    joint_count_ = static_cast<int>(skin_.joints.size());
    joint_matrices_.assign(static_cast<size_t>(std::max(joint_count_, 0)),
                           ae::skeleton::Mat4::identity());
    ae::log_info_cat("animation",
                     "Clip player data set: joints=" + std::to_string(joint_count_) +
                         " clips=" + std::to_string(clips_.size()));
    return joint_count_ > 0;
}

void AnimationClipPlayer::play(std::string_view name, bool loop) {
    active_animation_ = nullptr;
    current_clip_name_.clear();
    current_time_ = 0.0F;
    duration_ = 0.0F;
    loop_ = loop;
    paused_ = false;

    for (const auto& clip : clips_) {
        if (clip.name == name) {
            active_animation_ = &clip;
            current_clip_name_ = clip.name;
            duration_ = clip_duration_seconds(clip);
            ae::log_info_cat("animation",
                             "Playing clip \"" + current_clip_name_ + "\" duration=" +
                                 std::to_string(duration_) + " loop=" + (loop_ ? "1" : "0"));
            return;
        }
    }
    ae::log_warning_cat("animation", "Clip not found: " + std::string(name));
}

void AnimationClipPlayer::stop() {
    active_animation_ = nullptr;
    current_clip_name_.clear();
    current_time_ = 0.0F;
    duration_ = 0.0F;
    paused_ = false;
}

void AnimationClipPlayer::pause(bool p) {
    paused_ = p;
}

const ae::skeleton::Mat4* AnimationClipPlayer::tick(float dt) {
    if (active_animation_ == nullptr || joint_count_ <= 0) {
        return nullptr;
    }
    if (!paused_ && dt > 0.0F) {
        current_time_ += dt;
        if (duration_ > 0.0F) {
            if (loop_) {
                current_time_ = std::fmod(current_time_, duration_);
                if (current_time_ < 0.0F) {
                    current_time_ += duration_;
                }
            } else if (current_time_ > duration_) {
                current_time_ = duration_;
            }
        }
    }

    ae::skeleton::evaluate_animation(*active_animation_, skin_, current_time_, joint_matrices_);
    return joint_matrices_.data();
}

} // namespace ae::animation
