#pragma once

#include "ae/render/skeletal_animation.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ae::animation {

// ============================================================
// Joint space transform (decomposed: easier to blend)
// ============================================================

struct JointTransform {
    float tx {0.0F}, ty {0.0F}, tz {0.0F};       // translation
    float qx {0.0F}, qy {0.0F}, qz {0.0F}, qw {1.0F}; // rotation quaternion
    float sx {1.0F}, sy {1.0F}, sz {1.0F};        // scale

    /// Convert to column-major Mat4 (T * R * S)
    render::Mat4 to_mat4() const;

    /// Blend two transforms with factor t in [0,1]
    static JointTransform blend(const JointTransform& a, const JointTransform& b, float t);

    static JointTransform identity();
};

// ============================================================
// A pose is a collection of joint-space transforms
// ============================================================

struct AnimationPose {
    std::vector<JointTransform> local_transforms;  // per-joint local transforms
    std::vector<render::Mat4> global_matrices;      // computed global (world) matrices

    /// Compute global matrices from local transforms using the given parent indices.
    /// Joints must be in topological order (parents before children).
    void compute_globals(const std::vector<int>& parent_indices);
};

// ============================================================
// Animation clip — a named, time-based sequence of keyframes
// ============================================================

struct AnimationClip {
    std::string name;
    const render::GltfAnimation* source {nullptr};  // points into GltfModel
    float duration_seconds {0.0F};                   // total clip length
    bool looping {true};
};

// ============================================================
// Animation instance — playback state for a single clip
// ============================================================

struct ClipInstance {
    const AnimationClip* clip {nullptr};
    float time {0.0F};         // current playback time
    float playback_speed {1.0F};
    bool active {false};
    bool finished {false};

    void advance(float dt);
    void reset();
    [[nodiscard]] float normalized_time() const;  // 0..1
};

// ============================================================
// Blend tree types
// ============================================================

/// Blend parameter types
enum class BlendParamType : std::uint8_t {
    Float,    // 1D blend
    Vec2,     // 2D blend
};

/// A single child in a blend tree, with a parameter threshold
struct BlendSample {
    std::string clip_name;
    float threshold {0.0F};             // 1D threshold
    float threshold_x {0.0F};           // 2D X threshold
    float threshold_y {0.0F};           // 2D Y threshold
    float playback_speed {1.0F};
};

/// 1D blend space: blends between clips based on a single float parameter
struct BlendSpace1D {
    std::vector<BlendSample> samples;
    float current_parameter {0.0F};

    /// Returns the two nearest samples and blend factor
    /// Returns false if there are fewer than 2 samples.
    bool get_blend_pair(std::size_t& idx_a, std::size_t& idx_b, float& t) const;
};

/// 2D blend space: blends between 3 clips via barycentric interpolation
struct BlendSpace2D {
    std::vector<BlendSample> samples;
    float current_parameter_x {0.0F};
    float current_parameter_y {0.0F};

    /// Returns indices of the 3 closest samples forming a triangle and their barycentric weights.
    /// Returns false if there are fewer than 3 samples.
    bool get_barycentric_blend(std::size_t& idx_a, std::size_t& idx_b,
                               std::size_t& idx_c, float& wa, float& wb, float& wc) const;
};

// ============================================================
// State machine types
// ============================================================

/// Animation state identifier
using AnimStateId = std::string;

/// A single state in the animation state machine
struct AnimationState {
    AnimStateId id;
    std::string clip_name;               // default clip for this state
    float default_speed {1.0F};
    BlendSpace1D blend_1d;              // optional 1D blend override
    BlendSpace2D blend_2d;              // optional 2D blend override
};

/// A transition between two animation states
struct AnimationTransition {
    AnimStateId from;
    AnimStateId to;
    std::string trigger;                 // event name that fires this transition
    float blend_duration {0.2F};         // crossfade time in seconds
    bool has_exit_time {false};          // wait for clip to finish?
    float exit_time_normalized {0.9F};   // normalized time to exit at
};

// ============================================================
// Animation events / notifies
// ============================================================

/// A timed event embedded in an animation clip
struct AnimationEvent {
    float time {0.0F};                   // time in the clip when this fires
    std::string name;                    // event identifier (e.g., "footstep", "fire")
    std::string payload;                 // optional string payload
    bool has_fired {false};             // internal: prevents re-triggering
};

/// Callback for animation events
using AnimationEventCallback = std::function<void(const std::string& event_name,
                                                    const std::string& payload)>;

}  // namespace ae::animation
