#pragma once

#include "ae/ui/ahamkara_ui.h"

namespace ahamkara::client {

enum class ClientMenuMode {
    Gameplay,
    MainMenu,
    PauseOverlay,
    SettingsFromMainMenu,
    SettingsFromPause,
    Character,
};

// ============================================================================
// ClientMenuState — single owner for pause/menu visibility and transitions.
//
// Replaces scattered booleans in DebugUiController, debug_client.cpp, and
// ThreadedLocalRuntime.  Menu transitions (ESC, controller start, "PLAY"
// button) all flow through this owner.  The simulation pause is a consequence
// of menu visibility — no separate paused flag.
//
// This class is also the boundary between gameplay and menus: gameplay asks for
// pause/cursor policy, while UI render code only asks for explicit transitions.
// ============================================================================

class ClientMenuState {
public:
    ClientMenuState();

    [[nodiscard]] ClientMenuMode mode() const { return mode_; }
    [[nodiscard]] bool visible() const;
    [[nodiscard]] ae::ui::MenuScreen screen() const;
    [[nodiscard]] bool simulation_should_pause() const;
    [[nodiscard]] bool cursor_should_capture() const { return visible(); }
    [[nodiscard]] bool gameplay_input_enabled() const { return !simulation_should_pause(); }

    /// Returns the internal MenuState reference for legacy ImGui render helpers.
    /// Call set_mode()/transition helpers after rendering; do not treat
    /// MenuState mutations as gameplay state.
    [[nodiscard]] ae::ui::MenuState& menu_state() { return menu_state_; }
    [[nodiscard]] const ae::ui::MenuState& menu_state() const { return menu_state_; }

    /// Toggle: if hidden → show PauseOverlay; if showing (not MainMenu) → hide.
    /// MainMenu on ESC is a no-op (only dismissable via START/PLAY).
    /// Returns true if the simulation pause state changed.
    [[nodiscard]] bool toggle_menu();

    void start_gameplay();
    void resume_gameplay();
    void open_settings();
    void back_from_settings();
    void open_character();
    void back_to_pause();

    /// Compatibility wrappers for existing call sites.
    void close_menu() { resume_gameplay(); }
    void show_pause();
    void show_main_menu();

private:
    void sync_menu_state();

    ClientMenuMode mode_ {ClientMenuMode::MainMenu};
    ae::ui::MenuState menu_state_;
};

}  // namespace ahamkara::client
