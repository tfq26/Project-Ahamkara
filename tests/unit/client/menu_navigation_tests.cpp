// Menu navigation flow tests for the Flashback main menu shell.
//
// Covers the deterministic state transitions of ClientMenuState (start,
// settings, pause, quit-to-menu, cancel/back), focus wrapping for menu lists,
// and the unavailable-settings catalog.  These tests run headless: the menu
// state machine and the settings catalog are compiled directly and do not
// depend on GLFW/OpenGL/ImGui.

#include "ahamkara/client/client_menu_state.h"
#include "ae/ui/menu_nav.h"
#include "ae/ui/settings_catalog.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

// Simple test framework macros (matches tests/src convention).
#define TEST(name)                       \
    do {                                 \
        printf("  TEST: %s ... ", name); \
        fflush(stdout);

#define END_TEST(result)                        \
    printf((result) ? "PASSED\n" : "FAILED\n"); \
    if (!(result)) {                            \
        failures++;                             \
    }                                           \
    }                                           \
    while (0)

static int failures = 0;

namespace {

using ae::ui::MenuScreen;
using ahamkara::client::ClientMenuMode;
using ahamkara::client::ClientMenuState;

// ---------------------------------------------------------------------------
// ClientMenuState: deterministic transitions
// ---------------------------------------------------------------------------

void test_initial_state_is_main_menu() {
    ClientMenuState state;
    TEST("initial mode is MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
    TEST("initial state is visible");
    END_TEST(state.visible());
    TEST("initial screen maps to MainMenu");
    END_TEST(state.screen() == MenuScreen::MainMenu);
    TEST("initial state pauses the simulation");
    END_TEST(state.simulation_should_pause());
    TEST("initial state disables gameplay input");
    END_TEST(!state.gameplay_input_enabled());
}

void test_start_gameplay_from_main_menu() {
    ClientMenuState state;
    state.start_gameplay();
    TEST("start_gameplay moves to Gameplay");
    END_TEST(state.mode() == ClientMenuMode::Gameplay);
    TEST("start_gameplay hides the menu");
    END_TEST(!state.visible());
    TEST("start_gameplay screen is None");
    END_TEST(state.screen() == MenuScreen::None);
    TEST("start_gameplay unpauses the simulation");
    END_TEST(!state.simulation_should_pause());
}

void test_toggle_opens_pause_from_gameplay() {
    ClientMenuState state;
    state.start_gameplay();
    const bool changed = state.toggle_menu();
    TEST("toggle from gameplay reports a change");
    END_TEST(changed);
    TEST("toggle from gameplay opens PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
    TEST("toggle from gameplay pauses the simulation");
    END_TEST(state.simulation_should_pause());
}

void test_toggle_resumes_from_pause() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    const bool changed = state.toggle_menu();
    TEST("toggle from pause reports a change");
    END_TEST(changed);
    TEST("toggle from pause resumes gameplay");
    END_TEST(state.mode() == ClientMenuMode::Gameplay);
    TEST("toggle from pause unpauses the simulation");
    END_TEST(!state.simulation_should_pause());
}

void test_toggle_noop_on_main_menu() {
    ClientMenuState state;
    const bool changed = state.toggle_menu();
    TEST("toggle on main menu reports no change");
    END_TEST(!changed);
    TEST("toggle on main menu keeps MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
}

void test_settings_from_main_menu_roundtrip() {
    ClientMenuState state;
    state.open_settings();
    TEST("open_settings from main menu -> SettingsFromMainMenu");
    END_TEST(state.mode() == ClientMenuMode::SettingsFromMainMenu);
    TEST("settings screen is Settings");
    END_TEST(state.screen() == MenuScreen::Settings);
    TEST("settings from main menu keeps menu visible");
    END_TEST(state.visible());

    state.back_from_settings();
    TEST("back_from_settings returns to MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
    TEST("back_from_settings screen is MainMenu");
    END_TEST(state.screen() == MenuScreen::MainMenu);
}

void test_settings_from_pause_roundtrip() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    state.open_settings();
    TEST("open_settings from pause -> SettingsFromPause");
    END_TEST(state.mode() == ClientMenuMode::SettingsFromPause);
    TEST("settings from pause screen is Settings");
    END_TEST(state.screen() == MenuScreen::Settings);

    state.back_from_settings();
    TEST("back_from_settings from pause settings returns to PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
    TEST("back_from_settings keeps menu visible");
    END_TEST(state.visible());
}

void test_cancel_from_settings_main_menu() {
    ClientMenuState state;
    state.open_settings();
    const bool changed = state.handle_cancel();
    TEST("cancel from main-menu settings reports a change");
    END_TEST(changed);
    TEST("cancel from main-menu settings returns to MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
}

void test_cancel_from_settings_pause() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    state.open_settings();
    const bool changed = state.handle_cancel();
    TEST("cancel from pause settings reports a change");
    END_TEST(changed);
    TEST("cancel from pause settings returns to PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
}

void test_cancel_from_character() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    state.open_character();
    TEST("open_character enters Character");
    END_TEST(state.mode() == ClientMenuMode::Character);
    const bool changed = state.handle_cancel();
    TEST("cancel from character reports a change");
    END_TEST(changed);
    TEST("cancel from character returns to PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
    TEST("cancel from character keeps the menu open");
    END_TEST(state.visible());
}

void test_cancel_from_pause_resumes() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    const bool changed = state.handle_cancel();
    TEST("cancel from pause reports a change");
    END_TEST(changed);
    TEST("cancel from pause resumes gameplay");
    END_TEST(state.mode() == ClientMenuMode::Gameplay);
}

void test_cancel_from_gameplay_opens_pause() {
    ClientMenuState state;
    state.start_gameplay();
    const bool changed = state.handle_cancel();
    TEST("cancel from gameplay reports a change");
    END_TEST(changed);
    TEST("cancel from gameplay opens PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
}

void test_cancel_noop_on_main_menu() {
    ClientMenuState state;
    const bool changed = state.handle_cancel();
    TEST("cancel on main menu reports no change");
    END_TEST(!changed);
    TEST("cancel on main menu keeps MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
}

void test_quit_to_menu_from_pause() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    state.show_main_menu();
    TEST("show_main_menu from pause -> MainMenu");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
    TEST("show_main_menu screen is MainMenu");
    END_TEST(state.screen() == MenuScreen::MainMenu);
}

void test_character_roundtrip() {
    ClientMenuState state;
    state.start_gameplay();
    (void)state.toggle_menu();
    state.open_character();
    TEST("open_character -> Character screen");
    END_TEST(state.screen() == MenuScreen::Character);
    state.back_to_pause();
    TEST("back_to_pause returns to PauseOverlay");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);
}

void test_screen_mapping() {
    ClientMenuState state;
    state.start_gameplay();
    TEST("gameplay screen maps to None");
    END_TEST(state.screen() == MenuScreen::None);

    state.open_settings();
    TEST("SettingsFromMainMenu maps to Settings");
    END_TEST(state.screen() == MenuScreen::Settings);

    state.show_main_menu();
    state.start_gameplay();
    (void)state.toggle_menu();
    state.open_settings();
    TEST("SettingsFromPause maps to Settings");
    END_TEST(state.screen() == MenuScreen::Settings);
}

void test_no_dead_end_screens() {
    // Every reachable mode must have at least one action that moves the player
    // forward or back — no mode may be a dead end.
    ClientMenuState state;

    state.show_main_menu();
    TEST("main menu has a forward path (start)");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
    state.start_gameplay();
    TEST("start leaves the main menu");
    END_TEST(state.mode() == ClientMenuMode::Gameplay);

    (void)state.toggle_menu();
    TEST("gameplay has a pause path");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);

    state.resume_gameplay();
    TEST("pause has a resume path");
    END_TEST(state.mode() == ClientMenuMode::Gameplay);

    (void)state.toggle_menu();
    state.open_settings();
    TEST("pause has a settings path");
    END_TEST(state.mode() == ClientMenuMode::SettingsFromPause);
    state.back_from_settings();
    TEST("pause settings has a back path");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);

    state.open_character();
    TEST("pause has a character path");
    END_TEST(state.mode() == ClientMenuMode::Character);
    state.back_to_pause();
    TEST("character has a back path");
    END_TEST(state.mode() == ClientMenuMode::PauseOverlay);

    state.show_main_menu();
    state.open_settings();
    TEST("main menu has a settings path");
    END_TEST(state.mode() == ClientMenuMode::SettingsFromMainMenu);
    (void)state.handle_cancel();
    TEST("main-menu settings has a back path");
    END_TEST(state.mode() == ClientMenuMode::MainMenu);
}

void test_deterministic_transitions() {
    // The same action from the same mode always lands on the same mode.
    ClientMenuState a;
    ClientMenuState b;
    a.open_settings();
    b.open_settings();
    TEST("open_settings is deterministic");
    END_TEST(a.mode() == b.mode());

    (void)a.handle_cancel();
    (void)b.handle_cancel();
    TEST("cancel is deterministic");
    END_TEST(a.mode() == b.mode());

    a.start_gameplay();
    b.start_gameplay();
    (void)a.toggle_menu();
    (void)b.toggle_menu();
    TEST("toggle is deterministic");
    END_TEST(a.mode() == b.mode());

    a.open_character();
    b.open_character();
    a.back_to_pause();
    b.back_to_pause();
    TEST("character roundtrip is deterministic");
    END_TEST(a.mode() == b.mode());
}

// ---------------------------------------------------------------------------
// Focus wrapping
// ---------------------------------------------------------------------------

void test_wrap_focus_down_past_last() {
    TEST("wrap_focus down past the last item wraps to first");
    END_TEST(ae::ui::wrap_focus(2, 3, 1) == 0);
    TEST("wrap_focus multi-step wrap lands correctly");
    END_TEST(ae::ui::wrap_focus(2, 3, 2) == 1);
}

void test_wrap_focus_up_past_first() {
    TEST("wrap_focus up past the first item wraps to last");
    END_TEST(ae::ui::wrap_focus(0, 3, -1) == 2);
    TEST("wrap_focus multi-step up wraps correctly");
    END_TEST(ae::ui::wrap_focus(0, 3, -2) == 1);
}

void test_wrap_focus_single_item() {
    TEST("wrap_focus with one item stays at 0");
    END_TEST(ae::ui::wrap_focus(0, 1, 1) == 0);
    TEST("wrap_focus with one item and negative delta stays at 0");
    END_TEST(ae::ui::wrap_focus(0, 1, -3) == 0);
}

void test_wrap_focus_empty_list() {
    TEST("wrap_focus with zero items returns 0");
    END_TEST(ae::ui::wrap_focus(0, 0, 1) == 0);
    TEST("wrap_focus with negative count returns 0");
    END_TEST(ae::ui::wrap_focus(2, -1, 1) == 0);
}

void test_wrap_focus_out_of_range_normalized() {
    TEST("wrap_focus normalizes an out-of-range positive index");
    END_TEST(ae::ui::wrap_focus(5, 3, 0) == 2);
    TEST("wrap_focus normalizes an out-of-range negative index");
    END_TEST(ae::ui::wrap_focus(-1, 3, 0) == 2);
}

// ---------------------------------------------------------------------------
// Unavailable settings catalog
// ---------------------------------------------------------------------------

void test_unavailable_settings_catalog_present() {
    const auto& catalog = ae::ui::unavailable_settings();
    TEST("unavailable settings catalog is not empty");
    END_TEST(!catalog.empty());
}

void test_unavailable_settings_entries_complete() {
    const auto& catalog = ae::ui::unavailable_settings();
    bool all_complete = true;
    for (const auto& entry : catalog) {
        if (entry.id.empty() || entry.label.empty() || entry.reason.empty()) {
            all_complete = false;
            break;
        }
    }
    TEST("every unavailable setting has id, label and reason");
    END_TEST(all_complete);
}

void test_unavailable_settings_ids_unique() {
    const auto& catalog = ae::ui::unavailable_settings();
    std::set<std::string> ids;
    bool unique = true;
    for (const auto& entry : catalog) {
        if (!ids.insert(entry.id).second) {
            unique = false;
            break;
        }
    }
    TEST("unavailable setting ids are unique");
    END_TEST(unique);
}

void test_supported_settings_not_unavailable() {
    // Settings that are actually wired to ClientConfig must not be presented
    // as unavailable.
    const auto& catalog = ae::ui::unavailable_settings();
    std::set<std::string> ids;
    for (const auto& entry : catalog)
        ids.insert(entry.id);

    TEST("resolution is not marked unavailable");
    END_TEST(ids.find("resolution") == ids.end());
    TEST("fullscreen is not marked unavailable");
    END_TEST(ids.find("fullscreen") == ids.end());
    TEST("brightness is not marked unavailable");
    END_TEST(ids.find("brightness") == ids.end());
    TEST("mouse sensitivity is not marked unavailable");
    END_TEST(ids.find("mouse_sensitivity") == ids.end());
    TEST("audio enable is not marked unavailable");
    END_TEST(ids.find("audio_enabled") == ids.end());
    TEST("master volume is not marked unavailable");
    END_TEST(ids.find("master_volume") == ids.end());
    TEST("sfx volume is not marked unavailable");
    END_TEST(ids.find("sfx_volume") == ids.end());
}

} // namespace

// ============================================================
// Main
// ============================================================

int main() {
    printf("Menu Navigation Tests\n");
    printf("=====================\n\n");

    test_initial_state_is_main_menu();
    test_start_gameplay_from_main_menu();
    test_toggle_opens_pause_from_gameplay();
    test_toggle_resumes_from_pause();
    test_toggle_noop_on_main_menu();
    test_settings_from_main_menu_roundtrip();
    test_settings_from_pause_roundtrip();
    test_cancel_from_settings_main_menu();
    test_cancel_from_settings_pause();
    test_cancel_from_character();
    test_cancel_from_pause_resumes();
    test_cancel_from_gameplay_opens_pause();
    test_cancel_noop_on_main_menu();
    test_quit_to_menu_from_pause();
    test_character_roundtrip();
    test_screen_mapping();
    test_no_dead_end_screens();
    test_deterministic_transitions();

    printf("\nFocus Wrap Tests\n");
    printf("================\n\n");

    test_wrap_focus_down_past_last();
    test_wrap_focus_up_past_first();
    test_wrap_focus_single_item();
    test_wrap_focus_empty_list();
    test_wrap_focus_out_of_range_normalized();

    printf("\nUnavailable Settings Catalog Tests\n");
    printf("==================================\n\n");

    test_unavailable_settings_catalog_present();
    test_unavailable_settings_entries_complete();
    test_unavailable_settings_ids_unique();
    test_supported_settings_not_unavailable();

    printf("\n");
    if (failures > 0) {
        printf("*** %d TEST(S) FAILED ***\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
