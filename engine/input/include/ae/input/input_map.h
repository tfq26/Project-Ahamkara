#pragma once

#include "ae/platform/key.h"
#include "ae/platform/gamepad.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ae::input {

enum class InputAction {
    MoveX, MoveY, LookX, LookY,
    Jump, Crouch, Sprint, Slide,
    Fire, Reload, Ability,
    Weapon1, Weapon2, Weapon3,
    Menu, Scoreboard,
    Count
};

struct InputBinding {
    bool is_gamepad = false;
    ae::KeyCode key = ae::KeyCode::Unknown;
    ae::GamepadButton button = ae::GamepadButton::South;
    float scale = 1.0F;
};

struct ActionState {
    float value = 0.0F;    // 0-1 for axes, 0/1 for buttons
    bool pressed = false;  // edge-triggered (down this frame)
    bool released = false; // edge-triggered (up this frame)
    bool held = false;     // continuous
};

class InputMap {
public:
    InputMap();

    void bind(InputAction action, ae::KeyCode key, float scale = 1.0F);
    void bind(InputAction action, ae::GamepadButton button, float scale = 1.0F);
    void clear_bindings();

    void poll(const void* window, const ae::GamepadState& gamepad);
    ActionState get(InputAction action) const;

    bool load_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;

    static const char* action_name(InputAction action);

private:
    struct ActionBindings { std::vector<InputBinding> bindings; };
    std::unordered_map<int, ActionBindings> bindings_;
    std::unordered_map<int, ActionState> state_;
    void update_action(int idx, float value, bool pressed);
};

} // namespace ae::input
