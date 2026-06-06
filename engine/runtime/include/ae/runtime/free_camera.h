#pragma once

#include "ae/core/math.h"

namespace ae {

class FreeCamera {
public:
    static constexpr float default_pitch_limit_degrees = 89.0F;

    FreeCamera();
    explicit FreeCamera(const Vec3& position, float yaw_degrees = 0.0F, float pitch_degrees = 0.0F);

    void set_position(const Vec3& position);
    void set_rotation(float yaw_degrees, float pitch_degrees);

    void move(const Vec3& world_offset);
    void move_local(const Vec3& local_offset);
    void look(float yaw_delta_degrees, float pitch_delta_degrees);

    [[nodiscard]] const Vec3& position() const;
    [[nodiscard]] float yaw_degrees() const;
    [[nodiscard]] float pitch_degrees() const;

    [[nodiscard]] const Vec3& forward() const;
    [[nodiscard]] const Vec3& right() const;
    [[nodiscard]] const Vec3& up() const;

    [[nodiscard]] Mat4 view_matrix() const;

private:
    void update_basis_vectors();

    Vec3 position_ {};
    float yaw_degrees_ {0.0F};
    float pitch_degrees_ {0.0F};
    Vec3 forward_ {0.0F, 0.0F, 1.0F};
    Vec3 right_ {1.0F, 0.0F, 0.0F};
    Vec3 up_ {0.0F, 1.0F, 0.0F};
};

}  // namespace ae
