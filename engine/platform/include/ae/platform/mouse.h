#pragma once

#include "ae/core/types.h"

namespace ae {

enum class MouseButton : u8 {
    Left = 0,
    Right = 1,
    Middle = 2,
    Count
};

struct MouseState {
    float cursor_x {0.0F};
    float cursor_y {0.0F};
    float delta_x {0.0F};
    float delta_y {0.0F};
    bool button_down[3] {false, false, false};
};

}  // namespace ae
