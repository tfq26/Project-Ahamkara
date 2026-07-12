---
type: opencode-task
status: completed
created: 2026-07-04
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
  - game
  - engine/core
related_feature:
report: reports/subagents/TASK-20260704-1400-spatial-partitioning-report.md
---

# TASK-20260704-1400-spatial-partitioning

## Goal

Add world-scale spatial partitioning and occlusion hooks so large destinations can be managed without brute-force rendering.

## Background

Phase 7 is where the world grows beyond a single static level. These slices should add spatial scale, streaming, and destination metadata without breaking the current render loop or level loading path.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Level-driven world meshes report](../../queue-tasks/completed/TASK-20260620-1200-level-driven-world-meshes.md)
- [Blender headless level generator report](../../queue-tasks/completed/TASK-20260620-1415-blender-headless-level-generator.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep the level import/render path working while you extend scale or residency.
- Keep the slice additive to the existing level pipeline.
- Keep streaming or partitioning deterministic enough to test.
- spatial partitioning at level scale
- occlusion hooks
- interior portal/PVS readiness

Out of bounds:

- No combat rule rewrites.
- No animation/audio polish work.
- No new fidelity pipeline unless it is directly required for streaming.
- combat redesign
- animation/audio polish
- service orchestration work

## Likely Files

  - `engine/render/src/`
  - `engine/render/include/ae/render/`
  - `game/src/world.cpp`
  - `game/include/ahamkara/game/world.h`

## Implementation Plan

1. Trace the current world-scale boundary and choose the smallest slice seam.
2. Add the new residency/partitioning behavior without tearing out the current level path.
3. Validate the load/unload or culling behavior under the existing build/test flow.

## Acceptance Bar

- Large-space partitioning is explicit and testable.
- The slice preserves the current level path.
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
