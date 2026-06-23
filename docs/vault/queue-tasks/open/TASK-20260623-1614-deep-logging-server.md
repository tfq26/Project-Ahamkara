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
  - server
related_feature:
report:
---

# TASK-20260623-1614-deep-logging-server

## Goal

Instrument `server` (dedicated server) with deep, level-gated logging under
category `Server`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there.

## Scope

In bounds (logging only):
- Server start/listen/bind, shutdown (Info; bind failures at Error).
- Client connect/disconnect/handshake, slot assignment (Info).
- Authoritative tick milestones, snapshot build + send (Debug; per-tick Trace).
- Activity/deathmatch lifecycle, match phase transitions, scoring (Debug).
- Anti-cheat triggers, dropped/invalid inputs (Warning).
- `#define AE_LOG_CATEGORY "Server"`.

Out of bounds: behavior changes; per-tick Info spam; anything perturbing the
authoritative fixed-timestep loop in steady state.

## Likely Files

- `server/src/*` (esp. `dedicated_server_main.cpp`) and the activity code it
  drives in `game/src/activities/*` (log under `Server` where server-owned).

## Implementation Plan

1. Add the `Server` category define per TU.
2. Listen/connect lifecycle (Info); tick/snapshot (Debug/Trace); anti-cheat (Warn).

## Acceptance Bar

- `Server` logs at correct levels; default run unchanged; cheap when disabled.
- Authoritative loop determinism/perf unaffected.
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
