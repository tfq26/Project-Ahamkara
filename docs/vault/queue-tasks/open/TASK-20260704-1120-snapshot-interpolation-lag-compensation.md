---
type: opencode-task
status: open
created: 2026-07-04
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - server
  - client
  - engine/network
related_feature:
report:
---

# TASK-20260704-1120-snapshot-interpolation-lag-compensation

## Goal

Split remote snapshot interpolation from hit validation so lag compensation and server rewind are explicit combat/runtime concerns.

## Background

Phase 4 turns the combat loop into a server-authoritative sim path. These slices should establish the authority seam first, then layer prediction, interpolation, and reliability around it.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Phase 4 reconciliation report](../../queue-tasks/completed/TASK-20260622-1100-phase4-reconciliation-replay-fix.md)
- [Phase 4 reliable-channel report](../../queue-tasks/completed/TASK-20260622-1110-phase4-reliable-channel.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep the server as the authority for sim progression and replicated state.
- Keep client code as an input/presentation consumer, not the source of truth.
- Keep the slice deterministic and replay-friendly.
- remote interpolation ownership
- delta/compression handling
- server rewind for hit validation

Out of bounds:

- No matchmaking or service orchestration work.
- No weapon balance or combat tuning pass.
- No HUD redesign beyond thin adapters needed for validation.
- combat balance pass
- activity or service orchestration
- unrelated world-scale streaming work

## Likely Files

  - `game/include/ahamkara/game/net_types.h`
  - `game/src/world.cpp`
  - `game/include/ahamkara/game/world.h`
  - `game/src/client_prediction.cpp`
  - `client/src/local_play.cpp`

## Implementation Plan

1. Audit the current sim/network ownership and identify the explicit seam.
2. Move the authority or transport boundary into the intended subsystem while keeping adapters thin.
3. Add or refresh tests that lock the new flow down.

## Acceptance Bar

- Interpolation is separate from hit validation.
- Server rewind or lag compensation is explicit.
- Build and tests remain green.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

If validation is skipped or fails, explain why in the report.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the slice keeps the ownership boundary explicit and does not leak
runtime authority back into the wrong subsystem.
