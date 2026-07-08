---
type: opencode-task
status: open
created: 2026-07-04
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - client
  - engine/render
  - engine/audio
related_feature:
report:
---

# TASK-20260704-1210-weapon-animation-layers

## Goal

Add first-person weapon animation layers for sway, bob, recoil kick, reload, and melee so the weapon stops feeling static.

## Background

Phase 5 makes characters, weapons, and feedback feel alive. These slices should connect the animation/audio/VFX runtimes to gameplay without making presentation own the combat rules.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Player movement/camera controller report](../../queue-tasks/completed/TASK-20260628-0106-player-movement-camera-controller.md)
- [Weapon presentation separation report](../../queue-tasks/completed/TASK-20260628-0107-weapon-presentation-separation.md)
- [First-person camera/viewmodel rig report](../../queue-tasks/completed/TASK-20260628-0109-first-person-camera-viewmodel-rig.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep animation, audio, and VFX as presentation/runtime layers that consume gameplay state.
- Keep first-person and character presentation consistent with the current gameplay rig.
- Keep the slice safe for headless/server builds where applicable.
- weapon sway/bob/recoil layers
- reload and melee motion layers
- attachable component motion hooks

Out of bounds:

- No final art production or asset authoring sprint.
- No combat balance or networking redesign.
- No loading or streaming overhaul.
- weapon balance changes
- final animation art production
- network or service work

## Likely Files

  - `game/include/ahamkara/game/weapon_runtime.h`
  - `game/include/ahamkara/game/weapon_registry.h`
  - `client/src/debug_scene_bridge.cpp`
  - `client/src/debug_render_runtime.cpp`
  - `engine/animation/src/`

## Implementation Plan

1. Trace the current presentation/runtime boundary for the target subsystem.
2. Move the presentation hooks into the intended seam and keep the gameplay owner clean.
3. Add focused validation so the new runtime path stays explicit.

## Acceptance Bar

- Weapon presentation has explicit animation layering.
- The runtime remains reusable for future weapon archetypes.
- Build and tests remain green.

## Review Tier

- `low` - primary reviewer signoff only

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
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Confirm the slice keeps the ownership boundary explicit and does not leak
runtime authority back into the wrong subsystem.
