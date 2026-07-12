---
type: opencode-task
status: completed
created: 2026-07-04
queued_by: codex
assigned_to: phase4-netc
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - server
related_feature:
report: docs/reports/subagents/TASK-20260704-1100-server-tick-ownership-report.md
---

# TASK-20260704-1100-server-tick-ownership

## Goal

Move authoritative tick ownership onto the server path so sim progression, replicated state, and input consumption have one explicit source of truth.

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
- server-owned tick progression
- input consumption on the authoritative path
- clear sim/input clock alignment

Out of bounds:

- No matchmaking or service orchestration work.
- No weapon balance or combat tuning pass.
- No HUD redesign beyond thin adapters needed for validation.
- prediction/reconciliation redesign
- matchmaking or activity orchestration
- HUD or menu ownership refactors

## Likely Files

  - `server/src/dedicated_server_main.cpp`
  - `game/src/world.cpp`
  - `game/include/ahamkara/game/world.h`
  - `game/include/ahamkara/game/net_types.h`
  - `game/include/ahamkara/game/client_prediction.h`

## Implementation Plan

1. Audit the current sim/network ownership and identify the explicit seam.
2. Move the authority or transport boundary into the intended subsystem while keeping adapters thin.
3. Add or refresh tests that lock the new flow down.

## Acceptance Bar

- The server is the explicit authority for the sim step.
- Client code no longer owns the tick timing contract.
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
