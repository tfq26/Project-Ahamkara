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
  - engine/platform
related_feature:
report:
---

# TASK-20260623-1606-deep-logging-platform

## Goal

Instrument `engine/platform` (window/GL context/OS integration) with deep,
level-gated logging under category `Platform`, per the parent epic's standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: this
component builds only in non-headless (GUI) configs (`engine/platform` is added
under `AHAMKARA_BUILD_CLIENT`), so validate with the `debug` preset.

## Scope

In bounds (logging only):
- Window create/destroy, GL context creation, swap interval, fullscreen (Info).
- Resize, focus, raw-mouse-motion availability (Debug).
- Window/GL creation failures + fallbacks (Warning/Error).
- `#define AE_LOG_CATEGORY "Platform"`.

Out of bounds: behavior changes; per-frame Info spam.

## Likely Files

- `engine/platform/src/*`, `engine/platform/include/*`

## Implementation Plan

1. Add the `Platform` category define per TU.
2. Window/context lifecycle (Info); events (Debug); failures (Warning/Error).

## Acceptance Bar

- `Platform` logs at correct levels; default run unchanged; cheap when disabled.
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
