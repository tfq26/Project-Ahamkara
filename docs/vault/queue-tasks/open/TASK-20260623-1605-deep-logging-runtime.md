---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: open
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/runtime
related_feature:
report:
---

# TASK-20260623-1605-deep-logging-runtime

## Goal

Instrument `engine/runtime` with deep, level-gated logging under category
`Runtime`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there.

## Scope

In bounds (logging only):
- `Application` start/shutdown, runtime mode selection (Info).
- Fixed-timestep accumulator: step counts, spiral-of-death guard trips (Debug).
- Per-frame / per-tick milestones (Trace, gated).
- Startup failures / mode misconfig (Warning/Error).
- `#define AE_LOG_CATEGORY "Runtime"`.

Out of bounds: behavior changes; per-frame Info spam; anything that perturbs
fixed-timestep timing in steady state.

## Likely Files

- `engine/runtime/src/*`, `engine/runtime/include/*`

## Implementation Plan

1. Add the `Runtime` category define per TU.
2. Lifecycle (Info); accumulator/spiral-guard (Debug); frame/tick (Trace).

## Acceptance Bar

- `Runtime` logs at correct levels; default run unchanged; cheap when disabled.
- Fixed-timestep behavior/determinism unaffected.
- Build clean; existing tests green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, status/`report:`, move to `review-needed/`.
