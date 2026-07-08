---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [render]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0103-legacy-opengl-compat-collapse

## Task

Remove the remaining legacy OpenGL compatibility dependencies from the debug render path and collapse the compatibility layer where it is no longer needed.

## Status

implemented

## Scope

In bounds: Audit remaining compatibility-layer usage, remove or simplify matrix-era dependencies where core-profile helpers exist, keep renderer behavior stable, leave explicit fallbacks only where core path is not yet available.

Out of bounds: Introducing a new renderer backend, changing lighting/post-processing behavior, large asset or gameplay changes.

## Files Changed

- `engine/render/src/gl_compat.h` — Removed dead code: entire immediate-mode batch system declarations (aecBegin/aecEnd/aecVertex3f/aecVertex2f/aecNormal3f/aecTexCoord2f), all lighting stub declarations (aecLight*/aecColorMaterial/aecShadeModel), all client-state stub declarations (aecEnableClientState/aecVertexPointer/etc.), unused vertex types (VertexP3, VertexP2), unused GLCompatState fields (current_nx/ny/nz, current_u/v, begin_mode, batch_vertices, batch_vertices_2d, all 8 lighting fields), 16 unused legacy enum constant defines, and 14 unused macro remaps. Updated header documentation explaining what remains and why.
- `engine/render/src/gl_compat.cpp` — Removed dead code: flush_batch_3d() (~50 lines), flush_batch_2d() (~56 lines), quad_to_triangles(), quad_to_triangles_2d(), aecBegin/aecEnd (20 lines), aecVertex3f/aecVertex2f (18 lines), aecNormal3f/aecTexCoord2f (7 lines), all lighting/color-material/shade-model/client-state stub implementations (12 empty functions). Fixed bug: added `GL_POLYGON_OFFSET_FILL` (0x0B7D) to `is_core_cap()` — this core cap was previously missing, causing decal rendering's polygon-offset calls to be silently no-oped.

## What Changed

The GL compatibility layer was ~508 lines (cpp) + ~211 lines (h) = ~719 lines. After cleanup: ~276 lines (cpp) + ~130 lines (h) = ~406 lines total — **~313 lines of dead code removed (44% reduction)**.

Specifically:
- **Removed**: all lighting stubs, immediate-mode batch system, client-state stubs, unused vertex types, unused state fields, unused macro remaps, unused enum constants (all confirmed zero consumers via full codebase audit)
- **Preserved**: Mat4 math helpers, GLCompatState (current color + projection/modelview), draw_user_arrays(), aecEnable/aecDisable (core cap pass-through), aecColor3f/4f/3ub, init/shutdown/begin_frame lifecycle, diagnostic counters
- **Fixed**: GL_POLYGON_OFFSET_FILL now properly passes through to real glEnable, restoring correct decal rendering (polygon offset to prevent z-fighting)

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: Pass (only pre-existing entt warnings)
- 14/14 tests pass (0 failures)
- Clean compile — no consumer broke (all removed code had zero callers)
- Not runtime-confirmed: visual verification of decal polygon offset fix and general rendering requires a GL display

## Known Gaps

- The color-state flow (glColor3f → GLCompatState.current_r → vertex struct) is still present. Removing this would require touching ~50 call sites across 4 consumer files — a larger follow-up task.
- The Mat4 math functions could eventually be replaced with glm (the project already depends on it), but that's a larger migration.
- Runtime visual confirmation of the POLYGON_OFFSET_FILL fix not done (requires GL display).

## Runtime Risks

- Minimal. All removed code was confirmed to have zero consumers via exhaustive codebase audit. The only behavioral change is the POLYGON_OFFSET_FILL fix, which restores previously-broken behavior (decal polygon offset was silently no-oped).
- If any third-party or future code still expects the removed macros (glBegin, glLightfv, etc.), they will get linker errors, which is a safe failure mode.

## Cross-Agent Dependencies

- Follow-up task: migrate color-state flow to direct parameter passing (eliminates the remaining glColor3f remap)
- Follow-up task: replace Mat4 with glm::mat4 (removes the last matrix-era custom code)

## Recommended Next Step

Codex review. Visual confirmation of decal rendering (particularly polygon-offset for z-fighting) should be done by a human with a GL display.

## Confidence

`high` — dead code only removal plus one bug fix, all verified by build + full test suite.
