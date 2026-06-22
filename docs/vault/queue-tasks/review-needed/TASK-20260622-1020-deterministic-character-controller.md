---
type: opencode-task
status: review-needed
created: 2026-06-22
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - engine/runtime
related_feature:
report: ../../../reports/subagents/TASK-20260622-1020-deterministic-character-controller-report.md
---

# TASK-20260622-1020-deterministic-character-controller

## Goal

Consolidate the player character movement into a deterministic, fixed-timestep
controller (accel / friction / air control / jump) with tunable constants, and
strengthen movement test coverage. Roadmap **Phase 2 — Player, Movement & Camera
Feel** (the non-visual, headless-testable half).

## Roadmap Source

`docs/roadmap/roadmap.md` — Part I Phase 2.

## Verify First (the tree has in-progress work)

Read the CURRENT movement code and tests before changing anything:
`game/src/world.cpp` (movement integration), `game/src/movement*`/
`ahamkara/game/movement.h`, `tests/src/movement_tests.cpp`. Confirm what the
controller already does so this is a consolidation, not a rewrite.

## Scope

In bounds:
- One clear deterministic movement integration on fixed dt (ground accel,
  friction, air control, jump; crouch if already present). No variable-delta math.
- Expose key tuning constants via `ae::ConfigVar` (e.g. speed, accel, jump) so
  they are hot-reloadable, with sane defaults.
- Extend `movement_tests` to lock in deterministic behavior (same inputs → same
  trajectory) for the consolidated path.

Out of bounds:
- First-person camera/viewmodel visuals (display-gated), mantle/slide/vault
  polish, netcode prediction (later phase).

## Acceptance Bar

- Movement runs on fixed dt deterministically; tuning constants are config-driven.
- `ahamkara_movement_tests` covers the deterministic behavior and passes.
- Build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
