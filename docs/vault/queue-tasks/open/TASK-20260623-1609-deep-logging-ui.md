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
  - engine/ui
related_feature:
report:
---

# TASK-20260623-1609-deep-logging-ui

## Goal

Instrument `engine/ui` with deep, level-gated logging under category `UI`,
per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
only in non-headless (GUI) configs — validate with the `debug` preset.

## Scope

In bounds (logging only):
- UI/ImGui init + shutdown, font/atlas load (Info; failures at Error).
- Panel/menu/screen open/close, focus changes, UI actions emitted (Debug).
- Per-frame UI build (Trace, gated).
- `#define AE_LOG_CATEGORY "UI"`.

Out of bounds: behavior changes; per-frame Info spam.

## Likely Files

- `engine/ui/src/*`, `engine/ui/include/*`

## Implementation Plan

1. Add the `UI` category define per TU.
2. Init/shutdown (Info); menu/action transitions (Debug); per-frame (Trace).

## Acceptance Bar

- `UI` logs at correct levels; default run unchanged; cheap when disabled.
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
