# Ahamkara Roadmap

This is the single consolidated roadmap for Ahamkara. It merges what were
previously separate files in `docs/roadmap/` (the FPS/Tiger-class roadmap, the
engine-foundations roadmap, the 7/10 roadmap, and the streamlining plan) into
one document.

How the parts relate:

- **Part I — FPS Playable Roadmap** is the long-horizon north star (Destiny /
  Tiger-class) and the primary planning frame.
- **Part II — Engine Foundations Roadmap** is the engine-only runtime/render
  foundation work that Part I depends on.
- **Part III — 7/10 Roadmap** is a concrete near/mid-term milestone: make the
  engine look, sound, and play like an indie FPS rather than a debug tool.
- **Part IV — Streamlining & Hardening Plan** is the maintainability/efficiency
  track that runs alongside everything.

Where parts overlap (e.g., the renderer, animation, ECS, and audio appear in
several), treat **Part I** as the strategic sequencing and the others as more
detailed, file-level plans. Status of fidelity work (HDR/post) is **deferred**
per `../vault/memory/decision-log.md` and is intentionally kept out of the
active queue until a concrete trigger fires.

## Contents

- [Phase A — Autonomous Validation & Playtest Harness](#phase-a--autonomous-validation--playtest-harness)
- [Part I — FPS Playable Roadmap (Tiger-Class North Star)](#part-i--fps-playable-roadmap-tiger-class-north-star)
- [Part II — Engine Foundations Roadmap](#part-ii--engine-foundations-roadmap)
- [Part III — 7/10 Roadmap (Indie-FPS Feel Milestone)](#part-iii--710-roadmap-indie-fps-feel-milestone)
- [Part IV — Streamlining & Hardening Plan](#part-iv--streamlining--hardening-plan)
- [Part V — Autonomous Scale Roadmap (10,000+ Atomic Tasks)](#part-v--autonomous-scale-roadmap-10000-atomic-tasks)

---

## Phase A — Autonomous Validation & Playtest Harness

This is the prerequisite track for agent-driven testing. Build it before the
roadmap grows into a much larger phase/task tree, because later phases should
inherit a reliable way to validate gameplay without human hand-holding.

**Goal:** frontier agents can move, interact, combat-test, and recover through
real gameplay flows while producing deterministic evidence.

**Workstreams**
- Action vocabulary and input injection for automated clients.
- Scenario runner for scripted movement, interaction, combat, and respawn.
- Gameplay affordances for objects, triggers, pickups, and encounter scripting.
- Deterministic replay, snapshots, and failure artifact capture.
- Validation outputs that fit the queue/report workflow.

**Exit criteria:** an automated agent can complete a representative playtest
route and emit machine-readable evidence that a later agent can review.

**Status:** partial

Core harness landing is in place. The remaining work here is broader scenario
coverage, richer failure artifacts, and server-session validation so later
phases can assume autonomous playtest execution from the start.

---

## Part I — FPS Playable Roadmap (Tiger-Class North Star)

The long-horizon plan for turning Ahamkara into a playable FPS engine whose
architecture and feel target **Bungie's Tiger engine** (Destiny / Destiny 2).
It lays out the full breadth of systems a Destiny-like FPS needs, sequenced so
the engine is *playable early* and *fidelity comes later*.

### How To Read This

- **North star ≠ next sprint.** Tiger-class is a multi-year, multi-discipline
  target. This is intentionally exhaustive so nothing important is forgotten; it
  is **not** a claim that all of it is near-term.
- Phases are ordered by dependency and by **function-before-fidelity**. Earlier
  phases unblock later ones; fidelity/polish phases are deliberately late.
- Each phase lists **Goal · Workstreams · Exit criteria · Status**.
- Status tags: `done`, `partial`, `in-review`, `queued`, `planned`, `deferred`.
- The **Playable FPS Gate** (after Phase 4) defines the minimum bar for "this is
  actually a game you can play." Everything past it is depth toward Tiger-class.

### 1. Current State Snapshot (grounding)

What exists in the repo today (source is the truth; this is orientation):

- **Runtime core:** fixed-timestep accumulator + deterministic RNG, hot-reload
  config, categorized logging. Fixed-timestep adoption into the sim loop is the
  current foundation milestone (`partial`).
- **Rendering:** OpenGL `RenderBackend` abstraction; a forward **PBR renderer**
  (single directional light) + **shadow map**; frustum culling + LOD; glTF
  loader + skeletal animation; **level-driven world meshes** through PBR
  (`in-review`); **UV/texture path** plumbed (`in-review`); `MapGeometry`
  hardcoded arena still present; legacy fixed-function GL still in the debug
  renderer; HDR/render-targets **deferred**.
- **Simulation/world:** struct-array world state with EnTT present but largely
  unused (ECS migration seeded, not adopted).
- **Physics/collision:** Jolt-backed collision world, trace/queries, layers.
- **Animation:** animation graph, state machine, IK, aim/recoil, character/weapon
  rig, render bridge (foundation present; deep runtime integration pending).
- **Audio:** event-queue audio engine + listener; not yet a full 3D subsystem.
- **Netcode:** UDP client/server skeleton, ~60 Hz input, server snapshots,
  snapshot interpolator, sequence tracking, network clock/simulator. Prediction/
  reconciliation/lag-comp not yet real.
- **Content pipeline:** glTF→`.aemesh`, TGA→`.aetex`, `.mat`→`.aemat`,
  `.lvl`→`.aelevel`, asset registry + pack. **Authoring stack** (Blender +
  TrenchBroom + JSON spec; Path A emitter + Path B Blender generator) landed
  (`in-review`).
- **UI:** Dear ImGui; debug HUD/metrics/menus.
- **Services:** Wish engine + Nakama bridge (matchmaking/backend seam) present.

### 2. North Star: What "Tiger-Class" Implies

Destiny's engine is defined less by raw graphics and more by the **combination**
of: a deterministic, **server-authoritative** simulation with responsive client
prediction; **seamlessly streamed destinations** (large patrol spaces stitched
from sectors); a deep, data-driven **sandbox** of weapons, abilities, and
modifiers; **activities** (strikes/raids/crucible) layered on a shared world; a
**hero-quality sensory layer** (animation, audio, VFX, gunfeel); and an
**industrial content + tooling pipeline**. Targeting it means treating netcode,
streaming, and the sandbox/data model as first-class — not afterthoughts.

### 3. Guiding Principles

1. **Function before fidelity.** Ship the simplest playable thing; defer polish.
2. **Data-driven & tool-authored.** Gameplay, levels, sandbox, encounters are
   data compiled by tools, not hardcoded in engine/gameplay code.
3. **Deterministic simulation.** Fixed timestep, deterministic RNG, ordered sim
   containers — the bedrock for prediction/reconciliation and replays.
4. **Server-authoritative by default.** The server owns truth; clients predict.
5. **One pipeline per concern.** Retire legacy/split paths (fixed-function GL,
   the hardcoded arena, duplicate state pipelines).
6. **Lean / YAGNI.** Don't build abstractions or fidelity until a concrete need
   exists (HDR is deferred until lighting tuning/post-FX demand it).
7. **Validate honestly.** Separate implemented / build-validated / test-validated
   / runtime-confirmed; state gaps, don't hide them.

### 4. Phased Roadmap

#### Phase 0 — Foundation Hardening · `partial`

**Goal:** One coherent, deterministic runtime spine the rest can stand on.

**Workstreams**
- Adopt the fixed-timestep accumulator as the *only* sim path; feed
  interpolation alpha to the renderer (separate sim vs render ticks).
- Core client-runtime cleanups: render/present semantics, input routing
  ownership, UI screen-state split (queued cleanup tasks).
- Begin ECS adoption: core components (Transform, Health, Movement); migrate
  world arrays incrementally to EnTT.
- Retire incidental legacy paths where cheap; keep behavior stable.

**Exit criteria:** sim on a fixed timestep with interpolated rendering; cleanup
tasks accepted; ECS holds at least player + dummies.

#### Phase 1 — World Rendering & Authoring · `partial / in-review`

**Goal:** Author a map externally, import it, and see it render acceptably.

**Workstreams**
- Level-driven world meshes via PBR (`in-review`); textured materials via the UV
  path (`in-review`); textured-material showcase (`queued`).
- Level-driven **sky + ambient + distance fog** (`queued`) — legibility.
- Authoring stack: Path A spec→`.lvl` emitter (`in-review`) + Path B Blender
  generator (`in-review`); TrenchBroom greybox flow.
- Generalize beyond the hardcoded arena; begin a level-sized spatial structure
  (replace the fixed 4×4 grid).
- Basic lighting that reads well: directional sun + shadow map, sane ambient.

**Exit criteria:** a spec/Blender-authored level imports and renders with
textured meshes, level-driven sky/fog, spawns, and collision — runtime-confirmed
on a display.

#### Phase 2 — Player, Movement & Camera Feel · `planned`

**Goal:** Moving through a map feels good in first person.

**Workstreams**
- Deterministic character controller on the fixed timestep (accel, air control,
  crouch/slide/mantle/jump), tuned Destiny-like.
- First-person camera + viewmodel rig; FOV, view bob, landing/impulse.
- Input mapping/bindings (KB/M + controller) with clean routing.
- Collision response polish (step/slope/ledge) on the Jolt collision world.

**Exit criteria:** smooth, responsive, tunable, deterministic first-person
traversal with a viewmodel.

**Recommended Execution Order**

1. `TASK-20260622-1020-deterministic-character-controller` - completed
   movement core; treat this as the fixed-step foundation, not a Phase 2
   destination.
2. `TASK-20260628-0106-player-movement-camera-controller` - extract the
   remaining locomotion/camera logic out of `World`.
3. `TASK-20260628-0108-world-orchestration-boundary` - keep `World` thin after
   the movement split lands.
4. `TASK-20260628-0107-weapon-presentation-separation` - keep weapon runtime
   lean before wiring richer first-person presentation.
5. `TASK-20260628-0109-first-person-camera-viewmodel-rig` - connect the active
   camera to a proper first-person weapon/viewmodel rig.
6. `TASK-20260628-0110-collision-response-polish` - finish step/slope/ledge
   response after the movement controller is factored out.
7. Input/binding cleanup is already covered by earlier runtime tasks; only queue
   a follow-up if a real gap remains after the controller refactor.

#### Phase 3 — Sandbox & Combat Core · `planned`

**Goal:** You can shoot, and it feels like a shooter.

**Workstreams**
- Data-driven **weapon system** (archetypes, fire modes, RPM, recoil, spread,
  reload, ammo/reserves).
- Hitscan + projectile paths; damage model, headshots/crits, falloff, surfaces.
- **Abilities** (grenade/melee/class ability) with cooldowns and energy.
- Damage feedback (hitmarkers, numbers, damage events on the event bus).
- Deterministic spread/recoil via the deterministic RNG (prediction-safe).

**Recommended Execution Order**

1. `TASK-20260704-1000-weapon-runtime-foundation` - establish the base runtime
   seam and cache-friendly ownership before adding more combat rules.
2. `TASK-20260704-1010-weapon-fire-control` - normalize fire modes, ammo,
   reload, reserves, and deterministic recoil/spread.
3. `TASK-20260704-1020-combat-hit-resolution` - split hitscan/projectile
   damage resolution from feedback and lock down the actual combat results.
4. `TASK-20260704-1030-combat-abilities-core` - add melee/grenade/class
   ability cooldown and energy plumbing after the combat base exists.

**Exit criteria:** a data-authored loadout of weapons + abilities with damage,
reload, and feedback, usable against targets.

#### Phase 4 — Netcode: Server-Authoritative Sim · `planned` (Tiger-defining)

**Goal:** The same combat works in real multiplayer with good feel under latency.

**Workstreams**
- Server-authoritative tick; clients send inputs.
- **Client-side prediction** + **reconciliation** (replay buffered inputs).
- **Snapshot interpolation** for remotes (build on the existing interpolator)
  with state delta compression.
- **Lag compensation / server rewind** for hit validation.
- Connection lifecycle, reliability (acks/resends), clock sync, jitter buffering;
  network condition simulation for testing.

**Exit criteria:** 2+ clients fight on a server with predicted local feel, smooth
remotes, and server-validated hits under simulated latency/loss.

#### ⛳ Playable FPS Gate (minimum bar)

After Phase 4 the engine should support: load a map → move in first person →
use a sandbox of weapons/abilities → fight other players on a server with
prediction and server-authoritative hits → with HUD feedback and audio. That is
**"a playable FPS."** Everything below deepens toward Tiger-class.

#### Phase 5 — Animation & Sensory Polish · `partial`

**Goal:** Characters, weapons, and the world feel alive and responsive.

**Workstreams**
- Drive rendered characters + viewmodels from the animation runtime (graph/state
  machine), with blending, additive layers, IK, aim/recoil.
- First-person weapon animations + procedural layers (sway/bob/recoil kick).
- **3D audio engine**: spatialization, listener/routing/occlusion, weapon/foley/
  ambience buses, voice — from event queue to real subsystem.
- VFX/particles, decals, muzzle/impact effects, screen feedback (hitmarkers,
  shake).

**Exit criteria:** combat reads with weight; safe for headless/server builds.

#### Phase 6 — Rendering Fidelity · `planned` (HDR `deferred`, not queued)

**Goal:** Make spaces look good; enable a modern lighting/post pipeline.

**Workstreams**
- **HDR + offscreen render targets** + tonemap/gamma (`deferred` until a
  trigger; see decision log). Keep this in the roadmap only, not the active
  queue, until a concrete fidelity trigger lands → unblocks post.
- Cascaded shadow maps; mesh shadow casters.
- Multiple light types + clustered/tiled culling.
- IBL / reflection probes; ambient via irradiance; emissive + bloom.
- SSAO/GTAO; AA (MSAA→TAA + motion vectors); color grading.
- Real skybox/atmosphere + time-of-day; height/aerial fog.
- (Static) lightmap baking + light probes.
- Retire remaining legacy fixed-function GL into the modern pipeline.

**Exit criteria:** linear HDR pipeline with CSM, multi-light, ambient/IBL, AO,
AA, sky/atmos — coherent, tunable maps.

#### Phase 7 — World Scale, Streaming & Destinations · `planned`

**Goal:** Maps grow into Destiny-style streamed destinations and patrol spaces.

**Workstreams**
- Spatial partition at level scale (BVH/octree/grid); occlusion culling (use the
  existing query infra), portal/PVS for interiors.
- **Sector/region streaming**: load/unload by area; async asset streaming;
  residency + (later) GPU-compressed textures/virtual texturing.
- LOD chains + impostors; instanced/GPU-driven static rendering, batching/sorting.
- Destination metadata layer (regions, landing zones, ambient population) —
  "planet = streamed map at scale."

**Exit criteria:** a large multi-sector space streams seamlessly at stable
framerate with culling, LOD, and a patrol-style layout.

#### Phase 8 — Gameplay Systems & Content · `planned`

**Goal:** The systems that make it a *game*, not a tech demo.

**Workstreams**
- Inventory/loadout, gear/perks/mods, progression, currencies.
- **AI combatants**: perception, navmesh + pathfinding, behavior (cover, squads),
  archetypes, bosses/encounter mechanics.
- Encounter/spawn scripting; objectives, triggers, public-event-style flows.
- Interactables, pickups, ammo bricks, chests/rewards.
- Save/persistence of character + world state.

**Exit criteria:** a short playable activity — spawn, fight AI through scripted
encounters with objectives and rewards.

#### Phase 9 — Online Services & Activities · `partial` (Wish/Nakama seam exists)

**Goal:** Group play, matchmaking, and shared-world plumbing.

**Workstreams**
- Matchmaking + party/fireteam, session orchestration, dedicated server fleet
  (build on Wish engine + Nakama bridge).
- Activity framework (PvE/PvP/social) over the netcode core.
- Accounts/identity, social (roster/invites), basic anti-cheat (server authority
  + validation + telemetry).
- Live-content hooks (rotating modifiers as data).

**Exit criteria:** players matchmake into a server-hosted activity as a fireteam
and complete it.

#### Phase 10 — Tooling, Performance & Ship-Readiness · `planned`

**Goal:** Make the engine fast, observable, and productive to build content in.

**Workstreams**
- **Job system** (work-stealing) + **frame allocator**; thread sim/render/jobs;
  parallel culling, animation, particles, packet (de)serialization.
- Profiling (CPU/GPU per pass), memory budgeting, frame pacing; perf gates in CI.
- Stronger authoring (editor ergonomics or a deeper DCC pipeline + thin in-engine
  inspector), live reload, console/cvars.
- Telemetry/crash reporting, runtime-boundary tests, platform packaging, CI
  build+test matrix.

**Exit criteria:** multi-threaded engine within a frame budget on target
hardware, with content tooling and CI sufficient for sustained production.

### 5. Cross-Cutting Tracks (continuous)

- **Determinism:** fixed timestep, deterministic RNG, ordered sim containers,
  replay/record support.
- **Performance & threading:** job system + frame allocator land mid-roadmap but
  influence API design from the start (data-oriented, batch-friendly).
- **Data model / ECS:** progressively move authoritative state to ECS; keep
  cosmetic/render-only state separate from gameplay state.
- **Testing & validation:** focused tests at utility/runtime boundaries;
  net-condition simulation; honest runtime-confirmation discipline; autonomous
  playtest harnesses for movement, interaction, combat, and recovery flows.
- **Docs & memory:** keep `docs/systems/` truthful; record decisions; keep the
  queue/reports loop tight.
- **Security/services hygiene:** server authority, input validation, no secrets
  in content.

### 6. Near-Term Execution Order (function before fidelity)

1. Accept in-review slices (level meshes revised, UV plumbing, Path A/B).
2. Build the autonomous validation harness so later phases can be exercised
   without manual playthroughs.
3. Core cleanups: `render-present-semantics`, `input-routing-cleanup`,
   `ui-screen-split-plan`.
4. `runtime-confirm-prototype-levels` (needs a GL display).
5. `textured-material-showcase` → make textures visibly work.
6. `level-driven-sky-and-fog` → cheap legibility.
7. Begin Phase 2 (player/movement feel).
8. Define ownership boundaries for the gameplay runtime:
   - `World` remains the orchestrator for match state, simulation stepping,
     dummies/projectiles/collision, respawn, and replay/history snapshots.
   - `Player` owns player-specific runtime data such as loadout, armor, active
     weapon runtime, and player-local damage handling.
   - A movement/camera controller should own locomotion, stance, mantle, and
     first-person camera math instead of keeping that logic in `World`.
   - Weapon presentation should be separated from weapon runtime so
     attachments, viewmodels, and animation live in a presentation layer.
9. HDR/render-target foundation stays **deferred** until a trigger fires and
   remains out of the active queue until then.

These map to the queued tasks in `../vault/queue-tasks/`.

### 7. Honest Caveats

- A true Tiger-class engine is the work of a large team over years; this is a
  **direction and decomposition**, not a delivery schedule.
- Netcode (Phase 4), streaming (Phase 7), and the sandbox/data model are the
  hardest, most differentiating, and most underestimated areas — invest early in
  their foundations.
- Scope ruthlessly per phase; prefer a thin *playable* vertical slice over
  broad-but-inert systems.

---

## Part II — Engine Foundations Roadmap

Intentionally engine-only. It excludes Wish Engine, Nakama, match orchestration,
and backend concerns. The goal is to turn Ahamkara into a coherent engine/runtime
foundation that Flashback and future games can stand on.

### 1. Runtime Core

The engine needs one clear simulation model.

- Fixed-timestep simulation should be the default for local play, not just a note.
- Render and simulation state should be separated cleanly so interpolation is a
  first-class runtime behavior.
- The client loop should stop owning bespoke timing logic in multiple places.
- Job system, frame allocator, and future threading work should plug into the
  same runtime model instead of growing in parallel.

### 2. Rendering

The renderer has a strong base but still needs consolidation.

- Finish the modern retained/GPU-driven path and retire remaining legacy-style
  rendering paths where possible.
- Keep one stable entity/material/shadow pipeline instead of split debug-era and
  production-era paths.
- Continue reducing conflict around `debug_renderer.cpp` by pushing
  responsibilities into narrower files and clearer interfaces.

### 3. Simulation Data Model

The game/runtime boundary still needs cleanup.

- Continue moving world state away from fixed arrays and toward the ECS/system-
  driven model already seeded in the codebase.
- Keep authoritative gameplay data separate from cosmetic/render-only state.
- Reduce duplicate state pipelines across world, renderer snapshots, and
  networking snapshots.

### 4. Sensory Systems

Animation and audio foundations exist; the remaining work is deep runtime
integration.

- Drive rendered characters and viewmodels from the animation runtime.
- Turn audio from an event queue into a real 3D engine subsystem with listener
  state, routing, and playback ownership.
- Keep both safe for headless/server builds while feeling native in client builds.

### 5. Content Pipeline

The engine needs a stronger path from authored content to runtime assets.

- Harden mesh, material, texture, level, and audio import flows.
- Keep runtime asset formats stable and tool-driven.
- Reduce "debug-only" content assumptions baked into client/runtime code.

### 6. Tooling and Maintainability

- Remove layering violations such as low-level systems depending on high-level
  gameplay types.
- Keep docs synced with actual runtime behavior, especially around rendering and
  networking ownership.
- Expand focused tests around utility/runtime boundaries, not only gameplay
  outcomes.

### Recommended Execution Order

1. Runtime core and fixed-timestep adoption
2. Render-path stabilization
3. Simulation/ECS cleanup
4. Animation and audio runtime integration
5. Asset pipeline hardening
6. Tooling/editor ergonomics

---

## Part III — 7/10 Roadmap (Indie-FPS Feel Milestone)

A concrete milestone: **Current 4/10 → Target 7/10.** Six workstreams in
dependency order, each scoped to ~400–2000 lines, avoiding architectural
rewrites. The goal is not a production renderer — it's an engine that looks,
sounds, and plays like an indie FPS rather than a debug tool.

> Note: several Stream 1/2 items (PBR renderer, shadow map, render-backend
> textures, UV path, animation-render bridge) have since been implemented or are
> in review; treat this part as the file-level plan and check current status in
> `../vault/memory/current-state.md`.

### Stream 1: Modern GL Renderer (Blocks: Stream 2, Stream 6)

**Why first:** Every visual system depends on the renderer not being immediate-
mode GL 1.x. **Lines:** ~2000 · **Risk:** Medium · **Depends on:** Nothing

#### 1a. GPU Buffer Abstraction
- `engine/render/include/ae/render/gpu_buffer.h`
- `RenderBackend` already has `create_vertex_buffer()` / `create_index_buffer()` —
  audit and ensure they use VAOs internally
- Add `GpuMesh` struct: `BufferHandle vbo, BufferHandle ibo, uint32_t index_count, uint32_t vertex_count`
- Convert `MapGeometry` from raw `unsigned int vbo_*` to `BufferHandle`
- Convert `GpuModel` / `HumanoidMesh` to use `GpuMesh`

#### 1b. PBR Shader (GGX Microfacet)
- `engine/render/shaders/pbr.vert` and `pbr.frag` (GLSL 330)
- Supports: albedo texture, normal map, metallic-roughness, single directional light
- `ae::render::PbrMaterial` struct: `TextureHandle albedo, normal, orm, Vec3 emissive, float metallic, roughness`
- `RenderBackend` additions: `create_texture()`, `TextureHandle`, `bind_texture()`
- Replace `glColor3f()` calls with PBR material uniforms for entity rendering

#### 1c. Shadow Mapping
- Single 2048×2048 depth map from a directional sun light
- `engine/render/src/shadow_pass.cpp` — render depth from light's perspective
- Debug: visualize shadow frustum; integrate before the main color pass

#### 1d. Retained Mesh Drawing
- Replace `glBegin(GL_QUADS)` / `glVertex3f()` with VBO-backed draw calls
- Keep immediate-mode only for true debug lines (axes, grids)
- `draw_box()`, `draw_ground_grid()`, `draw_player_marker()` → VBO upload once, reuse

#### Files Modified
| File | Change |
|------|--------|
| `engine/render/include/ae/render/render_backend.h` | Add `create_texture()`, `TextureHandle`, `bind_texture()` |
| `engine/render/include/ae/render/gpu_buffer.h` | NEW — `GpuMesh`, `GpuMaterial` structs |
| `engine/render/src/render_backend_opengl.cpp` | Implement texture creation/upload/bind |
| `engine/render/src/pbr_renderer.cpp` | NEW — PBR draw pass with GGX shader |
| `engine/render/src/shadow_pass.cpp` | NEW — shadow map rendering |
| `engine/render/shaders/pbr.vert` / `pbr.frag` | NEW |
| `engine/render/shaders/shadow.vert` / `shadow.frag` | NEW |
| `engine/render/src/debug_renderer.cpp` | Replace immediate-mode entity drawing with PBR path |
| `engine/render/src/debug_renderer_gpu.cpp` | Convert to VBO-based retained meshes |
| `engine/render/src/map_geometry.cpp` | Move VBOs to `BufferHandle` |
| `engine/render/CMakeLists.txt` | Add new sources |

#### Success Criteria
- `glBegin`/`glEnd` only in debug lines/axes/grid/HUD
- Entity meshes rendered via `glDrawElements` with PBR shader
- Shadow from sun direction visible on ground
- 1000+ boxes under 8ms on integrated GPU

### Stream 2: Wire Animation to GPU

**Why second:** The animation framework is ~95% complete and produces joint
matrices that never reach the GPU — highest visual impact per line.
**Lines:** ~200 · **Risk:** Low · **Depends on:** Stream 1 (shaders)

#### 2a. Animation-Render Bridge
- `engine/animation/src/animation_render_bridge.cpp` — feeds `AnimationDriver`
  output to `RenderBackend`
- `DebugScene`: add `const float* joint_matrices` + `int joint_count`
- `DebugRenderer::render()`: if joint data present, `draw_gpu_mesh_skinned()`
- `debug_client.cpp`: create `AnimationDriver`, tick, populate `scene.joint_matrices`

#### 2b. Simple Locomotion Integration
- Idle/walk/sprint clips (procedural or glTF)
- Map `MovementState` → animation graph parameters
- Movement input → `AnimationDriver::tick()` → joint matrices → GPU

#### Success Criteria
- Player character visibly animates; dummies show hit reactions; zero server/
  headless change

### Stream 3: ECS-Driven Simulation (Blocks: Stream 6)

**Why third:** `World` uses fixed-size C arrays with manual indexing; ECS is the
right architecture for an entity-heavy FPS and unlocks clean replication.
**Lines:** ~500 · **Risk:** Medium · **Depends on:** Nothing

#### 3a. Migrate Core Entity Types
- `ProjectileState[]` / `TargetDummyState[]` → `entt::registry` components
- Remove `sync_*_to_array()`; consumers use registry views directly
- Add `PlayerComponent` (prep for multiplayer)

#### 3b. System Functions Over Methods
- `tick_dummies`, `step_projectiles`, `tick_particles`, `tick_decals`,
  `damage_system` as free functions over the registry

#### 3c. Clean Up World
- Remove fixed arrays; `World` exposes `registry()` + ECS snapshot API

#### Success Criteria
- `World` has zero fixed-size entity arrays; all sim iterates `registry.view<T>()`;
  world tests pass unmodified

### Stream 4: Input Action Map

**Why fourth:** Clean input handling is a force multiplier.
**Lines:** ~300 · **Risk:** Low · **Depends on:** Nothing

#### 4a. Input Action System
- `engine/input/include/ae/input/input_map.h` — `InputAction`, `InputBinding`, `InputMap`
- Actions: Move/Look axes, Jump, Crouch, Sprint, Slide, Fire, Reload, Ability,
  Weapon1-3, Menu, Scoreboard, PushToTalk
- `bind(action, key|button)`, `poll(action, window, gamepad) → float`,
  `load_from_file("client/config/input.cfg")`

#### 4b. Replace Raw Input Checks
- `WindowInputProvider` + `PlayerInputCommand` use `InputMap`; merge
  `ControllerBindings` into it

#### Success Criteria
- New keybinding = config edit only; controller + keyboard share action names;
  ImGui "Controls" tab reads/writes config

### Stream 5: Spatial Audio

**Why fifth:** Audio is flat; miniaudio 3D spatialization is a small change with
huge immersion payoff. **Lines:** ~400 · **Risk:** Low · **Depends on:** Nothing

#### 5a. Enable miniaudio Spatialization
- `AudioEngine::play_3d(sound_id, position, volume)` and
  `set_listener(position, forward, up)`

#### 5b. Audio Asset Pipeline
- `.wav` → runtime sound (or passthrough); `audio_bank.cpp` registry; preload
  weapon/footstep/impact/UI sounds

#### 5c. Wire Into Game
- `World::queue_audio_event()` → `play_3d()` with source position; camera feeds
  listener; surface material → footstep variation

#### Success Criteria
- Gunshots have direction + distance falloff; footsteps vary by surface; listener
  follows camera

### Stream 6: One Real Game Mode (the payoff)

**Why last:** Capstone. With rendering, animation, ECS, input, and audio in place,
a real mode makes everything visible. **Lines:** ~400 · **Risk:** Low ·
**Depends on:** Streams 1, 3

#### 6a. Deathmatch Mode
- `game/src/game_mode_deathmatch.cpp` — spawn, track kills, end at score limit
- `MatchState` phases: Lobby → Warmup → InProgress → MatchEnd
- Scoreboard (ImGui, Tab); kill feed (last 5)

#### 6b. Server Loop Integration
- Server reads `GameModeRules`; `MatchState::tick()` phases automatically; on kill
  update scores/win condition; announce + restart

#### 6c. Client-Side Feedback
- Scoreboard overlay, kill feed, round countdown, "You killed X"/"Killed by X"

#### Success Criteria
- Full round trip: spawn → move → shoot → kill → score → win → restart

### Execution Order & Dependencies

```
Stream 1 (Renderer) ──────────────────────┐
                                           ├──→ Stream 2 (Animation)
Stream 3 (ECS) ─────────┐                  │
                         ├──→ Stream 6 ────┘
Stream 4 (Input) ────────┤    (Deathmatch)
                         │
Stream 5 (Audio) ────────┘
```

Streams 1, 3, 4, 5 are independent. Stream 2 needs Stream 1; Stream 6 needs 1 + 3.
**Recommended order:** 1 → 2 → 3 → 6, with 4 and 5 in parallel.

### Budget Estimate

| Stream | Lines | Risk | Hours (solo) |
|---|---|---|---|
| 1. Renderer | ~2000 | Medium | 12-16 |
| 2. Animation | ~200 | Low | 2-3 |
| 3. ECS | ~500 | Medium | 6-8 |
| 4. Input | ~300 | Low | 3-4 |
| 5. Audio | ~400 | Low | 4-5 |
| 6. Deathmatch | ~400 | Low | 4-5 |
| **Total** | **~3800** | | **31-41 hours** |

### What Won't Change (intentionally)

- No deferred rendering, HDR, bloom, SSAO — keep forward rendering (HDR is
  deferred; see decision log)
- No IK foot placement, ragdoll, or procedural animation beyond skeleton anim
- No matchmaking/lobby — local/LAN only at this milestone
- No vehicle physics, destructible environments, or water sim
- No editor GUI — continue Blender/TrenchBroom via the asset pipeline

---

## Part IV — Streamlining & Hardening Plan

Reduces codebase friction, improves maintainability, and removes obvious
efficiency traps **without** widening the feature surface. Runs alongside the
broader work, biased toward making the repo easier to reason about, build, and
evolve.

### Current Friction Snapshot

- Very large implementation files: `engine/render/src/debug_renderer.cpp`,
  `game/src/world.cpp`, `tools/asset_importer.cpp`,
  `client/src/headless_clients.cpp`, `server/src/dedicated_server_main.cpp`
- Layering drift: `ae_network` depends on `ahamkara_game`; collision/gameplay
  overlap around Jolt ownership
- Build-system duplication and branch drift across targets; tests + module
  `CMakeLists.txt` are critical merge surfaces
- Utility duplication: CLI parsing duplicated in client/server; importer parsing
  growing in one executable
- Partial portability: `engine/core/src/config.cpp` still uses `<sys/stat.h>`
- Incomplete primitives: `JobSystem::submit_after()` stubbed; some profiler/timer
  fields exist but are not measured

### Principles

- Prefer decomposition over cleverness.
- Move duplicated logic into small shared helpers before adding more features.
- Fix ownership boundaries before optimizing across them.
- Keep runtime-facing code lean and tool/import code explicit.
- Preserve headless server safety: render/audio/editor code stays out of
  server-only paths.

### Phase 1: Build And Boundary Cleanup

**Why first:** If the target graph and module boundaries are unstable, every
later cleanup is harder to verify.

1. Normalize module dependencies — remove `ae_network -> ahamkara_game`; move
   `PacketEnvelope`/`SequenceTracker` to a layer that breaks the cycle.
2. Audit all `CMakeLists.txt` for duplicate link propagation, client-only modules
   leaking into headless targets, missing include boundaries.
3. Define a stable build matrix: `ahamkara_client`, `ahamkara_server`,
   `ahamkara_asset_importer`, key test suites.
4. Replace `stat()` polling in `engine/core/src/config.cpp` with
   `std::filesystem::last_write_time()`.

**Primary files:** `CMakeLists.txt`, `engine/network/CMakeLists.txt`,
`game/CMakeLists.txt`, `tests/CMakeLists.txt`, `engine/core/src/config.cpp`

### Phase 2: Large File Decomposition

**Why second:** The largest files carry too many responsibilities.

1. Split `engine/render/src/debug_renderer.cpp` by responsibility (frame
   lifecycle, GPU upload/draw helpers, overlay/HUD, profiler UI, map/scene).
2. Split `game/src/world.cpp` by system area (projectile/combat, movement,
   collision bridge, dummy/AI sim, camera/debug state).
3. Split `tools/asset_importer.cpp` into helpers (manifest, registry/cache, TGA,
   material, dispatch).
4. Move shared CLI/simulator-config helpers out of `headless_clients.cpp` and
   `dedicated_server_main.cpp`.

### Phase 3: Shared Utility Consolidation

**Why third:** After decomposition, common helpers extract cleanly.

1. Shared CLI parsing (`engine/core/include/ae/core/cli_utils.h`).
2. Shared text parsing (trim, `key=value`, token split).
3. Shared binary file IO helpers for compiled asset formats.
4. One platform GL include shim if GL remains in multiple files.

### Phase 4: Runtime Ownership Simplification

**Why fourth:** Reduce duplicate simulation ownership; make end-to-end reasoning
easier.

1. Decide the physics ownership model (migrate `World` toward `ae_collision`, or
   keep direct Jolt and document the bridge).
2. Reduce duplicate state pipelines (clarify authoritative/predicted/interpolated;
   centralize reconciliation).
3. Make dormant subsystems opt-in through narrow adapters (animation, audio,
   movement debug).
4. Avoid parallel "almost the same" systems.

### Phase 5: Efficiency Passes That Also Improve Clarity

1. Fix incomplete infrastructure (`JobSystem::submit_after()`; remove dead
   profiler fields).
2. Reduce unnecessary work in tools/runtime (incremental import; avoid full
   rewrites of unchanged outputs).
3. Replace wasteful structures (stop duplicating full map geometry into every
   spatial cell; tighten handle pools).
4. Make instrumentation trustworthy (measure `gpu_time_entities_ms` or remove it;
   use occlusion query results or remove the half-wired path).

### Phase 6: Test And Documentation Hardening

1. Turn branch-history knowledge into permanent docs (subsystem status,
   integrated vs dormant, boundary rules).
2. Add targeted regression tests for extracted helpers (CLI, config reload,
   importer cache, compiled-asset roundtrips).
3. Add one integration checklist doc for high-conflict files.
4. Reduce ambiguity in test expectations; avoid mixing "old preserved" and "new
   validated" in one test.

### Suggested Execution Order

1 → 2 → 3 → 4 → 5 → 6 (highest return, lowest chaos).

### Immediate Next Actions

1. Break `ae_network -> ahamkara_game`
2. Extract shared CLI parsing from client/server mains
3. Replace `stat()`-based config polling with `std::filesystem`
4. Split `tools/asset_importer.cpp` into helper translation units
5. Start carving `debug_renderer.cpp` into smaller files
6. Decide whether `World` migrates to `ae_collision` this milestone or next

### Success Criteria

- Clean ownership boundaries in the main build graph.
- The worst large files are no longer single-file subsystems.
- Common parsing/serialization helpers are shared, not duplicated.
- Runtime ownership between networking, world sim, and collision is explicit.
- Build/test/docs tell the same story without tribal memory.

---

## Part V — Autonomous Scale Roadmap (10,000+ Atomic Tasks)

This is the expansion layer for the roadmap. The point is not to create a
single impossible-to-review mega-list; it is to define phase families that can
be decomposed into thousands of small, agent-safe queue items with one owner,
one validation path, and one reviewable outcome each.

### Operating Rules

- Every queue task must be small enough for one worker to finish without human
  debugging.
- Each task must have one primary subsystem boundary and one observable
  acceptance bar.
- Manual playtesting is not an acceptable final validation path when the
  autonomous harness can exercise the flow.
- If a flow still requires human driving, the first task is to make it
  automatable.
- Frontier agents should validate by replay, state capture, scripted scenario,
  or headless simulation before a human is asked to verify anything visually.

### Task Budget

| Phase | Budget | Outcome |
|---|---:|---|
| 11. Autonomous Validation Mesh | 850 | Scenario-driven testing, replay, evidence capture, bot-safe play loops |
| 12. Traversal and Presentation Expansion | 950 | More movement verbs, camera states, accessibility, and viewmodel depth |
| 13. Combat Sandbox Scale-Out | 1500 | Weapon families, attachments, damage models, recoil, and ammo economy |
| 14. Encounter, AI, and Objective Systems | 1400 | Enemy archetypes, behaviors, objectives, and encounter scripting |
| 15. Activities, Missions, and Progression | 1100 | Mission flow, rewards, loadouts, inventory, and progression loops |
| 16. World Scale and Destination Content | 1300 | Streaming spaces, region metadata, patrol content, and world events |
| 17. Social, Live Ops, and Services | 1000 | Parties, matchmaking, presence, live modifiers, and service seams |
| 18. Tools, Authoring, and Content Factory | 1500 | Higher-throughput content creation, validation, and import tooling |
| 19. Performance, Stability, and Ship Hardening | 1100 | Budgets, crash safety, threading, and deterministic verification |
| 20. Platform, Accessibility, and Release Variants | 500 | Input remap, accessibility, packaging, localization, and variants |

Total planning budget: **11,200 atomic tasks**.

### Phase Families

- **Phase 11:** validation mesh, scenario runner, artifact capture, and
  evidence-driven automation.
- **Phase 12:** traversal expansion, camera/viewmodel states, and accessibility
  coverage.
- **Phase 13:** combat sandbox scale-out across weapon families, damage, and
  feedback.
- **Phase 14:** encounters, AI, objective logic, and enemy archetypes.
- **Phase 15:** activities, missions, rewards, loadouts, and progression.
- **Phase 16:** destination scale, streaming, region metadata, and world
  events.
- **Phase 17:** social systems, matchmaking, live ops, and service validation.
- **Phase 18:** authoring tooling, import validation, and content factory
  throughput.
- **Phase 19:** performance budgets, stability, deterministic replay, and CI
  gating.
- **Phase 20:** platform variants, accessibility, localization, and packaging.

### Expansion Rules

1. Split each phase family into one queue task per focused behavior change.
2. Keep one validation target per task.
3. Prefer small vertical slices over broad feature bundles.
4. Use the autonomous harness for all gameplay-facing verification.
5. Use reports to record what was proven, what remains manual, and what was
   intentionally deferred.

The companion slice map in `../vault/workflows/phase-slice-map.md` should be
kept in sync with this document whenever a phase is activated.

## Related

- [Architecture](../systems/architecture.md) · [Networking](../systems/networking.md)
  · [Renderer backend](../systems/renderer_backend.md) ·
  [Asset pipeline](../systems/asset_pipeline.md)
- [Decision log](../vault/memory/decision-log.md) ·
  [Current state](../vault/memory/current-state.md) ·
  [Open questions](../vault/memory/open-questions.md)
