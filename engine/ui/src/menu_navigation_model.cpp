#include "ae/ui/menu_navigation_model.h"

#include <algorithm>

namespace ae::ui {

void MenuNavigationModel::set_count(int count) {
    count_ = std::max(count, 0);
    enabled_.assign(static_cast<std::size_t>(count_), true);
    ensure_valid_focus();
}

bool MenuNavigationModel::is_enabled(int index) const {
    return index >= 0 && index < count_ && enabled_[static_cast<std::size_t>(index)];
}

void MenuNavigationModel::set_enabled(int index, bool enabled) {
    if (index < 0 || index >= count_)
        return;
    enabled_[static_cast<std::size_t>(index)] = enabled;
    ensure_valid_focus();
}

void MenuNavigationModel::set_focus(int index) {
    if (count_ <= 0) {
        focus_ = -1;
        return;
    }
    const int clamped = std::clamp(index, 0, count_ - 1);
    if (is_enabled(clamped)) {
        focus_ = clamped;
        return;
    }
    // The requested slot is disabled: settle on the nearest enabled slot,
    // preferring the one that follows (matches forward nav order).
    const int fwd = find_next_enabled(clamped, 1);
    if (fwd >= 0) {
        focus_ = fwd;
        return;
    }
    focus_ = find_next_enabled(clamped, -1);
}

void MenuNavigationModel::reset() {
    focus_ = find_next_enabled(-1, 1);
}

int MenuNavigationModel::find_next_enabled(int from_index, int step) const {
    if (count_ <= 0)
        return -1;
    for (int i = 0; i < count_; ++i) {
        const int next = from_index + step;
        const int candidate = ((next % count_) + count_) % count_;
        if (is_enabled(candidate))
            return candidate;
        from_index = candidate;
    }
    return -1;
}

void MenuNavigationModel::ensure_valid_focus() {
    if (focus_ >= 0 && focus_ < count_ && is_enabled(focus_))
        return;
    if (focus_ < 0 || focus_ >= count_) {
        // No in-range anchor (or the list was empty) — start at the first
        // enabled slot.
        reset();
        return;
    }
    // In range but the slot is disabled — move to the nearest enabled one.
    set_focus(focus_);
}

void MenuNavigationModel::move_next() {
    if (!has_focus()) {
        reset();
        return;
    }
    focus_ = find_next_enabled(focus_, 1);
}

void MenuNavigationModel::move_prev() {
    if (!has_focus()) {
        reset();
        return;
    }
    focus_ = find_next_enabled(focus_, -1);
}

} // namespace ae::ui
