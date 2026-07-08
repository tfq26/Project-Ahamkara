---
type: opencode-task
status: completed
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
related_feature:
report: reports/subagents/1900-viewmodel-offset-tuning-report.md
---

# TASK-20260704-1900-viewmodel-offset-tuning

## Goal

Tune viewmodel FOV scaling, position, and orientation so every weapon sits in a consistent, natural-looking first-person location on screen. The gun should look like it's held by the player, not floating at a default origin.

## Background

Currently `kWeaponViewmodelTransforms` are all zero and all weapons use a single arms mesh. The viewmodel is anchored to the camera but has no per-weapon position/scale/FOV correction. In professional FPS games, each weapon has tuned viewmodel offsets (position, FOV scale, rotation) so it looks correct from the player's eye. Without this, weapons look too close, too far, or misaligned relative to where the player's hands would be.

## First Read

- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Current viewmodel code](../../../client/include/ahamkara/client/weapon_viewmodel_data.h)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep viewmodel offsets in the client presentation layer (not in gameplay code).
- Add per-weapon position (x,y,z) offset from camera origin.
- Add per-weapon FOV scale factor (weapon renders at different FOV than the world).
- Add per-weapon rotation offset (tilt/angle so the gun points where the player looks).
- Keep the gameplay camera/anchor path unchanged.
- Make offsets data-driven (easy to tune without recompiling where practical).

Out of bounds:

- No weapon balance or firing rule changes.
- No hand/arm IK (separate task).
- No reload animation (separate task).
- No ADS animation (separate task).

## Likely Files

- `client/include/ahamkara/client/weapon_viewmodel_data.h`
- `client/src/weapon_presentation.cpp`
- `client/include/ahamkara/client/weapon_presentation.h`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/include/ae/render/debug_renderer.h`
- `client/src/debug_scene_bridge.cpp`

## Implementation Plan

1. Read current viewmodel transform path and camera anchor wiring.
2. Extend `WeaponViewmodelTransform` to include position offset (x,y,z) and FOV scale factor.
3. Wire the new offsets into the viewmodel rendering path.
4. Apply reasonable default offsets per weapon (AR-15, shotgun, rocket launcher).
5. Validate that the weapon stays stable relative to camera movement.

## Acceptance Bar

- Each weapon has non-zero position/rotation/FOV offsets that place it in a natural FPS position.
- Offsets are data-driven and in the presentation layer.
- Viewmodel stays anchored to camera during all player movement.
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

Confirm the viewmodel offset data stays in the presentation layer and does not leak into gameplay or weapon-runtime ownership.
