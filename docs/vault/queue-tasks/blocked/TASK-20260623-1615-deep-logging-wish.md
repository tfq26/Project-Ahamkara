---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: blocked
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: normal
escalation_tier: low
revision: 1
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - wish
related_feature:
report: reports/subagents/TASK-20260623-1615-deep-logging-wish-report.md
review: ../../../reports/subagents/TASK-20260623-1615-deep-logging-wish-codex-review.md
---

# TASK-20260623-1615-deep-logging-wish

## Goal

Instrument `wish` (wish engine: protocol, runtime, integrations) with deep,
level-gated logging under category `Wish`, per the parent epic's standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. See also the
wish docs in `docs/wish/`.

## Scope

In bounds (logging only):
- Wish engine init/shutdown, runtime startup (Info).
- Protocol message parse/dispatch, request/response lifecycle (Debug).
- Integration/bridge events (e.g. Nakama bridge), session lifecycle (Info/Debug).
- Per-message detail (Trace, gated).
- Parse failures, protocol errors, integration failures (Warning/Error).
- `#define AE_LOG_CATEGORY "Wish"`. No secrets/tokens in logs.

Out of bounds: behavior changes; per-message Info spam.

## Likely Files

- `wish/src/*`, `wish/include/*`

## Implementation Plan

1. Add the `Wish` category define per TU.
2. Engine/runtime lifecycle (Info); protocol/session (Debug); per-message (Trace).

## Acceptance Bar

- `Wish` logs at correct levels; default run unchanged; cheap when disabled.
- No secret/token leakage.
- Build clean; existing tests (incl. `ahamkara_nakama_bridge_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1615-deep-logging-wish-report.md`, move to `review-needed/`.

## Codex Review Outcome

Codex found no actual `Wish` logging calls in the diff. Add real category-gated
logging in `wish/integrations/nakama/src/nakama_bridge.cpp` and any other
in-scope `wish` C++ entry points, then resubmit.

## Deferred Note

This slice is deferred until the user says the project is in a working state
again. Do not treat it as active queue work before then.
