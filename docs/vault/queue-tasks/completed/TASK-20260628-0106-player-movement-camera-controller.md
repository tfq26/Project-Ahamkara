---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - client
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0106-player-movement-camera-controller-report.md
---

# TASK-20260628-0106-player-movement-camera-controller

## Goal

Extract locomotion, stance, mantle, and first-person camera math out of `World`
into a dedicated player movement/camera controller.

## Background

`World` still owns the fixed-step simulation shell and too much player-adjacent
behavior. The roadmap now calls for clearer ownership boundaries: `Player` owns
player runtime data, while a movement/controller layer should own how the
player moves and how the camera is derived.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../roadmap/roadmap.md)
- [Engine assessment](../../features/2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Introduce a dedicated controller type that owns movement/camera behavior.
- Move all of these out of `World`:
  - jump buffer and coyote time
  - slide timer and crouch gating
  - mantle detection and resolution
  - ladder/ledge handling
  - camera anchor position/yaw/pitch derivation
  - any movement-debug fill helpers that only exist to support that state
- Keep the controller data-oriented and deterministic.
- Preserve current feel and current input semantics.
- Add tests for the controller or for the extracted movement helper functions.

Out of bounds:

- Weapon runtime or presentation work.
- Match state, respawn rules, or player health/shield ownership.
- Rewriting the physics integration or switching away from the current KCC path.
- Netcode/prediction redesign.
- Changing camera feel as part of a design pass instead of a refactor pass.

## Likely Files

- `game/include/ahamkara/game/world.h`
- `game/src/world.cpp`
- `game/include/ahamkara/game/movement.h`
- new `game/include/ahamkara/game/player_movement_controller.h`
- new `game/src/player_movement_controller.cpp`

## Implementation Plan

1. Identify the full movement state currently living in `World`.
2. Define a new controller interface that takes `PlayerInputCommand`, current
   player state, collision context, and the existing movement configuration.
3. Move the helper functions together rather than leaving half of them in
   `World` and half in the new type.
4. Keep `World` responsible only for stepping the sim and passing dependencies
   in/out.
5. Add a focused unit test for the controller behavior and keep the gameplay
   integration tests green.

## Expected Shape

- `World` should call something like `movement_controller.tick(...)` and then
  write the returned player/camera state back to `Player` and the camera anchor.
- The controller should own any state that changes because of movement itself
  rather than because of match logic.
- The controller should not know about weapons, menus, HUD, or match timers.

## Must Preserve

- Current first-person feel, including movement acceleration, jump timing,
  coyote/jump-buffer behavior, slide timing, mantle behavior, and ladder/ledge
  handling.
- Current input semantics for move/look/jump/crouch/sprint/slide.
- Current interaction with the existing physics character controller path.
- Current camera orientation behavior and eye-height logic.
- Current gameplay tests unless a new controller-specific test replaces them.

## Must Not Touch

- Weapon state or weapon switching.
- Match state, respawn state, scores, or kill tracking.
- Menu/HUD state.
- Network prediction/reconciliation logic.
- Any visual-only behavior outside the camera anchor and movement debug output.

## Acceptance Bar

- `World` no longer owns the player locomotion and camera math directly.
- Movement/camera behavior stays stable.
- Build passes and tests stay green.
- No weapon, menu, or match-state logic is folded into the new controller.

## Review Tier

- `medium` - primary reviewer signoff plus a quick sanity pass

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Verify that the extracted controller preserves the current locomotion feel and
that the world still orchestrates simulation timing rather than recomputing
movement details itself.

## Completion Note

- Extracted movement/camera runtime state into `PlayerMovementController`.
- Moved the first-person camera anchor, slide/crouch gating, jump buffer,
  coyote timer, mantle/ladder handling, and movement-debug fill helpers out of
  `World`.
- Kept `World` as the simulation shell that orchestrates physics and gameplay
  progression.
- Added a focused controller test and validated with a debug build plus the
  full debug test suite.
