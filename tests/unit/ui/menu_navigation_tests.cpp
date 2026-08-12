// Unit tests for ae::ui::MenuNavigationModel — the deterministic focus model
// backing the Flashback menu shell. Pure logic: no rendering dependencies.
#include "ae/ui/menu_navigation_model.h"

#include <cassert>
#include <iostream>

namespace {

using ae::ui::MenuNavigationModel;

void test_empty_model_has_no_focus() {
    MenuNavigationModel model;
    assert(model.count() == 0);
    assert(!model.has_focus());
    assert(model.focus_index() == -1);
    assert(model.activate() == -1);
    std::cout << "test_empty_model_has_no_focus passed.\n";
}

void test_set_count_focuses_first_enabled_slot() {
    MenuNavigationModel model;
    model.set_count(3);
    assert(model.count() == 3);
    assert(model.has_focus());
    assert(model.focus_index() == 0);
    std::cout << "test_set_count_focuses_first_enabled_slot passed.\n";
}

void test_reset_skips_disabled_slots() {
    MenuNavigationModel model;
    model.set_count(4);
    model.set_enabled(0, false);
    model.reset();
    assert(model.has_focus());
    assert(model.focus_index() == 1);
    std::cout << "test_reset_skips_disabled_slots passed.\n";
}

void test_move_next_wraps_around() {
    MenuNavigationModel model;
    model.set_count(3);
    model.move_next();
    assert(model.focus_index() == 1);
    model.move_next();
    assert(model.focus_index() == 2);
    model.move_next();
    assert(model.focus_index() == 0); // wraps to first
    std::cout << "test_move_next_wraps_around passed.\n";
}

void test_move_prev_wraps_around() {
    MenuNavigationModel model;
    model.set_count(3);
    model.move_prev();
    assert(model.focus_index() == 2); // wraps to last
    model.move_prev();
    assert(model.focus_index() == 1);
    std::cout << "test_move_prev_wraps_around passed.\n";
}

void test_move_skips_disabled_slots() {
    MenuNavigationModel model;
    model.set_count(5);
    model.set_enabled(1, false);
    model.set_enabled(3, false);
    // Focus is 0; moving forward skips 1 and lands on 2.
    model.move_next();
    assert(model.focus_index() == 2);
    // Moving forward again skips 3 and lands on 4.
    model.move_next();
    assert(model.focus_index() == 4);
    // Moving forward wraps around to the first enabled slot (0).
    model.move_next();
    assert(model.focus_index() == 0);
    // From 0, moving forward skips 1 and lands on 2.
    model.move_next();
    assert(model.focus_index() == 2);
    std::cout << "test_move_skips_disabled_slots passed.\n";
}

void test_move_prev_skips_disabled_slots() {
    MenuNavigationModel model;
    model.set_count(5);
    model.set_enabled(1, false);
    model.set_enabled(3, false);
    model.set_focus(4);
    // Moving backward skips 3 and lands on 2.
    model.move_prev();
    assert(model.focus_index() == 2);
    // Moving backward skips 1 and lands on 0.
    model.move_prev();
    assert(model.focus_index() == 0);
    std::cout << "test_move_prev_skips_disabled_slots passed.\n";
}

void test_all_disabled_clears_focus() {
    MenuNavigationModel model;
    model.set_count(3);
    model.set_enabled(0, false);
    model.set_enabled(1, false);
    model.set_enabled(2, false);
    assert(!model.has_focus());
    assert(model.focus_index() == -1);
    // Movement is a no-op when nothing is enabled.
    model.move_next();
    assert(!model.has_focus());
    model.move_prev();
    assert(!model.has_focus());
    assert(model.activate() == -1);
    std::cout << "test_all_disabled_clears_focus passed.\n";
}

void test_set_focus_clamps_and_avoids_disabled() {
    MenuNavigationModel model;
    model.set_count(3);
    // Out-of-range focus clamps into the valid range.
    model.set_focus(99);
    assert(model.focus_index() == 2);
    model.set_focus(-1);
    assert(model.focus_index() == 0);
    // Focusing a disabled slot moves to the nearest enabled slot.
    model.set_enabled(1, false);
    model.set_focus(1);
    assert(model.has_focus());
    assert(model.focus_index() != 1);
    assert(model.is_enabled(model.focus_index()));
    std::cout << "test_set_focus_clamps_and_avoids_disabled passed.\n";
}

void test_disabling_focused_slot_moves_focus() {
    MenuNavigationModel model;
    model.set_count(3);
    model.set_focus(1);
    model.set_enabled(1, false);
    assert(model.has_focus());
    assert(model.focus_index() != 1);
    std::cout << "test_disabling_focused_slot_moves_focus passed.\n";
}

void test_activate_returns_focused_index() {
    MenuNavigationModel model;
    model.set_count(2);
    model.set_focus(1);
    assert(model.activate() == 1);
    model.move_next();
    assert(model.activate() == 0);
    std::cout << "test_activate_returns_focused_index passed.\n";
}

void test_set_count_preserves_valid_focus() {
    MenuNavigationModel model;
    model.set_count(3);
    model.move_next(); // focus == 1
    model.set_count(3);
    assert(model.focus_index() == 1);
    std::cout << "test_set_count_preserves_valid_focus passed.\n";
}

void test_horizontal_movement_mirrors_vertical() {
    MenuNavigationModel model;
    model.set_count(3);
    model.set_focus(2);
    model.move_right();
    assert(model.focus_index() == 0);
    model.move_left();
    assert(model.focus_index() == 2);
    std::cout << "test_horizontal_movement_mirrors_vertical passed.\n";
}

} // namespace

int main() {
    test_empty_model_has_no_focus();
    test_set_count_focuses_first_enabled_slot();
    test_reset_skips_disabled_slots();
    test_move_next_wraps_around();
    test_move_prev_wraps_around();
    test_move_skips_disabled_slots();
    test_move_prev_skips_disabled_slots();
    test_all_disabled_clears_focus();
    test_set_focus_clamps_and_avoids_disabled();
    test_disabling_focused_slot_moves_focus();
    test_activate_returns_focused_index();
    test_set_count_preserves_valid_focus();
    test_horizontal_movement_mirrors_vertical();
    std::cout << "All menu navigation tests passed.\n";
    return 0;
}
