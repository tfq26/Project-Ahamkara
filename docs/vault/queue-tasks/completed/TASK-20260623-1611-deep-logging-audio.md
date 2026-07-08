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
  - engine/audio
related_feature:
report: reports/subagents/TASK-20260623-1611-deep-logging-audio-report.md
review: ../../../reports/subagents/TASK-20260623-1611-deep-logging-audio-codex-review.md
---

# TASK-20260623-1611-deep-logging-audio

## Goal

Instrument `engine/audio` with deep, level-gated logging under category `Audio`,
per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
only in non-headless (GUI) configs — validate with the `debug` preset.

## Scope

In bounds (logging only):
- Audio engine init/shutdown, device/backend selection, master volume (Info).
- Sound/clip load, bus/category config, listener setup (Debug).
- Play/stop events, voice allocation (Trace, gated; avoid per-sound Info spam).
- Failed device init, missing sound assets, fallbacks (Warning/Error).
- `#define AE_LOG_CATEGORY "Audio"`.

Out of bounds: behavior changes; per-event Info spam.

## Likely Files

- `engine/audio/src/*`, `engine/audio/include/*`

## Implementation Plan

1. Add the `Audio` category define per TU.
2. Engine/device lifecycle + loads (Info/Debug); play events (Trace); errors.

## Acceptance Bar

- `Audio` logs at correct levels; default run unchanged; cheap when disabled.
- Build clean; existing tests green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1611-deep-logging-audio-report.md`, move to `completed/`.
