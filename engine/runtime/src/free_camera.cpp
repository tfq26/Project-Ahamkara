#include "ae/runtime/free_camera.h"
#include "ae/core/log.h"

#define AE_LOG_CATEGORY "Runtime"

namespace ae {
namespace {

constexpr Vec3 world_up {0.0F, 1.0F, 0.0F};

}  // namespace

FreeCamera::FreeCamera() {
    update_basis_vectors();
    log_debug_cat(AE_LOG_CATEGORY, "FreeCamera created (default)");
}

FreeCamera::FreeCamera(const Vec3& position, float yaw_degrees, float pitch_degrees)
    : position_(position) {
    set_rotation(yaw_degrees, pitch_degrees);
    log_debug_cat(AE_LOG_CATEGORY, "FreeCamera created at (" +
                  std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) + ")");
}

void FreeCamera::set_position(const Vec3& position) {
    position_ = position;
}

void FreeCamera::set_rotation(float yaw_degrees, float pitch_degrees) {
    yaw_degrees_ = wrap_degrees(yaw_degrees);
    pitch_degrees_ = clamp(pitch_degrees, -default_pitch_limit_degrees, default_pitch_limit_degrees);
    update_basis_vectors();
}

void FreeCamera::move(const Vec3& world_offset) {
    position_ += world_offset;
}

void FreeCamera::move_local(const Vec3& local_offset) {
    position_ += right_ * local_offset.x;
    position_ += up_ * local_offset.y;
    position_ += forward_ * local_offset.z;
}

void FreeCamera::look(float yaw_delta_degrees, float pitch_delta_degrees) {
    set_rotation(yaw_degrees_ + yaw_delta_degrees, pitch_degrees_ + pitch_delta_degrees);
}

const Vec3& FreeCamera::position() const {
    return position_;
}

float FreeCamera::yaw_degrees() const {
    return yaw_degrees_;
}

float FreeCamera::pitch_degrees() const {
    return pitch_degrees_;
}

const Vec3& FreeCamera::forward() const {
    return forward_;
}

const Vec3& FreeCamera::right() const {
    return right_;
}

const Vec3& FreeCamera::up() const {
    return up_;
}

Mat4 FreeCamera::view_matrix() const {
    return Mat4::look_at(position_, position_ + forward_, up_);
}

void FreeCamera::update_basis_vectors() {
    const float yaw_radians = to_radians(yaw_degrees_);
    const float pitch_radians = to_radians(pitch_degrees_);
    const float cos_pitch = std::cos(pitch_radians);

    forward_ = {
        std::sin(yaw_radians) * cos_pitch,
        std::sin(pitch_radians),
        std::cos(yaw_radians) * cos_pitch
    };
    forward_ = forward_.normalized();
    right_ = cross(world_up, forward_).normalized();
    up_ = cross(forward_, right_).normalized();
}

}  // namespace ae
