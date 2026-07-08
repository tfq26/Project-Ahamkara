# Report: Pause/Menu State Owner Centralization (Revised)

- **Task**: TASK-20260615-1230-pause-menu-state-owner
- **Date**: 2026-06-15
- **Agent**: opencode (deepseek-v4-pro)
- **Revision**: 2 — removes duplicate `screen_` tracking; `menu_state_` is sole truth

## What Was Implemented

Created `ClientMenuState` (`client/include/ahamkara/client/client_menu_state.h`, `client/src/client_menu_state.cpp`) as the single owner for all pause/menu visibility and screen state.

Previously scattered across:
- `menu_state_` field in `DebugUiController` (screen + visibility)
- `paused_` atomic in `ThreadedLocalRuntime` (simulation pause)
- Inline `glfwSetInputMode` cursor management in `debug_client.cpp`
- Inline ESC detection logic in `debug_client.cpp`

### Files changed (narrow scope)

| File | Change |
|------|--------|
| `client/include/ahamkara/client/client_menu_state.h` | **New** — central pause/menu owner |
| `client/src/client_menu_state.cpp` | **New** — implementation |
| `client/CMakeLists.txt` | Added `client_menu_state.cpp` |
| `client/include/ahamkara/client/debug_ui_controller.h` | Constructor takes `ClientMenuState&` |
| `client/src/debug_ui_controller.cpp` | Delegates visibility/screen/toggle to `ClientMenuState` |
| `client/src/debug_client.cpp` | Creates `ClientMenuState`; single `set_paused()` call site reads from `menu_state` |

### Truly centralized pause control (revision 1)

The `simulation.set_paused()` call now appears at exactly **one** point in the main loop:

```cpp
// At end of frame loop — ClientMenuState is the single source of truth
simulation.set_paused(menu_state.simulation_should_pause());
```

Previously, `set_paused()` was called from three scattered locations in `debug_client.cpp`. Now the simulation pause is a pure consequence of `ClientMenuState::visible()`.

## What Was Validated

- **Build (debug)**: `cmake --build --preset debug` — succeeded (macOS arm64, Apple Clang)
- **Headless build**: `cmake --preset debug-headless` — fails due to pre-existing `ae_render` link dependency in `ahamkara_game` (not caused by this task; `ahamkara_game` includes `ae/render/compiled_level.h` which requires `ae_render`, not built in headless mode)
- **Runtime**: Not tested locally (requires display — same constraint as original task)

## What Was Not Validated

- Runtime menu open/close (requires display)
- Headless tests (blocked by pre-existing headless build issue)

## Remaining Follow-Up

- `DebugUiActions::pause_state_changed` / `pause_simulation` are still populated by render functions but no longer consumed by the client loop — vestigial, can be removed in a follow-up
- Headless preset should separate `ahamkara_game`'s render dependency