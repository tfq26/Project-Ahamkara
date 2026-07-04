---
type: opencode-task
task_type: child
parent: TASK-20260623-1600-deep-logging-epic.md
status: completed
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
related_feature:
report: reports/subagents/TASK-20260623-1607-deep-logging-render-report.md
review: ../../../reports/subagents/TASK-20260623-1607-deep-logging-render-codex-review.md
---

# TASK-20260623-1607-deep-logging-render

## Goal

Instrument `engine/render` with deep, level-gated logging under category
`Render`, per the parent epic's Shared Logging Standard. (High value: the
silent level-render path is what motivated this epic.)

## Background

Parent: [TASK-20260623-1600-deep-logging-epic](TASK-20260623-1600-deep-logging-epic.md).
**Depends on** child 1601. Read the Shared Logging Standard there. NOTE: builds
only in non-headless (GUI) configs — validate with the `debug` preset.

## Scope

In bounds (logging only):
- Renderer/backend init/shutdown, shader compile/link (Info; errors at Error).
- `LevelRenderScene::build`: per-mesh resolve success/skip, instance + texture
  counts, material binds (Info/Debug) — make "N rendered / M skipped" explicit.
- GPU resource create/destroy (mesh/texture/model) (Debug).
- Shadow/PBR/debug passes, draw-call counts, fog/sky env application (Trace).
- Missing/unresolved assets, GL errors, fallbacks (Warning/Error).
- `#define AE_LOG_CATEGORY "Render"`.

Out of bounds: behavior changes; per-frame Info spam (draw counts are Trace).

## Likely Files

- `engine/render/src/*` (esp. `level_render.cpp`, `debug_renderer.cpp`,
  `pbr_renderer.cpp`, `render_backend_opengl.cpp`, loaders), `engine/render/include/*`

## Implementation Plan

1. Add the `Render` category define per TU.
2. Init/shader/scene-build/resource logs (Info/Debug); pass/draw detail (Trace).
3. Make level-scene build outcomes explicit (resolved vs skipped meshes).

## Acceptance Bar

- `Render` logs at correct levels; default run unchanged; cheap when disabled.
- Level-scene build clearly reports resolved/skipped meshes + counts.
- Build clean; existing tests (incl. `ahamkara_level_render_tests`) green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Reporting Required

Standard: report, master log, status/`report: reports/subagents/TASK-20260623-1607-deep-logging-render-report.md`, move to `completed/`.
