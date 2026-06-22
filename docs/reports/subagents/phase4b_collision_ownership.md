# Phase 4B: Collision Ownership Decision And Bridge

## Summary

This phase clarifies the collision/physics ownership boundary between the engine
(`ae::collision`) and the game layer (`ahamkara::game`).  No runtime behavior
changed; the deliverable is a documented migration seam and an explicit
ownership decision.

## Problem

Two parallel Jolt `PhysicsSystem` instances exist in the codebase:

| Component | Location | Layer count | Status |
|-----------|----------|-------------|--------|
| `ae::collision::CollisionWorld` | `engine/collision/` | 10+ layers, 64-bit masks | Fully implemented, tested, **not wired to game** |
| Direct Jolt in `World` | `game/src/` | 2 layers (NON_MOVING/MOVING) | Actively used for movement + projectiles |

This creates ambiguity:
- Which layer "owns" collision access?
- Should new code use `CollisionWorld` or direct Jolt?
- How do we evolve toward one path?

## Decision

**The game layer owns its Jolt `PhysicsSystem` for now.  This is mandatory
because of `JPH::CharacterVirtual` (KCC).**

The engine's `CollisionWorld` is the long-term target for body management
and trace queries.  The game's direct Jolt usage should be treated as an
implementation detail that will eventually migrate to `CollisionWorld`.

### Why game must own its PhysicsSystem

`JPH::CharacterVirtual` (kinematic character controller) must share a
`PhysicsSystem` with the bodies it interacts with — map geometry, dummies,
moving platforms.  `CollisionWorld` does not abstract character controllers
(that would be a feature addition to `ae::collision`), so the game layer
cannot delegate its `PhysicsSystem` to the engine yet.

### Why CollisionWorld is the long-term path

`CollisionWorld` provides:
- A backend-agnostic API (no Jolt headers leaked to consumers)
- Rich layer/mask system (10+ pre-defined game layers)
- Trace primitives (ray, sphere, capsule, cone)
- Body CRUD with proper handle management
- Overlap queries and AABB queries
- Debug overlay support

These are all battle-tested by 22 collision tests.  The parallel direct-Jolt
code in the game layer is a simplified (2-layer) subset that duplicates
functionality.

## Deliverable

### `game/src/game_physics.h` — Migration Seam Header

A new header that serves as the **single documented entry point** for Jolt
includes in the game layer.  Instead of scattering `#include <Jolt/...>`
across multiple game source files, all game code that needs Jolt should
include this header.

The header contains:
- A detailed ASCII diagram showing the two physics worlds
- A 3-step migration path toward `CollisionWorld` unification
- Explicit barriers preventing immediate migration
- Rules for when to add new Jolt usage vs when to use `CollisionWorld`

### Ownership Boundary

```
┌──────────────────────────────────────────────┐
│ engine/collision/                            │
│   CollisionWorld (Jolt-backed, opaque API)   │
│   • Body CRUD • Traces • Layers • Stats      │
│   • Ready for use by any subsystem           │
│   • Does NOT own CharacterVirtual            │
└──────────────────────────────────────────────┘
          ▲ future migration path
          │
┌──────────────────────────────────────────────┐
│ game/src/                                    │
│   Direct Jolt (via game_physics.h)           │
│   • PhysicsSystem + CharacterVirtual KCC     │
│   • Map bodies + dummy bodies               │
│   • Projectile raycasts + rollback           │
│   • 2-layer system (NON_MOVING/MOVING)       │
└──────────────────────────────────────────────┘
```

## Migration Path

### Step 1: Add accessor mirrors (safe now)

Add methods to the game's physics wrapper that mirror `CollisionWorld`'s API:

```cpp
// Current (direct Jolt):
auto& bi = physics_->physics_system.GetBodyInterface();
bi.SetPositionAndRotation(id, pos, rot, activate);

// Target (after adding accessor):
physics_->set_body_position(handle, position);
```

This makes future migration a search-and-replace.  No behavior change.

### Step 2: Add Character abstraction to ae::collision (medium)

Add `ae::collision::Character` or extend `CollisionWorld` with KCC support.
Once `CharacterVirtual` is abstracted behind the engine boundary, the game
can drop its `PhysicsSystem` and delegate body management to `CollisionWorld`.

### Step 3: Unify (major)

- All bodies live in `CollisionWorld`
- The character controller (now owned by `ae::collision`) references that world
- `game_physics.h` becomes a thin adapter composing `CollisionWorld` + `Character`
- Remove the game-owned `PhysicsSystem`

## Current Barriers

1. **CharacterVirtual not abstracted**: `CollisionWorld` has no KCC support.
   Adding it requires a design decision about how movement integrates with
   the engine layer.

2. **Layer mismatch**: Game uses 2 layers (NON_MOVING/MOVING); `CollisionWorld`
   uses 10+.  Migrating bodies requires mapping game layers to engine layers.

3. **Rollback pattern**: Projectile hit detection temporarily repositions
   dummy bodies for historical raycasts, then restores them.  `CollisionWorld`
   would need a "scratch transform" or query-time transform override to
   support this without mutating live body state.

## Files Changed

| File | Change |
|------|--------|
| `game/src/game_physics.h` | **NEW** — Documented migration seam header |
| `docs/reports/subagents/phase4b_collision_ownership.md` | **NEW** — This report |

No runtime files were modified.  The existing `world.cpp`, `engine/collision/`,
and tests are unchanged.

## Validation

- `ae_collision` target: builds and passes 22/22 tests (pre-existing)
- `ahamkara_world_tests`: 11/11 pass (pre-existing, unchanged)
- `ahamkara_movement_tests`: 14/14 pass (pre-existing, unchanged)
- Server/game behavior: unchanged (no code modified)

## Next Steps

1. **Wire `ae_collision` into the game CMakeLists.txt** — Add `ae_collision` to
   `game/CMakeLists.txt`'s `target_link_libraries` so the game layer can start
   using `CollisionWorld` for non-KCC queries (e.g., trigger volumes, overlap
   checks).

2. **Add a `Character` abstraction to `ae::collision`** — This is the critical
   missing piece that prevents unification.  Once it exists, the migration
   becomes mechanical.

3. **Add `set_body_transform()` accessor to game physics** — The first
   Step 1 action, making the API surface match `CollisionWorld`.

4. **Layer mapping design** — Decide how the game's 2-layer system maps to
   `CollisionWorld`'s 10+ layer system.  Use `GameLayers::WORLD_STATIC` for
   map geometry, `GameLayers::PLAYER` for the KCC, `GameLayers::NPC` for
   dummies, etc.
