# World Ownership Boundary

Documenting the ownership split for `ahamkara::game::World`.  This is a
canonical reference for what belongs where, updated with every refactor pass.

## What World Owns (match orchestration)

World is the simulation coordinator.  It owns:

| Concern | Examples | Why here |
|---------|----------|----------|
| Physics | Jolt physics system, colliders, movement sim state | Shared across all entities, lifecycle tied to world |
| ECS | `entt::registry` | Authoritative data store |
| Simulation timing | tick counter, fixed-step accumulator, history buffer | Orchestration concern |
| Match lifecycle | match time, phase, score, kills, deaths, match-over flag | Rules engine |
| NPC state | target dummies, projectile state | World-entity management |
| Respawn | respawn orchestration (timing, spawn points) | Match rule |
| Network | replication/forwarding, snapshot building | Session concern |

## What Player owns (player-local state)

`ahamkara::game::Player` owns everything single-player-identity:

| Owned by Player | Currently in World? |
|-----------------|---------------------|
| `ReplicatedPlayerState` (position, velocity, health) | **Moved** |
| `Loadout` (weapon slots) | **Moved** |
| `WeaponRuntime` (ammo, reload, cooldown) | **Moved** |
| `ArmorConfig` | **Moved** |
| `fire_recoil_index_` | Still in World — move to Player or WeaponRuntime |
| `reload_key_was_down_` | Still in World — input edge detection |
| `weapon_switch_queued_` / `queued_weapon_slot_` | Still in World — deferred switch state |
| `slide_timer_seconds_`, `crouch_active_` | Still in World — player-local movement |
| `respawn_timer_` | Still in World — player lifecycle |
| `damage_feedback_timer_` | Still in World — player-local feedback |

## What the Movement Controller owns

Future `PlayerMovementController` owns:

- Locomotion state (walk/sprint/crouch/slide/jump)
- Jump buffer, coyote time, mantle detection
- Ladder/ledge handling
- Camera anchor derivation (position/yaw/pitch from player state)

Currently most of this is in World.  Task TASK-20260628-0106 extracts it.

## What the Client Presentation Layer owns

The client layer owns all visual/audio presentation:

| Owned by client | Currently in World? |
|-----------------|---------------------|
| Damage numbers (floating text) | Still in World — move to client-side `WorldPresentation` |
| Particles (muzzle flash, impacts) | Still in World |
| Decals (bullet holes) | Still in World |
| Hitmarker state (duration, critical flag) | Still in World |
| Muzzle flash timer | Still in World |
| Audio player reference | Still in World |
| `is_client_` flag | Still in World — rename to `is_prediction_` to clarify its role |

## What the Weapon Presentation Layer owns

`ahamkara::client::WeaponViewmodelPresentation` owns:

- Viewmodel mesh resolution (`weapon_viewmodel_mesh_path()`)
- Weapon model cache (`WeaponModelCache`)
- Animation clip playback (`AnimationClipPlayer` via cache)
- Joint matrices for GPU skinning
- Per-weapon orientation transforms (`WeaponViewmodelTransform`)

This was extracted from `weapon_registry.h` in TASK-20260628-0107.

## Anti-Patterns — What Must NOT Go Into World

| Don't put in World | Put it here instead |
|--------------------|---------------------|
| Player-local timers (reload, recoil, slide) | `Player` or `WeaponRuntime` |
| Input edge detection | `Player` |
| Camera math | `PlayerMovementController` |
| Visual effects (particles, decals, hitmarkers) | Client-side `WorldPresentation` |
| Audio device references | Client-side via audio engine |
| Per-weapon mesh paths or transforms | `WeaponViewmodelPresentation` |
| HUD state or menu state | `ClientFramePipeline` |

## Migration Status

| Phase | What moved | Status |
|-------|-----------|--------|
| Phase 4 | `player_` aggregate | Complete |
| TASK-20260628-0107 | Viewmodel transforms, mesh paths | Complete |
| TASK-20260628-0106 | Movement/camera controller | Planned |
| Future | Presentation arrays (particles, decals, etc.) | Planned |
| Future | Remaining player-local fields | Planned |

See `docs/reports/subagents/` for per-task reports with exact file changes.
