---
type: review
status: final
created: 2026-06-20
reviewer: codex
reviewer_role: primary
reviewer_model: gpt-5-codex
task: TASK-20260620-1200-level-driven-world-meshes
report: TASK-20260620-1200-level-driven-world-meshes-report.md
decision: revise
escalation_tier: low
secondary_review:
subsystems:
  - engine/render
  - client
---

# Codex Review

## Task

TASK-20260620-1200-level-driven-world-meshes

## Report

[TASK-20260620-1200-level-driven-world-meshes-report.md](TASK-20260620-1200-level-driven-world-meshes-report.md)

## Decision

`revise`

## Escalation Tier

`low`

## Scope Check

The implementation stayed broadly within the level-render/PBR activation scope,
but it missed an important render-order constraint and did not satisfy the
requested draw-call assembly test coverage.

## Evidence Checked

- `git status`
- `git diff --stat`
- task and report contents
- `client/src/debug_render_runtime.cpp`
- `engine/render/src/debug_renderer.cpp`
- `engine/render/include/ae/render/level_render.h`
- `engine/render/src/level_render.cpp`
- `tests/src/level_render_tests.cpp`
- `cmake --build --preset debug`
- `./scripts/run-tests.sh --preset debug`

## Findings

1. `PbrRenderer` is currently called after `DebugRenderer::render(scene)`, but
   `DebugRenderer::render()` has already drawn crosshair, metrics, HUD, damage
   numbers, menu overlay, and scene overlay before returning. Level meshes drawn
   afterward can overwrite those overlays wherever they pass depth/color writes.
   The PBR pass needs to run in a true 3D/world phase before debug/HUD/UI
   overlays, or the debug renderer needs to expose split world/overlay phases.

2. The acceptance bar asked for a GL-free level-to-draw-call assembly test using
   an in-memory `LevelAsset`. The new `level_render_tests.cpp` covers transform
   composition and scalar material mapping only; it does not prove that a level
   mesh instance produces the expected render instance/draw-call count.

## Validation Assessment

Validation commands pass locally:

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

All 10 tests passed. The passing tests do not cover the render-order regression,
and the implementation was not runtime-confirmed in a GL window.

## Secondary Review

Not requested because the primary review found revision-required issues.

## Risks

- First authored level mesh could visually corrupt HUD/crosshair/menu overlays.
- The path is still unproven with actual authored content because no level in
  the queue currently contains a mesh instance.

## Next Action

Move this task back to `open/` with the following required revision actions:

1. Move/split the PBR level-mesh pass so it runs before screen-space overlays.
2. Add a focused GL-free test proving level mesh instance assembly produces the
   expected render instance/draw-call shape.
3. Re-run `cmake --build --preset debug` and `./scripts/run-tests.sh --preset debug`.
