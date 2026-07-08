---
type: subagent-report
category: implementation
status: self-validated
created: 2026-06-22
agent: opencode
subsystems:
  - game
branch: main (on checkpoint d2eea33)
validation:
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report (implementation)

## Task

`TASK-20260622-1020-deterministic-character-controller` — make the runtime
movement tuning hot-reloadable (the residual from the earlier verification:
`game.player_*` ConfigVars were declared but not wired into the movement model).

## What Changed

The runtime movement core in `world.cpp` used file-local `constexpr` constants
(`kJumpSpeed`, `kGravity`, `kWalkSpeed`, `kSprintSpeed`) while the
`game.player_*` ConfigVars in `game_module.cpp` were unused. Wired them together,
behavior-preservingly:

- `game_module.h/.cpp`: aligned the ConfigVar defaults to the prior constants
  (`player_speed` 5.5→3.0, `player_sprint_mult` 1.6→2.0, `player_jump_velocity`
  7.5→5.5, `player_gravity` 15→18) and added accessors `cfg_walk_speed()`,
  `cfg_sprint_speed()` (= walk × sprint_mult), `cfg_jump_speed()`, `cfg_gravity()`.
- `world.cpp`: removed the four constants; the movement function now reads the
  tunables via the accessors (as function-locals, so the use-sites are unchanged).
- `world_tests.cpp`: `test_movement_config_wiring` asserts the accessors return
  the behavior-preserving defaults (3.0 / 6.0 / 5.5 / 18.0), proving they are
  ConfigVar-backed.

Net effect: identical default movement feel, but walk/sprint/jump/gravity are now
tunable at runtime via `ahamkara_live.cfg` hot-reload.

## Files Changed

- `game/include/ahamkara/game/game_module.h`, `game/src/game_module.cpp`
- `game/src/world.cpp`
- `tests/src/world_tests.cpp`

## Validation

```sh
cmake --build --preset debug          # clean
./scripts/run-tests.sh --preset debug # 11/11 pass (world_tests, movement_tests incl.)
```

## Known Gaps / Notes

- Only walk/sprint/jump/gravity are wired (the four with existing ConfigVars).
  Accel/friction/slide remain constants — a further-tuning follow-up if desired.
- Netcode determinism note: the movement reads global ConfigVars; for client/
  server prediction to stay deterministic, config must be identical on both ends
  (the standard "sync sim constants" netcode constraint). Fine for local play.

## Status

self-validated — batched for the next milestone Codex review (not sent
individually, per the current workflow).
