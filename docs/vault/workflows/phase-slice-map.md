# Phase Slice Map

Status: Living reference

This document breaks the roadmap into phase-owned slices so the supervisor and
worker can fan out work without guessing ownership.

Rules:

- One slice should map to one worker worktree.
- Tasks inside a slice should share a dependency boundary.
- A slice may spawn child subagents only for isolated subtasks.
- Existing queue task IDs are listed where they already exist.
- Future work is written as concrete slice work items and should become one
  queue note each when scheduled.
- Deferred items stay out of active work until the user reactivates them.

The active slice notes are materialized in `docs/vault/queue-tasks/open/`. HDR
stays deferred and is intentionally not queued right now.

## Phase 0 - Foundation Hardening

### Slice 0A - Runtime core

Tasks:

- `TASK-20260622-1000-fixed-timestep-sim-adoption`
- log-level/env wiring for runtime core
- render/present semantics cleanup
- input routing ownership cleanup
- pause/menu state ownership split

### Slice 0B - ECS migration seed

Tasks:

- `TASK-20260622-1010-ecs-migration-first-slice`
- player and dummy component extraction
- authoritative state separation from cosmetic state

### Slice 0C - UI/runtime boundary cleanup

Tasks:

- UI screen-state split plan
- debug HUD/menu ownership separation
- follow-up queue cleanup for any remaining runtime/UI coupling

## Phase 1 - World Rendering and Authoring

### Slice 1A - Level mesh rendering

Tasks:

- `TASK-20260620-1200-level-driven-world-meshes`
- level import/render path validation
- world mesh generalization beyond the hardcoded arena

### Slice 1B - Textured materials

Tasks:

- `TASK-20260620-1330-pbr-uv-plumbing`
- `TASK-20260628-0104-textured-material-authoring-slice`
- albedo/UV/compiled texture path wiring

### Slice 1C - Sky and fog legibility

Tasks:

- `TASK-20260628-0105-level-sky-fog-wiring-slice`
- level-driven sky color
- ambient color wiring
- fog fallback behavior

### Slice 1D - Authoring stack

Tasks:

- `TASK-20260620-1415-blender-headless-level-generator`
- `TASK-20260620-1400-level-spec-and-lvl-emitter`
- spec -> `.lvl` -> compiled `.aelevel` path

### Slice 1E - Runtime confirmation

Tasks:

- display-backed confirmation of textured level rendering
- display-backed confirmation of sky/fog behavior
- display-backed confirmation of import parity

## Phase 2 - Player, Movement, and Camera Feel

### Slice 2A - Locomotion controller

Tasks:

- `TASK-20260628-0106-player-movement-camera-controller`
- deterministic movement core
- stance and jump response
- camera motion tied to locomotion

### Slice 2B - World orchestration boundary

Tasks:

- `TASK-20260628-0108-world-orchestration-boundary`
- keep `World` thin
- move player-specific ownership out of `World`

### Slice 2C - Weapon presentation

Tasks:

- `TASK-20260628-0107-weapon-presentation-separation`
- weapon runtime vs presentation split
- viewmodel ownership

### Slice 2D - First-person camera and viewmodel rig

Tasks:

- `TASK-20260628-0109-first-person-camera-viewmodel-rig`
- first-person camera alignment
- weapon/viewmodel rig wiring

### Slice 2E - Collision response polish

Tasks:

- `TASK-20260628-0110-collision-response-polish`
- step response
- slope response
- ledge response

### Slice 2F - Input and bindings follow-up

Tasks:

- remaining binding cleanup after the controller split
- controller vs keyboard/mouse routing validation

## Phase 3 - Sandbox and Combat Core

### Slice 3A - Weapon runtime foundation

Tasks:

- `TASK-20260704-1000-weapon-runtime-foundation`
- base weapon runtime seam
- ammo/reload/cooldown ownership
- cache-friendly weapon data ownership

### Slice 3B - Fire control

Tasks:

- `TASK-20260704-1010-weapon-fire-control`
- fire modes
- ammo/reserve handling
- deterministic recoil and spread

### Slice 3C - Hit resolution

Tasks:

- `TASK-20260704-1020-combat-hit-resolution`
- hitscan and projectile damage split
- damage falloff
- surface hit behavior

### Slice 3D - Abilities core

Tasks:

- `TASK-20260704-1030-combat-abilities-core`
- melee/grenade/class ability cooldowns
- energy plumbing
- combat state hooks

## Phase 4 - Server-Authoritative Simulation

### Slice 4A - Server tick ownership

Tasks:

- server-authoritative tick
- client input packaging
- sim/input clock alignment
- authoritative simulation boundary ownership
- headless/server tick bootstrap

### Slice 4B - Prediction and reconciliation

Tasks:

- client-side prediction
- state reconciliation
- buffered input replay
- rollback-safe state capture

### Slice 4C - Snapshot interpolation and lag compensation

Tasks:

- remote interpolation
- state delta compression
- server rewind for hit validation
- remote entity smoothing

### Slice 4D - Connection lifecycle and reliability

Tasks:

- connection setup/teardown
- ack/resend handling
- jitter buffering
- timeout and recovery handling

### Slice 4E - Network validation

Tasks:

- latency/loss simulation
- regression tests for predicted play
- validation of server-authoritative hits
- sync-state smoke checks

## Phase 5 - Animation and Sensory Polish

### Slice 5A - Character animation runtime

Tasks:

- drive rendered characters from the animation graph/state machine
- blend layers and additive poses
- IK and aim/recoil integration
- animation-to-render bridge

### Slice 5B - Weapon animation layers

Tasks:

- first-person weapon animation
- sway/bob/recoil kick
- reload and melee motion layers
- attachable component motion hooks

### Slice 5C - Audio subsystem

Tasks:

- turn the audio event queue into a real 3D subsystem
- listener/routing/occlusion
- weapon/foley/ambience buses
- audio ownership cleanup

### Slice 5D - VFX and feedback

Tasks:

- muzzle and impact effects
- decals
- screen shake and hit feedback
- damage-response signal path

## Phase 6 - Rendering Fidelity

### Slice 6A - HDR and render targets

Tasks:

- HDR/offscreen render target foundation
- tonemap/gamma path
- keep this deferred until the user reactivates it
- preserve future post-processing hooks

### Slice 6B - Shadows and multi-light

Tasks:

- cascaded shadow maps
- mesh shadow casters
- multiple light types

### Slice 6C - Ambient and reflections

Tasks:

- IBL and reflection probes
- ambient irradiance
- emissive and bloom

### Slice 6D - AO and anti-aliasing

Tasks:

- SSAO/GTAO
- MSAA to TAA path
- motion vectors
- color grading

### Slice 6E - Atmosphere and fog

Tasks:

- skybox/atmosphere
- time-of-day
- height and aerial fog

### Slice 6F - Legacy GL retirement

Tasks:

- remove remaining fixed-function compatibility seams
- keep the renderer on the core-profile path

## Phase 7 - World Scale, Streaming, and Destinations

### Slice 7A - Spatial partitioning

Tasks:

- world-scale partitioning
- occlusion culling
- portal/PVS support for interiors

### Slice 7B - Streaming and residency

Tasks:

- async asset streaming
- load/unload by area
- residency tracking

### Slice 7C - LOD and batching

Tasks:

- LOD chains
- impostors
- instanced/GPU-driven batching

### Slice 7D - Destination metadata

Tasks:

- regions
- landing zones
- ambient population metadata

## Phase 8 - Gameplay Systems and Content

### Slice 8A - Inventory and loadout

Tasks:

- inventory/loadout
- gear/perks/mods
- progression and currencies

### Slice 8B - AI combatants

Tasks:

- perception
- navmesh/pathfinding
- behavior and archetypes

### Slice 8C - Encounter scripting

Tasks:

- objectives and triggers
- spawn scripting
- encounter flows

### Slice 8D - Persistence and rewards

Tasks:

- pickups and rewards
- save/persistence
- character/world state persistence

## Phase 9 - Online Services and Activities

### Slice 9A - Matchmaking and parties

Tasks:

- party/fireteam handling
- session orchestration
- matchmaking integration

### Slice 9B - Activity framework

Tasks:

- PvE/PvP/social activity routing
- session type rules
- activity lifecycle

### Slice 9C - Identity and social

Tasks:

- accounts
- roster/invites
- basic anti-cheat and validation telemetry

### Slice 9D - Live content hooks

Tasks:

- rotating modifiers as data
- service-driven content toggles

## Phase 10 - Tooling, Performance, and Ship Readiness

### Slice 10A - Job system and frame allocator

Tasks:

- work-stealing job system
- frame allocator
- thread sim/render/jobs integration

### Slice 10B - Profiling and budgets

Tasks:

- CPU/GPU profiling
- memory budgeting
- frame pacing and perf gates

### Slice 10C - Authoring and inspector tooling

Tasks:

- stronger authoring pipeline
- live reload
- console/cvars
- thin in-engine inspector if needed

### Slice 10D - Telemetry and crash reporting

Tasks:

- runtime telemetry
- crash reporting
- diagnostic capture

### Slice 10E - CI and packaging

Tasks:

- runtime-boundary tests
- platform packaging
- CI build/test matrix
