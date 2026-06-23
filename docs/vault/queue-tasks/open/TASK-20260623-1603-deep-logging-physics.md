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
  - engine/physics
related_feature:
report:
---

# TASK-20260623-1603-deep-logging-physics

## Goal

Instrument `engine/physics` (Jolt integration) with deep, level-gated logging
under category `Physics`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there.

## Scope

In bounds (logging only):
- Physics system init/shutdown, allocator/job-system setup (Info).
- Body create/destroy, shape creation, character controller setup (Debug).
- Contacts/penetration resolution, step counts, activation changes (Trace).
- Invalid shapes / failed body creation / fallbacks (Warning/Error).
- `#define AE_LOG_CATEGORY "Physics"`.

Out of bounds: behavior changes; logging inside the deterministic step at
Info/Warning in steady state (gate to Debug/Trace).

## Likely Files

- `engine/physics/src/*`, `engine/physics/include/*`

## Implementation Plan

1. Add the `Physics` category define per TU.
2. Init/shutdown + body/shape lifecycle (Info/Debug); step/contact detail (Trace).
3. Keep determinism/perf intact — no steady-state hot-path Info/Warning.

## Acceptance Bar

- `Physics` logs at correct levels; default run unchanged; cheap when disabled.
- Fixed-timestep determinism/perf unaffected.
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
