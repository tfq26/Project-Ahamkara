#pragma once

namespace ahamkara::client {

enum class CameraMode {
    DebugThirdPerson,
    FirstPerson,
};

[[nodiscard]] constexpr CameraMode next_camera_mode(CameraMode mode) {
    switch (mode) {
        case CameraMode::DebugThirdPerson:
            return CameraMode::FirstPerson;
        case CameraMode::FirstPerson:
            return CameraMode::DebugThirdPerson;
    }

    return CameraMode::DebugThirdPerson;
}

[[nodiscard]] constexpr const char* camera_mode_name(CameraMode mode) {
    switch (mode) {
        case CameraMode::DebugThirdPerson:
            return "3P";
        case CameraMode::FirstPerson:
            return "FP";
    }

    return "3P";
}

}  // namespace ahamkara::client
