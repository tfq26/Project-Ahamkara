#include "ae/core/log.h"
#include "ae/animation/state_machine.h"

#include <algorithm>


#define AE_LOG_CATEGORY "Animation"

namespace ae::animation {

void StateMachine::add_state(const AnimStateId& id, const std::string& clip_name,
                              float default_speed) {
    StateEntry entry;
    entry.state.id = id;
    entry.state.clip_name = clip_name;
    entry.state.default_speed = default_speed;
    entry.has_blend_1d = false;
    entry.has_blend_2d = false;
    states_[id] = std::move(entry);
}

void StateMachine::add_blend_1d_state(const AnimStateId& id, const BlendSpace1D& blend) {
    StateEntry entry;
    entry.state.id = id;
    entry.state.blend_1d = blend;
    entry.has_blend_1d = true;
    entry.has_blend_2d = false;
    states_[id] = std::move(entry);
}

void StateMachine::add_blend_2d_state(const AnimStateId& id, const BlendSpace2D& blend) {
    StateEntry entry;
    entry.state.id = id;
    entry.state.blend_2d = blend;
    entry.has_blend_1d = false;
    entry.has_blend_2d = true;
    states_[id] = std::move(entry);
}

void StateMachine::add_transition(const AnimStateId& from, const AnimStateId& to,
                                   const std::string& trigger,
                                   float blend_duration) {
    AnimationTransition t;
    t.from = from;
    t.to = to;
    t.trigger = trigger;
    t.blend_duration = blend_duration;
    t.has_exit_time = false;
    transitions_.push_back(t);
}

void StateMachine::add_exit_time_transition(const AnimStateId& from, const AnimStateId& to,
                                             float exit_time_normalized,
                                             float blend_duration) {
    AnimationTransition t;
    t.from = from;
    t.to = to;
    t.trigger = "";  // exit-time transitions don't use triggers
    t.blend_duration = blend_duration;
    t.has_exit_time = true;
    t.exit_time_normalized = exit_time_normalized;
    transitions_.push_back(t);
}

void StateMachine::set_initial_state(const AnimStateId& id) {
    if (states_.find(id) == states_.end()) {
        return;
    }
    activate_state(id);
}

void StateMachine::trigger(const std::string& name) {
    // Already transitioning? Queue triggers for after transition completes.
    // For simplicity, we currently only handle immediate triggers.

    for (const auto& t : transitions_) {
        if (t.from == current_state_id_ && t.trigger == name) {
            // Begin crossfade
            if (states_.find(t.to) != states_.end()) {
                next_state_id_ = t.to;
                transition_duration_ = t.blend_duration;
                transition_timer_ = transition_duration_;

                // Reset event fired flags for the new state's clips
                const auto& entry = states_[t.to];
                const auto& clip_name = entry.state.clip_name;
                if (clip_events_.find(clip_name) != clip_events_.end()) {
                    auto& flags = event_fired_flags_[clip_name];
                    std::fill(flags.begin(), flags.end(), false);
                }
            }
            return;
        }
    }
}

void StateMachine::tick(float dt) {
    if (current_state_id_.empty()) return;

    // Handle transition
    if (is_transitioning()) {
        transition_timer_ -= dt;
        if (transition_timer_ <= 0.0F) {
            // Transition complete
            transition_timer_ = 0.0F;
            activate_state(next_state_id_);
        }
    }

    // Handle exit-time transitions
    if (!is_transitioning()) {
        for (const auto& t : transitions_) {
            if (t.has_exit_time && t.from == current_state_id_) {
                // Check if any active clip has reached exit time
                for (const auto& ac : active_clips_) {
                    if (ac.normalized_time >= t.exit_time_normalized && ac.weight > 0.5F) {
                        if (states_.find(t.to) != states_.end()) {
                            next_state_id_ = t.to;
                            transition_duration_ = t.blend_duration;
                            transition_timer_ = transition_duration_;
                        }
                        break;
                    }
                }
            }
        }
    }

    // Evaluate active clips
    evaluate_active_clips();

    // Check for animation events
    if (event_callback_) {
        for (const auto& ac : active_clips_) {
            auto events_it = clip_events_.find(ac.clip_name);
            if (events_it == clip_events_.end()) continue;

            auto& flags = event_fired_flags_[ac.clip_name];
            const auto& events = events_it->second;
            float clip_time = ac.normalized_time;

            // Handle looping: reset fired flags when wrapping
            for (std::size_t i = 0; i < events.size(); ++i) {
                if (!flags[i] && clip_time >= events[i].time) {
                    flags[i] = true;
                    event_callback_(events[i].name, events[i].payload);
                }
                // Reset flag if we've looped past it
                if (flags[i] && clip_time < events[i].time - 0.1F) {
                    flags[i] = false;
                }
            }
        }
    }
}

void StateMachine::set_blend_param(float value) {
    auto it = states_.find(current_state_id_);
    if (it == states_.end() || !it->second.has_blend_1d) return;
    it->second.state.blend_1d.current_parameter = value;
}

void StateMachine::set_blend_param(float x, float y) {
    auto it = states_.find(current_state_id_);
    if (it == states_.end() || !it->second.has_blend_2d) return;
    it->second.state.blend_2d.current_parameter_x = x;
    it->second.state.blend_2d.current_parameter_y = y;
}

void StateMachine::add_clip_events(const std::string& clip_name,
                                    const std::vector<AnimationEvent>& events) {
    clip_events_[clip_name] = events;
    event_fired_flags_[clip_name] = std::vector<bool>(events.size(), false);
}

void StateMachine::activate_state(const AnimStateId& id) {
    current_state_id_ = id;
    next_state_id_.clear();
    evaluate_active_clips();
}

void StateMachine::evaluate_active_clips() {
    active_clips_.clear();

    auto current_it = states_.find(current_state_id_);
    if (current_it == states_.end()) return;

    const auto& current_state = current_it->second;

    // For simple states, use the default clip
    auto add_clip = [&](const std::string& clip_name, float weight, float speed) {
        ActiveClip ac;
        ac.clip_name = clip_name;
        ac.weight = weight;
        ac.playback_speed = speed;
        active_clips_.push_back(ac);
    };

    if (current_state.has_blend_1d) {
        const auto& blend = current_state.state.blend_1d;
        std::size_t idx_a = 0, idx_b = 0;
        float t = 0.0F;
        if (blend.get_blend_pair(idx_a, idx_b, t)) {
            if (idx_a == idx_b) {
                add_clip(blend.samples[idx_a].clip_name, 1.0F,
                         blend.samples[idx_a].playback_speed);
            } else {
                add_clip(blend.samples[idx_a].clip_name, 1.0F - t,
                         blend.samples[idx_a].playback_speed);
                add_clip(blend.samples[idx_b].clip_name, t,
                         blend.samples[idx_b].playback_speed);
            }
        }
    } else if (current_state.has_blend_2d) {
        // 2D blend not yet fully integrated in evaluation; fall back to default
        add_clip(current_state.state.clip_name, 1.0F,
                 current_state.state.default_speed);
    } else {
        add_clip(current_state.state.clip_name, 1.0F,
                 current_state.state.default_speed);
    }

    // If transitioning, include the next state's clips with crossfade weight
    if (is_transitioning()) {
        float transition_t = 1.0F - (transition_timer_ / transition_duration_);
        auto next_it = states_.find(next_state_id_);
        if (next_it != states_.end()) {
            const auto& next_state = next_it->second;
            if (next_state.has_blend_1d) {
                const auto& blend = next_state.state.blend_1d;
                std::size_t idx_a = 0, idx_b = 0;
                float bt = 0.0F;
                if (blend.get_blend_pair(idx_a, idx_b, bt)) {
                    add_clip(blend.samples[idx_a].clip_name, transition_t * (idx_a == idx_b ? 1.0F : 1.0F - bt),
                             blend.samples[idx_a].playback_speed);
                    if (idx_a != idx_b) {
                        add_clip(blend.samples[idx_b].clip_name, transition_t * bt,
                                 blend.samples[idx_b].playback_speed);
                    }
                }
            } else {
                add_clip(next_state.state.clip_name, transition_t,
                         next_state.state.default_speed);
            }
        }

        // Renormalize weights
        float total_weight = 0.0F;
        for (const auto& ac : active_clips_) {
            total_weight += ac.weight;
        }
        if (total_weight > 0.001F) {
            for (auto& ac : active_clips_) {
                ac.weight /= total_weight;
            }
        }
    }
}

}  // namespace ae::animation
