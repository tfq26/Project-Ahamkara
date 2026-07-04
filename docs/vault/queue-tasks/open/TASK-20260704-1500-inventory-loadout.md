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
  - client
related_feature:
report:
---

# TASK-20260704-1500-inventory-loadout

## Goal

Add inventory, loadout, gear/perk/mod, progression, and currency ownership so the game has player-facing collection state.

## Background

Phase 8 makes the game loop richer: inventory, AI, encounters, and persistence. These slices should grow the game systems without mixing in rendering or service orchestration work.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [AI navgrid A* report](../../queue-tasks/completed/TASK-20260622-1300-ai-navgrid-astar.md)
- [AI path follower report](../../queue-tasks/completed/TASK-20260622-1310-ai-path-follower.md)
- [AI navgrid from collision report](../../queue-tasks/completed/TASK-20260622-1320-ai-navgrid-from-collision.md)
- [Milestone review AI nav/movement report](../../queue-tasks/completed/TASK-20260622-1330-milestone-review-ai-nav-and-movement.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep gameplay ownership in the game layer.
- Keep AI and persistence deterministic enough to validate.
- Keep the slice compatible with later combat and service work.
- inventory/loadout data shape
- gear/perk/mod ownership
- progression and currency hooks

Out of bounds:

- No multiplayer service integration.
- No renderer fidelity work.
- No streaming/world-scale refactor.
- matchmaking/service orchestration
- combat balance pass
- render fidelity work

## Likely Files

  - `game/include/ahamkara/game/player.h`
  - `game/src/world.cpp`
  - `game/include/ahamkara/game/gameplay_types.h`
  - `client/src/`

## Implementation Plan

1. Identify the current gameplay seam for the target system.
2. Move ownership into the intended gameplay data/runtime type.
3. Validate the new behavior with focused tests or smoke checks.

## Acceptance Bar

- Inventory/loadout ownership is explicit.
- Player-specific state stays in the player layer.
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
