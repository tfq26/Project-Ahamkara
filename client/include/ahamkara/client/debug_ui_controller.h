#pragma once

#include "ae/ui/ahamkara_ui.h"
#include "ahamkara/client/client_config.h"
#include "ahamkara/client/client_menu_state.h"
#include "ahamkara/client/debug_scene_bridge.h"

namespace ae {
class PlatformWindow;
namespace input { class InputMap; }
struct GamepadState;
}

namespace ae::render {
struct DebugScene;
}

namespace ahamkara::client {

struct DebugUiActions {
    bool config_applied {false};
    bool quit_application {false};
    bool restart_match {false};
};

class DebugUiController {
public:
    explicit DebugUiController(ClientMenuState& menu_state,
                               const ClientConfig& client_config);

    [[nodiscard]] bool visible() const;
    [[nodiscard]] int active_menu_tab() const;

    DebugUiActions handle_menu_toggle(bool toggle_requested, ClientConfig& client_config);
    DebugUiActions render(
        ae::input::InputMap& input_map,
        ae::PlatformWindow& window,
        const ae::GamepadState& gamepad,
        const ClientSimulationSnapshot& current_snapshot,
        const ae::render::DebugScene& render_scene,
        ClientConfig& client_config);

private:
    void load_from_config(const ClientConfig& client_config);
    void apply_to_config(ClientConfig& client_config) const;

    ClientMenuState& menu_state_;
    bool show_scoreboard_ {false};
    bool show_rebind_ {false};
};

}  // namespace ahamkara::client
