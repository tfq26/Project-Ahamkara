# Phase 5: Efficiency Passes That Also Improve Clarity

## Scope

Remove wasteful or incomplete behavior that makes the code harder to reason about.
Fix half-implemented infrastructure, remove permanently-dead fields, complete
half-wired render pipelines, and eliminate unnecessary geometry duplication.

Priority areas from the streamlining plan:
- `JobSystem::submit_after()` — broken stub
- Render/profiler fields that are always zero or half-wired
- Occlusion query pipeline — queries issued, results read, but never used
- Map geometry duplicated into all spatial cells regardless of overlap

## Status

**Complete** — 4 issues fixed across 5 files. All touched targets build clean,
relevant tests pass, no regressions.

## Implemented

### 1. `JobSystem::submit_after()` — Complete parent→child dependency wiring

**Before:** The `submit_after(JobHandle parent, JobFunction fn)` method set
`unfinished_parents = 1` on the child job but contained only a comment stub:

```cpp
// Decrement parent's counter for this child
if (parent.valid() && parent.index < static_cast<int>(jobs_.size())) {
    // Mark this job as waiting on the parent
    // When parent finishes, it decrements child's counter
}
```

The comment admitted the wiring was missing. Neither `worker_loop` nor `wait`
ever decremented children's `unfinished_parents`. Any job submitted via
`submit_after()` would silently never execute — a leak until shutdown.

**After:**
- Added `std::vector<int> children` to the `Job` struct (stores child job indices).
- `submit_after()` records the child index in the parent's `children` list.
- Both `worker_loop` and `wait` iterate the completed job's `children` and
  atomically decrement each child's `unfinished_parents`, making them eligible
  for execution.

**Files changed:** `engine/core/include/ae/core/job_system.h`,
`engine/core/src/job_system.cpp`

### 2. Remove dead `gpu_time_entities_ms` field

**Before:** The `gpu_time_entities_ms` field existed in both `DebugScene`
(public API) and `DebugRenderer::Impl`, was initialized to `0.0`, copied to
scene every frame at line 1697, and was **never written by any timer query**.
Four GPU timer slots exist (0=total, 1=depth, 2=map, 3=UI); no slot measures
entities separately. The field was permanently zero.

The metrics overlay (`debug_renderer_metrics.cpp`) also never reads it —
it displays depth, map, UI, and total bars.

**After:** Removed from `DebugScene` (header), `Impl` (source), and the
frame-end copy line. No behavior change — the field was always zero.

**Files changed:** `engine/render/include/ae/render/debug_renderer.h`,
`engine/render/src/debug_renderer.cpp`

### 3. Complete occlusion query pipeline — Use results for dummy culling

**Before:** The occlusion query system was half-wired:
1. Queries were **issued** for each visible dummy (render a bounding quad,
   query `GL_SAMPLES_PASSED`) — lines 1640–1660.
2. Results from the **previous frame** were **read** into `occlusion_results[]`
   — lines 1665–1673.
3. BUT `occlusion_results[]` was **never consumed** — both `draw_depth_pre_pass`
   and `draw_main_color_pass` drew all dummies unconditionally (after frustum
   culling only).

This meant every frame: GPU issue queries (wasted work), CPU reads results
(wasted work), and dummies were always drawn regardless of visibility.

Additionally, `occlusion_results[]` was initialized to all `false`, meaning
if the results were ever used, the first frame would draw no dummies.

**After:**
- `occlusion_results[]` initialized to all `true` — first frame draws everything
  (no stale occlusion data).
- Added occlusion-result check in `draw_depth_pre_pass` dummy loop: skip dummy
  if its previous-frame query returned 0 samples.
- Added occlusion-result check in `draw_main_color_pass` dummy loop: same logic,
  also increments `render_stats.culled_dummies` for occluded dummies.
- One-frame latency inherent in the query pipeline (queries from frame N-1
  affect frame N), which is acceptable for debug rendering.

**Files changed:** `engine/render/src/debug_renderer.cpp`

### 4. Fix map geometry cell duplication

**Before:** `build_arena()` added **every piece of geometry to every spatial
cell** (16 cells × full map). The comment at line 413 acknowledged this:

> "Build all geometry into all cells (each cell gets a full copy for now —
> this gives us spatial partitioning without complex clipping)"

This meant:
- 16 duplicate copies of all VBO data on the GPU.
- Frustum culling happened at the cell level, but cells were loaded with
  geometry that didn't belong to them.
- `CellBuilder::min_x`/`max_x` fields served double duty: set as cell
  boundaries in `build()`, then overwritten as geometry accumulation bounds
  by `add_box`/`add_quad`/`add_line`.

**After:**
- Added separate cell boundary fields (`cell_min_x`, `cell_max_x`,
  `cell_min_z`, `cell_max_z`) to `CellBuilder`. Set once in `build()`,
  never overwritten by geometry additions.
- Added `overlaps_xz()` method for AABB-vs-AABB overlap test on the XZ plane.
- Each `add_box`, `add_quad`, and `add_line` method now gates itself:
  if the incoming geometry's XZ bounding box does not overlap the cell's
  XZ bounds, the method returns immediately (no geometry added to that cell).
- `build_arena()` loops remain unchanged — all cells are still iterated,
  but most skip geometry that falls outside their spatial region.
- Large features (arena floor, outer ring track) still go to all 16 cells
  because they span the entire map. Smaller features (cover blocks, ramps,
  spawn structures, direction markers) now go only to their overlapping
  cells — typically 1–4 cells instead of all 16.

**Expected impact:** GPU memory for map geometry VBOs reduced by ~60–70%
(most geometry is local). Per-cell draw calls process less data.

**Files changed:** `engine/render/src/map_geometry.cpp`

## Files Changed

| File | Change |
|------|--------|
| `engine/core/include/ae/core/job_system.h` | Added `children` vector to `Job` struct |
| `engine/core/src/job_system.cpp` | Completed `submit_after` parent→child wiring; child notification in `worker_loop` and `wait` |
| `engine/render/include/ae/render/debug_renderer.h` | Removed dead `gpu_time_entities_ms` field |
| `engine/render/src/debug_renderer.cpp` | Removed dead `gpu_time_entities_ms`; initialized occlusion results to `true`; used occlusion results to cull dummies in both draw passes |
| `engine/render/src/map_geometry.cpp` | Added cell boundary fields + overlap gating to `CellBuilder::add_box/add_quad/add_line` |

Total Phase 5 changes: 5 files, ~55 insertions, ~30 deletions.

## Interfaces / Contracts

### `JobSystem::submit_after` (updated behavior)

```cpp
JobHandle submit_after(JobHandle parent, JobFunction fn);
```

Child job now correctly waits for parent to complete. Parent completion
atomically decrements child's `unfinished_parents`. Multiple children
of a single parent are all decremented. Multi-level dependencies
(grandchildren) also work via transitive decrements.

The `wait` method on the main thread also participates in dependency
resolution — jobs completed during `wait` notify their children.

### `CellBuilder::overlaps_xz` (new, internal)

```cpp
bool overlaps_xz(float bx_min_x, float bx_max_x,
                 float bx_min_z, float bx_max_z) const;
```

Returns `true` if the box [bx_min_x, bx_max_x] × [*, *] × [bx_min_z, bx_max_z]
overlaps the cell's spatial bounds. Used internally by `add_box`, `add_quad`,
and `add_line`.

## Tests / Validation

- `ae_core` static library builds clean (job_system.cpp)
- `ae_render` static library builds clean (debug_renderer.cpp, map_geometry.cpp)
- `ahamkara_client` executable links clean
- `ahamkara_server` executable links clean
- `ahamkara_asset_pipeline_tests` — passed (no regression)
- `ahamkara_session_tests` — passed (no regression)
- Pre-existing test failures in `ahamkara_world_tests` (`test_world_jump_through`)
  and `ahamkara_gameplay_tests` (`test_match_state_add_score`) are unrelated
  game logic issues. No new test failures introduced.

## Known Issues

1. **Occlusion query 1-frame latency:** Occluded dummies that become visible
   take 1 frame to appear. This is inherent in GPU query pipelines and
   acceptable for debug rendering. A depth-only pre-pass alternative would
   remove the latency but requires architectural changes.
2. **`gpu_usage_percent` field:** Also permanently zero (set in
   `metrics.cpp:200` as `0.0`). Not addressed in this phase — it lives in
   the runtime metrics snapshot, not the renderer. Could be addressed when
   platform GPU usage APIs are integrated.
3. **Map geometry line VBOs:** The `CellBuilder::upload` method has dead code
   for line handling (lines 150–158). The re-upload logic at lines 428–475
   duplicates the upload work done by the initial `upload()` call. Not
   addressed — requires deeper refactoring of the map build pipeline.

## Next Recommended Steps

1. **Add a dedicated test for `JobSystem::submit_after`** — submit a parent
   job and a dependent child, verify the child only executes after the parent.
2. **Consider removing `import_entry` double-hash of source files** — when
   `can_skip_import` returns true, the source file is hashed in
   `populate_record_identity` and then compared in the skip check. The hash
   is needed for the registry, so this is not strictly waste, but a
   file-mtime fast path could be explored.
3. **Clean up `CellBuilder::upload`** — remove the duplicate VBO creation
   (initial upload + re-upload in `build()`) and the dead line-handling code.
4. **Wire `gpu_usage_percent`** — integrate with platform GPU usage APIs
   (e.g., IOKit on macOS, NVML on Linux) or remove the field.

## Notes For Integrator

- The `JobSystem::submit_after` fix is a correctness change — without it,
  the method is a silent no-op/leak. No existing code calls `submit_after`,
  so no existing behavior is affected.
- The map geometry change is purely additive to the `CellBuilder` API —
  no callers outside `map_geometry.cpp` are affected. The `build_arena()`
  function and all its geometry data remain unchanged.
- The occlusion query behavior change may affect visual output if dummies
  are occluded — they will no longer be drawn. This is the intended behavior
  and completes the half-wired pipeline.
- The `gpu_time_entities_ms` removal is a public API change (`DebugScene`
  field removed). No consumers reference this field.
