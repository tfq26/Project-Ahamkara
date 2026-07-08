---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - client
  - game
  - engine/render
  - engine/animation
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0107-weapon-presentation-separation-report.md
---

# TASK-20260628-0107-weapon-presentation-separation

## Goal

Separate weapon presentation from weapon runtime so attachments, viewmodels,
and animation live outside the pure ammo/reload/cooldown state.

## Background

`WeaponRuntime` is the right place for ammo and reload state, but the engine
still needs a cleaner seam for how weapons look and animate. This task keeps the
runtime lean while paving the way for swappable model parts, animation clips,
and future weapon-specific presentation logic.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../roadmap/roadmap.md)
- [Engine assessment](../../features/2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Introduce or finish a presentation layer for weapons that sits beside runtime.
- Keep `WeaponRuntime` focused only on ammo, reload, cooldown, equip state,
  and other pure gameplay timing/state.
- Move these concerns out of runtime:
  - viewmodel selection
  - attachment selection and attachment transforms
  - model cache lookup / weapon model resolution
  - animation-facing state (pose, clip selection, presentation-only timers)
  - any per-weapon render metadata that does not affect gameplay rules
- Preserve the current firing/reload behavior and current tests.
- If a shared cache is needed, make it explicitly read-only from the runtime
  side.

Out of bounds:

- Reworking weapon balance, damage, or firing rules.
- Full art production of every weapon.
- Network replication changes beyond what the presentation seam needs.
- Moving gameplay ammo/reload semantics into the client presentation layer.
- Creating a new inheritance hierarchy unless a concrete polymorphic need exists.

## Likely Files

- `game/include/ahamkara/game/weapon_runtime.h`
- `game/src/weapon_runtime.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `client/include/ahamkara/client/weapon_presentation.h`
- `client/src/weapon_animation_controller.cpp`
- `client/src/weapon_presentation.cpp`
- `engine/render/include/ae/render/weapon_model_cache.h`
- `engine/render/src/weapon_model_cache.cpp`

## Implementation Plan

1. Define the runtime/presentation boundary in code first, before moving logic.
2. Keep runtime state transitions in `WeaponRuntime` and presentation state in a
   separate type.
3. Move model lookup / attachment / animation-facing logic into the presentation
   layer one concern at a time.
4. Keep any shared model cache behind the presentation layer, not the runtime.
5. Validate that switching, firing, reloading, and current tests still work.

## Expected Shape

- Runtime should answer: "what weapon is active, how much ammo is left, and is
  the weapon ready?"
- Presentation should answer: "what model/attachment/pose should be shown for
  that active weapon?"
- The runtime should not need to know whether the weapon is rendered as a rifle,
  a mesh hierarchy, or a future animated character-held item.

## Must Preserve

- Current ammo counts, reload timing, cooldown timing, and equip semantics.
- Current weapon switching behavior and slot mapping.
- Current tests for `WeaponRuntime` and gameplay flows that depend on it.
- Current build paths for headless/client/server targets.
- The ability to add future weapon types without rewriting the runtime seam.

## Must Not Touch

- Damage math, weapon balance, or projectile/hitscan behavior.
- Match or player state ownership outside weapon runtime.
- Menu/HUD presentation.
- Network snapshot shape unless the presentation seam truly requires it.
- Any art/content creation work that is not directly needed to prove the seam.

## Acceptance Bar

- Weapon runtime no longer owns presentation concerns.
- Presentation code can vary by weapon without changing ammo/reload semantics.
- Build passes and tests stay green.
- Weapon presentation can be swapped or extended without touching weapon ammo
  semantics.

## Review Tier

- `medium` - primary reviewer signoff plus a quick sanity pass

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report: reports/subagents/TASK-20260628-0107-weapon-presentation-separation-report.md` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the runtime layer remains reusable for future weapon types and that the
presentation layer is the only place that knows about viewmodel/model specifics.

## Completion Note

- Clamped the weapon joint upload path to the renderer buffer limit so rigs
  larger than 8 joints cannot overflow `weapon_joint_matrices`.
- Restored respawn/restart weapon-runtime reset and preserved the prior
  gameplay contract, including the old 150-round reserve ammo on reset.
- Switched damage-number feedback to use net post-armor damage instead of raw
  input damage.
- Added regression tests for respawn/restart weapon reset and actual damage
  feedback. Build + test suite pass.
