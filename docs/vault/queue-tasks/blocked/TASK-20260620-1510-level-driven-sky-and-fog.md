---
type: opencode-task
status: blocked
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
  - client
related_feature:
report: ../../../reports/subagents/TASK-20260620-1510-level-driven-sky-and-fog-report.md
---

# TASK-20260620-1510-level-driven-sky-and-fog

## Goal

Drive sky color, ambient, and distance fog from the loaded `LevelAsset` instead
of hardcoded renderer values, and ensure fog reads as real depth in a space. A
cheap, high-payoff legibility win for maps (no HDR required).

## Background

`DebugRenderer` already has a screen-space sky pass and fog uniforms
(`u_fog_color_loc`, `u_fog_params_loc`) but uses a hardcoded day/night palette.
`LevelAsset` carries `sky_color`, `ambient`, `ground_material`, and
`skybox_material`, none of which currently affect the runtime render. This wires
the level's environment settings into the existing passes.

## First Read

- [Renderer backend](../../systems/renderer_backend.md)
- [Asset pipeline](../../systems/asset_pipeline.md) (level world settings)
- [Architecture](../../systems/architecture.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Scope

In bounds:

- Thread the loaded `LevelAsset` sky/ambient into the renderer (clear color / sky
  pass tint and the ambient term).
- Apply distance fog using the level's sky color as fog color; expose a sane
  density (per level or a tuned default).
- Optionally render a simple ground tint from `ground_material`/level settings.

Out of bounds:

- Real skybox cubemap / procedural atmosphere / time-of-day (later phase).
- HDR / tonemapping (deferred), height fog, aerial perspective.

## Implementation Plan

1. Pass the level's environment settings to the render path (extend the scene/
   submission or a small renderer setter).
2. Use them for the sky/clear tint, ambient, and fog color/density.
3. Keep a sensible fallback when no level is loaded (current behavior).

## Acceptance Bar

- Loading a level visibly changes sky tint / ambient / fog per its settings
  (validated on a display; build/test-validated here).
- No regression to the default (no-level) look.
- Build + existing tests stay green.

## Review Tier

- `low`.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Runtime-visual confirmation needs a GL display; report what was vs was not
visually confirmed. Follow claim hygiene.

## Reporting Required

Standard: write report, append master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
