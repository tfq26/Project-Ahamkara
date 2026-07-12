---
type: opencode-task
status: completed
created: 2026-07-04
queued_by: codex
assigned_to: oz
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - client
  - engine/ui
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260704-1010-1020-1030-combat-core-report.md
---

# TASK-20260704-1030-combat-abilities-core

## Goal

Add the core melee / grenade / class-ability plumbing as explicit player-owned
combat state with cooldowns and energy, so future combat slices have a stable
runtime seam to build on.

## Background

After the weapon runtime and combat resolution slices, the next phase-3 gap is
the rest of the sandbox combat loop. This task should keep ability ownership
player-specific and deterministic rather than mixing it back into world or UI
state.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Engine assessment](../2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)
- [World orchestration boundary report](../../queue-tasks/completed/TASK-20260628-0108-world-orchestration-boundary.md)
- [Player movement/camera controller report](../../queue-tasks/completed/TASK-20260628-0106-player-movement-camera-controller.md)

## Scope

In bounds:

- Define or harden the runtime state needed for melee, grenade, and class
  abilities.
- Keep cooldown/energy logic deterministic and player-owned.
- Add the smallest useful input/state plumbing needed to exercise ability use.
- Surface any required HUD state through the existing UI path without making the
  UI own the combat rules.

Out of bounds:

- Final animation, VFX, or audio production.
- Weapon fire-mode and ammo changes outside of ability interactions.
- Matchmaking, networking, or scoreboard redesign.
- Moving quest / buff / debuff / progression into the player class.

## Likely Files

- `game/include/ahamkara/game/player.h`
- `game/src/world.cpp`
- `game/include/ahamkara/game/net_types.h`
- `client/src/window_input_provider.cpp`
- `client/include/ahamkara/client/controller_bindings.h`
- `engine/ui/src/hud_element.cpp`
- `game/include/ahamkara/game/gameplay_types.h`

## Implementation Plan

1. Identify the current ability input and state plumbing.
2. Add or tighten player-owned cooldown/energy state where it belongs.
3. Keep UI and input as adapters, not owners, of combat rules.
4. Verify the ability state is still deterministic and easy to save/replicate
   later.

## Acceptance Bar

- Ability cooldown and energy are player-owned and explicit.
- Input and HUD act as adapters, not combat rule owners.
- The task does not pull unrelated quest/buff/progression state into `Player`.
- Build and tests remain green.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the combat ability path stays player-owned and that UI/input only act
as thin adapters.
