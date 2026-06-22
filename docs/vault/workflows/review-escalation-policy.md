# Review Escalation Policy

Status: Active

Use this policy to decide whether a queued task needs only primary review or a
two-tier primary plus secondary review.

## Escalation Tiers

- `low` - primary reviewer signoff is enough
- `high` - primary reviewer does the first pass; if it clears obvious flaws, a
  stronger secondary reviewer batch-checks the change before final completion

## Low Escalation

Use `low` for:

- docs and reporting tasks
- queue hygiene and bookkeeping
- small scoped refactors with low behavioral risk
- obvious bug fixes with narrow blast radius
- simple config or naming cleanup

## High Escalation

Use `high` for:

- frame lifecycle and shutdown behavior
- render/present ordering
- threading and ownership changes
- input routing and pause/menu state
- shared engine/runtime abstractions
- tasks likely to hide subtle regressions even if they compile

## Reviewer Roles

- `primary reviewer`:
  cheaper default reviewer; checks scope, obvious flaws, validation evidence,
  and whether the task is ready for stronger review
- `secondary reviewer`:
  stronger reviewer; checks the batch or diff after primary review passes it
  forward

## Two-Tier Flow

For `high` escalation tasks:

1. Worker completes task and writes report.
2. Primary reviewer reviews first.
3. If obvious flaws exist, send back `revise` without escalation.
4. If the change looks sound, write a secondary-review handoff note.
5. Secondary reviewer reviews the diff/report.
6. Primary reviewer records the final decision after reading secondary feedback.

## Batch Secondary Review

For several related `high` escalation tasks, the primary reviewer may prepare a
single batch handoff using
[secondary-review-batch.md](../templates/secondary-review-batch.md).
