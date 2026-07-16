# World Ownership Boundary

Status: Current Flashback gameplay boundary inside the transitional monorepo

This document describes the current ownership split around
`ahamkara::game::World`. It is a Flashback game design document, not a generic
Ahamkara engine contract. When the repositories split, this file moves with
Flashback. See [the repository split](../architecture/repository-split.md).

## What World Owns (match orchestration)

World is the simulation coordinator.  It owns:

| Concern | Examples | Why here |
|---------|----------|----------|
| Physics | Jolt game bridge and colliders | Shared across simulated entities; lifecycle tied to the world |
| ECS | `entt::registry` | Authoritative data store |
| Simulation timing | tick counter, fixed-step accumulator, history buffer | Orchestration concern |
| Match lifecycle | match time, phase, score, kills, deaths, match-over flag | Rules engine |
| NPC state | target dummies, projectile state | World-entity management |
| Respawn | respawn orchestration (timing, spawn points) | Match rule |

`World::advance_sim()` advances the authoritative simulation while
`World::apply_input()` consumes player input; `tick()` is the convenience path
used by local and prediction callers. [src: file:
game/include/ahamkara/game/world.h:59-78]

The EnTT registry is authoritative for projectiles; the vector exposed to
presentation is a per-tick projection. [src: file:
game/include/ahamkara/game/world.h:84-90] [src: file:
game/include/ahamkara/game/world.h:209-213]

## What Player owns (player-local state)

`ahamkara::game::Player` owns the replicated player snapshot, loadout, armor,
weapon runtime, and ability runtime. `World` delegates weapon and ability
operations to it. [src: file: game/include/ahamkara/game/player.h:66-76]
[src: file: game/include/ahamkara/game/player.h:84-136]

Some player-local state still remains in `World`: recoil index, reload edge
detection, deferred weapon switching, and respawn timing. This is a boundary
fact, not a task list. [src: file: game/include/ahamkara/game/world.h:221-240]
[src: file: game/include/ahamkara/game/world.h:272-277]

## What the Movement Controller owns

`PlayerMovementController` owns:

- Locomotion state (walk/sprint/crouch/slide/jump)
- Jump buffer, coyote time, mantle detection
- Mantle, ladder, and ledge handling
- Camera anchor derivation (position/yaw/pitch from player state)

It currently stores its own movement simulation/debug state, desired velocity,
slide timer, crouch state, and camera anchor. [src: file:
game/include/ahamkara/game/player_movement_controller.h:17-80]

## What the Client Presentation Layer owns

The client layer owns all visual/audio presentation:

| Presentation concern still stored in `World` | Current representation |
|-----------------|---------------------|
| Damage numbers | Fixed array and count |
| Particles | Fixed array and count |
| Decals | Fixed array and count |
| Hitmarker and muzzle flash | Timers and critical flag |
| Audio playback | `IAudioPlayer*` plus a per-tick event queue |
| Execution role | `is_client_` and `is_server_` booleans |

These members demonstrate that presentation and execution-policy state has not
yet been fully separated from simulation. [src: file:
game/include/ahamkara/game/world.h:237-262]

## What the Weapon Presentation Layer owns

`ahamkara::client::WeaponViewmodelPresentation` owns:

- Viewmodel mesh resolution (`weapon_viewmodel_mesh_path()`)
- Weapon model cache (`WeaponModelCache`)
- Animation clip playback (`AnimationClipPlayer` via cache)
- Joint matrices for GPU skinning
- Per-weapon orientation transforms (`WeaponViewmodelTransform`)

The client-owned type holds the render cache and exposes animation playback and
GPU skinning matrices. [src: file:
client/include/ahamkara/client/weapon_presentation.h:9-34]

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

Do not add migration status or implementation checklists here. Mutable repair
work belongs in [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues);
this document changes only when the implemented ownership boundary changes.
