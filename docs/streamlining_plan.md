# Ahamkara Streamlining And Hardening Plan

## Goal

This plan focuses on reducing codebase friction, improving maintainability, and
removing obvious efficiency traps without widening the engine feature surface.
It is intended to run alongside the broader integration work, but with a bias
toward making the repository easier to reason about, build, and evolve.

## Current Friction Snapshot

The biggest streamlining hotspots right now are:

- Very large implementation files:
  - `engine/render/src/debug_renderer.cpp`
  - `game/src/world.cpp`
  - `tools/asset_importer.cpp`
  - `client/src/headless_clients.cpp`
  - `server/src/dedicated_server_main.cpp`
- Layering drift:
  - `ae_network` currently depends on `ahamkara_game`
  - collision and gameplay still overlap around Jolt ownership
- Build-system duplication and branch drift:
  - many targets evolved in parallel
  - tests and module `CMakeLists.txt` are now critical merge surfaces
- Utility duplication:
  - CLI parsing duplicated in client/server
  - importer parsing and ad hoc file formats growing in one executable
- Partial portability:
  - `engine/core/src/config.cpp` still uses `<sys/stat.h>`
- Incomplete infrastructure primitives:
  - `JobSystem::submit_after()` is stubbed
  - some profiler/timer fields exist but are not fully measured

## Principles

- Prefer decomposition over cleverness.
- Move duplicated logic into small shared helpers before adding more features.
- Fix ownership boundaries before optimizing runtime behavior across them.
- Keep runtime-facing code lean and tool/import code explicit.
- Preserve headless server safety: render/audio/editor code must stay out of
  server-only paths.

## Phase 1: Build And Boundary Cleanup

### Why first

If the target graph and module boundaries are unstable, every later cleanup
becomes harder to verify.

### Work

1. Normalize module dependencies.
   - Remove `ae_network -> ahamkara_game`.
   - Move `PacketEnvelope` or `SequenceTracker` to a layer that breaks the
     dependency cycle cleanly.

2. Audit all `CMakeLists.txt` files for:
   - duplicate or accidental link propagation
   - client-only modules leaking into headless targets
   - missing target-local include boundaries

3. Define a stable build matrix:
   - `ahamkara_client`
   - `ahamkara_server`
   - `ahamkara_asset_importer`
   - key test suites

4. Convert platform-sensitive code to standard facilities where possible.
   - Replace `stat()` polling in `engine/core/src/config.cpp` with
     `std::filesystem::last_write_time()`.

### Primary files

- `CMakeLists.txt`
- `engine/network/CMakeLists.txt`
- `game/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `engine/core/src/config.cpp`

## Phase 2: Large File Decomposition

### Why second

The largest files are now carrying too many responsibilities, which slows
debugging, review, and integration.

### Work

1. Split `engine/render/src/debug_renderer.cpp` by responsibility:
   - frame lifecycle / render orchestration
   - GPU model upload + draw helpers
   - debug overlay and HUD drawing
   - profiler UI
   - map/default-scene drawing

2. Split `game/src/world.cpp` by system area:
   - projectile/combat
   - movement integration
   - collision bridge
   - dummy/AI-ish simulation
   - camera/debug state population

3. Split `tools/asset_importer.cpp` into importer helpers:
   - manifest parsing
   - registry/cache handling
   - TGA texture import
   - material import
   - entry dispatch

4. Split network entrypoint glue:
   - move shared CLI parsing and simulator config helpers out of
     `client/src/headless_clients.cpp` and
     `server/src/dedicated_server_main.cpp`

### Primary files

- `engine/render/src/debug_renderer.cpp`
- `game/src/world.cpp`
- `tools/asset_importer.cpp`
- `client/src/headless_clients.cpp`
- `server/src/dedicated_server_main.cpp`

## Phase 3: Shared Utility Consolidation

### Why third

Once the big files are decomposed, the common helper patterns become easier to
extract without creating a junk drawer.

### Work

1. Create shared CLI parsing helpers.
   - `engine/core/include/ae/core/cli_utils.h`
   - float/bool/flag parsing
   - simulator-config parsing helper

2. Create shared text parsing helpers for tools/config.
   - trimming
   - `key=value` parsing
   - token splitting

3. Create shared binary file IO helpers for compiled asset formats.
   - write/read primitives
   - string serialization
   - header/version helpers

4. Create one platform GL include shim if GL remains in multiple files.

### Primary files

- `engine/core/include/ae/core/cli_utils.h`
- `engine/core/include/ae/core/text_parse.h`
- `engine/render/src/platform/`
- `tools/asset_importer.cpp`
- `engine/render/src/compiled_*.cpp`

## Phase 4: Runtime Ownership Simplification

### Why fourth

This is where we reduce duplicate simulation ownership and make the code easier
to reason about end-to-end.

### Work

1. Decide the physics ownership model.
   - Either migrate `World` toward `ae_collision`
   - or explicitly keep direct Jolt in game for now and document the bridge

2. Reduce duplicate state pipelines in networking and movement.
   - clarify authoritative state, predicted state, interpolated state
   - centralize transform/state reconciliation points

3. Make dormant subsystems opt-in through narrow adapters.
   - animation driver adapter
   - audio event adapter
   - movement debug adapter

4. Avoid parallel “almost the same” systems.
   - if `CollisionWorld` is the future, stop adding more direct Jolt helpers in
     game code

### Primary files

- `game/src/world.cpp`
- `engine/collision/src/collision_world.cpp`
- `client/src/headless_clients.cpp`
- `server/src/dedicated_server_main.cpp`
- animation/audio integration call sites

## Phase 5: Efficiency Passes That Also Improve Clarity

### Why fifth

These are not micro-optimizations. They matter because they simplify behavior
and remove accidental waste.

### Work

1. Fix incomplete infrastructure:
   - implement `JobSystem::submit_after()`
   - remove dead or permanently-zero profiler fields

2. Reduce unnecessary work in tools/runtime:
   - continue incremental asset import
   - avoid full rewrites of unchanged compiled outputs

3. Replace obviously wasteful structures:
   - stop duplicating full map geometry into every spatial cell
   - tighten render-side handle pools if needed after integration

4. Make performance instrumentation trustworthy:
   - either measure `gpu_time_entities_ms` correctly or remove it
   - either use occlusion query results or remove the half-wired path

### Primary files

- `engine/core/src/job_system.cpp`
- `engine/render/src/map_geometry.cpp`
- `engine/render/src/debug_renderer.cpp`
- `tools/asset_importer.cpp`

## Phase 6: Test And Documentation Hardening

### Why sixth

A streamlined codebase still rots if the verification story is fuzzy.

### Work

1. Turn branch-history knowledge into permanent docs.
   - current subsystem status
   - integrated vs dormant
   - known boundary rules

2. Add targeted regression tests for extracted helpers.
   - CLI parsing
   - config reload semantics
   - importer manifest/cache behavior
   - compiled asset roundtrips

3. Add one integration checklist doc for high-conflict files.

4. Shrink ambiguity in test expectations.
   - use tolerant assertions only where physically necessary
   - avoid mixing “old behavior preserved” and “new behavior validated” in the
     same tests when possible

### Primary files

- `docs/subagent_reports/master_summary.md`
- `docs/architecture.md`
- `tests/src/*`
- future helper tests

## Suggested Execution Order

If we want the highest return with the lowest chaos, run the phases like this:

1. Phase 1: Build And Boundary Cleanup
2. Phase 2: Large File Decomposition
3. Phase 3: Shared Utility Consolidation
4. Phase 4: Runtime Ownership Simplification
5. Phase 5: Efficiency Passes That Also Improve Clarity
6. Phase 6: Test And Documentation Hardening

## Immediate Next Actions

These are the best first tickets to open:

1. Break `ae_network -> ahamkara_game`
2. Extract shared CLI parsing from client/server mains
3. Replace `stat()`-based config polling with `std::filesystem`
4. Split `tools/asset_importer.cpp` into helper translation units
5. Start carving `debug_renderer.cpp` into smaller renderer/private files
6. Decide whether `World` is migrating to `ae_collision` in this milestone or
   the next one

## Success Criteria

We should consider this streamlining pass successful when:

- The main build graph has clean ownership boundaries.
- The worst large files are no longer single-file subsystems.
- Common parsing/serialization helpers are shared instead of duplicated.
- Runtime ownership between networking, world simulation, and collision is
  explicit.
- Build/test/docs tell the same story without relying on tribal memory.
