---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: completed
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
related_feature:
report: reports/subagents/TASK-20260623-1612-deep-logging-game-report.md
review: ../../../reports/subagents/TASK-20260623-1612-deep-logging-game-codex-review.md
---

# TASK-20260623-1612-deep-logging-game

## Goal

Instrument `game` (World/simulation/activities) with deep, level-gated logging
under category `Game`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there.

## Scope

In bounds (logging only):
- World construction, world-definition/level + collider load (Info, with counts).
- Match/weapon/reload state transitions, spawn/respawn, switch (Info/Debug).
- Projectile/dummy spawn + death, damage application, hit/kill events (Debug).
- ECS registry create/destroy of authoritative entities (Debug/Trace).
- Per-tick milestones (Trace, gated).
- Missing weapon/asset/world-definition, invalid state (Warning/Error).
- `#define AE_LOG_CATEGORY "Game"`.

Out of bounds: behavior changes; logging inside the deterministic fixed-timestep
tick at Info/Warning in steady state (gate to Debug/Trace) — preserve
determinism and perf.

## Likely Files

- `game/src/*` (esp. `world.cpp`, `world_projectile.cpp`, `world_dummy_sim.cpp`,
  `activities/*`, `deathmatch_mode.cpp`), `game/include/*`

## Implementation Plan

1. Add the `Game` category define per TU.
2. World/level load + match/weapon transitions (Info/Debug); entity + damage
   events (Debug); per-tick (Trace).

## Acceptance Bar

- `Game` logs at correct levels; default run unchanged; cheap when disabled.
- Fixed-timestep determinism/perf unaffected; no per-tick Info spam.
- Build clean; existing tests (incl. `ahamkara_world_tests`,
  `ahamkara_gameplay_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1612-deep-logging-game-report.md`, move to `completed/`.
