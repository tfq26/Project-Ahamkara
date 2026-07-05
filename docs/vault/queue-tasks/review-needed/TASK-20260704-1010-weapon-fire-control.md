---
type: opencode-task
status: review-needed
created: 2026-07-04
queued_by: codex
assigned_to: oz
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - engine/render
  - client
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260704-1010-1020-1030-combat-core-report.md
---

# TASK-20260704-1010-weapon-fire-control

## Goal

Make weapon firing semantics explicit and deterministic: fire modes, ammo,
reserves, reload timing, RPM, and recoil/spread should all be owned by the
combat runtime path rather than hidden in presentation or ad hoc world code.

## Background

This follows the weapon runtime foundation slice. The engine already has a
weapon registry, active runtime state, and a presentation boundary. The next
step is to make the combat control loop itself predictable and data-driven for
future AR/shotgun/pistol/rocket work.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Engine assessment](../2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [First-person camera + viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)

## Scope

In bounds:

- Make the firing path read directly from weapon archetype/runtime data.
- Keep fire mode handling explicit for automatic, semi-automatic, hitscan, and
  projectile weapons.
- Keep ammo, reserves, reload timing, RPM, and cooldown logic deterministic.
- Wire recoil/spread generation through the existing deterministic RNG path.
- Preserve current gameplay feel unless the task note says otherwise.

Out of bounds:

- Damage model redesign.
- Final weapon balance pass.
- Animation art production.
- Player or world ownership refactors outside firing/control needs.

## Likely Files

- `game/include/ahamkara/game/weapon_runtime.h`
- `game/src/weapon_runtime.cpp`
- `game/include/ahamkara/game/weapon_loader.h`
- `game/src/weapon_loader.cpp`
- `game/include/ahamkara/game/weapon_registry.h`
- `game/src/world_projectile.cpp`
- `game/include/ahamkara/game/world_projectile.h`
- `game/src/world.cpp`

## Implementation Plan

1. Audit the current weapon firing path and identify any implicit special
   casing.
2. Normalize fire mode, ammo, reserve, reload, and cooldown handling through
   the runtime data model.
3. Route recoil/spread through deterministic state rather than ad hoc math.
4. Keep current visible behavior stable unless the current implementation is
   demonstrably inconsistent.

## Acceptance Bar

- Fire mode, ammo, reserve, reload, and cooldown behavior are explicit and
  data-driven.
- Recoil/spread is deterministic and reproducible.
- Current build and gameplay tests remain green.

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

Confirm the firing path is still gameplay-owned and that no presentation logic
leaks back into the runtime or projectile step code.
