# Phase 4A: Networking Ownership Simplification

## Summary

Clarified the three-layer state ownership model in the Ahamkara networking pipeline
without changing any protocol behavior.  Every boundary between
**authoritative**, **predicted**, and **interpolated** state is now explicitly
labeled in comments and a long-standing ambiguity around `World::is_client_`
has been resolved.

## What Changed

### 1. `World::is_client_` — renamed semantics and server fix

**File:** `game/include/ahamkara/game/world.h`

- Added a doc comment on `set_is_client()` explaining that `true` means
  "client-side prediction world" (enables cosmetic-only effects like
  hitmarkers that should not be replicated), and `false` means "authoritative
  simulation."
- Annotated the member `bool is_client_ {true}` with the inline comment
  `// True = prediction/visual copy; false = authoritative`.

**File:** `server/src/dedicated_server_main.cpp`

- The server now explicitly calls `world.set_is_client(false)` after
  constructing its authoritative `World`.  Previously the server relied on
  the default (`true`), which meant hitmarker timers were being set on the
  server even though the server never renders them.  This was a semantic
  no-op (hitmarkers are cosmetic-only) but confusing to readers.

### 2. Server loop: authoritative → snapshot boundary annotated

**File:** `server/src/dedicated_server_main.cpp`

Added three labeled comment blocks inside the server tick loop:

| Section               | Label                       |
|-----------------------|-----------------------------|
| World construction    | `Authoritative world`       |
| `world.tick()` call   | `Authoritative tick`        |
| Snapshot population   | `Snapshot boundary`         |
| History buffer record | `History record`            |

Each block explains which side of the ownership boundary the code sits on.

### 3. Client loop: three state layers documented

**File:** `client/src/headless_clients.cpp`

- Added a block comment at the top of `run_network_client()` that documents
  the three state layers:

  ```
  Layer 1 — PREDICTED:    ClientPredictionManager (local World copy)
  Layer 2 — AUTHORITATIVE: ServerSnapshot::local_player (ground truth)
  Layer 3 — INTERPOLATED:  SnapshotInterpolator lerp (smooth render)
  ```

- Added inline section headers in the main loop:

  | Section            | Label                          |
  |--------------------|--------------------------------|
  | Input build + send | `PREDICTED LAYER`              |
  | Snapshot reception | `AUTHORITATIVE LAYER`          |
  | `reconcile()` call | `Reconciliation boundary`      |
  | Interpolation      | `INTERPOLATED LAYER`           |

### 4. `log_interpolated_state` → `log_state_comparison`

**File:** `client/src/headless_clients.cpp`

- Renamed to better describe what it does (diagnostic comparison, not
  production rendering logic).
- Simplified parameter list: receives `ReplicatedPlayerState authoritative`
  and `ae::u32 snap_tick` directly instead of the full `ServerSnapshot`
  struct.  This eliminates the redundant `raw_snapshot.local_player` access
  pattern at the call site.
- Added a doc comment enumerating which state each parameter represents and
  where it comes from.

Call site updated accordingly:
```cpp
log_state_comparison(interpolated_player, newer_snap.local_player,
                     newer_snap.server_tick, &pred_state, ...);
```

### 5. `ClientPredictionManager` doc improvement (header)

**File:** `game/include/ahamkara/game/client_prediction.h`

- No source changes.  The existing doc comment already describes the
  prediction/reconciliation model well.  The new integration comments in
  `headless_clients.cpp` now cross-reference it explicitly.

## State Ownership Model (Post-Change)

```
┌─ CLIENT ────────────────────────────────────────────────────────────────┐
│                                                                         │
│  Input ─────► apply_input() ──► PREDICTED World ◄── reconcile() ──────┐ │
│                 │                        │              ▲             │ │
│                 │ send to                │ render       │             │ │
│                 │ server                 ▼              │             │ │
│                 │                  [predicted state]    │             │ │
│                 │                                       │             │ │
│  ┌─ Network ───┼───────────────────────────────────────┼──────────┐  │ │
│  │              ▼                                       │          │  │ │
│  │        ┌──────────┐                                 │          │  │ │
│  │        │  SERVER  │                                 │          │  │ │
│  │        │ World    │────► Snapshot ──► HistoryBuffer  │          │  │ │
│  │        │ (authoritative)                             │          │  │ │
│  │        └──────────┘                                 │          │  │ │
│  │              │                                       │          │  │ │
│  │              ▼                                       │          │  │ │
│  │       ServerSnapshotPacket ──► AUTHORITATIVE state ──┘          │  │ │
│  │                                     │                           │  │ │
│  └─────────────────────────────────────┼───────────────────────────┘  │ │
│                                        │                              │ │
│                                        ▼                              │ │
│                              SnapshotInterpolator                     │ │
│                                        │                              │ │
│                                        ▼                              │ │
│                              INTERPOLATED state ──► render            │ │
│                                                                       │ │
└───────────────────────────────────────────────────────────────────────┘
```

**Ownership rules:**
1. Only the **server** writes `World` tick-by-tick (authoritative).
2. The **client's `ClientPredictionManager::world_`** is a prediction copy;
   it is overwritten during reconciliation when it diverges.
3. `ServerSnapshot::local_player` is a **frozen snapshot** of authoritative
   state at a specific tick; the interpolator smooths between two snapshots.
4. The **interpolated state** is transient — computed each frame from
   authoritative snapshots and never stored back into the prediction world.

## Validation

- `headless_clients.cpp` — syntax-only compilation: **PASS** (0 errors)
- `dedicated_server_main.cpp` — syntax-only compilation: **PASS** (0 errors)
- `network_smoke_tests.cpp` — syntax-only compilation: **PASS** (0 errors)
- Full project build: blocked by pre-existing `world.cpp` refactoring
  (`jolt_` → `physics_` rename in progress, unrelated to these changes).
- No protocol behavior changed — packet formats, tick rates, interpolation
  logic, and reconciliation thresholds are untouched.

## Future Work (not in this phase)

1. **Reconciliation on first snapshot:** `ClientPredictionManager::reconcile`
   sets world state to authoritative but does not replay unacknowledged
   inputs when `last_ack_ == 0`.  This means inputs accumulated before the
   first snapshot are effectively dropped.  Fixing this requires a
   behavior change and is deferred.

2. **`is_client_` → `is_prediction_` rename:** The flag name is misleading
   (it describes the *role* of the World, not where the binary runs).  A
   rename would be a broader refactor touching many files.

3. **Interpolated render usage:** The current `run_network_client` loop
   computes `interpolated_player` but only uses it for diagnostic logging;
   in a real renderer it would drive the camera.  This stub path is
   intentional for headless/CI clients.
