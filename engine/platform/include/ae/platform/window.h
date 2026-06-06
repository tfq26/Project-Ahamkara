#pragma once

#include "ae/platform/gamepad.h"
#include "ae/platform/key.h"
#include "ae/platform/mouse.h"

#include <memory>
#include <string>
#include <string_view>

namespace ae {

struct WindowConfig {
    std::string title {"Ahamkara"};
    int width {1280};
    int height {720};
    bool fullscreen {false};
    bool create_opengl_context {false};
};

class PlatformWindow {
public:
    static std::unique_ptr<PlatformWindow> create(const WindowConfig& config);

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;
    PlatformWindow(PlatformWindow&&) = delete;
    PlatformWindow& operator=(PlatformWindow&&) = delete;
    virtual ~PlatformWindow();

    /// Process pending OS events. Returns `false` when a close has been
    /// requested.
    virtual bool poll_events() = 0;

    /// Whether the given key is held down this frame.
    [[nodiscard]] virtual bool is_key_down(KeyCode key) const = 0;

    /// Edge-triggered: true only on the frame the key transitions to down.
    [[nodiscard]] virtual bool is_key_pressed(KeyCode key) const = 0;

    /// Edge-triggered: true only on the frame the key transitions to up.
    [[nodiscard]] virtual bool is_key_released(KeyCode key) const = 0;

    /// Current cursor state (position, delta, buttons).
    [[nodiscard]] virtual MouseState mouse_state() const = 0;

    /// Current primary gamepad state. If no supported controller is connected,
    /// `connected` is false and all fields are reset.
    [[nodiscard]] virtual const GamepadState& gamepad_state() const = 0;

    /// Raw controller diagnostics for calibration and binding tools.
    [[nodiscard]] virtual const GamepadDebugState& gamepad_debug_state() const = 0;

    /// Whether the window has been requested to close.
    [[nodiscard]] virtual bool should_close() const = 0;

    /// Programmatically request the window to close.
    virtual void request_close() = 0;

    /// Update the native window title.
    virtual void set_title(std::string_view title) = 0;

    /// Native backend window handle. Intended for narrow backend integrations
    /// such as the temporary debug renderer; gameplay code should not use it.
    [[nodiscard]] virtual void* native_handle() const = 0;

protected:
    PlatformWindow() = default;
};

}  // namespace ae
