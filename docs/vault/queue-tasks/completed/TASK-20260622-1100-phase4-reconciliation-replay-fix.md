---
type: opencode-task
status: complete
created: 2026-06-22
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
related_feature:
report: ../../../reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-report.md
review: ../../../reports/subagents/TASK-20260622-1100-phase4-reconciliation-replay-fix-codex-review.md
---

# TASK-20260622-1100-phase4-reconciliation-replay-fix

## Goal

Fix the documented first-snapshot reconciliation gap: `ClientPredictionManager::
reconcile()` skips replaying unacknowledged inputs when `last_processed_input == 0`
(the first snapshot), dropping pre-snapshot inputs. Replay should be unconditional
after a reset. Roadmap **Phase 4** hardening.

## Verify First (done)

`game/src/client_prediction.cpp` wraps the unacked-input replay in
`if (last_ack_ != 0)`. The discard loop already removed server-acked inputs from
`pending_inputs_` (using `sequence <= last_processed_input`), so the remaining
pending inputs are exactly the unacked ones and should always be replayed on the
corrected authoritative base. Confirmed real (matches phase4a future-work item #1).

## Scope

In bounds:
- Remove the `last_ack_ != 0` guard so reconciliation replays the unacked
  pending inputs on every reset, including the first snapshot.
- Add a deterministic test proving first-snapshot reconciliation replays inputs
  (authoritative reset + replayed inputs == authoritative state then those inputs).

Out of bounds:
- Changing the reconcile error threshold, the discard logic, the fixed-step
  assumption, or any other netcode behavior.

## Acceptance Bar

- After a reset on the first snapshot, unacked pending inputs are replayed.
- A test verifies the post-reconcile state equals authoritative-then-replay.
- Build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
