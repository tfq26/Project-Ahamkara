#include "ae/input/input_map.h"
#include "ae/platform/window.h"
#include "ae/core/cli_utils.h"
#include "ae/core/log.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#define AE_LOG_CATEGORY "Input"

namespace ae::input {
namespace {

const char* kActionNames[] = {
    "move_x", "move_y", "look_x", "look_y",
    "jump", "crouch", "sprint", "slide",
    "fire", "reload", "ability",
    "weapon1", "weapon2", "weapon3",
    "menu", "scoreboard"
};

int action_index(const std::string& name) {
    for (int i = 0; i < static_cast<int>(InputAction::Count); ++i)
        if (name == kActionNames[i]) return i;
    return -1;
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

InputMap::InputMap() {
    // Default bindings
    bind(InputAction::MoveX, ae::KeyCode::D, 1.0F);
    bind(InputAction::MoveX, ae::KeyCode::A, -1.0F);
    bind(InputAction::MoveY, ae::KeyCode::W, 1.0F);
    bind(InputAction::MoveY, ae::KeyCode::S, -1.0F);
    bind(InputAction::Jump, ae::KeyCode::Space);
    bind(InputAction::Crouch, ae::KeyCode::LeftControl);
    bind(InputAction::Sprint, ae::KeyCode::LeftShift);
    bind(InputAction::Slide, ae::KeyCode::C);
    bind(InputAction::Fire, ae::KeyCode::Unknown);
    bind(InputAction::Reload, ae::KeyCode::R);
    bind(InputAction::Ability, ae::KeyCode::Q);
    bind(InputAction::Weapon1, ae::KeyCode::Num1);
    bind(InputAction::Weapon2, ae::KeyCode::Num2);
    bind(InputAction::Weapon3, ae::KeyCode::Num3);
    bind(InputAction::Menu, ae::KeyCode::Escape);
    bind(InputAction::Scoreboard, ae::KeyCode::Tab);

    // Controller defaults
    bind(InputAction::Jump, ae::GamepadButton::South);
    bind(InputAction::Crouch, ae::GamepadButton::East);
    bind(InputAction::Sprint, ae::GamepadButton::LeftBumper);
    bind(InputAction::Reload, ae::GamepadButton::West);
    bind(InputAction::Slide, ae::GamepadButton::North);
    bind(InputAction::Ability, ae::GamepadButton::RightBumper);
    bind(InputAction::Menu, ae::GamepadButton::Start);
    bind(InputAction::Scoreboard, ae::GamepadButton::Back);
}

void InputMap::bind(InputAction action, ae::KeyCode key, float scale) {
    InputBinding b; b.is_gamepad = false; b.key = key; b.scale = scale;
    bindings_[static_cast<int>(action)].bindings.push_back(b);
}

void InputMap::bind(InputAction action, ae::GamepadButton button, float scale) {
    InputBinding b; b.is_gamepad = true; b.button = button; b.scale = scale;
    bindings_[static_cast<int>(action)].bindings.push_back(b);
}

void InputMap::clear_bindings() { bindings_.clear(); }

void InputMap::poll(const void* window_ptr, const ae::GamepadState& gamepad) {
    auto* w = static_cast<const ae::PlatformWindow*>(window_ptr);
    if (!w) {
        log_warning_cat(AE_LOG_CATEGORY, "poll called with null window pointer");
        return;
    }

    // Reset edge triggers
    for (auto& [_, st] : state_) { st.pressed = false; st.released = false; }

    for (int i = 0; i < static_cast<int>(InputAction::Count); ++i) {
        float val = 0.0F;
        bool held = false;
        auto it = bindings_.find(i);
        if (it == bindings_.end()) { update_action(i, val, false); continue; }

        for (const auto& b : it->second.bindings) {
            if (b.is_gamepad) {
                if (!gamepad.connected) continue;
                if (gamepad.is_button_down(b.button)) { val += b.scale; held = true; }
            } else {
                if (b.key == ae::KeyCode::Unknown) continue;
                if (w->is_key_down(b.key)) {
                    val += b.scale;
                    held = true;
                }
            }
        }

        if (val > 1.0F) val = 1.0F;
        if (val < -1.0F) val = -1.0F;

        bool pressed = held && !state_[i].held;
        bool released = !held && state_[i].held;
        update_action(i, val, pressed || released ? val : state_[i].value);
        state_[i].pressed = pressed;
        state_[i].released = released;
        state_[i].held = held;
    }
}

ActionState InputMap::get(InputAction action) const {
    auto it = state_.find(static_cast<int>(action));
    return it != state_.end() ? it->second : ActionState{};
}

void InputMap::update_action(int idx, float value, bool pressed) {
    state_[idx].value = value;
    if (pressed) state_[idx].pressed = true;
}

bool InputMap::load_from_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        log_warning_cat(AE_LOG_CATEGORY, "Cannot open input bindings file: " + path);
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        int ai = action_index(key);
        if (ai < 0) continue;
        // Parse key or button
        if (val.size() > 3 && val[0] == 'G' && val[1] == 'P' && val[2] == '_') {
            int bid = std::stoi(val.substr(3));
            bind(static_cast<InputAction>(ai), static_cast<ae::GamepadButton>(bid));
        } else {
            int kid = std::stoi(val);
            bind(static_cast<InputAction>(ai), static_cast<ae::KeyCode>(kid));
        }
    }
    log_info_cat(AE_LOG_CATEGORY, "Input bindings loaded from " + path);
    return true;
}

bool InputMap::save_to_file(const std::string& path) const {
    std::ofstream f(path);
    if (!f) {
        log_warning_cat(AE_LOG_CATEGORY, "Cannot write input bindings file: " + path);
        return false;
    }
    for (int i = 0; i < static_cast<int>(InputAction::Count); ++i) {
        auto it = bindings_.find(i);
        if (it == bindings_.end()) continue;
        for (const auto& b : it->second.bindings) {
            if (b.is_gamepad) f << kActionNames[i] << "=GP_" << static_cast<int>(b.button) << "\n";
            else f << kActionNames[i] << "=" << static_cast<int>(b.key) << "\n";
        }
    }
    return true;
}

const char* InputMap::action_name(InputAction action) {
    int i = static_cast<int>(action);
    return (i >= 0 && i < static_cast<int>(InputAction::Count)) ? kActionNames[i] : "unknown";
}

} // namespace ae::input
