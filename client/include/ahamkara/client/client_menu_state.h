#pragma once

#include "ae/ui/ahamkara_ui.h"

namespace ahamkara::client {

// ============================================================================
// ClientMenuState — single owner for pause/menu visibility and transitions.
//
// Replaces scattered booleans in DebugUiController, debug_client.cpp, and
// ThreadedLocalRuntime.  Menu transitions (ESC, controller start, "PLAY"
// button) all flow through this owner.  The simulation pause is a consequence
// of menu visibility — no separate paused flag.
//
// IMPORTANT: menu_state_ (ae::ui::MenuState) is the single source of truth for
// screen + visibility.  The UI rendering functions in ahamkara_ui.cpp mutate
// menu_state_ directly, so ClientMenuState reads from it, never duplicates its
// fields.  There is no separate screen_ member to keep in sync.
// ============================================================================

class ClientMenuState {
public:
    ClientMenuState();

    [[nodiscard]] bool visible() const {
        return menu_state_.visible && menu_state_.screen != ae::ui::MenuScreen::None;
    }
    [[nodiscard]] ae::ui::MenuScreen screen() const { return menu_state_.screen; }
    [[nodiscard]] bool simulation_should_pause() const { return visible(); }

    /// Returns the internal MenuState reference (for screen rendering).
    /// Mutated by ahamkara_ui.cpp render functions — that is intentional.
    [[nodiscard]] ae::ui::MenuState& menu_state() { return menu_state_; }
    [[nodiscard]] const ae::ui::MenuState& menu_state() const { return menu_state_; }

    /// Toggle: if hidden → show PauseOverlay; if showing (not MainMenu) → hide.
    /// MainMenu on ESC is a no-op (only dismissable via START/PLAY).
    /// Returns true if the simulation pause state changed.
    [[nodiscard]] bool toggle_menu();

    /// Force-close the menu (e.g. PLAY pressed on MainMenu).
    void close_menu();

    /// Show the pause overlay directly.
    void show_pause();

    /// Show the main menu.
    void show_main_menu();

private:
    ae::ui::MenuState menu_state_;
};

}  // namespace ahamkara::client
