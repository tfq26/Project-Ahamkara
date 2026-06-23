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
  - engine/input
related_feature:
report:
---

# TASK-20260623-1610-deep-logging-input

## Goal

Instrument `engine/input` with deep, level-gated logging under category `Input`,
per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
only in non-headless (GUI) configs — validate with the `debug` preset.

## Scope

In bounds (logging only):
- Device connect/disconnect (keyboard/mouse/gamepad), binding/config load (Info).
- Action-mapping resolution, rebinds, mode switches (Debug).
- Per-frame raw input sampling (Trace, gated).
- Unmapped actions / device errors (Warning).
- `#define AE_LOG_CATEGORY "Input"`.

Out of bounds: behavior changes; per-frame Info spam.

## Likely Files

- `engine/input/src/*`, `engine/input/include/*`

## Implementation Plan

1. Add the `Input` category define per TU.
2. Device/binding lifecycle (Info); mapping (Debug); raw sampling (Trace).

## Acceptance Bar

- `Input` logs at correct levels; default run unchanged; cheap when disabled.
- Build clean; existing tests green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report:`, move to `review-needed/`.
