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
  - engine/network
related_feature:
report:
---

# TASK-20260623-1604-deep-logging-network

## Goal

Instrument `engine/network` with deep, level-gated logging under category
`Network`, per the parent epic's Shared Logging Standard.

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there.

## Scope

In bounds (logging only):
- Socket open/bind/connect/close; connection + session lifecycle (Info).
- Reliable channel: acks, retransmits, sequence/bitfield handling (Debug).
- Per-packet send/recv with size/seq (Trace, gated).
- Timeouts, malformed packets, drops, reconnect/fallback (Warning/Error).
- `#define AE_LOG_CATEGORY "Network"`. Never log payload secrets.

Out of bounds: behavior changes; per-packet Info spam.

## Likely Files

- `engine/network/src/*`, `engine/network/include/*`

## Implementation Plan

1. Add the `Network` category define per TU.
2. Connection lifecycle (Info); reliability detail (Debug); per-packet (Trace).
3. Surface error/timeout branches at Warning/Error.

## Acceptance Bar

- `Network` logs at correct levels; default run unchanged; cheap when disabled.
- No payload/secret leakage.
- Build clean; existing tests (incl. `ahamkara_reliable_channel_tests`,
  `ahamkara_session_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, status/`report:`, move to `review-needed/`.
