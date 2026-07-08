---
type: subagent-report
category: render
status: validated_with_known_gaps
created: 2026-07-08
agent: oz
subsystems:
  - engine/render
  - client
branch: agent/oz/lod-batching
validation:
  - ae_render (compiled)
  - ahamkara_level_render_tests (pass)
  - ahamkara_lod_batching_tests (pass)
  - Pre-existing server build error blocks full run-tests.sh
---

# Subagent Report: LOD Batching

## Task

Add LOD chains, mesh-based draw-call batching, and sorting improvements to the level render path so large worlds remain performant. Task: `docs/vault/queue-tasks/open/TASK-20260704-1420-lod-batching.md`.

## Status

`validated_with_known_gaps` — the render module builds cleanly, both render-related test suites pass (level_render_tests + lod_batching_tests). Pre-existing compile failures in `server/wish/admin/` (unrelated to this change) prevent the full `run-tests.sh` from completing.

## What Changed

### 1. LOD chain in LevelRenderInstance

- Replaced `GpuModel model` (single mesh) with `GpuModel lod_models[3]` (one per LOD level: High/Medium/Low).
- `LevelRenderScene::build()` now loads the base mesh asset for LOD0, then attempts `mesh_asset_id + ".lod1"` for LOD1 and `mesh_asset_id + ".lod2"` for LOD2. Missing LOD files are skipped gracefully — at draw time, `resolve_instance_lod()` falls back to the nearest higher-detail level with meshes.
- `LevelRenderScene::destroy()` releases all three LOD models per instance.

### 2. LOD-aware submit()

- `submit()` now accepts a `const float* camera_position` parameter.
- `resolve_instance_lod()` computes squared distance from camera to instance center (from `model_matrix[12-14]`), calls `select_lod()`, and falls back through higher-detail levels if the selected LOD model has no meshes.

### 3. Draw-call batching

- Extracted `batch_level_draw_calls()`: collects all (mesh, instance) pairs from all instances at their resolved LOD, then sorts by a composite key of `(vbo_positions.id << 32) | ibo_indices.id`. Identical meshes are now submitted consecutively, reducing GPU state changes.
- Both `resolve_instance_lod()` and `batch_level_draw_calls()` are pure/GL-free and unit-tested.

### 4. Caller update

- `debug_render_runtime.cpp` now passes `renderer.camera_position()` to `level_scene->submit()`.

### 5. Backward compatibility

- Existing test `test_draw_call_assembly` in `level_render_tests.cpp` was updated to use `lod_models[0]` instead of the removed `model` member.

## Files Changed

- `engine/render/include/ae/render/level_render.h`
- `engine/render/src/level_render.cpp`
- `client/src/debug_render_runtime.cpp`
- `tests/src/lod_batching_tests.cpp` (new)
- `tests/CMakeLists.txt`
- `tests/src/level_render_tests.cpp` (API rename)

## Design

- **LOD naming convention**: `mesh_asset_id` → LOD0 (full detail), `mesh_asset_id + ".lod1"` → medium, `mesh_asset_id + ".lod2"` → minimal. This is a file-naming convention; the renderer falls back gracefully if files are absent.
- **LOD selection**: Uses the existing `select_lod(distance_sq)` thresholds (144 → LOD1, 900 → LOD2). Returns the resolved `LodLevel` so callers can track stats.
- **Batching**: Sort-by-mesh-handle is a simple approach that works without instanced draw calls. Future GPU instancing (glDrawElementsInstanced) can be added on top of this sorted ordering.
- **No renderer rewrite**: All changes are additive to the existing `LevelRenderScene` / `PbrRenderer` path. The shader and PBR draw-call path are untouched.

## Test Coverage

8 tests in `ahamkara_lod_batching_tests`:
- `test_lod_near_camera` — distance 0 → LOD0 (High)
- `test_lod_medium_distance` — distance 15 → LOD1 (Medium)
- `test_lod_far_distance` — distance 40 → LOD2 (Low)
- `test_lod_fallback_when_lod2_missing` — LOD2 absent → fallback to LOD1
- `test_lod_fallback_when_all_missing` — LOD1+LOD2 absent → fallback to LOD0
- `test_lod_y_axis_separation` — vertical distance triggers LOD
- `test_batch_sort_by_mesh` — sorted calls group same-mesh instances consecutively
- `test_batch_lod_selection` — draw-call count changes with distance (3 at LOD0, 2 at LOD1, 1 at LOD2)

## Validation Run

```sh
cd /Users/taufeeqali/Projects/Ahamkara-lod-batching
cmake --build build/debug --target ae_render ahamkara_level_render_tests ahamkara_lod_batching_tests
./build/debug/tests/ahamkara_lod_batching_tests   # passed
./build/debug/tests/ahamkara_level_render_tests    # passed
```

## Known Gaps

- Pre-existing server build errors (`admin_server.h` SocketHandle access, `::close` resolution) prevent full `run-tests.sh` from completing. These are unrelated to this change.
- No authored LOD mesh files (`.lod1`, `.lod2`) exist in the asset tree yet — they must be produced by the asset pipeline. The fallback logic means the renderer works correctly without them (all instances render at LOD0).
- Runtime visual confirmation (LOD switching visible in a GL window) is not possible in this headless environment.
- The batching is sort-only; GPU instancing (`glDrawElementsInstanced`) is a follow-up optimization.

## Confidence

`high` — all changes are additive to the existing render path, the pure-math functions are fully unit-tested, and the existing render tests pass without modification other than a member rename.
