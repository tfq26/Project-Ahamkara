#include "ae/platform/window.h"

#include "ae/core/log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cctype>
#include <cstring>
#include <string>
#include <stdexcept>

namespace ae {
namespace {

// --- GLFW ↔ ae key mapping --------------------------------------------------

KeyCode glfw_to_ae_key(int glfw_key) {
    switch (glfw_key) {
        case GLFW_KEY_SPACE:          return KeyCode::Space;
        case GLFW_KEY_APOSTROPHE:     return KeyCode::Apostrophe;
        case GLFW_KEY_COMMA:          return KeyCode::Comma;
        case GLFW_KEY_MINUS:          return KeyCode::Minus;
        case GLFW_KEY_PERIOD:         return KeyCode::Period;
        case GLFW_KEY_SLASH:          return KeyCode::Slash;
        case GLFW_KEY_0:              return KeyCode::Num0;
        case GLFW_KEY_1:              return KeyCode::Num1;
        case GLFW_KEY_2:              return KeyCode::Num2;
        case GLFW_KEY_3:              return KeyCode::Num3;
        case GLFW_KEY_4:              return KeyCode::Num4;
        case GLFW_KEY_5:              return KeyCode::Num5;
        case GLFW_KEY_6:              return KeyCode::Num6;
        case GLFW_KEY_7:              return KeyCode::Num7;
        case GLFW_KEY_8:              return KeyCode::Num8;
        case GLFW_KEY_9:              return KeyCode::Num9;
        case GLFW_KEY_SEMICOLON:      return KeyCode::Semicolon;
        case GLFW_KEY_EQUAL:          return KeyCode::Equal;
        case GLFW_KEY_A:              return KeyCode::A;
        case GLFW_KEY_B:              return KeyCode::B;
        case GLFW_KEY_C:              return KeyCode::C;
        case GLFW_KEY_D:              return KeyCode::D;
        case GLFW_KEY_E:              return KeyCode::E;
        case GLFW_KEY_F:              return KeyCode::F;
        case GLFW_KEY_G:              return KeyCode::G;
        case GLFW_KEY_H:              return KeyCode::H;
        case GLFW_KEY_I:              return KeyCode::I;
        case GLFW_KEY_J:              return KeyCode::J;
        case GLFW_KEY_K:              return KeyCode::K;
        case GLFW_KEY_L:              return KeyCode::L;
        case GLFW_KEY_M:              return KeyCode::M;
        case GLFW_KEY_N:              return KeyCode::N;
        case GLFW_KEY_O:              return KeyCode::O;
        case GLFW_KEY_P:              return KeyCode::P;
        case GLFW_KEY_Q:              return KeyCode::Q;
        case GLFW_KEY_R:              return KeyCode::R;
        case GLFW_KEY_S:              return KeyCode::S;
        case GLFW_KEY_T:              return KeyCode::T;
        case GLFW_KEY_U:              return KeyCode::U;
        case GLFW_KEY_V:              return KeyCode::V;
        case GLFW_KEY_W:              return KeyCode::W;
        case GLFW_KEY_X:              return KeyCode::X;
        case GLFW_KEY_Y:              return KeyCode::Y;
        case GLFW_KEY_Z:              return KeyCode::Z;
        case GLFW_KEY_LEFT_BRACKET:   return KeyCode::LeftBracket;
        case GLFW_KEY_BACKSLASH:      return KeyCode::Backslash;
        case GLFW_KEY_RIGHT_BRACKET:  return KeyCode::RightBracket;
        case GLFW_KEY_GRAVE_ACCENT:   return KeyCode::Backtick;
        case GLFW_KEY_ESCAPE:         return KeyCode::Escape;
        case GLFW_KEY_ENTER:          return KeyCode::Enter;
        case GLFW_KEY_TAB:            return KeyCode::Tab;
        case GLFW_KEY_BACKSPACE:      return KeyCode::Backspace;
        case GLFW_KEY_INSERT:         return KeyCode::Insert;
        case GLFW_KEY_DELETE:         return KeyCode::Delete;
        case GLFW_KEY_RIGHT:          return KeyCode::Right;
        case GLFW_KEY_LEFT:           return KeyCode::Left;
        case GLFW_KEY_DOWN:           return KeyCode::Down;
        case GLFW_KEY_UP:             return KeyCode::Up;
        case GLFW_KEY_PAGE_UP:        return KeyCode::PageUp;
        case GLFW_KEY_PAGE_DOWN:      return KeyCode::PageDown;
        case GLFW_KEY_HOME:           return KeyCode::Home;
        case GLFW_KEY_END:            return KeyCode::End;
        case GLFW_KEY_CAPS_LOCK:      return KeyCode::CapsLock;
        case GLFW_KEY_SCROLL_LOCK:    return KeyCode::ScrollLock;
        case GLFW_KEY_NUM_LOCK:       return KeyCode::NumLock;
        case GLFW_KEY_PRINT_SCREEN:   return KeyCode::PrintScreen;
        case GLFW_KEY_PAUSE:          return KeyCode::Pause;
        case GLFW_KEY_F1:             return KeyCode::F1;
        case GLFW_KEY_F2:             return KeyCode::F2;
        case GLFW_KEY_F3:             return KeyCode::F3;
        case GLFW_KEY_F4:             return KeyCode::F4;
        case GLFW_KEY_F5:             return KeyCode::F5;
        case GLFW_KEY_F6:             return KeyCode::F6;
        case GLFW_KEY_F7:             return KeyCode::F7;
        case GLFW_KEY_F8:             return KeyCode::F8;
        case GLFW_KEY_F9:             return KeyCode::F9;
        case GLFW_KEY_F10:            return KeyCode::F10;
        case GLFW_KEY_F11:            return KeyCode::F11;
        case GLFW_KEY_F12:            return KeyCode::F12;
        case GLFW_KEY_F13:            return KeyCode::F13;
        case GLFW_KEY_F14:            return KeyCode::F14;
        case GLFW_KEY_F15:            return KeyCode::F15;
        case GLFW_KEY_F16:            return KeyCode::F16;
        case GLFW_KEY_F17:            return KeyCode::F17;
        case GLFW_KEY_F18:            return KeyCode::F18;
        case GLFW_KEY_F19:            return KeyCode::F19;
        case GLFW_KEY_F20:            return KeyCode::F20;
        case GLFW_KEY_F21:            return KeyCode::F21;
        case GLFW_KEY_F22:            return KeyCode::F22;
        case GLFW_KEY_F23:            return KeyCode::F23;
        case GLFW_KEY_F24:            return KeyCode::F24;
        case GLFW_KEY_F25:            return KeyCode::F25;
        case GLFW_KEY_KP_0:           return KeyCode::KeyPad0;
        case GLFW_KEY_KP_1:           return KeyCode::KeyPad1;
        case GLFW_KEY_KP_2:           return KeyCode::KeyPad2;
        case GLFW_KEY_KP_3:           return KeyCode::KeyPad3;
        case GLFW_KEY_KP_4:           return KeyCode::KeyPad4;
        case GLFW_KEY_KP_5:           return KeyCode::KeyPad5;
        case GLFW_KEY_KP_6:           return KeyCode::KeyPad6;
        case GLFW_KEY_KP_7:           return KeyCode::KeyPad7;
        case GLFW_KEY_KP_8:           return KeyCode::KeyPad8;
        case GLFW_KEY_KP_9:           return KeyCode::KeyPad9;
        case GLFW_KEY_KP_DECIMAL:     return KeyCode::KeyPadDecimal;
        case GLFW_KEY_KP_DIVIDE:      return KeyCode::KeyPadDivide;
        case GLFW_KEY_KP_MULTIPLY:    return KeyCode::KeyPadMultiply;
        case GLFW_KEY_KP_SUBTRACT:    return KeyCode::KeyPadSubtract;
        case GLFW_KEY_KP_ADD:         return KeyCode::KeyPadAdd;
        case GLFW_KEY_KP_ENTER:       return KeyCode::KeyPadEnter;
        case GLFW_KEY_KP_EQUAL:       return KeyCode::KeyPadEqual;
        case GLFW_KEY_LEFT_SHIFT:     return KeyCode::LeftShift;
        case GLFW_KEY_LEFT_CONTROL:   return KeyCode::LeftControl;
        case GLFW_KEY_LEFT_ALT:       return KeyCode::LeftAlt;
        case GLFW_KEY_LEFT_SUPER:     return KeyCode::LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT:    return KeyCode::RightShift;
        case GLFW_KEY_RIGHT_CONTROL:  return KeyCode::RightControl;
        case GLFW_KEY_RIGHT_ALT:      return KeyCode::RightAlt;
        case GLFW_KEY_RIGHT_SUPER:    return KeyCode::RightSuper;
        case GLFW_KEY_MENU:           return KeyCode::Menu;
        default:                      return KeyCode::Unknown;
    }
}

GamepadButton glfw_button_to_ae_button(int glfw_button) {
    switch (glfw_button) {
        case GLFW_GAMEPAD_BUTTON_A: return GamepadButton::South;
        case GLFW_GAMEPAD_BUTTON_B: return GamepadButton::East;
        case GLFW_GAMEPAD_BUTTON_X: return GamepadButton::West;
        case GLFW_GAMEPAD_BUTTON_Y: return GamepadButton::North;
        case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return GamepadButton::LeftBumper;
        case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return GamepadButton::RightBumper;
        case GLFW_GAMEPAD_BUTTON_BACK: return GamepadButton::Back;
        case GLFW_GAMEPAD_BUTTON_START: return GamepadButton::Start;
        case GLFW_GAMEPAD_BUTTON_GUIDE: return GamepadButton::Guide;
        case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return GamepadButton::LeftThumb;
        case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return GamepadButton::RightThumb;
        case GLFW_GAMEPAD_BUTTON_DPAD_UP: return GamepadButton::DPadUp;
        case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButton::DPadRight;
        case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return GamepadButton::DPadDown;
        case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return GamepadButton::DPadLeft;
        default: return GamepadButton::Count;
    }
}

GamepadAxis glfw_axis_to_ae_axis(int glfw_axis) {
    switch (glfw_axis) {
        case GLFW_GAMEPAD_AXIS_LEFT_X: return GamepadAxis::LeftX;
        case GLFW_GAMEPAD_AXIS_LEFT_Y: return GamepadAxis::LeftY;
        case GLFW_GAMEPAD_AXIS_RIGHT_X: return GamepadAxis::RightX;
        case GLFW_GAMEPAD_AXIS_RIGHT_Y: return GamepadAxis::RightY;
        case GLFW_GAMEPAD_AXIS_LEFT_TRIGGER: return GamepadAxis::LeftTrigger;
        case GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
        default: return GamepadAxis::Count;
    }
}

int find_primary_gamepad() {
    for (int joystick_index = GLFW_JOYSTICK_1; joystick_index <= GLFW_JOYSTICK_LAST; ++joystick_index) {
        if (glfwJoystickPresent(joystick_index) && glfwJoystickIsGamepad(joystick_index)) {
            return joystick_index;
        }
    }

    return -1;
}

int find_primary_joystick() {
    for (int joystick_index = GLFW_JOYSTICK_1; joystick_index <= GLFW_JOYSTICK_LAST; ++joystick_index) {
        if (glfwJoystickPresent(joystick_index)) {
            return joystick_index;
        }
    }

    return -1;
}

float normalize_trigger_axis(float value) {
    if (value < 0.0F) {
        return (value + 1.0F) * 0.5F;
    }

    return value;
}

std::string lowercase_copy(const std::string& value) {
    std::string lowered = value;
    for (char& character : lowered) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return lowered;
}

bool is_xbox_like_controller_name(const std::string& name) {
    const std::string lowered = lowercase_copy(name);
    return lowered.find("xbox") != std::string::npos
        || lowered.find("xinput") != std::string::npos
        || lowered.find("microsoft") != std::string::npos;
}

bool is_playstation_like_controller_name(const std::string& name) {
    const std::string lowered = lowercase_copy(name);
    return lowered.find("playstation") != std::string::npos
        || lowered.find("dualshock") != std::string::npos
        || lowered.find("dualsense") != std::string::npos
        || lowered.find("wireless controller") != std::string::npos;
}

// --- GLFW-backed PlatformWindow ---------------------------------------------

class GlfwWindow final : public PlatformWindow {
public:
    explicit GlfwWindow(const WindowConfig& config) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialise GLFW.");
        }

        if (config.create_opengl_context) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        } else {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWmonitor* monitor = nullptr;
        if (config.fullscreen) {
            monitor = glfwGetPrimaryMonitor();
            if (monitor == nullptr) {
                glfwTerminate();
                throw std::runtime_error("Failed to acquire the primary monitor for fullscreen mode.");
            }
        }

        window_ = glfwCreateWindow(
            config.width,
            config.height,
            config.title.c_str(),
            monitor,
            nullptr);

        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window.");
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetJoystickCallback([](int joystick_id, int event) {
            const char* joystick_name = glfwGetJoystickName(joystick_id);
            const std::string name = joystick_name != nullptr ? joystick_name : "Unknown Controller";
            if (event == GLFW_CONNECTED) {
                log_info("Controller connected: " + name);
            } else if (event == GLFW_DISCONNECTED) {
                log_info("Controller disconnected: " + name);
            }
        });

        glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
            const auto ak = glfw_to_ae_key(key);
            if (ak == KeyCode::Unknown) return;
            const auto idx = static_cast<usize>(ak);
            self->key_state_[idx] = (action != GLFW_RELEASE);
            if (action == GLFW_PRESS)  self->key_pressed_this_frame_[idx]  = true;
            if (action == GLFW_RELEASE) self->key_released_this_frame_[idx] = true;
        });

        glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
            self->mouse_state_.delta_x += static_cast<float>(x) - self->mouse_state_.cursor_x;
            self->mouse_state_.delta_y += static_cast<float>(y) - self->mouse_state_.cursor_y;
            self->mouse_state_.cursor_x = static_cast<float>(x);
            self->mouse_state_.cursor_y = static_cast<float>(y);
        });

        glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int) {
            auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
            if (button >= 0 && button < 3) {
                self->mouse_state_.button_down[button] = (action == GLFW_PRESS);
            }
        });

        log_info("Platform window created (" + std::to_string(config.width)
                 + "x" + std::to_string(config.height) + ").");
    }

    ~GlfwWindow() override {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
    }

    bool poll_events() override {
        // Reset edge-triggered state each frame.
        std::memset(key_pressed_this_frame_, 0, sizeof(key_pressed_this_frame_));
        std::memset(key_released_this_frame_, 0, sizeof(key_released_this_frame_));
        mouse_state_.delta_x = 0.0F;
        mouse_state_.delta_y = 0.0F;

        glfwPollEvents();

        if (glfwWindowShouldClose(window_)) {
            return false;
        }

        update_gamepad_state();
        return true;
    }

    [[nodiscard]] bool is_key_down(KeyCode key) const override {
        const auto idx = static_cast<usize>(key);
        if (idx >= kKeyCount) return false;
        return key_state_[idx];
    }

    [[nodiscard]] bool is_key_pressed(KeyCode key) const override {
        const auto idx = static_cast<usize>(key);
        if (idx >= kKeyCount) return false;
        return key_pressed_this_frame_[idx];
    }

    [[nodiscard]] bool is_key_released(KeyCode key) const override {
        const auto idx = static_cast<usize>(key);
        if (idx >= kKeyCount) return false;
        return key_released_this_frame_[idx];
    }

    [[nodiscard]] MouseState mouse_state() const override {
        return mouse_state_;
    }

    [[nodiscard]] const GamepadState& gamepad_state() const override {
        return gamepad_state_;
    }

    [[nodiscard]] const GamepadDebugState& gamepad_debug_state() const override {
        return gamepad_debug_state_;
    }

    [[nodiscard]] bool should_close() const override {
        return glfwWindowShouldClose(window_);
    }

    void request_close() override {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    void set_title(std::string_view title) override {
        glfwSetWindowTitle(window_, std::string(title).c_str());
    }

    [[nodiscard]] void* native_handle() const override {
        return window_;
    }

private:
    void reset_debug_state() {
        gamepad_debug_state_ = {};
    }

    void update_gamepad_state() {
        reset_debug_state();
        const int mapped_gamepad_index = find_primary_gamepad();
        if (mapped_gamepad_index >= 0) {
            GLFWgamepadstate glfw_state {};
            if (!glfwGetGamepadState(mapped_gamepad_index, &glfw_state)) {
                gamepad_state_ = {};
                return;
            }

            GamepadState next_state {};
            next_state.connected = true;
            next_state.standardized_mapping = true;
            if (const char* joystick_name = glfwGetGamepadName(mapped_gamepad_index); joystick_name != nullptr) {
                next_state.name = joystick_name;
            } else {
                next_state.name = "Unknown Controller";
            }

            gamepad_debug_state_.connected = true;
            gamepad_debug_state_.standardized_mapping = true;
            gamepad_debug_state_.name = next_state.name;

            for (int axis_index = 0; axis_index <= GLFW_GAMEPAD_AXIS_LAST; ++axis_index) {
                const GamepadAxis axis = glfw_axis_to_ae_axis(axis_index);
                if (axis == GamepadAxis::Count) {
                    continue;
                }

                next_state.axes[static_cast<usize>(axis)] = glfw_state.axes[axis_index];
            }

            next_state.axes[static_cast<usize>(GamepadAxis::LeftTrigger)] =
                normalize_trigger_axis(next_state.axes[static_cast<usize>(GamepadAxis::LeftTrigger)]);
            next_state.axes[static_cast<usize>(GamepadAxis::RightTrigger)] =
                normalize_trigger_axis(next_state.axes[static_cast<usize>(GamepadAxis::RightTrigger)]);

            for (int button_index = 0; button_index <= GLFW_GAMEPAD_BUTTON_LAST; ++button_index) {
                const GamepadButton button = glfw_button_to_ae_button(button_index);
                if (button == GamepadButton::Count) {
                    continue;
                }

                const usize mapped_index = static_cast<usize>(button);
                next_state.button_down[mapped_index] = glfw_state.buttons[button_index] == GLFW_PRESS;
                next_state.button_pressed[mapped_index] =
                    next_state.button_down[mapped_index] && !gamepad_state_.button_down[mapped_index];
                next_state.button_released[mapped_index] =
                    !next_state.button_down[mapped_index] && gamepad_state_.button_down[mapped_index];
            }

            const int button_count = std::min(static_cast<int>(GLFW_GAMEPAD_BUTTON_LAST + 1), static_cast<int>(kRawGamepadButtonCount));
            for (int i = 0; i < button_count; ++i) {
                gamepad_debug_state_.raw_button_down[i] = glfw_state.buttons[i] == GLFW_PRESS;
                gamepad_debug_state_.raw_button_pressed[i] =
                    gamepad_debug_state_.raw_button_down[i] && !previous_debug_state_.raw_button_down[i];
                gamepad_debug_state_.raw_button_released[i] =
                    !gamepad_debug_state_.raw_button_down[i] && previous_debug_state_.raw_button_down[i];
                if (gamepad_debug_state_.raw_button_pressed[i]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_button_code(static_cast<u8>(i));
                }
            }

            const int axis_count = std::min(static_cast<int>(GLFW_GAMEPAD_AXIS_LAST + 1), static_cast<int>(kRawGamepadAxisCount));
            for (int i = 0; i < axis_count; ++i) {
                gamepad_debug_state_.raw_axes[i] = glfw_state.axes[i];
                const float raw_val = glfw_state.axes[i];
                const float positive_value = (i == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || i == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER)
                    ? normalize_trigger_axis(raw_val)
                    : (raw_val > 0.0F ? raw_val : 0.0F);
                const bool positive_down = positive_value > 0.5F;
                const bool negative_down = raw_val < -0.5F;
                gamepad_debug_state_.raw_axis_positive_down[i] = positive_down;
                gamepad_debug_state_.raw_axis_negative_down[i] = negative_down;
                gamepad_debug_state_.raw_axis_positive_pressed[i] =
                    positive_down && !previous_debug_state_.raw_axis_positive_down[i];
                gamepad_debug_state_.raw_axis_negative_pressed[i] =
                    negative_down && !previous_debug_state_.raw_axis_negative_down[i];
                if (gamepad_debug_state_.raw_axis_positive_pressed[i]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_axis_positive_code(static_cast<u8>(i));
                }
                if (gamepad_debug_state_.raw_axis_negative_pressed[i]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_axis_negative_code(static_cast<u8>(i));
                }
            }

            gamepad_state_ = next_state;
            previous_debug_state_ = gamepad_debug_state_;
            return;
        }

        const int joystick_index = find_primary_joystick();
        if (joystick_index < 0) {
            gamepad_state_ = {};
            return;
        }

        GamepadState next_state {};
        next_state.connected = true;
        if (const char* joystick_name = glfwGetJoystickName(joystick_index); joystick_name != nullptr) {
            next_state.name = joystick_name;
        } else {
            next_state.name = "Unknown Joystick";
        }

        gamepad_debug_state_.connected = true;
        gamepad_debug_state_.standardized_mapping = false;
        gamepad_debug_state_.name = next_state.name;

        int axis_count = 0;
        if (const float* axes = glfwGetJoystickAxes(joystick_index, &axis_count); axes != nullptr) {
            if (axis_count > 0) next_state.axes[static_cast<usize>(GamepadAxis::LeftX)] = axes[0];
            if (axis_count > 1) next_state.axes[static_cast<usize>(GamepadAxis::LeftY)] = axes[1];
            if (axis_count > 2) next_state.axes[static_cast<usize>(GamepadAxis::RightX)] = axes[2];
            if (axis_count > 3) next_state.axes[static_cast<usize>(GamepadAxis::RightY)] = axes[3];
            if (axis_count > 4) {
                next_state.axes[static_cast<usize>(GamepadAxis::LeftTrigger)] = normalize_trigger_axis(axes[4]);
            }
            if (axis_count > 5) {
                next_state.axes[static_cast<usize>(GamepadAxis::RightTrigger)] = normalize_trigger_axis(axes[5]);
            }

            const int count = std::min(axis_count, static_cast<int>(kRawGamepadAxisCount));
            for (int i = 0; i < count; ++i) {
                gamepad_debug_state_.raw_axes[i] = axes[i];
                const float positive_value = axes[i] > 0.0F ? axes[i] : normalize_trigger_axis(axes[i]);
                const bool positive_down = positive_value > 0.5F;
                const bool negative_down = axes[i] < -0.5F;
                gamepad_debug_state_.raw_axis_positive_down[i] = positive_down;
                gamepad_debug_state_.raw_axis_negative_down[i] = negative_down;
                gamepad_debug_state_.raw_axis_positive_pressed[i] =
                    positive_down && !previous_debug_state_.raw_axis_positive_down[i];
                gamepad_debug_state_.raw_axis_negative_pressed[i] =
                    negative_down && !previous_debug_state_.raw_axis_negative_down[i];
                if (gamepad_debug_state_.raw_axis_positive_pressed[i]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_axis_positive_code(static_cast<u8>(i));
                }
                if (gamepad_debug_state_.raw_axis_negative_pressed[i]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_axis_negative_code(static_cast<u8>(i));
                }
            }
        }

        int button_count = 0;
        if (const unsigned char* buttons = glfwGetJoystickButtons(joystick_index, &button_count); buttons != nullptr) {
            const GamepadButton raw_button_map_xbox[] = {
                GamepadButton::South,
                GamepadButton::East,
                GamepadButton::North,
                GamepadButton::West,
                GamepadButton::LeftBumper,
                GamepadButton::RightBumper,
                GamepadButton::Back,
                GamepadButton::Start,
                GamepadButton::Guide,
                GamepadButton::LeftThumb,
                GamepadButton::RightThumb
            };
            const GamepadButton raw_button_map_generic[] = {
                GamepadButton::South,
                GamepadButton::East,
                GamepadButton::West,
                GamepadButton::North,
                GamepadButton::LeftBumper,
                GamepadButton::RightBumper,
                GamepadButton::Back,
                GamepadButton::Start,
                GamepadButton::Guide,
                GamepadButton::LeftThumb,
                GamepadButton::RightThumb
            };
            const GamepadButton raw_button_map_mac[] = {
                GamepadButton::South,        // 0: Cross / A
                GamepadButton::East,         // 1: Circle / B
                GamepadButton::Count,        // 2: Unmapped
                GamepadButton::West,         // 3: Square / X
                GamepadButton::North,        // 4: Triangle / Y
                GamepadButton::Count,        // 5: Unmapped
                GamepadButton::LeftBumper,   // 6: L1 / Left Bumper
                GamepadButton::RightBumper,  // 7: R1 / Right Bumper
                GamepadButton::Count,        // 8: L2 / Left Trigger (button)
                GamepadButton::Count,        // 9: R2 / Right Trigger (button)
                GamepadButton::Back,         // 10: Share / Back / Create
                GamepadButton::Start,        // 11: Options / Start
                GamepadButton::Guide,        // 12: PS Button / Guide
                GamepadButton::LeftThumb,    // 13: L3 / Left Stick Press
                GamepadButton::RightThumb    // 14: R3 / Right Stick Press
            };

            const GamepadButton* raw_button_map = raw_button_map_generic;
            int raw_button_map_size = sizeof(raw_button_map_generic) / sizeof(raw_button_map_generic[0]);

#if defined(__APPLE__)
            // On macOS, all gamepads are mapped to a uniform HID layout when accessed as generic joysticks.
            raw_button_map = raw_button_map_mac;
            raw_button_map_size = sizeof(raw_button_map_mac) / sizeof(raw_button_map_mac[0]);
#else
            if (is_xbox_like_controller_name(next_state.name)) {
                raw_button_map = raw_button_map_xbox;
                raw_button_map_size = sizeof(raw_button_map_xbox) / sizeof(raw_button_map_xbox[0]);
            }
#endif

            const int mapped_button_count = std::min(button_count, raw_button_map_size);
            for (int button_index = 0; button_index < mapped_button_count; ++button_index) {
                const GamepadButton button = raw_button_map[button_index];
                if (button == GamepadButton::Count) {
                    continue;
                }

                const usize mapped_index = static_cast<usize>(button);
                next_state.button_down[mapped_index] = buttons[button_index] == GLFW_PRESS;
                next_state.button_pressed[mapped_index] =
                    next_state.button_down[mapped_index] && !gamepad_state_.button_down[mapped_index];
                next_state.button_released[mapped_index] =
                    !next_state.button_down[mapped_index] && gamepad_state_.button_down[mapped_index];
            }

            // Populate debug state using the same standardized mapping
            std::memset(gamepad_debug_state_.raw_button_down, 0, sizeof(gamepad_debug_state_.raw_button_down));
            std::memset(gamepad_debug_state_.raw_button_pressed, 0, sizeof(gamepad_debug_state_.raw_button_pressed));
            std::memset(gamepad_debug_state_.raw_button_released, 0, sizeof(gamepad_debug_state_.raw_button_released));

            for (int button_index = 0; button_index < mapped_button_count; ++button_index) {
                const GamepadButton button = raw_button_map[button_index];
                if (button == GamepadButton::Count) {
                    continue;
                }

                const usize mapped_index = static_cast<usize>(button);
                gamepad_debug_state_.raw_button_down[mapped_index] = buttons[button_index] == GLFW_PRESS;
                gamepad_debug_state_.raw_button_pressed[mapped_index] =
                    gamepad_debug_state_.raw_button_down[mapped_index] && !previous_debug_state_.raw_button_down[mapped_index];
                gamepad_debug_state_.raw_button_released[mapped_index] =
                    !gamepad_debug_state_.raw_button_down[mapped_index] && previous_debug_state_.raw_button_down[mapped_index];
                if (gamepad_debug_state_.raw_button_pressed[mapped_index]) {
                    gamepad_debug_state_.last_pressed_code =
                        encode_gamepad_button_code(static_cast<u8>(mapped_index));
                }
            }
        }

        int hat_count = 0;
        if (const unsigned char* hats = glfwGetJoystickHats(joystick_index, &hat_count); hats != nullptr && hat_count > 0) {
            const unsigned char hat = hats[0];
            gamepad_debug_state_.hat_mask = hat;
            const struct {
                GamepadButton button;
                unsigned char mask;
            } dpad_map[] = {
                {GamepadButton::DPadUp, GLFW_HAT_UP},
                {GamepadButton::DPadRight, GLFW_HAT_RIGHT},
                {GamepadButton::DPadDown, GLFW_HAT_DOWN},
                {GamepadButton::DPadLeft, GLFW_HAT_LEFT}
            };

            for (const auto& entry : dpad_map) {
                const usize mapped_index = static_cast<usize>(entry.button);
                next_state.button_down[mapped_index] = (hat & entry.mask) != 0;
                next_state.button_pressed[mapped_index] =
                    next_state.button_down[mapped_index] && !gamepad_state_.button_down[mapped_index];
                next_state.button_released[mapped_index] =
                    !next_state.button_down[mapped_index] && gamepad_state_.button_down[mapped_index];

                gamepad_debug_state_.raw_button_down[mapped_index] = next_state.button_down[mapped_index];
                gamepad_debug_state_.raw_button_pressed[mapped_index] =
                    next_state.button_pressed[mapped_index] && !previous_debug_state_.raw_button_down[mapped_index];
                gamepad_debug_state_.raw_button_released[mapped_index] =
                    next_state.button_released[mapped_index] && previous_debug_state_.raw_button_down[mapped_index];

                if (next_state.button_pressed[mapped_index]) {
                    gamepad_debug_state_.last_pressed_code = encode_gamepad_button_code(static_cast<u8>(mapped_index));
                }
            }
        }

        gamepad_state_ = next_state;
        previous_debug_state_ = gamepad_debug_state_;
    }

    static constexpr usize kKeyCount = static_cast<usize>(KeyCode::Menu) + 1;

    GLFWwindow* window_ {nullptr};
    bool key_state_[kKeyCount] {};
    bool key_pressed_this_frame_[kKeyCount] {};
    bool key_released_this_frame_[kKeyCount] {};
    MouseState mouse_state_ {};
    GamepadState gamepad_state_ {};
    GamepadDebugState gamepad_debug_state_ {};
    GamepadDebugState previous_debug_state_ {};
};

}  // namespace

// --- Public factory ----------------------------------------------------------

PlatformWindow::~PlatformWindow() = default;

std::unique_ptr<PlatformWindow> PlatformWindow::create(const WindowConfig& config) {
    return std::make_unique<GlfwWindow>(config);
}

}  // namespace ae
