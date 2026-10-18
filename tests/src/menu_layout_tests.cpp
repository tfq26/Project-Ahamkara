#include "ae/ui/menu_system.h"

#include <cassert>
#include <iostream>

int main() {
    using ae::ui::menu_coordinate_to_pixels;

    assert(menu_coordinate_to_pixels(0.5f, 1280.0f) == 640.0f);
    assert(menu_coordinate_to_pixels(1.0f, 720.0f) == 720.0f);
    assert(menu_coordinate_to_pixels(54.0f, 620.0f) == 54.0f);
    assert(menu_coordinate_to_pixels(198.0f, 620.0f) == 198.0f);

    ae::ui::MenuSystem menu;
    bool setting_changed = false;
    std::string changed_setting;
    menu.register_action("setting_changed", [&](std::string_view setting) {
        setting_changed = true;
        changed_setting = setting;
    });
    menu.execute_action("setting_changed:suit_color_r");
    assert(setting_changed);
    assert(changed_setting == "suit_color_r");

    std::cout << "Menu mixed-unit layout tests passed.\n";
    return 0;
}
