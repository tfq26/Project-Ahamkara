// Unit tests for ahamkara::client::ClientMenuState — the single owner of
// menu visibility / gameplay-pause transitions. Headless-friendly: no GLFW
// or rendering dependencies (only the ae/ui MenuScreen enum).
#include "ahamkara/client/client_menu_state.h"

#include <cassert>
#include <iostream>

namespace {

using ae::ui::MenuScreen;
using ahamkara::client::ClientMenuMode;
using ahamkara::client::ClientMenuState;

void test_initial_state_is_main_menu() {
    ClientMenuState state;
    assert(state.mode() == ClientMenuMode::MainMenu);
    assert(state.visible());
    assert(state.screen() == MenuScreen::MainMenu);
    assert(state.simulation_should_pause());
    assert(state.cursor_should_capture());
    assert(!state.gameplay_input_enabled());
    std::cout << "test_initial_state_is_main_menu passed.\n";
}

void test_start_gameplay_hides_menu_and_unpauses() {
    ClientMenuState state;
    state.start_gameplay();
    assert(state.mode() == ClientMenuMode::Gameplay);
    assert(!state.visible());
    assert(state.screen() == MenuScreen::None);
    assert(!state.simulation_should_pause());
    assert(state.gameplay_input_enabled());
    std::cout << "test_start_gameplay_hides_menu_and_unpauses passed.\n";
}

void test_toggle_menu_from_gameplay_opens_pause() {
    ClientMenuState state;
    state.start_gameplay();
    const bool changed = state.toggle_menu();
    assert(changed);
    assert(state.mode() == ClientMenuMode::PauseOverlay);
    assert(state.visible());
    assert(state.screen() == MenuScreen::PauseOverlay);
    assert(state.simulation_should_pause());
    std::cout << "test_toggle_menu_from_gameplay_opens_pause passed.\n";
}

void test_toggle_menu_from_pause_resumes() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu(); // open pause
    const bool changed = state.toggle_menu();
    assert(changed);
    assert(state.mode() == ClientMenuMode::Gameplay);
    assert(!state.visible());
    assert(!state.simulation_should_pause());
    std::cout << "test_toggle_menu_from_pause_resumes passed.\n";
}

void test_toggle_menu_on_main_menu_is_noop() {
    ClientMenuState state;
    const bool changed = state.toggle_menu();
    assert(!changed);
    assert(state.mode() == ClientMenuMode::MainMenu);
    assert(state.visible());
    std::cout << "test_toggle_menu_on_main_menu_is_noop passed.\n";
}

void test_settings_flow_from_main_menu() {
    ClientMenuState state;
    state.open_settings();
    assert(state.mode() == ClientMenuMode::SettingsFromMainMenu);
    assert(state.screen() == MenuScreen::Settings);
    assert(state.visible());

    state.back_from_settings();
    assert(state.mode() == ClientMenuMode::MainMenu);
    assert(state.screen() == MenuScreen::MainMenu);

    // ESC from settings reached from the main menu is a no-op (the screen is
    // only dismissed through its BACK control).
    state.open_settings();
    const bool changed = state.toggle_menu();
    assert(!changed);
    assert(state.mode() == ClientMenuMode::SettingsFromMainMenu);
    std::cout << "test_settings_flow_from_main_menu passed.\n";
}

void test_settings_flow_from_pause() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu(); // open pause
    state.open_settings();
    assert(state.mode() == ClientMenuMode::SettingsFromPause);
    assert(state.screen() == MenuScreen::Settings);

    state.back_from_settings();
    assert(state.mode() == ClientMenuMode::PauseOverlay);
    assert(state.screen() == MenuScreen::PauseOverlay);
    std::cout << "test_settings_flow_from_pause passed.\n";
}

void test_character_flow_returns_to_pause() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu(); // open pause
    state.open_character();
    assert(state.mode() == ClientMenuMode::Character);
    assert(state.screen() == MenuScreen::Character);

    state.back_to_pause();
    assert(state.mode() == ClientMenuMode::PauseOverlay);
    assert(state.screen() == MenuScreen::PauseOverlay);
    std::cout << "test_character_flow_returns_to_pause passed.\n";
}

void test_compatibility_wrappers() {
    ClientMenuState state;
    state.show_pause();
    assert(state.screen() == MenuScreen::PauseOverlay);
    state.show_main_menu();
    assert(state.screen() == MenuScreen::MainMenu);
    state.close_menu(); // resume_gameplay
    assert(state.mode() == ClientMenuMode::Gameplay);
    assert(!state.visible());
    std::cout << "test_compatibility_wrappers passed.\n";
}

void test_pause_state_is_single_source_of_simulation_pause() {
    ClientMenuState state;
    // Any visible menu pauses the simulation.
    state.start_gameplay();
    assert(!state.simulation_should_pause());
    state.open_character();
    assert(state.simulation_should_pause());
    state.start_gameplay();
    assert(!state.simulation_should_pause());
    std::cout << "test_pause_state_is_single_source_of_simulation_pause passed.\n";
}

} // namespace

int main() {
    test_initial_state_is_main_menu();
    test_start_gameplay_hides_menu_and_unpauses();
    test_toggle_menu_from_gameplay_opens_pause();
    test_toggle_menu_from_pause_resumes();
    test_toggle_menu_on_main_menu_is_noop();
    test_settings_flow_from_main_menu();
    test_settings_flow_from_pause();
    test_character_flow_returns_to_pause();
    test_compatibility_wrappers();
    test_pause_state_is_single_source_of_simulation_pause();
    std::cout << "All client menu state tests passed.\n";
    return 0;
}
