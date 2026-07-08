---
type: subagent-report
category: verification
status: validated_with_known_gaps
created: 2026-06-22
agent: opencode
subsystems:
  - game
branch: main (on checkpoint 43ba9cd)
validation:
  - "code inspection (verify-first); no code changed"
---

# Subagent Report

## Task

Deterministic character controller: consolidate movement on fixed dt, expose
tuning via ConfigVar, strengthen movement tests (roadmap Phase 2). Task:
`TASK-20260622-1020-deterministic-character-controller`.

## Status

validated_with_known_gaps — **substantially already implemented**; one clean
residual (ConfigVar wiring).

## Evidence (already done)

- `game/src/movement.cpp`: a Quake/Source-style `accelerate_movement(player,
  sim_state, command, dt, MovementConfig)` with ground/air accel + friction,
  jump buffering, coyote time; plus slope physics, head-bob, landing impulse,
  surface multipliers. Operates purely on replicated state → trivially
  deterministic and headless-testable.
- `game/include/.../movement.h`: a full `MovementConfig` struct of tuning
  constants.
- Runs on fixed dt via the local sim loop (see fixed-timestep report).
- `tests/src/movement_tests.cpp`: substantive deterministic assertions (exact
  velocities given `ground_accel`/dt/wishspeed).

## Known Gap (the real residual)

The `game.player_*` `ae::ConfigVar`s declared in `game/src/game_module.cpp`
(`player_speed`, `player_sprint_mult`, `player_jump_velocity`, `player_gravity`)
have only logging `on_change` callbacks — they are **not wired into
`MovementConfig`**, which uses its hardcoded struct defaults. So the task's goal
"expose tuning via ConfigVar so it is hot-reloadable" is only half-done: the
vars exist but don't affect movement.

## Recommendation

Close as substantially-done, with a small follow-up slice to connect the
`game.player_*` ConfigVars to the `MovementConfig` actually used by
`accelerate_movement` (headless-testable via `movement_tests`). No code changed
here.

## Confidence

high — the movement model, config struct, and tests are evidenced; the
ConfigVar disconnection is evidenced in `game_module.cpp`.
