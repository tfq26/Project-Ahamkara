---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-22
agent: opencode
subsystems:
  - client
branch: main (on checkpoint 43ba9cd)
validation:
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Reduce overlap in input routing so important menu controls (notably ESC/pause)
aren't double-consumed and the routing is clearer. Task:
`TASK-20260615-1245-input-routing-cleanup` (retired local task record).

## Status

validated_with_known_gaps

## What Was Wrong

`ClientFramePipeline::stage_handle_menu_and_hotkeys()` detected the ESC menu
toggle **two redundant ways** and OR'd them:

```cpp
static bool esc_was_down = false;                       // process-global state
bool esc_is_down = glfwGetKey(glfw_win, GLFW_KEY_ESCAPE) == GLFW_PRESS; // raw GLFW
bool esc_just_pressed = esc_is_down && !esc_was_down;
const bool menu_toggle = esc_just_pressed
    || window_.is_key_pressed(ae::KeyCode::Escape)      // ...same edge again
    || debug_state.is_code_pressed(controller_bindings_.menu);
```

The first path is a raw-GLFW edge-detect with a `static` (process-global) flag
that **bypasses the platform input abstraction** and **duplicates** exactly what
`window_.is_key_pressed(Escape)` already provides.

## Change

Removed the raw-GLFW ESC edge-detect and the `static esc_was_down`; the menu
toggle now routes through the single platform edge-trigger + the controller
binding:

```cpp
const bool menu_toggle =
    window_.is_key_pressed(ae::KeyCode::Escape)
    || debug_state.is_code_pressed(controller_bindings_.menu);
```

`glfw_win` is retained (still used for cursor-mode `glfwSetInputMode`).

## Why This Is Safe (behavior-preserving)

Verified in `engine/platform/src/window_glfw.cpp` that `is_key_pressed` is a
genuine per-frame edge trigger:
- `poll_events()` clears `key_pressed_this_frame_` each frame (line 316), then
  `glfwPollEvents()` runs the key callback;
- the key callback sets `key_pressed_this_frame_[idx] = true` on `GLFW_PRESS`
  (line 283); `is_key_pressed` returns that array (line 340).
- Pipeline order guarantees freshness: Stage 1 `stage_poll_input()` runs before
  Stage 2 `stage_handle_menu_and_hotkeys()`.

So `is_key_pressed(Escape)` is functionally identical to the removed manual dance.

## Files Changed

- `client/src/client_frame_pipeline.cpp` (`stage_handle_menu_and_hotkeys`)

## Validation

```sh
cmake --build --preset debug          # client relinked, clean
./scripts/run-tests.sh --preset debug # 10/10 pass
```

Note: the task listed `debug-headless`, but this is client code (excluded from
the headless preset), so `debug` was used.

## Validation Results

- Build-validated: yes (clean). Test-validated: yes (10/10).
- Runtime-confirmed: NO — no GL display here, so ESC-opens/closes-menu was not
  observed live; equivalence is established by code analysis above.

## Scope

In bounds: removed one duplicated/bypassed ESC path; kept localized. Out of
bounds (untouched): full input-system/rebind rewrite, controller-navigation
redesign.

## Known Gaps / Follow-ups

- Input ownership is still spread across several mechanisms:
  `window_.is_key_pressed`, gamepad `debug_state` code bindings,
  `ae::input::InputMap` (used inside `DebugUiController`), and
  `process_debug_hotkeys`. A future slice could unify menu/gameplay actions on
  `InputMap`. Documented here per the task; out of scope for this localized fix.

## Cross-Agent Dependencies / Collision

- This edits `client/src/client_frame_pipeline.cpp`, which is owned by the
  still-`claimed` `TASK-20260615-1200-client-frame-pipeline`. The change is a
  small, self-contained block inside `stage_handle_menu_and_hotkeys`; flagging
  for the reviewer in case that task is revised concurrently. Checkpoint commit
  `43ba9cd` is a clean restore point.

## Recommended Next Step

If desired, a follow-up slice can route the ESC/menu toggle through `InputMap`
(a `Menu` action already exists) so keyboard + controller share one action path.

## Confidence

high — the duplication is removed, the equivalence is verified against the
platform implementation, and build + tests are green.
