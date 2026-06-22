---
type: opencode-task
status: open
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: low
escalation_tier: medium
deferred: true
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/render
  - client
related_feature:
report:
---

# TASK-20260620-1345-render-target-hdr-foundation

## Status: Deferred (do not pick up yet)

Deferred by user decision 2026-06-20: HDR is fidelity, not capability, and is the
riskiest task in the queue (rewrites the core frame loop) with no way to validate
it visually in a headless environment. Revisit only when a trigger fires:

- PBR material tuning shows clipped / washed-out highlights, OR
- bloom / SSAO / TAA is wanted (they need an offscreen HDR target), OR
- many dynamic lights or strong emissive surfaces are added.

See `docs/vault/memory/decision-log.md` (2026-06-20 HDR deferral) and
`docs/roadmap/roadmap.md` (Part I, Phase 6 — Rendering Fidelity).

## Goal

Add an offscreen render-target abstraction and an HDR color pipeline to the
renderer: render the world into a floating-point HDR target, then resolve to the
backbuffer through a fullscreen tonemap + gamma pass. This is the foundation that
makes PBR lighting correct and unblocks post-processing (bloom, SSAO, TAA).


## Background

- `RenderBackend` (engine/render/include/ae/render/render_backend.h) has no
  framebuffer/MRT abstraction and no HDR formats; all rendering goes straight to
  the default backbuffer (`begin_frame`/`end_frame`).
- `ShadowPass` already manages an internal depth FBO, so the pattern exists, but
  there is no general offscreen color target.
- PBR output is written directly to an 8-bit sRGB backbuffer with no tonemap, so
  lighting cannot be physically correct.
- Present semantics are being clarified separately in
  `open/TASK-20260615-1215-render-present-semantics.md` — coordinate so the HDR
  resolve fits that model rather than fighting it.

## First Read

- [Docs index](../../../README.md)
- [Renderer backend](../../systems/renderer_backend.md)
- [Roadmap (consolidated)](../../../roadmap/roadmap.md)
- [Render present semantics task](TASK-20260615-1215-render-present-semantics.md)
- [OpenCode standing instructions](../opencode-standing-instructions.md)

## Scope

In bounds:

- `RenderBackend`: a render-target abstraction —
  `RenderTargetHandle create_render_target(int w, int h, ColorFormat, bool depth)`,
  `bind_render_target(handle)` / `bind_default_framebuffer()`, `destroy_render_target`,
  and a way to sample the target's color as a texture. Support at least an
  `Rgba16F` HDR color format.
- Blend-state control on the backend (`set_blend_enabled`, `set_blend_func`),
  needed by the resolve/post passes.
- A fullscreen tonemap+gamma pass (ACES or Reinhard) reading the HDR target and
  writing the backbuffer.
- Wire the debug/PBR world render to draw into the HDR target, then run the
  resolve pass before present. Handle window resize (recreate targets).

Out of bounds (follow-ups):

- Bloom, SSAO/GTAO, TAA/MSAA, motion vectors.
- Deferred shading / G-buffer / MRT beyond the single HDR color target seam.
- Moving the legacy fixed-function debug drawing into the HDR target if it proves
  too invasive — if so, document the split and target the modern/PBR path first.

## Likely Files

- `engine/render/include/ae/render/render_backend.h`
- `engine/render/src/render_backend_opengl.cpp`
- `engine/render/src/debug_renderer.cpp` (frame structure / resolve hookup)
- new `engine/render/src/tonemap_pass.cpp` (+ header) or fold into debug_renderer
- `engine/render/shaders/` (fullscreen tonemap shader)
- `client/src/debug_render_runtime.cpp` / `client/src/debug_client.cpp` if the
  resolve/present ordering needs wiring there

## Implementation Plan

1. Design the minimal render-target API on `RenderBackend`; implement in the
   OpenGL backend (FBO + RGBA16F color + depth renderbuffer, completeness check).
2. Add blend-state controls.
3. Add a tonemap+gamma fullscreen pass sampling the HDR target.
4. Route world rendering into the HDR target; resolve to backbuffer before
   present; recreate targets on resize.
5. Keep behavior visually close to today (tune exposure so brightness is sane).

## Acceptance Bar

- The world renders into an HDR (RGBA16F) target and is tonemapped to the
  backbuffer; output brightness is reasonable (no blown-out/black frame).
- Render targets recreate correctly on window resize.
- Blend-state controls exist and are used by the resolve pass.
- Build passes; existing tests stay green.

## Review Tier

- `high` - changes the core frame structure and present flow; primary +
  secondary reviewer.

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

Runtime-visual confirmation is required for this one in practice but is not
automatable; name the exact command (e.g. `./scripts/start.sh local`) and report
what was observed vs only build/test-validated. Follow claim hygiene.

## Reporting Required

1. Write a report in `docs/reports/subagents/` using the report template.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move this task to `review-needed/` or `blocked/`.

## Notes For Codex Review

Confirm HDR target completeness/resize handling, that present ordering matches
the render-present-semantics direction, and that no automatable regression slips
in despite visual validation being manual.
