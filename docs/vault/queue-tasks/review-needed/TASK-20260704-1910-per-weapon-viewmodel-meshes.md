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
  - tools
related_feature:
report: reports/subagents/TASK-20260704-1910-per-weapon-viewmodel-meshes-report.md
---

# TASK-20260704-1910-per-weapon-viewmodel-meshes

## Goal

Wire each weapon to its own dedicated viewmodel mesh so the player sees the actual gun model (AR-15, shotgun, rocket launcher) instead of a generic arms mesh for every weapon.

## Background

Currently `weapon_viewmodel_mesh_path()` returns `"assets/compiled/models/viewmodel_arms.aemesh"` for every weapon index. The asset pipeline already has compiled viewmodel meshes for AR-15, pistol, rifle, rocket launcher, shotgun, and arms, but they are not connected. Each weapon should render its own model in first-person.

## First Read

- [Current viewmodel code](../../../client/include/ahamkara/client/weapon_viewmodel_data.h)
- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [Asset pipeline tools](../../../tools/blender/weapons/)

## Scope

In bounds:

- Map each weapon index to its correct compiled viewmodel mesh path.
- Verify the mesh files exist and are loadable.
- Keep the arms mesh as a fallback for unknown weapon indices.
- Preserve the weapon presentation layer seam.

Out of bounds:

- No new mesh authoring or asset creation.
- No hand/arm IK or animations.
- No weapon balance changes.
- No reload animation.

## Likely Files

- `client/include/ahamkara/client/weapon_viewmodel_data.h`
- `client/src/weapon_presentation.cpp`
- `client/include/ahamkara/client/weapon_presentation.h`
- `tools/blender/weapons/` (to verify mesh names)

## Implementation Plan

1. Read the compiled viewmodel mesh directory to confirm available `.aemesh` files.
2. Update `weapon_viewmodel_mesh_path()` to return the correct path per weapon index.
3. Update `kWeaponViewmodelCount` if needed to match the number of weapons with distinct models.
4. Verify each weapon loads and renders its own mesh.
5. Validate build and tests.

## Acceptance Bar

- Each weapon index renders its own viewmodel mesh (not the fallback arms mesh).
- Unknown weapon indices still fall back to arms mesh safely.
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

Confirm the weapon-to-mesh mapping stays in the presentation layer and does not leak into `WeaponRuntime` or gameplay code.
