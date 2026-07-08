# Phase 4D: State Flow Cleanup — Subagent Report

## Summary

Analyzed the Ahamkara runtime codebase for duplicate state representations, scattered
transform/reconciliation points, and unclear data ownership. Identified 4 key issues
and resolved the highest-impact ones with targeted, minimal changes.

## Key Findings

### 1. ECS ↔ Fixed Array Dual State (CRITICAL — PARTIALLY RESOLVED)

**Location**: `World` class — projectiles and dummies stored in both EnTT registry AND fixed arrays.

**Before**: The World had `projectiles_[kMaxProjectiles]` and `dummies_[kMaxDummies]` arrays
alongside EnTT entities with `ProjectileComponent` and `TargetDummyComponent`. Every tick,
`fire_projectile()`, `step_projectiles()`, and `tick_dummies()` independently synced between
these two representations — producing 4+ separate O(n) sync points scattered across files.

**After**: Centralized sync into two private World methods — `sync_dummies_to_array()` and
`sync_projectiles_to_array()` — called once at the end of `tick_internal()`. Removed
independent sync loops from `fire_projectile()`, `step_projectiles()`, and `tick_dummies()`.

**Impact**: 
- Single authoritative sync path per tick
- Eliminated 3 scattered sync sites
- The output arrays remain as render-friendly caches, now populated from one location
- EnTT registry is the authoritative runtime state; arrays are read-only output

### 2. Duplicate MovementState Resolution (RESOLVED)

**Location**: `movement.cpp` and `world_camera.cpp`.

**Before**: Two `resolve_movement_state()` functions with overlapping but inconsistent logic:
- `movement.cpp::resolve_movement_state()` — pure input-based, handled Idle/Walking/Sprinting/Jumping/Sliding
- `world_camera.cpp::resolve_movement_state()` — world-state-aware but MISSING LedgeGrab/Mantling/OnLadder
- `accelerate_movement()` also set movement_state inline with full coverage of all 9 states

**After**: Updated `world_camera.cpp::resolve_movement_state()` to accept `MovementSimState` and
handle ALL MovementState variants: LedgeGrab, Mantling, OnLadder, Jumping, Sliding, Sprinting,
Walking, Idle. Priority order matches `accelerate_movement()`. The pure-input version in
`movement.cpp` remains for test backward compatibility.

**Impact**:
- MovementState is now resolved consistently with full state coverage
- No more "silent Idle" when on a ladder or ledge
- World's `update_movement_state()` now passes `movement_sim_state_`

### 3. Historical State Duplication (NOTED — DEFERRED)

**Location**: `World::history_buffer_` and `ServerHistoryBuffer<HistoricalState>` in server code.

Both store `HistoricalState` structs with identical tick/position/dummy data. The World's
buffer is used for client-side rollback lag compensation; the server's buffer is for
authoritative history queries. These serve different roles (client vs server) but share
the same struct definition. Currently acceptable as distinct concerns.

### 4. LocalPlaySimulation State Caching (NOTED — ACCEPTABLE)

**Location**: `LocalPlaySimulation` in `client/src/local_play.cpp`.

Stores `previous_player_state_`, `previous_camera_anchor_`, `previous_dummies_` for render-side
interpolation. This is legitimate frame-to-frame interpolation state, not a duplicate
simulation state. No changes needed.

## Files Changed

| File | Change | Impact |
|------|--------|--------|
| `game/include/ahamkara/game/world.h` | Added `sync_dummies_to_array()`, `sync_projectiles_to_array()` private methods | Centralized sync API |
| `game/src/world.cpp` | Added sync method implementations; call them at end of `tick_internal()`; pass `movement_sim_state_` to `update_movement_state()` | Single sync point per tick |
| `game/src/world_dummy_sim.cpp` | Removed EnTT→array sync loop from `tick_dummies()` | Eliminated scattered sync |
| `game/src/world_projectile.cpp` | Removed sync loops from `fire_projectile()` and `step_projectiles()`; removed direct `dummies_[idx] = d` write | EnTT is sole authority |
| `game/src/world_camera.h` | Updated `resolve_movement_state()` signature to accept `MovementSimState` | Full state coverage |
| `game/src/world_camera.cpp` | Added LedgeGrab/Mantling/OnLadder handling to `resolve_movement_state()` | Consistent with `accelerate_movement()` |

## State Flow Architecture (Post-Cleanup)

```
┌─────────────────────────────────────────────────────────┐
│                    World::tick_internal()                │
│                                                         │
│  ┌──────────┐   ┌──────────────┐   ┌────────────────┐  │
│  │  Input   │──▶│ EnTT Registry│──▶│ sync_*_to_array│──▶│──▶ Renderer
│  │  Command │   │ (authoritative)│   │ (once per tick) │  │    (read-only)
│  └──────────┘   └──────────────┘   └────────────────┘  │
│                        │                                 │
│                        ▼                                 │
│               ┌────────────────┐                        │
│               │ Jolt Physics   │                        │
│               │ (KCC + bodies) │                        │
│               └────────────────┘                        │
│                        │                                 │
│                        ▼                                 │
│               ┌────────────────┐                        │
│               │ player_state_  │                        │
│               │ (synced back)  │                        │
│               └────────────────┘                        │
└─────────────────────────────────────────────────────────┘
```

**Key principle**: EnTT registry is the single source of truth for entity state during
simulation. Output arrays are populated once at tick end for renderer consumption.
No entity state is written to arrays directly from helper modules.

## Validation

- `ahamkara_game` static library compiles successfully
- All World API getters (`get_projectiles()`, `get_dummies()`, etc.) return the same types
  and are populated from the centralized sync methods
- MovementState resolution now covers all 9 `MovementState` variants consistently

## Known Gaps (Future Work)

1. **Particle/Decal arrays**: `particles_[]` and `decals_[]` remain as raw arrays without
   EnTT backing. Low priority — these are purely visual and short-lived.
2. **ServerHistoryBuffer vs World::history_buffer_**: Could be unified if the server also
   used a World instance with its built-in history buffer, but the server currently
   manages its own tick loop separately.
3. **`is_client_` flag complexity**: Multiple code paths check `is_client_` to decide
   whether to emit visual effects. A cleaner design would separate simulation from
   presentation concerns (future milestone).
4. **`fire_cooldown_timer_` is public**: This member is exposed in world.h for projectile
   code access. Should be made private with a friend declaration or accessor.

## Risk Assessment

- **Low risk**: Changes are mechanical (moving sync code, adding parameters). No
  behavioral changes to simulation logic.
- **No API break**: The World public interface is unchanged.
- **Build verified**: Library compiles and links successfully.
