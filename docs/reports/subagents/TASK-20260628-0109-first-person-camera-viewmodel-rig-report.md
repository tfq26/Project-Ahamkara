---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: codex
subsystems: [client, game, engine/render]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0109-first-person-camera-viewmodel-rig

## Task

Wire the active first-person camera to a proper viewmodel rig so the held
weapon is anchored to the camera instead of feeling like a floating debug prop.

## Status

implemented_and_validated

## Files Changed

- `engine/render/include/ae/render/debug_renderer.h`
- `engine/render/src/debug_renderer.cpp`
- `client/src/debug_scene_bridge.cpp`

## What Changed

Made the first-person weapon rig explicit instead of inferring it from general
camera state:

1. `DebugScene` now carries a dedicated first-person rig anchor
   (`viewmodel_position`, `viewmodel_forward`, `viewmodel_right`,
   `viewmodel_up`).
2. `build_debug_scene()` populates that rig directly from the active camera
   anchor so the renderer gets the same source of truth as the rest of the
   first-person view.
3. `DebugRenderer` now consumes the explicit rig fields when constructing the
   weapon transform, keeping the viewmodel path separate from `World` and
   ready for future weapon-specific animation work.
4. Current visibility behavior was preserved: menus still suppress the
   gameplay viewmodel/crosshair, while first-person gameplay keeps the weapon
   anchored to the camera.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Results

- Debug build: pass
- 16/16 tests pass
- No regressions introduced by the rig wiring

## Remaining Notes

- The rig is still using the existing procedural offsets/animation override
  path; this task only makes the camera anchor explicit and keeps the
  presentation layer boundary clean.
