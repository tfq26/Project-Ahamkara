---
type: subagent-report
category: implementation
status: implemented
created: 2026-07-05
agent: opencode
subsystems: [render]
branch: agent/phase6/render-fidelity
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260704-1350-legacy-gl-retirement

## Task

Retire the remaining fixed-function compatibility seams so the renderer stays firmly on the core-profile path.

## Status

implemented

## Background

The renderer already used a core-profile path for production rendering (PBR shaders, VAOs, shader-based drawing). However, two legacy OpenGL compatibility patterns remained:

1. **Apple-specific VAO extensions** (`glGenVertexArraysAPPLE` / `glBindVertexArrayAPPLE`) — conditionally compiled via `__APPLE__` guards in `debug_renderer.cpp`, while non-Apple platforms used the core `glGenVertexArrays` / `glBindVertexArray`.

2. **Legacy Apple OpenGL header** (`<OpenGL/gl.h>`) — the compatibility-profile header was used for macOS builds, which only exports the APPLE-suffixed VAO names. The core-profile header `<OpenGL/gl3.h>` exports the standard names.

## Changes

### `engine/render/include/ae/render/gl_platform.h`

Changed the macOS OpenGL include from `<OpenGL/gl.h>` to `<OpenGL/gl3.h>` (core profile). This allows `glGenVertexArrays` / `glBindVertexArray` to resolve directly on macOS without APPLE extensions.

### `engine/render/src/debug_renderer.cpp`

- **Removed** redundant manual OpenGL header includes (lines 10–26) — these are already provided by `debug_renderer_internal.h` → `gl_platform.h`.
- **Removed** all 6 `__APPLE__` conditional blocks around `glGenVertexArraysAPPLE` / `glBindVertexArrayAPPLE` in:
  - `ensure_overlay_resources()` — VAO creation
  - `draw_overlay_vertices()` — VAO bind/unbind
  - `ensure_world_resources()` — VAO creation
  - `draw_world_vertices()` — VAO bind/unbind

  All now use unconditional `glGenVertexArrays` / `glBindVertexArray`.

### `engine/render/include/ae/render/render_backend.h`

Updated the historical comment about immediate-mode debug drawing to reflect its retirement.

## Files Changed

| File | Change |
|---|---|
| `engine/render/include/ae/render/gl_platform.h` | `<OpenGL/gl.h>` → `<OpenGL/gl3.h>` on macOS |
| `engine/render/src/debug_renderer.cpp` | −42 lines: removed redundant includes + 6 APPLE conditionals |
| `engine/render/include/ae/render/render_backend.h` | Updated comment |

## Validation

### Commands

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug
```

### Results

- **Debug build**: Pass — 0 errors, 0 new warnings (only pre-existing entt deprecation warnings, unused-result warnings, and ld duplicate-library warnings)
- **Tests**: 17/17 pass (0 failures)

## Known Gaps / Future Work

- The `gl_compat.{h,cpp}` layer still provides `glColor3f`→`GLCompatState` color forwarding and `Mat4` math helpers for the debug renderer. These are not fixed-function — they're a thin state wrapper over core-profile VAO drawing — but could be migrated to a subsystem-specific math/color header if desired.
- `tools/controller_mapper.cpp` still uses `glBegin(GL_QUADS)` / `glEnd()` — this is a standalone tool, not part of the engine renderer, and can be addressed separately.
- `engine/ui/imstb_truetype.h` contains `glBegin`/`glEnd` fallback rendering — this is third-party embedded code used only as a last-resort bitmap fallback when the font atlas is unavailable.

## Confidence

High — the changes are purely mechanical removals of dead conditionals and header switches. All 17 tests pass, and the build has zero errors.
