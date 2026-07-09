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
stays deferred and is intentionally not queued right now. The Phase 11+ blocks
below are the new expansion track for agent fan-out; the earlier baseline
phases follow after them.

## Phase A - Autonomous Validation and Playtest Harness

### Slice A0 - Action and input abstraction

Tasks:

- automated input injection for local and headless clients
- action vocabulary for move, look, interact, combat, and recovery
- input playback and repeatable seed/state setup

### Slice A1 - Scenario runner

Tasks:

- scriptable playtest routes
- movement, pickup, and interaction smoke scenarios
- respawn and recovery loops

### Slice A2 - Gameplay affordances

Tasks:

- explicit interaction hooks for objects and triggers
- encounter and objective affordances for automation
- combat surfaces that can be exercised without manual driving

### Slice A3 - Validation and reporting

Tasks:

- deterministic replay/state capture
- failure artifacts and logs
- pass/fail output wired into the queue/report workflow

## Phase 11 - Autonomous Validation Mesh

### Slice 11A - Scenario runner expansion

Tasks:

- scripted movement, interaction, combat, and recovery routes
- repeatable state setup for local and headless sessions
- support for multiple validation scenarios per run

### Slice 11B - Action vocabulary

Tasks:

- input verbs for move, look, interact, fire, reload, use ability, pause
- camera and recovery actions for agent control
- controller and keyboard parity for the harness

### Slice 11C - Evidence capture

Tasks:

- deterministic replay capture
- logs, snapshots, and failure artifact bundles
- machine-readable pass/fail summary output

### Slice 11D - Queue/report wiring

Tasks:

- validation output linked into queue task reports
- report templates for automated playtest evidence
- failure metadata for later review and reruns

## Phase 12 - Traversal and Presentation Expansion

### Slice 12A - Locomotion verbs

Tasks:

- sprint, slide, mantle, dodge, crouch, and ledge behavior slices
- traversal edge cases exposed to the harness
- movement tuning slices with deterministic validation

### Slice 12B - Camera and viewmodel

Tasks:

- first-person camera transition slices
- FOV, bob, landing, and impulse presentation slices
- weapon/viewmodel state slices

### Slice 12C - Accessibility and remap

Tasks:

- input remapping slices
- controller parity slices
- accessibility fallback presentation slices

## Phase 13 - Combat Sandbox Scale-Out

### Slice 13A - Weapon families

Tasks:

- weapon archetype slices
- ammo/reserve and reload family slices
- fire-mode and recoil family slices

### Slice 13B - Damage model

Tasks:

- crit, falloff, shield, and surface interaction slices
- hitscan/projectile damage split slices
- damage feedback validation slices

### Slice 13C - Combat feedback

Tasks:

- hitmarker and combat UI slices
- audio/VFX feedback slices
- combat clarity and recovery slices

## Phase 14 - Encounter, AI, and Objective Systems

### Slice 14A - AI behavior

Tasks:

- senses, behavior, and squad coordination slices
- enemy archetype slices
- boss or elite behavior slices

### Slice 14B - Objectives and triggers

Tasks:

- objective condition slices
- trigger and checkpoint slices
- encounter validation slices

### Slice 14C - Encounter authoring

Tasks:

- encounter definition slices
- reward resolution slices
- scripted flow validation slices

## Phase 15 - Activities, Missions, and Progression

### Slice 15A - Activity flow

Tasks:

- activity state machine slices
- mission branching and modifier slices
- objective-to-reward state slices

### Slice 15B - Inventory and loadout

Tasks:

- inventory, gear, perk, and currency slices
- loadout save/load slices
- reward/persistence boundary slices

## Phase 16 - World Scale and Destination Content

### Slice 16A - Streaming and residency

Tasks:

- sector streaming slices
- residency and unload slices
- destination metadata slices

### Slice 16B - Patrol/world layout

Tasks:

- patrol space slices
- social space slices
- transition and encounter slice boundaries

## Phase 17 - Social, Live Ops, and Services

### Slice 17A - Social graph

Tasks:

- party, invite, roster, and presence slices
- session join/reconnect slices

### Slice 17B - Live ops

Tasks:

- live modifier slices
- seasonal content slices
- degraded-mode validation slices

## Phase 18 - Tools, Authoring, and Content Factory

### Slice 18A - Import/export validation

Tasks:

- asset linting slices
- schema and metadata slices
- import/export roundtrip slices

### Slice 18B - Batch generation

Tasks:

- batch transform slices
- content factory throughput slices
- automated report generation slices

## Phase 19 - Performance, Stability, and Ship Hardening

### Slice 19A - Perf budgets

Tasks:

- frame-time budget slices
- profiling and regression slices
- batch/parallel execution slices

### Slice 19B - Stability and diagnostics

Tasks:

- crash reporting slices
- deterministic replay slices
- memory and leak detection slices

## Phase 20 - Platform, Accessibility, and Release Variants

### Slice 20A - Platform variants

Tasks:

- packaging and build-flavor slices
- platform-constraint slices
- save/profile portability slices

### Slice 20B - Accessibility and localization

Tasks:

- localization slices
- text scaling and subtitle slices
- accessibility option slices

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

- `TASK-20260704-1740-ci-packaging`
- runtime-boundary tests (`ahamkara_runtime_boundary_tests`)
- platform packaging (CPack + install rules)
- CI build/test matrix (GitHub Actions)
- build matrix validation script (`scripts/validate-matrix.sh`)
