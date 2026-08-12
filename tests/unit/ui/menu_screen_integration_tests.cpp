// Integration tests for the Flashback JSON menu shell. Exercises the real
// assets/menus screens through ae::ui::MenuSystem using ImGui's core only
// (no GLFW/OpenGL backend), verifying focus determinism, disabled-button
// skipping, keyboard navigation and screen transitions.
#include "ae/ui/menu_system.h"
#include "imgui.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void run_frame(ae::ui::MenuSystem& menu) {
    ImGui::NewFrame();
    menu.render();
    ImGui::EndFrame();
}

// Simulate one clean key press. ImGui's event-trickle logic drops a press
// that lands in the same frame as a pending release, so the release is
// consumed in its own dedicated frame first.
void tap_key(ae::ui::MenuSystem& menu, ImGuiKey key) {
    run_frame(menu); // consume any pending release from a previous tap
    ImGui::GetIO().AddKeyEvent(key, true);
    run_frame(menu);
    ImGui::GetIO().AddKeyEvent(key, false); // queued; consumed by next tap
}

void test_main_menu_initial_focus_skips_disabled() {
    ae::ui::MenuSystem menu;
    assert(menu.load_from_directory("assets/menus"));
    menu.show_screen("main_menu");

    run_frame(menu);
    // main_menu buttons: CONTINUE (disabled, slot 0), PLAY (1), SETTINGS (2),
    // QUIT TO DESKTOP (3). Initial focus must be the first enabled button.
    assert(menu.has_focus());
    assert(menu.focused_index() == 1);
    std::cout << "test_main_menu_initial_focus_skips_disabled passed.\n";
}

void test_arrow_navigation_is_deterministic() {
    ae::ui::MenuSystem menu;
    assert(menu.load_from_directory("assets/menus"));
    menu.show_screen("main_menu");
    run_frame(menu);
    assert(menu.focused_index() == 1);

    // Down: PLAY -> SETTINGS
    tap_key(menu, ImGuiKey_DownArrow);
    assert(menu.focused_index() == 2);

    // Down: SETTINGS -> QUIT TO DESKTOP
    tap_key(menu, ImGuiKey_DownArrow);
    assert(menu.focused_index() == 3);

    // Down wraps around to PLAY, skipping the disabled CONTINUE slot.
    tap_key(menu, ImGuiKey_DownArrow);
    assert(menu.focused_index() == 1);

    // Up wraps back to QUIT TO DESKTOP.
    tap_key(menu, ImGuiKey_UpArrow);
    assert(menu.focused_index() == 3);

    // Disabled CONTINUE (slot 0) must never receive focus.
    assert(menu.focused_index() != 0);
    std::cout << "test_arrow_navigation_is_deterministic passed.\n";
}

void test_enter_activates_focused_button() {
    ae::ui::MenuSystem menu;
    assert(menu.load_from_directory("assets/menus"));
    bool quit_fired = false;
    menu.register_action("quit_application", [&quit_fired](std::string_view) {
        quit_fired = true;
    });
    menu.show_screen("main_menu");
    run_frame(menu);

    // Move focus to QUIT TO DESKTOP (slot 3) and activate it with Enter.
    tap_key(menu, ImGuiKey_UpArrow);
    assert(menu.focused_index() == 3);
    tap_key(menu, ImGuiKey_Enter);
    assert(quit_fired);

    // Space is also an activation key.
    bool quit_fired_2 = false;
    ae::ui::MenuSystem menu2;
    assert(menu2.load_from_directory("assets/menus"));
    menu2.register_action("quit_application", [&quit_fired_2](std::string_view) {
        quit_fired_2 = true;
    });
    menu2.show_screen("main_menu");
    run_frame(menu2);
    tap_key(menu2, ImGuiKey_UpArrow); // focus QUIT TO DESKTOP
    assert(menu2.focused_index() == 3);
    tap_key(menu2, ImGuiKey_Space);
    assert(quit_fired_2);
    std::cout << "test_enter_activates_focused_button passed.\n";
}

void test_push_and_pop_screen_via_navigation() {
    ae::ui::MenuSystem menu;
    assert(menu.load_from_directory("assets/menus"));
    menu.show_screen("main_menu");
    run_frame(menu);

    // Focus PLAY (slot 1) and activate -> pushes map_select.
    assert(menu.focused_index() == 1);
    tap_key(menu, ImGuiKey_Enter);
    assert(menu.current_screen() == "map_select");

    // The newly pushed screen syncs its focus on the following frame.
    run_frame(menu);
    // map_select focusable list: 5 map cards + BACK. First focus = card 0.
    assert(menu.has_focus());
    assert(menu.focused_index() == 0);

    // Navigate down to BACK (last slot) and activate -> pops back.
    for (int i = 0; i < 5; ++i) {
        tap_key(menu, ImGuiKey_DownArrow);
    }
    assert(menu.focused_index() == 5); // BACK
    tap_key(menu, ImGuiKey_Enter);
    assert(menu.current_screen() == "main_menu");
    std::cout << "test_push_and_pop_screen_via_navigation passed.\n";
}

} // namespace

int main() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0F, 720.0F);
    io.IniFilename = nullptr; // no settings persistence during tests
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0, bytes = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes);
    assert(pixels != nullptr);

    test_main_menu_initial_focus_skips_disabled();
    test_arrow_navigation_is_deterministic();
    test_enter_activates_focused_button();
    test_push_and_pop_screen_via_navigation();

    ImGui::DestroyContext();
    std::cout << "All menu screen integration tests passed.\n";
    return 0;
}
