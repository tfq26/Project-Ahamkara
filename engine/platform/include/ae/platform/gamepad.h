#pragma once

#include "ae/core/types.h"

#include <string>

namespace ae {

enum class GamepadButton : u8 {
    South,
    East,
    West,
    North,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftThumb,
    RightThumb,
    DPadUp,
    DPadRight,
    DPadDown,
    DPadLeft,
    Count
};

enum class GamepadAxis : u8 {
    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
    Count
};

constexpr usize kGamepadButtonCount = static_cast<usize>(GamepadButton::Count);
constexpr usize kGamepadAxisCount = static_cast<usize>(GamepadAxis::Count);
constexpr usize kRawGamepadButtonCount = 32;
constexpr usize kRawGamepadAxisCount = 8;

using GamepadInputCode = u32;

constexpr GamepadInputCode kInvalidGamepadInputCode = 0U;
constexpr GamepadInputCode kGamepadButtonCodeBase = 0x00010000U;
constexpr GamepadInputCode kGamepadAxisPositiveCodeBase = 0x00020000U;
constexpr GamepadInputCode kGamepadAxisNegativeCodeBase = 0x00030000U;
constexpr GamepadInputCode kGamepadHatCodeBase = 0x00040000U;

constexpr GamepadInputCode encode_gamepad_button_code(u8 index) {
    return kGamepadButtonCodeBase | static_cast<GamepadInputCode>(index);
}

constexpr GamepadInputCode encode_gamepad_axis_positive_code(u8 index) {
    return kGamepadAxisPositiveCodeBase | static_cast<GamepadInputCode>(index);
}

constexpr GamepadInputCode encode_gamepad_axis_negative_code(u8 index) {
    return kGamepadAxisNegativeCodeBase | static_cast<GamepadInputCode>(index);
}

constexpr GamepadInputCode encode_gamepad_hat_code(u8 hat_mask) {
    return kGamepadHatCodeBase | static_cast<GamepadInputCode>(hat_mask);
}

struct GamepadState {
    bool connected {false};
    bool standardized_mapping {false};
    std::string name {};
    float axes[kGamepadAxisCount] {};
    bool button_down[kGamepadButtonCount] {};
    bool button_pressed[kGamepadButtonCount] {};
    bool button_released[kGamepadButtonCount] {};

    [[nodiscard]] float axis(GamepadAxis axis) const {
        return axes[static_cast<usize>(axis)];
    }

    [[nodiscard]] bool is_button_down(GamepadButton button) const {
        return button_down[static_cast<usize>(button)];
    }

    [[nodiscard]] bool is_button_pressed(GamepadButton button) const {
        return button_pressed[static_cast<usize>(button)];
    }

    [[nodiscard]] bool is_button_released(GamepadButton button) const {
        return button_released[static_cast<usize>(button)];
    }
};

struct GamepadDebugState {
    bool connected {false};
    bool standardized_mapping {false};
    std::string name {};
    bool raw_button_down[kRawGamepadButtonCount] {};
    bool raw_button_pressed[kRawGamepadButtonCount] {};
    bool raw_button_released[kRawGamepadButtonCount] {};
    float raw_axes[kRawGamepadAxisCount] {};
    bool raw_axis_positive_down[kRawGamepadAxisCount] {};
    bool raw_axis_positive_pressed[kRawGamepadAxisCount] {};
    bool raw_axis_negative_down[kRawGamepadAxisCount] {};
    bool raw_axis_negative_pressed[kRawGamepadAxisCount] {};
    u8 hat_mask {0};
    GamepadInputCode last_pressed_code {kInvalidGamepadInputCode};

    [[nodiscard]] bool is_code_down(GamepadInputCode code) const {
        if ((code & 0xFFFF0000U) == kGamepadButtonCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadButtonCount ? raw_button_down[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadAxisPositiveCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadAxisCount ? raw_axis_positive_down[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadAxisNegativeCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadAxisCount ? raw_axis_negative_down[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadHatCodeBase) {
            return (hat_mask & static_cast<u8>(code & 0xFFU)) != 0;
        }
        return false;
    }

    [[nodiscard]] bool is_code_pressed(GamepadInputCode code) const {
        if ((code & 0xFFFF0000U) == kGamepadButtonCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadButtonCount ? raw_button_pressed[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadAxisPositiveCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadAxisCount ? raw_axis_positive_pressed[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadAxisNegativeCodeBase) {
            const usize index = static_cast<usize>(code & 0xFFFFU);
            return index < kRawGamepadAxisCount ? raw_axis_negative_pressed[index] : false;
        }
        if ((code & 0xFFFF0000U) == kGamepadHatCodeBase) {
            return last_pressed_code == code;
        }
        return false;
    }
};

}  // namespace ae
