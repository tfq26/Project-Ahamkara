#include "ae/core/math.h"
#include "ae/runtime/free_camera.h"

#include <cassert>
#include <cmath>

namespace {

bool nearly_equal(float lhs, float rhs, float tolerance = 0.0001F) {
    return std::fabs(lhs - rhs) <= tolerance;
}

void assert_vec3_near(const ae::Vec3& actual, const ae::Vec3& expected, float tolerance = 0.0001F) {
    assert(nearly_equal(actual.x, expected.x, tolerance));
    assert(nearly_equal(actual.y, expected.y, tolerance));
    assert(nearly_equal(actual.z, expected.z, tolerance));
}

void test_free_camera_default_orientation() {
    ae::FreeCamera camera {};

    assert(nearly_equal(camera.yaw_degrees(), 0.0F));
    assert(nearly_equal(camera.pitch_degrees(), 0.0F));
    assert_vec3_near(camera.forward(), {0.0F, 0.0F, 1.0F});
    assert_vec3_near(camera.right(), {1.0F, 0.0F, 0.0F});
    assert_vec3_near(camera.up(), {0.0F, 1.0F, 0.0F});
}

void test_free_camera_look_clamps_pitch_and_wraps_yaw() {
    ae::FreeCamera camera {};

    camera.look(450.0F, 120.0F);

    assert(nearly_equal(camera.yaw_degrees(), 90.0F));
    assert(nearly_equal(camera.pitch_degrees(), ae::FreeCamera::default_pitch_limit_degrees));
    assert(nearly_equal(camera.forward().length(), 1.0F));
    assert(nearly_equal(camera.right().length(), 1.0F));
    assert(nearly_equal(camera.up().length(), 1.0F));
}

void test_free_camera_local_movement_uses_camera_basis() {
    ae::FreeCamera camera {};
    camera.look(90.0F, 0.0F);
    camera.move_local({2.0F, 3.0F, 4.0F});

    assert_vec3_near(camera.position(), {4.0F, 3.0F, -2.0F});
}

void test_free_camera_view_matrix_translates_camera_origin() {
    ae::FreeCamera camera({3.0F, 4.0F, 5.0F});

    const ae::Mat4 view = camera.view_matrix();
    const ae::Vec3 camera_space_origin = view.transform_point(camera.position());

    assert_vec3_near(camera_space_origin, {0.0F, 0.0F, 0.0F});
}

}  // namespace

void run_camera_smoke_tests() {
    test_free_camera_default_orientation();
    test_free_camera_look_clamps_pitch_and_wraps_yaw();
    test_free_camera_local_movement_uses_camera_basis();
    test_free_camera_view_matrix_translates_camera_origin();
}
