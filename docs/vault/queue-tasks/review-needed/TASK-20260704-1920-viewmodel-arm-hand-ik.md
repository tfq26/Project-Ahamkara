---
type: opencode-task
status: open
created: 2026-07-04
queued_by: user
assigned_to: opencode
priority: critical
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - client
  - engine/render
  - engine/animation
related_feature:
report:
---

# TASK-20260704-1920-viewmodel-arm-hand-ik

## Goal

Add a first-person arm and hand rig with IK (inverse kinematics) so the player sees hands holding the weapon at the correct grip positions. Hands should stay attached to the gun during idle, movement, and weapon switching.

## Background

Currently the viewmodel only shows the weapon — there are no arms or hands visible. Professional FPS games render the character's hands gripping the weapon in specific positions (right hand on trigger/grip, left hand on foregrip/magwell). The arms connect from the camera origin (player's eye) down to the hands at the weapon grip points. This requires an IK solver that adjusts arm bone positions to keep hands locked to the weapon's grip sockets as the weapon moves/sways.

## First Read

- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Current animation code](../../../engine/animation/)
- [Weapon animation controller](../../../client/include/ahamkara/client/weapon_animation_controller.h)
- [Current viewmodel data](../../../client/include/ahamkara/client/weapon_viewmodel_data.h)

## Scope

In bounds:

- Add arm/hand mesh rendering to the first-person viewmodel pipeline.
- Define per-weapon grip socket positions (right hand grip, left hand foregrip).
- Implement simple IK (two-bone arm solver) that positions hands at grip sockets.
- Arms originate from camera origin/chest height and terminate at hand grip points.
- Keep IK data in the presentation layer.
- Preserve existing weapon sway/bob/recoil transform — IK should follow.

Out of bounds:

- No full-body IK or 3rd-person animation changes.
- No procedural reload animation (separate task).
- No weapon balance changes.
- No new arm/hand mesh authoring — reuse existing viewmodel_arms assets.

## Likely Files

- `client/include/ahamkara/client/weapon_viewmodel_data.h`
- `client/src/weapon_presentation.cpp`
- `client/include/ahamkara/client/weapon_presentation.h`
- `client/src/weapon_animation_controller.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `engine/animation/src/`
- `engine/animation/include/ae/animation/`
- `engine/render/src/debug_renderer.cpp`
- `client/src/debug_scene_bridge.cpp`

## Implementation Plan

1. Review existing arms mesh and animation IK facilities in the engine.
2. Define per-weapon grip socket transforms (right hand, left hand) in viewmodel data.
3. Add a two-bone IK solver (shoulder → elbow → hand) if one doesn't exist.
4. Wire arm/hand meshes to the first-person render path, driven by IK targeting grip sockets.
5. Verify hands stay locked to weapon during idle/weapon sway.
6. Validate build and tests.

## Acceptance Bar

- Arms and hands are visible in first-person view.
- Hands grip the weapon at correct positions (right hand on grip, left hand on foregrip).
- Hands follow weapon movement/sway/bob without detaching.
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

Confirm the IK and hand rig stays in the presentation layer, does not leak into `WeaponRuntime` or gameplay code, and preserves the existing animation-driven weapon transform path.
