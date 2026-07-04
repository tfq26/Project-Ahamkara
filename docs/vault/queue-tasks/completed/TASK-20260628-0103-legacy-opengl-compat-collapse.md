---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: high
primary_reviewer:
secondary_reviewer:
subsystems: [render]
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0103-legacy-opengl-compat-collapse-report.md
---

# TASK-20260628-0103-legacy-opengl-compat-collapse

## Goal

Remove the remaining legacy OpenGL compatibility dependencies from the debug
render path and collapse the compatibility layer where it is no longer needed.

## Background

The engine is on an OpenGL 3.3 core-profile path, but a few legacy-style calls
and matrix-era assumptions still linger. This task follows the roadmap's
rendering-fidelity and cleanup direction while keeping behavior stable.
Keep the cleanup future-proof for the deferred HDR/post path: preserve the
ability to introduce offscreen targets and tonemapping later without reintroducing
fixed-function assumptions or another compatibility layer.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Start here](../00-start-here.md)
- [Repo map](../01-repo-map.md)
- [Feature task workflow](../workflows/feature-task-workflow.md)
- [OpenCode task queue](../workflows/opencode-task-queue.md)
- [Engine assessment](../features/2026-06-28-engine-assessment.md)

## Scope

In bounds:

- Audit remaining compatibility-layer usage in render/debug overlay code.
- Remove or simplify matrix-era dependencies where core-profile helpers exist.
- Keep the renderer behavior stable and documented.
- Leave explicit fallbacks only where the core path is not yet available.

Out of bounds:

- Introducing a new renderer backend.
- Changing lighting or post-processing behavior beyond what is needed to
  preserve output.
- Large asset or gameplay changes.

## Likely Files

- `engine/render/src/gl_compat.h`
- `engine/render/src/gl_compat.cpp`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/src/debug_renderer_hud.cpp`
- `engine/render/src/debug_renderer_internal.h`

## Implementation Plan

1. Inspect the remaining compatibility calls and matrix assumptions.
2. Replace or isolate them behind explicit core-profile helpers.
3. Simplify the compatibility layer where it is now dead weight.
4. Verify the debug renderer still builds and renders the same major overlays.

## Acceptance Bar

- Remaining legacy GL dependencies are reduced or isolated.
- Matrix-era assumptions are removed where a core path exists.
- Debug renderer still builds and runs.
- Any intentional fallback is documented.

## Review Tier

- `high` - primary reviewer plus secondary reviewer before final completion

## Validation

Run when relevant:

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

If validation is skipped or fails, explain why in the report.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should treat this as a cleanup task with a high review bar because it
touches foundational render code.
