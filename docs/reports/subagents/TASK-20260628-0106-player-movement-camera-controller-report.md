---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: codex
subsystems: [game, client]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0106-player-movement-camera-controller

## Task

Extract locomotion, stance, mantle, and first-person camera math out of
`World` into a dedicated player movement/camera controller.

## Status

implemented_and_validated

## Files Changed

- `game/include/ahamkara/game/camera_anchor.h`
- `game/include/ahamkara/game/player_movement_controller.h`
- `game/src/player_movement_controller.cpp`
- `game/include/ahamkara/game/world.h`
- `game/src/world.cpp`
- `game/src/world_camera.h`
- `game/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/src/player_movement_controller_tests.cpp`

## What Changed

Moved the movement/camera runtime state out of `World` and into a dedicated
`PlayerMovementController`:

1. `CameraAnchor` now lives in its own header so it can be owned by the
   controller without creating include cycles.
2. The controller now owns slide timing, crouch gating, jump buffering, coyote
   time, the movement debug snapshot, and the derived camera anchor.
3. `World` now delegates movement/camera simulation to the controller and keeps
   the fixed-step orchestration shell.
4. Mantle and ladder handling were moved with the rest of the movement runtime
   so the controller owns the player-adjacent math instead of leaving it in
   `World`.
5. A focused controller test was added to cover reset behavior and the derived
   locomotion/camera outputs.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: pass
- 16/16 tests pass

## Known Gaps

- Moving-platform handling remains in the world shell as a separate follow-up
  concern.
- The controller is intentionally data-oriented and still uses the existing
  Jolt-backed character controller path rather than redesigning physics.

## Runtime Risks

Low. The refactor is behavior-preserving on the validated path and the new
controller test locks the extracted state transitions.

## Recommended Next Step

Codex review and queue promotion to `completed/`.

## Confidence

`high` - extracted controller boundary validated by the full debug build and
test suite.
