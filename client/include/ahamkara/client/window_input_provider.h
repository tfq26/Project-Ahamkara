#pragma once

#include "ahamkara/client/controller_bindings.h"
#include "ahamkara/client/local_play.h"

namespace ae {
class PlatformWindow;
}

namespace ahamkara::client {

class WindowInputProvider final : public IInputProvider {
public:
    WindowInputProvider(
        const ae::PlatformWindow& window,
        float mouse_sensitivity,
        const ControllerBindings& controller_bindings);

    ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) override;

private:
    const ae::PlatformWindow& window_;
    float mouse_sensitivity_;
    ControllerBindings controller_bindings_;
};

}  // namespace ahamkara::client
