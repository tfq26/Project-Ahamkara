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
report: reports/subagents/TASK-20260628-0109-first-person-camera-viewmodel-rig-report.md
review: ../../../reports/subagents/TASK-20260628-0109-first-person-camera-viewmodel-rig-codex-review.md
---

# TASK-20260628-0109-first-person-camera-viewmodel-rig

## Goal

Wire the active first-person camera to a proper viewmodel rig so the held weapon
is anchored to the camera instead of feeling like a floating debug prop.

## Background

Phase 2 needs a real first-person presentation path. The movement controller
should provide a stable camera anchor, and weapon presentation should consume
that anchor to position the held item. This task is about the runtime wiring,
not final animation polish.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../roadmap/roadmap.md)
- [Engine assessment](../../features/2026-06-28-engine-assessment.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Attach the first-person weapon/viewmodel to the current camera anchor.
- Use the weapon presentation layer rather than putting presentation logic back
  into `World` or `WeaponRuntime`.
- Keep the viewmodel aligned to the active camera and preserve the current
  player-facing weapon visibility behavior.
- Add any minimal offsets/poses needed to avoid the weapon floating away from
  the camera.
- Keep the implementation compatible with future per-weapon animation work.

Out of bounds:

- Creating final art assets for every weapon.
- Deep animation authoring or bone setup beyond what is needed to prove the rig.
- Changing the movement feel or camera behavior itself.
- Weapon balance or firing logic.

## Likely Files

- `client/src/debug_scene_bridge.cpp`
- `client/src/client_frame_pipeline.cpp`
- `client/include/ahamkara/client/weapon_presentation.h`
- `client/src/weapon_presentation.cpp`
- `client/include/ahamkara/client/weapon_animation_controller.h`
- `client/src/weapon_animation_controller.cpp`
- `game/include/ahamkara/game/player.h`
- `game/include/ahamkara/game/world.h`

## Implementation Plan

1. Read the current camera anchor and presentation path.
2. Bind the held weapon/viewmodel to that camera anchor through the presentation
   layer.
3. Keep the viewmodel transform data out of `World`.
4. Verify the weapon stays stable relative to camera motion and movement.

## Expected Shape

- The camera anchor is the source of truth for the first-person viewmodel.
- `World` should not know how the viewmodel is posed beyond exposing the
  camera/player state.
- The presentation layer should be able to swap models or future animations
  without rewriting camera code.

## Must Preserve

- Current camera motion and movement feel.
- Current firing/reload behavior.
- Current visibility rules for gameplay vs menus.
- Current tests and build targets.

## Must Not Touch

- Weapon ammo/reload semantics.
- Match state, respawn state, or movement physics.
- Menu/HUD ownership.
- A final animation pipeline if it would require a separate art pass.

## Acceptance Bar

- The held weapon is visibly anchored to the first-person camera.
- The rig does not require presentation logic to live in `World`.
- Build passes and tests stay green.

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
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the viewmodel is anchored to the active camera and that the worker did
not sneak movement or weapon-runtime logic back into the presentation layer.

## Codex Review Outcome

Accepted. The active camera now feeds an explicit first-person rig anchor into
the renderer, and the weapon presentation boundary stayed out of `World` and
weapon runtime ownership.
