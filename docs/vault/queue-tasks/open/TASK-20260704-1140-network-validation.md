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

# TASK-20260704-1140-network-validation

## Goal

Add validation coverage for predicted play under simulated latency/loss so the server-authoritative path has a durable regression check.

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
- latency/loss simulation checks
- predicted combat regression coverage
- server-authoritative hit validation smoke checks

Out of bounds:

- No matchmaking or service orchestration work.
- No weapon balance or combat tuning pass.
- No HUD redesign beyond thin adapters needed for validation.
- new netcode architecture
- combat balance tuning
- renderer or audio changes

## Likely Files

  - `tests/src/`
  - `server/src/dedicated_server_main.cpp`
  - `client/src/headless_clients.cpp`
  - `game/src/world.cpp`

## Implementation Plan

1. Audit the current sim/network ownership and identify the explicit seam.
2. Move the authority or transport boundary into the intended subsystem while keeping adapters thin.
3. Add or refresh tests that lock the new flow down.

## Acceptance Bar

- The slice adds a meaningful network regression check.
- The test output makes the authority assumption explicit.
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
