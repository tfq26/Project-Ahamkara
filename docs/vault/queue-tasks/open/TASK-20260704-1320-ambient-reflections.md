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
  - engine/render
  - client
related_feature:
report:
---

# TASK-20260704-1320-ambient-reflections

## Goal

Add ambient lighting, IBL, and reflection probe behavior so spaces feel less flat without requiring HDR activation.

## Background

Phase 6 is the fidelity track. HDR stays deferred until it is explicitly reactivated, but the remaining lighting and legacy-GL cleanup slices should keep the renderer on the modern path.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Phase slice map](../../workflows/phase-slice-map.md)
- [Viewmodel orientation contract report](../../queue-tasks/completed/TASK-20260704-0102-viewmodel-orientation-contract.md)
- [Legacy OpenGL compat collapse report](../../queue-tasks/completed/TASK-20260704-0103-legacy-opengl-compat-collapse.md)
- [Textured material authoring slice report](../../queue-tasks/completed/TASK-20260704-0104-textured-material-authoring-slice.md)
- [Level sky/fog wiring slice report](../../queue-tasks/completed/TASK-20260704-0105-level-sky-fog-wiring-slice.md)
- [Current state](../../memory/current-state.md)

## Scope

In bounds:

- Keep rendering changes additive and compatible with the current core-profile path.
- Keep HDR deferred unless the task explicitly reactivates it.
- Keep fidelity work separate from gameplay ownership.
- ambient irradiance
- IBL/reflection probes
- emissive and ambient response

Out of bounds:

- No streaming/world-scale rewrite.
- No gameplay rule changes.
- No deferred HDR resurrection unless the task says so.
- HDR activation
- world streaming rewrite
- combat/gameplay changes

## Likely Files

  - `engine/render/src/`
  - `engine/render/include/ae/render/`
  - `client/src/debug_render_runtime.cpp`
  - `client/src/debug_client.cpp`

## Implementation Plan

1. Inspect the current renderer seam and identify the smallest usable fidelity addition.
2. Move the feature through the modern renderer path without reintroducing legacy ownership.
3. Validate the result against the existing build/test flow.

## Acceptance Bar

- Ambient/reflection behavior is explicit in the renderer.
- The slice remains compatible with the current core-profile path.
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
