---
type: subagent-report
category: implementation
status: validated_with_known_gaps
created: 2026-06-22
agent: opencode
subsystems:
  - game
branch: main (on checkpoint 43ba9cd)
validation:
  - "cmake --build --preset debug"
  - "./scripts/run-tests.sh --preset debug"
---

# Subagent Report

## Task

Phase 4 hardening: fix the first-snapshot reconciliation gap so
`ClientPredictionManager::reconcile()` replays unacknowledged inputs even when
`last_processed_input == 0`. Task: `TASK-20260622-1100-phase4-reconciliation-replay-fix`.

## Status

validated_with_known_gaps (headless-validated; not exercised over a live socket)

## What Was Wrong

`reconcile()` reset the prediction world to authoritative state, then replayed
pending inputs only `if (last_ack_ != 0)`. On the first snapshot (server has
processed nothing → `last_processed_input == 0`), replay was skipped, so the
client's buffered-but-unacked inputs were dropped — a visible snap-back / lost
inputs on the first correction (documented gap, phase4a future-work #1).

## Change

Removed the `last_ack_ != 0` guard: after a reset, the unacked pending inputs are
**always** replayed. This is correct because the discard loop above already
removed server-acknowledged inputs (`sequence <= last_processed_input`), so the
pending queue holds exactly the unacked inputs on every snapshot.

## Files Changed

- `game/src/client_prediction.cpp` (remove guard; update comment)
- `tests/src/world_tests.cpp` (+`test_first_snapshot_reconciliation`)

## Test

`test_first_snapshot_reconciliation` applies two unacked forward inputs, then
reconciles a first snapshot (`last_processed_input = 0`) whose authoritative
position forces a reset, and asserts the post-reconcile state equals an
independently computed **authoritative-then-replay** state (plus a guard that the
inputs actually move the player, so the test is non-vacuous). Under the old
guarded code this fails (state would equal bare authoritative).

## Validation

```sh
cmake --build --preset debug          # clean
./scripts/run-tests.sh --preset debug # 10/10 pass (incl. world_tests)
```

## Scope / Known Gaps

- Only the guard removal + the test; no change to the reconcile threshold,
  discard logic, or fixed-step assumption.
- Not exercised over a real socket here (no live multiplayer harness in this
  environment); validated deterministically at the prediction-manager level.

## Confidence

high — the fix is a precise removal of a documented incorrect guard, with a
deterministic regression test that distinguishes fixed vs. buggy behavior.
