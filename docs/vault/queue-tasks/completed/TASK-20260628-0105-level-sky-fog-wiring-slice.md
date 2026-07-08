---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer:
secondary_reviewer:
subsystems: [engine/render, client, game]
related_feature: features/2026-06-28-engine-assessment.md
report:
---

# TASK-20260628-0105-level-sky-fog-wiring-slice

## Goal

Wire level-driven sky, ambient, and fog data through the renderer so the engine
uses level settings instead of hardcoded environment values, while keeping the
path compatible with later HDR/post-processing work.

## Background

This is the code/wiring half of the sky and fog work. It should make the
environment settings flow through the engine cleanly, but it should not depend
on HDR or any future offscreen-target pipeline.

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

- Thread level sky color and ambient through the render path.
- Apply fog color/density from level settings or a sane default.
- Preserve the current fallback look when no level data is present.
- Keep the implementation structured so HDR can be layered on later.

Out of bounds:

- Real skybox cubemaps, procedural atmosphere, time-of-day systems.
- HDR / tonemapping / offscreen render-target changes.
- Display-dependent artistic tuning.

## Likely Files

- `engine/render/src/debug_renderer.cpp`
- `engine/render/src/debug_renderer_hud.cpp`
- `engine/render/include/ae/render/debug_renderer.h`
- `client/src/debug_render_runtime.cpp`
- `game/include/ahamkara/game/world.h`

## Implementation Plan

1. Trace the level environment data into the render submission or renderer.
2. Replace hardcoded sky/ambient/fog inputs with level-driven values.
3. Preserve sane defaults when no level is loaded.
4. Keep the contract ready for later HDR/post-processing without rewrites.

## Acceptance Bar

- Level settings drive sky, ambient, and fog in code.
- No-level fallback remains intact.
- Build passes.
- The slice does not require HDR to function.

## Review Tier

- `low` - primary reviewer signoff only

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

This slice should be checked for clean wiring and fallback behavior, not for
visual artistry.
