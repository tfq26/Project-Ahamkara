#pragma once

namespace ae::ui {

// Returns the list index reached after moving `delta` steps from
// `current_index` over `count` items.  Both ends wrap, so menu focus never
// escapes the list: moving down past the last item returns to the first and
// moving up past the first returns to the last.
//
// The result is normalized into [0, count) even when `current_index` is out
// of range.  For an empty list (`count <= 0`) the result is 0.
[[nodiscard]] constexpr int wrap_focus(int current_index, int count, int delta) {
    if (count <= 0)
        return 0;
    int index = ((current_index % count) + count) % count;
    index = (index + delta) % count;
    if (index < 0)
        index += count;
    return index;
}

} // namespace ae::ui
