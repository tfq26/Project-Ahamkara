---
type: opencode-task
status: open
created: 2026-07-04
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: high
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - client
  - engine/render
  - engine/ui
related_feature: features/2026-06-28-engine-assessment.md
report:
---

# TASK-20260704-1020-combat-hit-resolution

## Goal

Split hitscan/projectile damage resolution from feedback so the engine has one
clear combat path for damage, crits, falloff, surfaces, hitmarkers, and damage
event output.

## Background

Phase 3 needs a combat loop that feels like a shooter and is still easy to
reason about. The projectile/hit resolution code already exists, but the
damage/feedback path still needs a clearer slice boundary before abilities and
deeper combat tuning land.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Engine assessment](../2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)
- [World orchestration boundary report](../../queue-tasks/completed/TASK-20260628-0108-world-orchestration-boundary.md)
- [Debug logging game report](../../queue-tasks/completed/TASK-20260623-1612-deep-logging-game.md)

## Scope

In bounds:

- Review the current projectile and damage path for a clean resolution seam.
- Keep hitscan and projectile damage resolution explicit and deterministic.
- Preserve or clarify crit/headshot, falloff, and surface interaction handling.
- Ensure hitmarker / damage feedback flow is driven by combat events rather than
  presentation guesses.
- Update the relevant snapshot or HUD bridge only if needed to keep the signal
  honest.

Out of bounds:

- Ability system work.
- Weapon fire-mode or ammo semantics.
- Final combat balance tuning.
- Animation art or VFX production beyond small wiring needs.

## Likely Files

- `game/src/world_projectile.cpp`
- `game/include/ahamkara/game/world_projectile.h`
- `game/src/world.cpp`
- `game/include/ahamkara/game/world.h`
- `game/include/ahamkara/game/net_types.h`
- `client/src/debug_scene_bridge.cpp`
- `engine/render/src/debug_renderer_hud.cpp`
- `engine/ui/src/hud_element.cpp`

## Implementation Plan

1. Trace the current hitscan/projectile path and identify where resolution and
   feedback are coupled.
2. Separate the damage model and combat resolution from player-facing feedback
   where practical.
3. Keep the feedback path honest so hitmarkers/numbers reflect actual combat
   outcomes.
4. Add or refresh tests that lock down the revised combat behavior.

## Acceptance Bar

- Hitscan/projectile resolution and damage reporting are explicit.
- Hitmarkers / damage feedback reflect real combat outcomes.
- No new ambiguity is introduced into projectile or world orchestration.
- Build and tests remain green.

## Review Tier

- `high` - primary reviewer plus secondary reviewer before completion

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

Confirm the combat signal path is easier to audit and that feedback no longer
guesses at what happened.
