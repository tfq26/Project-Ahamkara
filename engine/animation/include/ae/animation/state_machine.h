#pragma once

#include "ae/animation/types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ae::animation {

// ============================================================
// Animation State Machine
//
// Manages animation states and transitions. A "state" maps to
// an animation clip (or blend tree). Transitions fire on named
// trigger events with configurable crossfade duration.
//
// Usage pattern:
//   sm.add_state("Idle", "idle_clip");
//   sm.add_state("Run", "run_clip");
//   sm.add_transition("Idle", "Run", "start_moving", 0.15f);
//   sm.add_transition("Run", "Idle", "stop_moving", 0.2f);
//   ...
//   sm.trigger("start_moving");
//   sm.tick(dt);  // updates current clip, handles crossfade
// ============================================================

class StateMachine {
public:
    StateMachine() = default;

    // --- Construction (intended for init / asset loading) ---

    /// Add a state entry. If the state id already exists, it is replaced.
    void add_state(const AnimStateId& id, const std::string& clip_name,
                   float default_speed = 1.0F);

    /// Add a state with a 1D blend space.
    void add_blend_1d_state(const AnimStateId& id, const BlendSpace1D& blend);

    /// Add a state with a 2D blend space.
    void add_blend_2d_state(const AnimStateId& id, const BlendSpace2D& blend);

    /// Add a transition. State ids must exist.
    void add_transition(const AnimStateId& from, const AnimStateId& to,
                        const std::string& trigger,
                        float blend_duration = 0.2F);

    /// Add a transition with an exit-time condition.
    void add_exit_time_transition(const AnimStateId& from, const AnimStateId& to,
                                  float exit_time_normalized = 0.9F,
                                  float blend_duration = 0.15F);

    /// Set the initial state (must exist).
    void set_initial_state(const AnimStateId& id);

    // --- Runtime ---

    /// Fire a trigger. If a transition matches the current state + trigger,
    /// begin crossfading to the target.
    void trigger(const std::string& name);

    /// Tick the state machine. Advances clip playback, processes crossfade.
    void tick(float dt);

    /// Set blend parameters for the current state's blend space.
    void set_blend_param(float value);
    void set_blend_param(float x, float y);

    // --- Query ---

    /// Returns the currently playing clip name(s) and their weights.
    /// During a crossfade, returns two entries (current + next).
    struct ActiveClip {
        std::string clip_name;
        float weight {1.0F};        // 0..1 contribution to final pose
        float normalized_time {0.0F};
        float playback_speed {1.0F};
    };
    [[nodiscard]] const std::vector<ActiveClip>& active_clips() const { return active_clips_; }

    [[nodiscard]] const AnimStateId& current_state_id() const { return current_state_id_; }
    [[nodiscard]] bool is_transitioning() const { return transition_timer_ > 0.0F; }

    // --- Events ---

    /// Register an animation event callback. Called when a timed event fires
    /// during clip playback.
    void set_event_callback(AnimationEventCallback callback) {
        event_callback_ = std::move(callback);
    }

    /// Register timed events for a specific clip.
    void add_clip_events(const std::string& clip_name,
                         const std::vector<AnimationEvent>& events);

private:
    void activate_state(const AnimStateId& id);
    void evaluate_active_clips();

    struct StateEntry {
        AnimationState state;
        bool has_blend_1d {false};
        bool has_blend_2d {false};
    };

    std::unordered_map<AnimStateId, StateEntry> states_;
    std::vector<AnimationTransition> transitions_;
    AnimStateId current_state_id_;
    AnimStateId next_state_id_;                // empty when not transitioning
    float transition_timer_ {0.0F};             // 0 = not transitioning
    float transition_duration_ {0.0F};
    std::vector<ActiveClip> active_clips_;
    AnimationEventCallback event_callback_;
    std::unordered_map<std::string, std::vector<AnimationEvent>> clip_events_;
    std::unordered_map<std::string, std::vector<bool>> event_fired_flags_;
};

}  // namespace ae::animation
