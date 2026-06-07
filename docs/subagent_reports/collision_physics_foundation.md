# Task
Implement the first foundation for FPS collision/physics covering broadphase, ray/capsule/sphere traces, static triangle mesh collision, dynamic rigid bodies, trigger volumes, hitbox/hurtbox resolution, layer masks, CCD plan, server authority, and debug overlays — all built as a new `engine/collision` module backed by the existing Jolt Physics v5.0.0 integration.

# Outcome

**Fully implemented:**
- `engine/collision` static library (`ae_collision`) with public headers in `include/ae/collision/`
- Pure-data collision primitives: `Ray`, `Sphere`, `Capsule`, `AABB`, `Triangle`
- Unified `TraceResult` for all shape traces
- Ray trace, sphere sweep, capsule sweep, sphere overlap, AABB overlap, cone trace (deterministic Xorshift64)
- `CollisionWorld` class: body CRUD for Box/Sphere/Capsule/TriangleMesh shapes, Static/Kinematic/Dynamic body types, sensor flag for triggers, `step()` integration, broadphase AABB queries, stats
- 64-bit `CollisionLayer`/`CollisionMask` system with 10 pre-defined `GameLayers` and four pre-built masks
- `HitboxInstance` struct with hitbox/hurtbox separation, `resolve_hitbox_hurtbox_overlaps()` pure-math resolver
- `TriggerVolumeDef`/`TriggerEvent` data types; sensor bodies supported via `is_sensor` in `BodyDef`
- `DebugOverlay` struct + `populate_debug_overlay`/`populate_debug_hitboxes`/`populate_debug_trace` — renderer-agnostic (no render dependency)
- Jolt backend fully encapsulated behind `CollisionWorld::Impl` in internal header `src/jolt_backend.h`
- 22 tests covering AABB math, masks, hitbox resolution, body CRUD, all trace types, overlap queries, debug overlay
- Builds for both client (renderer) and server (headless)

**Partially implemented:**
- `ColliderShape::ConvexHull` — falls back to Box shape (not yet wired to JPH::ConvexHullShape)
- Trigger events — data types defined but no automatic enter/leave callbacks wired in `CollisionWorld::step()` yet
- `collision_mask` in `BodyDef` — plumbed into internal record but not yet applied at Jolt contact-filter level (pair filters are permissive)

**Not implemented:**
- Actual migration of `game/src/world.cpp` to use the new `CollisionWorld` (existing Jolt usage in world.cpp is unchanged)
- Integration of debug overlays into the client renderer's `DebugScene`
- CCD (Continuous Collision Detection) — Jolt supports it via `EMotionQuality::LinearCast`, the plan is documented but no API surface was added

# Files Changed

- `CMakeLists.txt` — added `add_subdirectory(engine/collision)` between core and network (headless-safe, no renderer dependency)
- `tests/CMakeLists.txt` — added `ahamkara_collision_tests` executable target and CTest registration
- `engine/collision/CMakeLists.txt` — NEW: module build, links `ae_core` + `Jolt`
- `engine/collision/include/ae/collision/types.h` — NEW: `Ray`, `Sphere`, `Capsule`, `AABB`, `Triangle`, `TraceResult`, `TriggerVolumeDef`, `TriggerEvent`, `HitboxInstance`
- `engine/collision/include/ae/collision/layers.h` — NEW: `CollisionLayer`, `CollisionMask`, `GameLayers`
- `engine/collision/include/ae/collision/trace.h` — NEW: `TraceParams`, `ray_trace`, `sphere_trace`, `capsule_trace`, `sphere_overlap`, `aabb_overlap`, `cone_trace_closest`, `resolve_hitbox_hurtbox_overlaps`
- `engine/collision/include/ae/collision/world.h` — NEW: `BodyType`, `ColliderShape`, `ColliderDef`, `BodyDef`, `BodyHandle`, `CollisionStats`, `CollisionWorld`
- `engine/collision/include/ae/collision/debug.h` — NEW: `DebugBox`, `DebugLine`, `DebugSphere`, `DebugColors`, `DebugOverlay`, `populate_debug_overlay`, `populate_debug_hitboxes`, `populate_debug_trace`
- `engine/collision/src/jolt_backend.h` — NEW: internal header defining `CollisionWorld::Impl`, Jolt layer mapping, `BPLayerInterfaceImpl`, shape factory, vector conversion helpers
- `engine/collision/src/collision_world.cpp` — NEW: `CollisionWorld` public method implementations
- `engine/collision/src/trace.cpp` — NEW: all trace/overlap/cone/hitbox implementations using Jolt `NarrowPhaseQuery`
- `engine/collision/src/debug.cpp` — NEW: debug overlay population implementations
- `tests/src/collision_tests.cpp` — NEW: 22 unit/integration tests

# Interfaces Added Or Changed

**Public headers (game-code visible, no Jolt dependency):**
- `ae/collision/types.h` — 9 structs + 2 enums, all trivially copyable where asserted
- `ae/collision/layers.h` — `CollisionLayer` (u8), `CollisionMask` (u64 wrapper), `GameLayers` (static constexpr layer indices + static mask factories)
- `ae/collision/trace.h` — 7 free functions taking `const CollisionWorld&`
- `ae/collision/world.h` — `CollisionWorld` class with 14 public methods, `BodyDef`/`ColliderDef`/`BodyHandle` types
- `ae/collision/debug.h` — `DebugOverlay` struct (fixed-size arrays, 256/256/128 caps) + 3 free functions

**Internal header (`src/jolt_backend.h`, NOT for game code):**
- `CollisionWorld::Impl` — full Jolt-backed implementation with `JPH::PhysicsSystem`, body map, shape factory
- `jolt_helpers` namespace — layer-to-Jolt mapping, BP layer constants
- `BPLayerInterfaceImpl`, `ObjectLayerPairFilterImpl`, `ObjectVsBPLayerFilterImpl`, `AllPassBroadPhaseLayerFilter`, `AllPassObjectLayerFilter` — Jolt filter implementations
- `to_jolt_rvec3`/`to_jolt_vec3`/`from_jolt_rvec3`/`from_jolt_vec3` — vector conversion helpers

**CMake target:**
- `ae_collision` — static library, PUBLIC depends on `ae_core` and `Jolt`

**Test target:**
- `ahamkara_collision_tests` — executable, PRIVATE depends on `ae_core` + `ae_collision`, registered with CTest

# Behavior

- **Build**: `ae_collision` compiles into a static library. Client and server both link correctly.
- **Runtime**: `CollisionWorld` owns a Jolt `PhysicsSystem` instance. Bodies can be added/removed/transformed. `step()` advances the physics simulation. Trace queries (`ray_trace`, `sphere_trace`, etc.) return `TraceResult` with hit position, normal, distance, and body index. Layer masks filter which bodies participate.
- **Headless**: The module has zero renderer dependencies. Server builds include it transitively through the root CMakeLists.txt placement.
- **Tests**: `ctest` runs 5 suites (pre-existing: smoke, world, movement, asset_pipeline; new: collision). All 5 pass — 100%.
- **Existing game code**: Unchanged. `world.cpp` continues to use Jolt directly. The `CollisionWorld` is a parallel, cleaner API ready for migration.

# Validation

**Build commands run:**
```
cmake --preset debug -S .
cmake --build build/debug --target ae_collision          # collision module
cmake --build build/debug --target ahamkara_collision_tests  # tests
cmake --build build/debug --target ahamkara_game         # game lib (unchanged, still builds)
cmake --build build/debug --target ahamkara_server       # dedicated server (builds)
cmake --build build/debug --target ahamkara_client       # debug client (builds)
```

**Tests run:**
```
ctest --test-dir build/debug --output-on-failure
```

**Results:** 5/5 tests pass (0 failures). 22 individual collision test cases pass:
- AABB: contains, overlaps, center/extents, expand from empty
- CollisionMask: set/clear/test, overlaps, pre-built player/projectile/camera/trigger masks
- Triangle: normal (CCW = Y+), reverse winding
- Hitbox/Hurtbox: basic overlap, self-damage skip, multiple boxes, damage multiplier
- CollisionWorld: create, add static box/sphere/capsule/sensor/mesh, remove, position/userdata/bounds
- Traces: ray hit, ray miss, layer mask filtering, sphere sweep to ground, capsule sweep to wall, sphere overlap, AABB query
- Debug: overlay clear, body population, hitbox population

**Warnings:** 2 pre-existing warnings from `world.cpp` about Jolt string literal operators — unrelated to this work.

# Known Gaps

1. **`ColliderShape::ConvexHull` not implemented** — falls back to BoxShape. Jolt supports `ConvexHullShape` but the vertex-to-hull API was deferred.
2. **Trigger enter/leave callbacks not wired** — `TriggerVolumeDef` and `TriggerEvent` types exist, and bodies can be marked `is_sensor`, but `CollisionWorld::step()` does not yet iterate contact pairs to emit enter/leave events.
3. **`BodyDef::collision_mask` not enforced at Jolt level** — The mask is stored in the internal body record and used by trace queries (via `MaskBodyFilter`), but the Jolt pair filter (`ObjectLayerPairFilterImpl`) is permissive. Per-body-pair mask filtering would require a `ContactListener` or custom `BodyFilter` in `PhysicsSystem::Init`.
4. **No CCD API surface** — Jolt's `EMotionQuality::LinearCast` and CCD are supported internally but no `CollisionWorld` method exposes CCD body creation or configuration. The plan is documented in headers.
5. **`game/src/world.cpp` not migrated** — Existing Jolt usage (character, projectile raycasts, map bodies) remains in `ahamkara_game` and creates a separate `JPH::PhysicsSystem` instance. Future work would unify under `CollisionWorld`.
6. **Debug overlays not wired to renderer** — `DebugOverlay` data is populated but not consumed by `DebugScene` or `DebugRenderer`. The renderer's `DebugScene` has `level_boxes[64]` that could be filled from `DebugOverlay::boxes[]`.
7. **No network serialization for collision types** — `TraceResult`, `HitboxInstance`, etc. are trivially copyable but have no serialization in `net_packets.h`.

# Risks

- **Dual Jolt initialization**: `game/src/world.cpp` calls `JPH::RegisterDefaultAllocator()` + `JPH::RegisterTypes()` in `initialize_jolt_once()`. The collision module also initializes Jolt via a static `JoltInitToken`. Jolt guards against double-registration, but two `PhysicsSystem` instances exist if both `World` and `CollisionWorld` are alive simultaneously. Integration work should migrate `World` to use `CollisionWorld` to eliminate the duplicate physics system.
- **Jolt v5.0.0 API surface**: The internal `jolt_backend.h` uses specific Jolt v5 APIs (`RShapeCast`, `TransformedShape::GetWorldSpaceBounds`, `CollideAABox` with collector pattern). Upgrading Jolt would require reviewing this file.
- **Static init order**: The `JoltInitToken` pattern (static local in `ensure_jolt_initialized()`) guarantees Jolt is initialized before `TempAllocatorImpl` construction, but relies on C++11 magic statics. This is safe for C++20 but worth noting.
- **Thread safety**: `CollisionWorld` uses `JPH::JobSystemThreadPool` with 1 thread. All queries are effectively single-threaded. Multithreaded simulation would require a mutex count > 0 in `PhysicsSystem::Init` and careful locking around `bodies` map access from trace callbacks.

# Next Recommended Steps

1. **Wire debug overlays into client renderer** — Map `DebugOverlay::boxes[]` → `DebugScene::level_boxes[]` in `debug_client.cpp::build_debug_scene()`. This gives immediate visual feedback for collision geometry.
2. **Migrate `game/src/world.cpp` to use `CollisionWorld`** — Replace the internal `JoltWorldImpl`/`JPH::PhysicsSystem` with a `CollisionWorld` member. Update `recreate_jolt_colliders()`, `update_projectiles()` (raycasts), and character movement to use the new API. This eliminates the duplicate physics system.
3. **Implement trigger enter/leave callbacks** — Add a `ContactListener` or post-step iteration in `CollisionWorld::step()` that detects sensor overlaps and populates `TriggerEvent` results.
4. **Expose CCD configuration** — Add `BodyDef::use_ccd` flag, map to `EMotionQuality::LinearCast` in `CollisionWorld::add_body()`, and add `ccd_step()` or integrate into `step()`.
5. **Add network serialization for collision primitives** — Extend `net_packets.h` with `write_trace_result`/`read_trace_result` and `write_hitbox_instance`/`read_hitbox_instance` for server-authoritative hit registration.
6. **Hook up `BodyDef::collision_mask` at contact level** — Either use Jolt's `ContactListener::OnContactValidate` to filter by per-body mask, or extend `ObjectLayerPairFilterImpl` to consult the body map.
7. **Implement `ConvexHull` shape** — Add `JPH::ConvexHullShapeSettings` path in `Impl::create_shape()` for `ColliderShape::ConvexHull`.

# Notes For Integration

- The module is added unconditionally in the root `CMakeLists.txt` before the client/server split, so it builds for both. It depends only on `ae_core` + `Jolt`.
- Game code that wants to use collision should link `ae_collision` (currently only the test target does). To use it from `ahamkara_game`, add `ae_collision` to `game/CMakeLists.txt`'s `target_link_libraries`.
- The `DebugOverlay` struct uses fixed-size arrays with compile-time caps (`kMaxDebugBoxes=256`, etc.). These are independent of the renderer's `DebugScene` caps (`level_boxes[64]`). Integration must handle truncation.
- All trace functions take `const CollisionWorld&` and are safe to call from any thread that holds a reference to the world, as long as `step()` is not running concurrently.
- The internal header `src/jolt_backend.h` must NOT be included from outside `engine/collision/src/`. It exposes Jolt types directly and would break the renderer/platform isolation contract.
