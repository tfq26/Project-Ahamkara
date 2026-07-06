---
type: opencode-task
status: review-needed
created: 2026-07-04
queued_by: user
assigned_to: opencode
report: reports/subagents/TASK-20260704-1930-reload-animation-sequence-report.md
priority: critical
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - client
  - engine/animation
  - game
related_feature:
report:
---

# TASK-20260704-1930-reload-animation-sequence

## Goal

Add a viewmodel reload animation sequence so the player sees the character's hand(s) remove the magazine/clip, insert a new one, and return to the grip — like professional FPS reloads. The animation should be driven by `WeaponRuntime::reload_timer()`.

## Background

Currently `WeaponAnimationController` tracks a `reload_active_` flag and timer but applies only a procedural animation override (axis-angle rotation). There is no visual clip removal, hand-to-magazine movement, or sequential reload choreography. A proper reload animation has phases: hand moves to magazine → release/remove magazine → hand moves to new magazine → insert → hand returns to grip. The reload timer from `WeaponRuntime` should drive the phase progression.

## Prerequisites

This task depends on viewmodel arm/hand IK (TASK-20260704-1920) being complete — hands must exist in the viewmodel before they can animate.

## First Read

- [Weapon animation controller](../../../client/include/ahamkara/client/weapon_animation_controller.h)
- [Weapon runtime](../../../game/include/ahamkara/game/weapon_runtime.h)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Current animation code](../../../engine/animation/include/ae/animation/)

## Scope

In bounds:

- Define reload animation phases: grab_mag → remove_mag → insert_mag → return.
- Each phase maps to a portion of the weapon's `reload_timer()` duration.
- Animate hand IK targets through each phase (hand moves to magazine position, back to grip).
- Optionally animate the weapon tilt/rotation during reload (weapon pivots to expose magwell).
- Per-weapon reload timing (AR-15, shotgun, rocket launcher all reload differently).
- Keep phase timing data-driven.

Out of bounds:

- No changes to weapon ammo/reload gameplay logic in `WeaponRuntime`.
- No multi-player specific networking changes for animation sync.
- No new mesh or art asset creation.
- No weapon balance changes.

## Likely Files

- `client/src/weapon_animation_controller.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `client/include/ahamkara/client/weapon_viewmodel_data.h`
- `client/src/weapon_presentation.cpp`
- `game/include/ahamkara/game/weapon_runtime.h` (read-only for `reload_timer()`)
- `engine/animation/src/`
- `engine/animation/include/ae/animation/`

## Implementation Plan

1. Read current reload timing and `WeaponAnimationController` flow.
2. Define per-weapon reload phase data (durations, hand target positions, weapon tilt).
3. Extend the animation controller to sequence through phases based on reload timer progress.
4. Update weapon transform per phase (tilt for mag access).
5. Update hand IK targets per phase (hand goes to mag, returns to grip).
6. Validate sync between reload timer completion and animation end.

## Acceptance Bar

- Reload animation has visible phases: hand moves to magazine area, magazine appears to be removed/replaced, hand returns.
- Animation timing matches `WeaponRuntime::reload_timer()` duration.
- Different weapons can have different reload behaviors.
- Build and tests remain green.

## Review Tier

- `medium` - primary reviewer signoff plus sanity pass

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
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the reload animation is driven by presentation-layer state (consuming `WeaponRuntime::reload_timer()` as read-only input), and that ammo/reload gameplay logic in `WeaponRuntime` is not modified.
