#include "ae/core/log.h"
#include "ae/animation/aim_recoil.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>


#define AE_LOG_CATEGORY "Animation"

namespace ae::animation {

// Simple deterministic random for recoil spread (no global state dependency)
namespace {
float recoil_random(float seed, float min_val, float max_val) {
    // Quick-and-dirty hash for spread
    float range = max_val - min_val;
    float frac = seed - std::floor(seed);
    return min_val + frac * range;
}
}  // namespace

void fire_recoil(RecoilState& state, const RecoilConfig& config, bool is_ads) {
    float ads_mult = is_ads ? config.ads_kick_multiplier : 1.0F;

    // Generate random spread
    float seed = state.current_pitch * 100.0F + state.current_yaw * 37.0F;
    float pitch_kick = recoil_random(seed, config.kick_pitch_min, config.kick_pitch_max) *
                       config.pattern_scale * ads_mult;
    float yaw_kick = recoil_random(seed + 0.5F, config.kick_yaw_min, config.kick_yaw_max) *
                     config.pattern_scale * ads_mult;
    float roll_kick = recoil_random(seed + 0.3F, config.kick_roll_min, config.kick_roll_max) *
                      config.pattern_scale * ads_mult;

    state.current_pitch += pitch_kick;
    state.current_yaw += yaw_kick;
    state.current_roll += roll_kick;

    // Add a bit of recovery velocity for smooth return
    state.recovery_velocity_pitch -= pitch_kick * config.recovery_speed * 0.5F;
    state.recovery_velocity_yaw -= yaw_kick * config.recovery_speed * 0.3F;
}

void apply_recoil(RecoilState& state, const RecoilConfig& config,
                  float dt, bool is_firing, bool is_ads,
                  JointTransform& out_offset) {
    // Spring-damper recovery toward zero
    // acceleration = -stiffness * position - damping * velocity
    float stiffness = config.recovery_speed * config.recovery_speed;
    float damping = 2.0F * config.recovery_damping * config.recovery_speed;

    // Pitch recovery
    float accel_pitch = -stiffness * state.current_pitch - damping * state.recovery_velocity_pitch;
    state.recovery_velocity_pitch += accel_pitch * dt;
    state.current_pitch += state.recovery_velocity_pitch * dt;

    // Yaw recovery
    float accel_yaw = -stiffness * state.current_yaw - damping * state.recovery_velocity_yaw;
    state.recovery_velocity_yaw += accel_yaw * dt;
    state.current_yaw += state.recovery_velocity_yaw * dt;

    // Roll recovery
    float accel_roll = -stiffness * state.current_roll - damping * state.recovery_velocity_roll;
    state.recovery_velocity_roll += accel_roll * dt;
    state.current_roll += state.recovery_velocity_roll * dt;

    // If not firing, apply stronger recovery
    if (!is_firing) {
        float extra_recovery = 1.0F + config.recovery_speed * dt * 2.0F;
        state.current_pitch *= std::exp(-extra_recovery * dt);
        state.current_yaw *= std::exp(-extra_recovery * dt);
        state.current_roll *= std::exp(-extra_recovery * 0.5F * dt);
    }

    // Clamp to reasonable range
    float max_accum = config.kick_pitch_max * 10.0F;
    state.current_pitch = std::clamp(state.current_pitch, -max_accum, max_accum);
    state.current_yaw = std::clamp(state.current_yaw, -max_accum, max_accum);
    state.current_roll = std::clamp(state.current_roll, -max_accum * 0.5F, max_accum * 0.5F);

    // Build output transform as a quaternion from pitch/yaw/roll offsets
    float half_pitch = state.current_pitch * 0.5F;
    float half_yaw = state.current_yaw * 0.5F;
    float half_roll = state.current_roll * 0.5F;

    float cp = std::cos(half_pitch), sp = std::sin(half_pitch);
    float cy = std::cos(half_yaw), sy = std::sin(half_yaw);
    float cr = std::cos(half_roll), sr = std::sin(half_roll);

    // Compose as: Y * X * Z (pitch then yaw then roll) as quaternion
    // Simplified: just use Euler-to-quat
    out_offset.qx = sp * cy * cr + cp * sy * sr;
    out_offset.qy = cp * sy * cr - sp * cy * sr;
    out_offset.qz = cp * cy * sr - sp * sy * cr;
    out_offset.qw = cp * cy * cr + sp * sy * sr;
}

}  // namespace ae::animation
