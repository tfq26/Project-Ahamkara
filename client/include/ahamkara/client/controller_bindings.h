#pragma once

#include "ae/platform/gamepad.h"

#include <string>

namespace ahamkara::client {

struct ControllerBindings {
    ae::GamepadInputCode jump {ae::encode_gamepad_button_code(0)};
    ae::GamepadInputCode crouch {ae::encode_gamepad_button_code(1)};
    ae::GamepadInputCode slide {ae::encode_gamepad_button_code(2)};
    ae::GamepadInputCode reload {ae::encode_gamepad_button_code(3)};
    ae::GamepadInputCode sprint {ae::encode_gamepad_button_code(4)};
    ae::GamepadInputCode ability {ae::encode_gamepad_button_code(5)};
    ae::GamepadInputCode interact {ae::encode_gamepad_button_code(8)};
    ae::GamepadInputCode metrics {ae::encode_gamepad_button_code(6)};
    ae::GamepadInputCode menu {ae::encode_gamepad_button_code(7)};
    ae::GamepadInputCode toggle_perspective {ae::encode_gamepad_button_code(9)};
    ae::GamepadInputCode aim {ae::encode_gamepad_axis_positive_code(4)};
    ae::GamepadInputCode fire {ae::encode_gamepad_axis_positive_code(5)};

    [[nodiscard]] bool load_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;
};

}  // namespace ahamkara::client
