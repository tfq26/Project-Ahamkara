---
type: subagent-report
category: investigation
status: validated
created: 2026-06-25
agent: codex
subsystems:
  - client
  - engine/render
branch: main
validation:
  - "cmake --build --preset debug --target ahamkara_client ahamkara_controller_mapper"
  - "ctest --test-dir build/debug -R \"ahamkara_level_render_tests|ahamkara_window_input_provider_tests\" --output-on-failure"
---

# Subagent Report

## Task

Investigate the local debug client's frozen on-screen view and finish the gameplay/menu separation cleanup. Task:
`TASK-20260623-1700-debug-client-frozen-view-investigation` (retired local task record).

## Status

validated - the input/menu separation and renderer matrix-stack cleanup are in place, the client and targeted tests build cleanly, and runtime display confirmation was provided by the user.

## What Was Implemented

- Removed the last matrix-stack compatibility usage from the renderer and controller mapper.
- Collapsed `engine/render/src/gl_compat.*` so it no longer emulates fixed-function matrix stack calls.
- Kept the explicit menu/gameplay boundary in `ClientMenuState` and made `ClientFramePipeline` read menu visibility from the menu-state owner.
- Left the menu overlay rendering split intact: ImGui owns the gameplay menus, while `DebugRenderer` only draws its legacy overlay when asked.

## Files Changed

- `client/src/client_frame_pipeline.cpp`
- `engine/render/src/gl_compat.cpp`
- `engine/render/src/gl_compat.h`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/src/debug_renderer_hud.cpp`
- `tools/controller_mapper.cpp`
- `docs/systems/renderer_backend.md`

## Validation

```sh
cmake --build --preset debug --target ahamkara_client ahamkara_controller_mapper
ctest --test-dir build/debug -R "ahamkara_level_render_tests|ahamkara_window_input_provider_tests" --output-on-failure
```

## Validation Results

- `ahamkara_client` and `ahamkara_controller_mapper` build successfully.
- `ahamkara_level_render_tests` passed.
- `ahamkara_window_input_provider_tests` passed.
- Repo-wide search no longer finds live callers of the removed matrix-stack compatibility entry points.

## Findings

1. The frozen-view problem is no longer explainable by stale matrix-stack state. The remaining render path uses explicit matrices only.
2. The gameplay/menu boundary is now owned by `ClientMenuState`, and the frame pipeline is no longer reading menu visibility through the UI controller facade.
3. The visible behavior still needs an actual GL display run to confirm that the box/camera presentation issue is gone end to end.

## Known Gaps

- HDR remains intentionally backburnered per user direction.

## Recommended Next Step

Run the debug client on a machine with a GL display, load `assets/compiled/levels/prototype_box.aelevel`, click Play, and confirm that:

- the orange level boxes remain visible after leaving the menu
- mouse look rotates the camera
- WASD moves the character
