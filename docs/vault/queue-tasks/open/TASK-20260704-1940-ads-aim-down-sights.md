---
type: opencode-task
status: open
created: 2026-07-04
queued_by: user
assigned_to: opencode
priority: high
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - client
  - game
  - engine/render
related_feature:
report:
---

# TASK-20260704-1940-ads-aim-down-sights

## Goal

Add aim-down-sights (ADS) behavior so the viewmodel smoothly transitions from hip-fire position to centered/zoomed position when the player aims. The weapon moves toward the center of the screen, FOV narrows, and the camera/viewmodel realigns as in professional FPS games.

## Background

Currently there is no aiming mechanic — the weapon stays in the same position regardless of input. ADS is a core FPS mechanic where holding aim brings the weapon up to eye level (centered), narrows the field of view for magnification, and reduces weapon sway. This requires interpolating the viewmodel transform between hip and ADS positions based on an aim input signal.

## First Read

- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [Viewmodel offset tuning task](../open/TASK-20260704-1900-viewmodel-offset-tuning.md)
- [Current input handling](../../../client/src/)
- [Current viewmodel data](../../../client/include/ahamkara/client/weapon_viewmodel_data.h)

## Scope

In bounds:

- Define per-weapon ADS transform (position offset, FOV target) relative to hip-fire position.
- Wire ADS input (right mouse button / gamepad aim trigger) to drive viewmodel transition.
- Smoothly interpolate viewmodel transform between hip and ADS over ~0.15-0.25s.
- Reduce camera FOV during ADS (zoom effect).
- Reduce weapon sway/bob during ADS.
- Keep ADS data in the presentation layer.

Out of bounds:

- No weapon accuracy/spread gameplay changes — purely visual.
- No hip-fire crosshair changes.
- No animation system changes beyond viewmodel transforms.
- No reload animation changes.

## Likely Files

- `client/include/ahamkara/client/weapon_viewmodel_data.h`
- `client/src/weapon_presentation.cpp`
- `client/include/ahamkara/client/weapon_presentation.h`
- `client/src/weapon_animation_controller.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `client/src/debug_scene_bridge.cpp`
- `engine/render/src/debug_renderer.cpp`
- `game/src/player.cpp`
- `game/include/ahamkara/game/player.h`

## Implementation Plan

1. Read current viewmodel transform pipeline and camera FOV handling.
2. Add per-weapon ADS offset and FOV target data.
3. Wire aim input signal through player/game layer to presentation.
4. Add smooth interpolation (lerp/smoothstep) between hip and ADS transforms.
5. Reduce camera FOV during ADS based on per-weapon zoom factor.
6. Reduce sway/bob intensity during ADS.
7. Validate transition is smooth and feels responsive.

## Acceptance Bar

- Holding aim input smoothly moves weapon toward screen center, narrows FOV.
- Releasing aim smoothly returns weapon to hip position, restores normal FOV.
- Different weapons can have different ADS positions and zoom levels.
- ADS does not break weapon firing or reloading.
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

Confirm ADS is purely a presentation-layer transform interpolation — no weapon accuracy/spread gameplay changes are introduced.
