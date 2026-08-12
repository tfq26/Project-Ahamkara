#pragma once

#include <cstddef>
#include <vector>

namespace ae::ui {

// ============================================================================
// MenuNavigationModel — deterministic focus tracking for linear menu lists.
//
// Pure state with no rendering or input dependencies, so it can be unit-tested
// headlessly. A screen's focusable elements are exposed as an ordered list of
// slots; the model guarantees the focused slot is always enabled (or that no
// slot is focused when nothing is enabled). Movement wraps around the list and
// skips disabled slots, so keyboard/controller navigation is deterministic
// regardless of screen content.
//
// Slots are identified by their index in the ordered focusable list. The
// caller is responsible for collecting the list in the same order each frame
// (menu screens are authored statically, so this is stable).
// ============================================================================
class MenuNavigationModel {
  public:
    /// Configure the number of focusable slots. New slots start enabled and
    /// the model keeps its current focus when it is still valid, otherwise it
    /// resets to the first enabled slot (or clears focus).
    void set_count(int count);

    [[nodiscard]] int count() const {
        return count_;
    }
    [[nodiscard]] int focus_index() const {
        return focus_;
    }
    [[nodiscard]] bool has_focus() const {
        return focus_ >= 0;
    }
    [[nodiscard]] bool is_enabled(int index) const;

    void set_enabled(int index, bool enabled);
    void set_focus(int index);
    /// Reset focus to the first enabled slot (or clear it when none enabled).
    void reset();

    /// Move focus forward, wrapping around the list and skipping disabled
    /// slots. No-op when nothing is enabled.
    void move_next();
    /// Move focus backward, wrapping around the list and skipping disabled
    /// slots.
    void move_prev();
    /// Horizontal movement currently mirrors vertical for linear menus; grid
    /// layouts can override these once they are introduced.
    void move_left() {
        move_prev();
    }
    void move_right() {
        move_next();
    }

    /// Returns the currently focused slot, or -1 when nothing is enabled.
    [[nodiscard]] int activate() const {
        return focus_;
    }

  private:
    /// Returns the first enabled slot encountered when stepping by `step`
    /// (1 = forward, -1 = backward) starting from `from_index`. Returns -1
    /// when no enabled slot exists.
    [[nodiscard]] int find_next_enabled(int from_index, int step) const;
    /// Reposition focus onto a valid (enabled) slot, or clear it.
    void ensure_valid_focus();

    int count_ {0};
    int focus_ {-1};
    std::vector<bool> enabled_;
};

} // namespace ae::ui
