#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/window_input_provider.h"
#include "ae/platform/window.h"

#include <cassert>
#include <cmath>
#include <string_view>

namespace {

class FakeWindow final : public ae::PlatformWindow {
public:
    bool poll_events() override { return true; }

    [[nodiscard]] bool is_key_down(ae::KeyCode key) const override {
        return key_down_[static_cast<int>(key)];
    }

    [[nodiscard]] bool is_key_pressed(ae::KeyCode key) const override {
        return key_pressed_[static_cast<int>(key)];
    }

    [[nodiscard]] bool is_key_released(ae::KeyCode) const override {
        return false;
    }

    [[nodiscard]] ae::MouseState mouse_state() const override {
        return mouse_;
    }

    [[nodiscard]] const ae::GamepadState& gamepad_state() const override {
        return gamepad_;
    }

    [[nodiscard]] const ae::GamepadDebugState& gamepad_debug_state() const override {
        return gamepad_debug_;
    }

    [[nodiscard]] bool should_close() const override { return false; }
    void request_close() override {}
    void set_title(std::string_view) override {}
    [[nodiscard]] void* native_handle() const override { return nullptr; }

    void set_key_down(ae::KeyCode key, bool down) {
        key_down_[static_cast<int>(key)] = down;
    }

    void set_key_pressed(ae::KeyCode key, bool pressed) {
        key_pressed_[static_cast<int>(key)] = pressed;
    }

    ae::MouseState mouse_ {};
    ae::GamepadState gamepad_ {};
    ae::GamepadDebugState gamepad_debug_ {};

private:
    static constexpr int kKeyStorageSize = static_cast<int>(ae::KeyCode::Menu) + 1;
    bool key_down_[kKeyStorageSize] {};
    bool key_pressed_[kKeyStorageSize] {};
};

void test_keyboard_and_mouse_work_with_idle_gamepad_connected() {
    FakeWindow window;
    window.set_key_down(ae::KeyCode::W, true);
    window.set_key_down(ae::KeyCode::D, true);
    window.mouse_.delta_x = 5.0F;
    window.mouse_.delta_y = -2.0F;
    window.gamepad_.connected = true;
    window.gamepad_debug_.connected = true;

    ahamkara::client::WindowInputProvider input(window, 0.5F, ahamkara::client::ControllerBindings {});
    const auto command = input.gather_input(1.0F / 60.0F);

    assert(command.move_axis.x == 1.0F);
    assert(command.move_axis.y == 1.0F);
    assert(std::fabs(command.look_delta.x - 2.5F) < 0.0001F);
    assert(std::fabs(command.look_delta.y + 1.0F) < 0.0001F);
}

void test_gamepad_input_layers_on_keyboard_and_mouse() {
    FakeWindow window;
    window.set_key_down(ae::KeyCode::A, true);
    window.set_key_pressed(ae::KeyCode::Space, true);
    window.mouse_.delta_x = 1.0F;
    window.gamepad_.connected = true;
    window.gamepad_.axes[static_cast<ae::usize>(ae::GamepadAxis::LeftX)] = 1.0F;
    window.gamepad_.axes[static_cast<ae::usize>(ae::GamepadAxis::RightX)] = 1.0F;

    ahamkara::client::WindowInputProvider input(window, 1.0F, ahamkara::client::ControllerBindings {});
    const auto command = input.gather_input(1.0F);

    assert(command.move_axis.x == 0.0F);
    assert(command.jump_pressed);
    assert(std::fabs(command.look_delta.x - 181.0F) < 0.0001F);
}

}  // namespace

int main() {
    test_keyboard_and_mouse_work_with_idle_gamepad_connected();
    test_gamepad_input_layers_on_keyboard_and_mouse();
    return 0;
}
