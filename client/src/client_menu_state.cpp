#include "ahamkara/client/client_menu_state.h"

#include "ae/core/log.h"

namespace ahamkara::client {

ClientMenuState::ClientMenuState() {
    sync_menu_state();
}

bool ClientMenuState::visible() const {
    return mode_ != ClientMenuMode::Gameplay;
}

ae::ui::MenuScreen ClientMenuState::screen() const {
    switch (mode_) {
        case ClientMenuMode::Gameplay:
            return ae::ui::MenuScreen::None;
        case ClientMenuMode::MainMenu:
            return ae::ui::MenuScreen::MainMenu;
        case ClientMenuMode::PauseOverlay:
            return ae::ui::MenuScreen::PauseOverlay;
        case ClientMenuMode::SettingsFromMainMenu:
        case ClientMenuMode::SettingsFromPause:
            return ae::ui::MenuScreen::Settings;
        case ClientMenuMode::Character:
            return ae::ui::MenuScreen::Character;
    }

    return ae::ui::MenuScreen::None;
}

bool ClientMenuState::simulation_should_pause() const {
    return visible();
}

bool ClientMenuState::toggle_menu() {
    // MainMenu only goes away via PLAY — ESC does nothing here.
    if (mode_ == ClientMenuMode::MainMenu) {
        return false;
    }

    const ClientMenuMode previous = mode_;
    const bool changed = handle_cancel();
    if (changed) {
        if (previous == ClientMenuMode::Gameplay) {
            ae::log_info("Menu opened: Pause Overlay");
        } else {
            ae::log_info("Menu closed: returning to previous screen");
        }
    }
    return changed;
}

bool ClientMenuState::handle_cancel() {
    switch (mode_) {
    case ClientMenuMode::MainMenu:
        // The main menu is a root screen; cancel never dismisses it.
        return false;
    case ClientMenuMode::SettingsFromMainMenu:
        mode_ = ClientMenuMode::MainMenu;
        break;
    case ClientMenuMode::SettingsFromPause:
    case ClientMenuMode::Character:
        mode_ = ClientMenuMode::PauseOverlay;
        break;
    case ClientMenuMode::Gameplay:
        // Cancel from gameplay opens the pause overlay.
        mode_ = ClientMenuMode::PauseOverlay;
        break;
    case ClientMenuMode::PauseOverlay:
        mode_ = ClientMenuMode::Gameplay;
        break;
    }
    sync_menu_state();
    return true;
}

void ClientMenuState::start_gameplay() {
    mode_ = ClientMenuMode::Gameplay;
    sync_menu_state();
    ae::log_info("Game started from main menu");
}

void ClientMenuState::resume_gameplay() {
    mode_ = ClientMenuMode::Gameplay;
    sync_menu_state();
}

void ClientMenuState::open_settings() {
    if (mode_ == ClientMenuMode::MainMenu || mode_ == ClientMenuMode::SettingsFromMainMenu) {
        mode_ = ClientMenuMode::SettingsFromMainMenu;
    } else {
        mode_ = ClientMenuMode::SettingsFromPause;
    }
    sync_menu_state();
}

void ClientMenuState::back_from_settings() {
    if (mode_ == ClientMenuMode::SettingsFromMainMenu) {
        mode_ = ClientMenuMode::MainMenu;
    } else {
        mode_ = ClientMenuMode::PauseOverlay;
    }
    sync_menu_state();
}

void ClientMenuState::open_character() {
    mode_ = ClientMenuMode::Character;
    sync_menu_state();
}

void ClientMenuState::back_to_pause() {
    mode_ = ClientMenuMode::PauseOverlay;
    sync_menu_state();
}

void ClientMenuState::show_pause() {
    mode_ = ClientMenuMode::PauseOverlay;
    sync_menu_state();
}

void ClientMenuState::show_main_menu() {
    mode_ = ClientMenuMode::MainMenu;
    sync_menu_state();
}

void ClientMenuState::sync_menu_state() {
    menu_state_.visible = visible();
    menu_state_.screen = screen();
}

}  // namespace ahamkara::client
