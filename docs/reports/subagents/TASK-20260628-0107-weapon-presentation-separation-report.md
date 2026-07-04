---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [client, game, engine/render, engine/animation]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0107-weapon-presentation-separation

## Task

Separate weapon presentation from weapon runtime so viewmodels, animation, and model cache live outside the pure ammo/reload/cooldown state.

## Status

implemented_and_validated

## Files Changed

- `game/include/ahamkara/game/player.h`
- `game/src/player.cpp`
- `game/src/world.cpp`
- `client/src/client_frame_pipeline.cpp`
- `tests/src/gameplay_tests.cpp`
- `tests/src/world_tests.cpp`

## What Changed

Finished the presentation/runtime separation and fixed the review regressions:

1. `WeaponViewmodelPresentation` continues to own weapon model resolution and
   animation playback in the client layer.
2. The world respawn/restart path now restores the prior gameplay contract by
   resetting the active weapon runtime and refilling reserve ammo to the old
   150-round value.
3. The client-side weapon joint copy now clamps to the renderer buffer size so
   larger future rigs cannot overflow `weapon_joint_matrices`.
4. Damage numbers now use post-armor health damage instead of raw input damage.
5. Regression tests were added for respawn/restart weapon reset and player
   damage feedback.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Results

- Debug build: pass
- 15/15 tests pass
- No regressions introduced by the final review fixes

## Remaining Notes

- `WeaponViewmodelPresentation` and `WeaponModelCache` are still the correct
  presentation-layer home for weapon meshes and animation; this task is now
  complete and no further runtime coupling was added.
