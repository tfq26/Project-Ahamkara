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
  - engine/runtime
  - game
related_feature:
report: ../../../reports/subagents/TASK-20260622-1000-fixed-timestep-sim-adoption-report.md
---

# TASK-20260622-1000-fixed-timestep-sim-adoption

## Goal

Make the local simulation advance on the engine's `FixedTimestepAccumulator`
(deterministic fixed dt, decoupled from render rate) and expose an interpolation
alpha to the render path. This is roadmap **Phase 0 — Foundation Hardening**, the
runtime-core milestone everything else (prediction, replay, threading) depends on.

## Roadmap Source

`docs/roadmap/roadmap.md` — Part I, Phase 0 (Foundation Hardening) and Part II
(Runtime Core); also `docs/systems/architecture.md` §1-2.

## Verify First (the tree has in-progress work)

Before implementing, read the CURRENT state and confirm what is already done:
`client/src/local_play.cpp`, `client/src/threaded_local_runtime.cpp`,
`game/src/world.cpp`, `engine/core/include/ae/core/tick.h`
(`FixedTimestepAccumulator`). The architecture doc says the sim historically took
a raw `delta_seconds`; verify whether it now uses the accumulator before changing
anything.

## Scope

In bounds:
- Drive the local sim loop with `FixedTimestepAccumulator`: accumulate real dt,
  run `world.tick(fixed_step, input)` zero-or-more times per frame, guard the
  spiral-of-death (max steps), reset on large pauses/level load.
- Produce `interpolation_alpha()` and make it available to the render path
  (snapshot/scene) so future interpolation can use it.
- Keep gameplay deterministic (fixed dt only; deterministic RNG).

Out of bounds:
- Render-thread split / job system, full ECS migration, visual interpolation of
  every entity (just plumb the alpha).

## Acceptance Bar

- The sim advances at a fixed rate independent of frame rate; no bespoke
  per-delta timing in multiple places.
- Interpolation alpha is produced and reaches the render path.
- Deterministic: same input sequence → same state.
- Build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

Headless is sufficient (sim/world/movement tests run without a display). Add or
extend a determinism/tick test if feasible.

## Reporting Required

Standard: report in `docs/reports/subagents/`, append master log, update task
`report:`/status, move to `review-needed/` or `blocked/`.
