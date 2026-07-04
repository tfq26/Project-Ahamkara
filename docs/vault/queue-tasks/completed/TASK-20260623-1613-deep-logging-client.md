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
  - client
related_feature:
report: reports/subagents/TASK-20260623-1613-deep-logging-client-report.md
review: ../../../reports/subagents/TASK-20260623-1613-deep-logging-client-codex-review.md
---

# TASK-20260623-1613-deep-logging-client

## Goal

Instrument `client` (local/network/sandbox clients, frame pipeline, prediction)
with deep, level-gated logging under category `Client`, per the parent epic's
standard. (High value: the level-load path was silent — make load outcomes
explicit.)

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
in non-headless (GUI) configs — validate with the `debug` preset. (A temporary
level-load log was already added in `debug_client.cpp`; fold it into the
standard.)

## Scope

In bounds (logging only):
- Client startup, config/bindings load, window/renderer bring-up (Info).
- Level load: success/failure, name, mesh-instance + rendered counts (Info/Warn)
  — generalize the existing `debug_client.cpp` log to all client entry points.
- Prediction/reconciliation: replay counts, mispredictions, ack handling (Debug).
- Snapshot apply/interpolation, menu/frontend state transitions (Debug).
- Frame pipeline stage timing/order (Trace, gated).
- `#define AE_LOG_CATEGORY "Client"`.

Out of bounds: behavior changes; per-frame Info spam.

## Likely Files

- `client/src/*` (esp. `debug_client.cpp`, `headless_clients.cpp`,
  `client_frame_pipeline.cpp`, `local_play.cpp`, `threaded_local_runtime.cpp`,
  `client_prediction*`), `client/include/*`

## Implementation Plan

1. Add the `Client` category define per TU.
2. Startup/level-load/menu (Info); prediction/snapshot (Debug); pipeline (Trace).
3. Replace the ad-hoc level-load log with the standard categorized form.

## Acceptance Bar

- `Client` logs at correct levels; default run unchanged; cheap when disabled.
- Level-load outcome is always logged (load ok/fail + counts) from every client.
- Build clean; existing tests (incl. `ahamkara_smoke_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1613-deep-logging-client-report.md`, move to `completed/`.
