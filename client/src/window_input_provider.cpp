#include "ahamkara/client/window_input_provider.h"

#include <cmath>
#include <string>

#include "ae/core/log.h"
#include "ae/core/math.h"
#include "ae/platform/gamepad.h"
#include "ae/platform/mouse.h"
#include "ae/platform/window.h"

namespace ahamkara::client {
namespace {

constexpr float kStickDeadzone = 0.2F;
constexpr float kTriggerDeadzone = 0.15F;
constexpr float kLookRateDegreesPerSecond = 180.0F;

float apply_deadzone(float value, float deadzone) {
    if (std::fabs(value) <= deadzone) {
        return 0.0F;
    }

    const float sign = value < 0.0F ? -1.0F : 1.0F;
    return sign * ((std::fabs(value) - deadzone) / (1.0F - deadzone));
}

bool has_connected_gamepad(const ae::GamepadState& state) {
    return state.connected;
}

}  // namespace

WindowInputProvider::WindowInputProvider(
    const ae::PlatformWindow& window,
    float mouse_sensitivity,
    const ControllerBindings& controller_bindings)
    : window_(window)
    , mouse_sensitivity_(mouse_sensitivity) {
    controller_bindings_ = controller_bindings;
}

ahamkara::game::PlayerInputCommand WindowInputProvider::gather_input(float delta_seconds) {
    ahamkara::game::PlayerInputCommand command {};
    const ae::GamepadState& gamepad = window_.gamepad_state();
    const ae::GamepadDebugState& debug_state = window_.gamepad_debug_state();

    if (has_connected_gamepad(gamepad)) {
        const float left_x = apply_deadzone(gamepad.axis(ae::GamepadAxis::LeftX), kStickDeadzone);
        const float left_y = apply_deadzone(gamepad.axis(ae::GamepadAxis::LeftY), kStickDeadzone);
        const float right_x = apply_deadzone(gamepad.axis(ae::GamepadAxis::RightX), kStickDeadzone);
        const float right_y = apply_deadzone(gamepad.axis(ae::GamepadAxis::RightY), kStickDeadzone);
        const float right_trigger =
            apply_deadzone((gamepad.axis(ae::GamepadAxis::RightTrigger) + 1.0F) * 0.5F, kTriggerDeadzone);

        command.move_axis.x = left_x;
        command.move_axis.y = -left_y;
        command.look_delta.x = right_x * mouse_sensitivity_ * kLookRateDegreesPerSecond * delta_seconds;
        command.look_delta.y = -right_y * mouse_sensitivity_ * kLookRateDegreesPerSecond * delta_seconds;
        command.sprint_held = debug_state.is_code_down(controller_bindings_.sprint);
        command.jump_pressed = debug_state.is_code_pressed(controller_bindings_.jump);
        command.crouch_held = debug_state.is_code_down(controller_bindings_.crouch);
        command.slide_pressed = debug_state.is_code_pressed(controller_bindings_.slide);
        command.fire_held = debug_state.is_code_down(controller_bindings_.fire) || right_trigger > 0.5F;
        command.reload_pressed = debug_state.is_code_pressed(controller_bindings_.reload);
        command.ability_pressed = debug_state.is_code_pressed(controller_bindings_.ability);
        return command;
    }

    if (window_.is_key_down(ae::KeyCode::W)) {
        command.move_axis.y += 1.0F;
    }
    if (window_.is_key_down(ae::KeyCode::S)) {
        command.move_axis.y -= 1.0F;
    }
    if (window_.is_key_down(ae::KeyCode::A)) {
        command.move_axis.x -= 1.0F;
    }
    if (window_.is_key_down(ae::KeyCode::D)) {
        command.move_axis.x += 1.0F;
    }

    command.sprint_held = window_.is_key_down(ae::KeyCode::LeftShift)
        || window_.is_key_down(ae::KeyCode::RightShift);
    command.jump_pressed = window_.is_key_pressed(ae::KeyCode::Space);
    command.crouch_held = window_.is_key_down(ae::KeyCode::LeftControl)
        || window_.is_key_down(ae::KeyCode::RightControl);
    command.slide_pressed = window_.is_key_pressed(ae::KeyCode::C);

    // Weapon switching: 1=AR-15 (Primary slot 0), 2=Shotgun (Secondary slot 1), 3=Rocket (Melee slot 2)
    if (window_.is_key_pressed(ae::KeyCode::Num1))
        command.weapon_slot = 0;
    else if (window_.is_key_pressed(ae::KeyCode::Num2))
        command.weapon_slot = 1;
    else if (window_.is_key_pressed(ae::KeyCode::Num3))
        command.weapon_slot = 2;

    const auto mouse = window_.mouse_state();
    command.fire_held = mouse.button_down[static_cast<ae::usize>(ae::MouseButton::Left)];
    command.reload_pressed = window_.is_key_pressed(ae::KeyCode::R);
    command.ability_pressed = window_.is_key_pressed(ae::KeyCode::E);
    command.look_delta.x = mouse.delta_x * mouse_sensitivity_;
    command.look_delta.y = mouse.delta_y * mouse_sensitivity_;

    static int s_look_diag_a = 0;
    if ((mouse.delta_x != 0.0F || mouse.delta_y != 0.0F) || (s_look_diag_a++ % 60) == 0) {
        ae::log_info_cat("LookDiag", "[A] client submit look_delta=(" +
            std::to_string(command.look_delta.x) + ", " +
            std::to_string(command.look_delta.y) + ")");
    }

    return command;
}

void WindowInputProvider::set_mouse_sensitivity(float mouse_sensitivity) {
    mouse_sensitivity_ = mouse_sensitivity;
}

}  // namespace ahamkara::client
