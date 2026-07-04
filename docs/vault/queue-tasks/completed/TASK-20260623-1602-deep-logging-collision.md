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
  - engine/collision
related_feature:
report: reports/subagents/TASK-20260623-1602-deep-logging-collision-report.md
---

# TASK-20260623-1602-deep-logging-collision

## Goal

Instrument `engine/collision` with deep, level-gated logging under category
`Collision`, following the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601 (logging levels + gating). Read the Shared Logging
Standard there before starting.

## Scope

In bounds (logging only):
- Collision world build/teardown and collider-set load (Info, with counts).
- Broadphase/narrowphase queries, ray/AABB/sweep tests (Debug/Trace, gated).
- Degenerate/empty/missing-collider and fallback branches (Warning).
- Use `#define AE_LOG_CATEGORY "Collision"` + the `*_cat` helpers.

Out of bounds: behavior changes, refactors, per-frame Info spam.

## Likely Files

- `engine/collision/src/*`, `engine/collision/include/*`

## Implementation Plan

1. Add the `Collision` category define per TU.
2. Log build/load (Info), query/test detail (Debug/Trace), errors (Warning/Error).
3. Keep hot-path query logs gated to Debug/Trace.

## Acceptance Bar

- Meaningful `Collision` logs at correct levels; nothing new at default level.
- No per-frame Info spam; disabled logs are cheap.
- Build clean; existing tests (incl. `ahamkara_collision_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, status/`report:`, move to `review-needed/`.
