#include "ahamkara/client/client_menu_state.h"

#include "ae/core/log.h"

namespace ahamkara::client {

ClientMenuState::ClientMenuState() {
    menu_state_.visible = true;
    menu_state_.screen  = ae::ui::MenuScreen::MainMenu;
}

bool ClientMenuState::toggle_menu() {
    // MainMenu only goes away via PLAY — ESC does nothing here
    if (menu_state_.screen == ae::ui::MenuScreen::MainMenu) {
        return false;
    }

    if (!visible()) {
        // Not in any menu — open pause overlay
        show_pause();
        ae::log_info("Menu opened: Pause Overlay");
        return true;
    }

    // Already in a menu (Pause, Settings, Character) — close it
    close_menu();
    ae::log_info("Menu closed: returning to game");
    return true;
}

void ClientMenuState::close_menu() {
    menu_state_.visible = false;
    menu_state_.screen  = ae::ui::MenuScreen::None;
    ae::log_info("Game started from main menu");
}

void ClientMenuState::show_pause() {
    menu_state_.visible = true;
    menu_state_.screen  = ae::ui::MenuScreen::PauseOverlay;
}

void ClientMenuState::show_main_menu() {
    menu_state_.visible = true;
    menu_state_.screen  = ae::ui::MenuScreen::MainMenu;
}

}  // namespace ahamkara::client
