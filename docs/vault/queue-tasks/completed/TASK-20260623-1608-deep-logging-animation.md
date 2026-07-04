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
  - engine/animation
related_feature:
report: reports/subagents/TASK-20260623-1608-deep-logging-animation-report.md
review: ../../../reports/subagents/TASK-20260623-1608-deep-logging-animation-codex-review.md
---

# TASK-20260623-1608-deep-logging-animation

## Goal

Instrument `engine/animation` with deep, level-gated logging under category
`Animation`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
only in non-headless (GUI) configs — validate with the `debug` preset.

## Scope

In bounds (logging only):
- Skeleton/clip load + bind, rig setup (Info).
- Blend/state-machine transitions, clip switches (Debug).
- Per-frame sampling/pose evaluation (Trace, gated).
- Missing clips/bones, retarget failures, fallbacks (Warning/Error).
- `#define AE_LOG_CATEGORY "Animation"`.

Out of bounds: behavior changes; per-frame Info spam.

## Likely Files

- `engine/animation/src/*`, `engine/animation/include/*`

## Implementation Plan

1. Add the `Animation` category define per TU.
2. Load/bind (Info); transitions (Debug); sampling (Trace); errors (Warn/Error).

## Acceptance Bar

- `Animation` logs at correct levels; default run unchanged; cheap when disabled.
- Build clean; existing tests green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1608-deep-logging-animation-report.md`, move to `completed/`.
