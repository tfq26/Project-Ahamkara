---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [client, render]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0101-gameplay-ui-separation

## Task

Separate gameplay presentation from menu presentation so the crosshair only appears during active gameplay, not when menus are visible.

## Status

implemented_not_validated — build and tests pass; runtime display confirmation not possible in this headless environment.

## Scope

In bounds: Trace menu/gameplay state flow in the client, make the gameplay overlay explicit, ensure crosshair hidden in menus and visible in gameplay.

Out of bounds: Redesigning menus, changing gameplay balance or camera feel, adding new UI screens.

## Files Changed

- `client/src/debug_scene_bridge.cpp` — Line 121: Changed `scene.menu_visible = false;` to `scene.menu_visible = inputs.menu_visible;`

## What Changed

The `DebugSceneBuildInputs` struct already carried a `menu_visible` bool populated from `menu_state_.visible()` by the frame pipeline (`client_frame_pipeline.cpp:181`), but `build_debug_scene` was ignoring it and always setting `scene.menu_visible = false`. The crosshair check in `stage_render_ui()` (`client_frame_pipeline.cpp:224`) reads `!render_submission_.scene.menu_visible`, which always evaluated to true since the scene value was hardcoded to false. This meant the crosshair was rendered even when menus were open.

Fix: pipe `inputs.menu_visible` through to `scene.menu_visible`. Now:
- `menu_state_.visible()` returns true when menus are open → crosshair hidden
- `menu_state_.visible()` returns false during gameplay → crosshair visible (when in first-person camera mode with `show_crosshair = true`)

No other behavior changed. The `menu_visible` field already existed in both structs and the frame pipeline was already setting it — the wire was simply disconnected in the bridge.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: Pass (only pre-existing entt warnings)
- 14/14 tests pass (0 failures)
- Not runtime-confirmed: this environment has no GL display; visual verification of crosshair behavior during menus requires a display.

## Known Gaps

- Runtime display confirmation not done — crosshair hiding during menus can only be verified with a GL-capable environment.
- No dedicated test for the menu_visible → show_crosshair interaction (tests don't cover UI rendering).

## Runtime Risks

Minimal — the change connects an existing wire; no new code paths or state management introduced. If `menu_state_.visible()` ever returns incorrect values, the crosshair would be incorrectly shown/hidden, but that would be a pre-existing bug in the menu state, not in this change.

## Cross-Agent Dependencies

None. This is a self-contained fix.

## Recommended Next Step

Codex review and, if needed, human runtime confirmation with a GL display (run `./scripts/start.sh local`, toggle Esc to verify crosshair disappears in menus and reappears during gameplay).

## Confidence

`high` — the plumbing is straightforward and verified by existing tests; the only risk is the lack of visual confirmation in this environment.
