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
  - engine/render
  - docs
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260704-1000-weapon-runtime-foundation-report.md
---

# TASK-20260704-1000-weapon-runtime-foundation

## Goal

Establish a clean base weapon-runtime seam so player-owned loadout and active
weapon state stay explicit, cache-friendly, and ready for future weapon
subclasses without pulling presentation back into gameplay.

## Background

Phase 3 starts with the weapon runtime boundary. Phase 2 already split the
camera/viewmodel/presentation path, so this slice should make the runtime side
clean enough to support future weapon archetypes, future caching, and future
subagents without reintroducing presentation ownership.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Engine assessment](../2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [World orchestration boundary report](../../queue-tasks/completed/TASK-20260628-0108-world-orchestration-boundary.md)
- [Viewmodel orientation contract report](../../queue-tasks/completed/TASK-20260628-0102-viewmodel-orientation-contract.md)

## Scope

In bounds:

- Audit the current ownership of active weapon runtime, loadout, and player
  runtime state.
- Finish or harden the base `WeaponRuntime` seam so future weapon-specific
  runtime behavior can hang off it without changing presentation code.
- Add or confirm a read-only weapon-model cache seam for runtime-adjacent
  lookup needs, without letting gameplay own rendering concerns.
- Keep the player-owned runtime state explicit and easy to reason about.
- Update docs/comments that describe the runtime boundary if they are stale.

Out of bounds:

- Changing weapon balance, fire behavior, ammo semantics, or damage numbers.
- Building final art assets or animations.
- Reworking weapon presentation or viewmodel layout.
- Touching menu/HUD state unless a doc comment or test absolutely requires it.

## Likely Files

- `game/include/ahamkara/game/weapon_runtime.h`
- `game/src/weapon_runtime.cpp`
- `game/include/ahamkara/game/player.h`
- `game/include/ahamkara/game/weapon_loader.h`
- `game/src/weapon_loader.cpp`
- `game/include/ahamkara/game/weapon_registry.h`
- `engine/render/include/ae/render/weapon_model_cache.h`
- `engine/render/src/weapon_model_cache.cpp`

## Implementation Plan

1. Inspect the current runtime and loadout ownership model and identify any
   remaining ambiguity.
2. Tighten the base runtime seam so active-weapon state stays purely gameplay
   oriented.
3. Confirm any cache or lookup helpers remain read-only from gameplay.
4. Update comments/docs so future weapon work starts from the same contract.

## Acceptance Bar

- `WeaponRuntime` is clearly the base runtime seam for future weapon-specific
  runtime behavior.
- Player-owned weapon state remains explicit and does not drift back into
  `World`.
- Presentation remains outside the runtime seam.
- Build and tests stay green.

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

Confirm the runtime seam stays gameplay-only and that any cache access needed
for weapon identity stays read-only from the runtime path.

## Revision Note

Codex review note: primary review passed, but this is high escalation and needs secondary review confirmation before it can close.
