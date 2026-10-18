# 04 — Gameplay Systems Engineer

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed baseline: `b705d74` on `dev/flashback-menu-shell`; this audit examined the dirty working tree described in [01-recon.md](01-recon.md).

## 1. Scope & Method

Read `audit/01-recon.md` first. Inspected gameplay movement and world integration, Jolt-backed collision/physics interfaces, input mapping, animation graph/state machine and character integration, AI perception/behavior/pathfinding, camera control, encounter scripting, persistence-related symbols, and associated test source. Used `rg`, `git diff`, `nl`, and direct source reads to verify implementation claims. The recon report records the dirty-tree counts and attempted build/test results; no builds or tests were run for this specialization. No game was launched. No source or configuration files were changed; this report is the only file written by this specialist, under `/audit/`.

The tree is materially dirty, so findings describe the current files, not necessarily committed `b705d74`. In particular, `game/src/player_movement_controller.cpp` and several animation/client/render files have local edits; AI, input, collision, and encounter implementation files inspected here are not listed as modified in the initial status. No behavioral/runtime validation was performed for individual findings.

## 2. Executive Summary

- **Gameplay rating: 2/5 — a useful FPS prototype foundation with major completeness and integration gaps.** The repository contains working-shaped building blocks, but several advertised gameplay mechanics are represented by config/state only or are not connected to authored contacts and runtime systems.
- Jolt-backed body/character collision, traces, an FPS movement controller, weapon combat, recoil, particles, decals, and camera feedback are present. The integration should be treated as **PARTIAL** pending independent runtime verification. [src: file: engine/collision/include/ae/collision/world.h:28-70; game/src/player_movement_controller.cpp:32-181; game/src/world_projectile.cpp:51-61; game/src/world.cpp:735-800]
- Player movement covers acceleration, jump buffering/coyote time, sprint, slide, collision capsule crouch, slope handling and head bob. `MovementSimState` also advertises moving platforms, ledge grabs and mantle state, but the controller does not populate most of those fields; surface material/ladder state has no visible gameplay writer. [src: file: game/include/ahamkara/game/movement.h:110-143; game/src/player_movement_controller.cpp:184-237; game/src/world_camera.cpp:40-60]
- Input has keyboard and gamepad-button bindings and file persistence, but the map has no axis binding type or mouse binding type. Loading appends to constructor defaults instead of replacing them, so a file entry does not reliably rebind an action. [src: file: engine/input/include/ae/input/input_map.h:12-56; engine/input/src/input_map.cpp:39-79,132-173]
- Animation includes clip/state-machine/graph/blend/IK primitives and the dirty tree adds character presentation wiring. Multi-clip blending is a linear interpolation of 4×4 matrix elements, explicitly a simplification that can distort rotations/scales; the 2D blend-space state falls back to its default clip. [src: file: engine/animation/src/animation_graph.cpp:84-153; engine/animation/src/state_machine.cpp:199-221]
- NPC AI has archetypes, a small finite state machine, perception, A*, and path following, but target selection is currently hard-coded to one player, perception is a 2D approximation, and failed route planning falls back to direct movement through the world. [src: file: game/src/ai_combatant.cpp:18-60,63-123,520-569,573-669]
- Encounter scripting is a data model plus a runtime manager, but several declared objective/trigger types do not execute, non-player-enter triggers do not dispatch their configured actions, and simultaneous waves mark complete before their enemies die. This is not yet a dependable authored encounter system. [src: file: game/include/ahamkara/game/encounter_scripting.h:21-123; game/src/encounter_scripting.cpp:162-350]
- No general single-player game save/load path was found in the inspected gameplay code. A `ProgressionSnapshot` has binary serialization, and input/config persistence exists; neither is a world/session save. [src: file: game/src/pickup_model.cpp:599-663; engine/input/src/input_map.cpp:132-173; repository search recorded during this audit]
- Highest leverage next steps: finish one collision-grounded movement slice; make input file loading replace/validate bindings; connect AI movement to collision; and make encounter trigger/objective dispatch complete and testable before adding more authored content.

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Physics backend / body simulation | `engine/physics/`, `engine/collision/`, Jolt backend in `game/src/world_jolt_bridge.*` | PARTIAL | 3 | Two Jolt-facing APIs exist; collision module wraps bodies, traces and virtual character. Actual gameplay world still bridges into Jolt internals. |
| Collision bodies, masks, traces | `engine/collision/include/ae/collision/`, `engine/collision/src/` | PARTIAL | 2 | Shapes, layers, AABB/ray/sphere/capsule/cone traces and mask-aware query filters exist. `BodyDef::collision_mask` is not applied to pair filtering; active-state semantics do not remove bodies from physics. |
| Player movement / controller | `game/include/ahamkara/game/movement.h`, `game/src/movement.cpp`, `game/src/player_movement_controller.cpp`, `game/src/world.cpp` | PARTIAL | 3 | Core FPS movement and CCT integration exist; material contact, ladder, ledge, moving-platform and mantle behavior are incomplete or unwired. |
| Input actions / bindings | `engine/input/include/ae/input/input_map.h`, `engine/input/src/input_map.cpp`, `client/src/controller_bindings.*` | PARTIAL | 2 | Keyboard and gamepad buttons; saved bindings append instead of replacing defaults. Stick/mouse-axis input is not modeled in this `InputMap`. |
| Animation runtime | `engine/animation/`, `engine/skeleton/`, `engine/render/src/skeletal_animation.cpp` | PARTIAL | 3 | Clip playback, state machine, blend spaces, graph, procedural and IK primitives; matrix-element blending and 2D blend fallback limit quality. |
| Character animation presentation | `client/include/ahamkara/client/animated_character.h`, `client/src/animated_character.cpp`, `client/src/weapon_animation_controller.cpp` | PARTIAL | 2 | Dirty-tree additions wire presentation, but no runtime/display validation was made; avoid treating new assets/wiring as verified gameplay. |
| NPC AI / behavior | `game/include/ahamkara/game/ai/ai_combatant.h`, `game/src/ai_combatant.cpp` | PARTIAL | 2 | Archetypes, perception, states, firing timers and respawn; simplified one-player targeting/perception, and no collision resolution in movement integration. |
| NPC pathfinding | `game/include/ahamkara/game/ai/nav_grid.h`, `nav_agent.h`, `path_follower.h`, `game/src/ai_combatant.cpp` | PARTIAL | 2 | Deterministic grid A* and follower exist; grid rasterizes wall-box centers only; some failed plans use direct-line fallback. |
| Camera and feedback | `game/src/world_camera.cpp`, `game/include/ahamkara/game/camera_anchor.h`, `game/src/world.cpp`, `game/src/world_projectile.cpp` | PARTIAL | 3 | Look clamp/wrap, eye height, bob, landing impulse, weapon recoil recovery, hitmarker/muzzle-flash timers, particles and decals exist. Not runtime-checked here. |
| Encounter scripting | `game/include/ahamkara/game/encounter_scripting.h`, `game/src/encounter_scripting.cpp`, `game/src/world.cpp` | PARTIAL | 1 | Data declares six objective types/five trigger types; runtime currently handles only a subset and action dispatch is inconsistent. |
| Game save/load | `game/` | MISSING | 1 | No general world/session save/load API found. Progression snapshot persistence is narrower. |
| Progression persistence | `game/include/ahamkara/game/pickup_model.h`, `game/src/pickup_model.cpp` | IMPLEMENTED | 3 | `ProgressionSnapshot::serialize/deserialize` exists; not evidence of restorable gameplay world state. |
| Automated behavioral verification | `tests/src/player_movement_controller_tests.cpp`, `tests/src/world_tests.cpp`, `tests/src/nav_grid_tests.cpp`, encounter tests under `tests/src/` | PARTIAL | 2 | Unit/integration tests are present; this specialist did not run them. Recon’s broad dirty-tree CTest result is in `01-recon.md`. |

## 4. Detailed Findings

### GAME-001 — Surface and traversal state is declared but not supplied by gameplay contacts

- **Severity:** HIGH
- **Category:** missing feature / design flaw
- **Evidence:** `MovementSimState` initializes `ground_material` to `Default` and has `ground_normal`, ladder, ledge, mantle and moving-platform fields. In the inspected gameplay sources, ground material is read in movement and ladder resolution but no code writes it from a collision contact. The controller only sets `on_ladder` when the material already equals `Ladder`; it does not detect or populate `ledge_grabbed`, `is_mantling`, or moving-platform state. `resolve_mantle` snaps a rising player vertically onto an `auto_step` collider when the player is already horizontally inside its footprint. [src: file: game/include/ahamkara/game/movement.h:113-143; game/src/player_movement_controller.cpp:103-104,184-237]
- **Impact:** Ice/mud/sand multipliers and ladder interaction are effectively dormant on the main controller path; mantle/ledge/moving-platform state is not a usable gameplay contract. An AI authoring a platformer/FPS traversal feature would see plausible APIs and config but lack an end-to-end contact mechanism.
- **Recommended fix:** Define a collision contact/material tagging path and update ground normal/material from `CharacterController` each step. Implement explicit ladder-volume and ledge/mantle transitions, with corresponding state reset and movement constraints; remove unused config/state until a complete implementation exists.
- **Effort:** M
- **Dependencies:** none

### GAME-002 — Encounter trigger dispatch and objective support are incomplete

- **Severity:** HIGH
- **Category:** bug / missing feature
- **Evidence:** The runtime switch advances `Timer`, `PreviousDone`, and `AllEnemiesDead` trigger states by setting only `fired`; it does not execute the `spawn_wave_id` or `objective_id` actions for those cases. `check_player_volume` is the only path shown dispatching `spawn_wave_id`, and it does not activate `objective_id`. `check_objectives` only has behavior for `KillAll` and `Survive`; the latter adds `0.0F` to elapsed time, while `Defend`, `Collect`, `Reach`, and `Boss` fall through. `notify_enemy_killed` increments objectives by searching whether an ID contains “kill” rather than using the declared objective type/target. [src: file: game/src/encounter_scripting.cpp:195-287,289-350; game/include/ahamkara/game/encounter_scripting.h:21-105]
- **Impact:** Authored encounter data overstates executable behavior. Timer/chain/all-dead triggers may appear fired without starting their configured wave, and several advertised objective types can never complete through this manager. This prevents reliable AI-created mission scripting and makes authored content brittle to naming conventions.
- **Recommended fix:** Centralize trigger activation so every condition dispatches configured actions exactly once. Track objectives by typed runtime IDs, update survive time from `dt`, implement or explicitly reject unsupported objective types during validation, and add contract tests for each trigger/objective/action combination.
- **Effort:** M
- **Dependencies:** none

### GAME-003 — Simultaneous waves complete before enemies are eliminated

- **Severity:** HIGH
- **Category:** bug
- **Evidence:** In `tick_wave`, the `simultaneous` branch sets `ws.complete = true` while also assigning `ws.enemies_alive = ws.total_to_spawn`; the encounter tick skips completed waves and concludes when all wave `complete` flags are true. [src: file: game/src/encounter_scripting.cpp:119-159,162-178]
- **Impact:** A simultaneous wave can complete its encounter immediately after spawning even while its runtime state says enemies remain alive. Kill-all pacing and encounter progression cannot safely use this option.
- **Recommended fix:** Separate “finished spawning” from “wave complete”; complete only when the spawn count is exhausted and alive count reaches zero. Add a test that starts a simultaneous wave, confirms it remains active with living enemies, then completes after kill notifications.
- **Effort:** S
- **Dependencies:** GAME-002

### GAME-004 — Input configuration does not replace defaults or validate serialized values

- **Severity:** MEDIUM
- **Category:** bug / tech debt
- **Evidence:** `InputMap` constructs default bindings. `load_from_file` parses each line and calls `bind` without clearing current bindings, so loaded bindings are appended to defaults. Numeric values go directly through `std::stoi` and enum casts without local parse/range validation. [src: file: engine/input/src/input_map.cpp:39-79,132-157]
- **Impact:** A user/agent cannot predictably remap an action by editing the file alone, and malformed or out-of-range values can throw or persist invalid key/button codes. This undermines deterministic setup and AI-authored control tests.
- **Recommended fix:** Parse into a temporary validated binding map, define whether files replace or overlay defaults, then commit only after full validation. Return actionable line-numbered parse errors rather than throwing from malformed input.
- **Effort:** S
- **Dependencies:** none

### GAME-005 — Collision body mask and active-state contracts diverge from physical simulation

- **Severity:** MEDIUM
- **Category:** bug / design flaw
- **Evidence:** `BodyDef` exposes `collision_mask` as “Which layers this body collides with,” but `add_body` creates/adds a Jolt body using only `def.layer`; the inspected pair filter only excludes static-static and debris-debris pairs. `set_body_active(false)` only marks the wrapper inactive and calls `DeactivateBody`; it does not remove the body. Wrapper queries/traces check `body.active`, while physical character update uses Jolt’s default layer filters rather than wrapper activity. [src: file: engine/collision/include/ae/collision/world.h:61-70,121-122; engine/collision/src/collision_world.cpp:47-71,147-162; engine/collision/src/character.cpp:101-120; engine/collision/src/trace.cpp:28-63; engine/collision/src/jolt_backend.h:96-109]
- **Impact:** Callers can configure mask/activity and see those values honored by wrapper queries while physical contacts still follow broader Jolt filters. Debugging “why can the player collide with this disabled/masked body?” will be difficult, and gameplay AI cannot rely on a single collision contract.
- **Recommended fix:** Apply per-body collision masks to Jolt pair filtering (or remove the misleading per-body field and document query-only masks). Define active as either physical inclusion/removal or query visibility, and make traces, character contacts, AABB queries and body simulation agree. Add tests spanning each API.
- **Effort:** M
- **Dependencies:** none

### GAME-006 — NPC movement can bypass obstacles and does not use the physics character path

- **Severity:** MEDIUM
- **Category:** design flaw / bug
- **Evidence:** Navigation rasterizes wall AABBs into blocked cells by testing cell centers. In patrol/investigate/engage/flank behaviors, if `plan_ai_path` yields no path, movement falls back to direct movement toward the waypoint/target; AI movement then copies the resulting 2D position into `TransformComponent`, with no character-controller or collision query in that path. [src: file: game/src/ai_combatant.cpp:403-457,513-529,539-569,573-669,696-721; game/include/ahamkara/game/ai/nav_grid.h:183-190]
- **Impact:** NPCs can walk through walls when a goal is blocked or the grid cannot find a route. Grid-center rasterization also does not account for NPC radius, so narrow spaces may be marked passable even when a body cannot fit. This weakens pathing and combat behavior in authored levels.
- **Recommended fix:** Do not direct-move when route planning fails; return a blocked/no-route state and replan when navigation changes. Inflate obstacle footprints by agent radius, and move NPCs through a collision-aware controller or explicitly keep them as nonphysical abstract agents with that limitation surfaced to game authors.
- **Effort:** M
- **Dependencies:** none

### GAME-007 — Animation blending is not safe for rotations and 2D blend spaces are not evaluated

- **Severity:** MEDIUM
- **Category:** tech debt / design flaw
- **Evidence:** `AnimationGraph::evaluate` blends multi-clip joint matrices element-by-element, even though comments acknowledge that proper TRS decomposition is not implemented. `StateMachine::evaluate_active_clips` states that 2D blend is not integrated and emits the state’s default clip. [src: file: engine/animation/src/animation_graph.cpp:117-153; engine/animation/src/state_machine.cpp:199-221]
- **Impact:** Crossfades can produce invalid/non-normalized joint transforms and visible deformation; 2D locomotion blend-space data may be authored but ignored. AI-generated character animation setups can be accepted structurally yet render incorrectly.
- **Recommended fix:** Blend translation/scale linearly and rotation using normalized quaternion interpolation after matrix decomposition, or blend local TRS before hierarchy evaluation. Implement 2D blend evaluation or reject unsupported state declarations during loading. Add pose-level numerical tests and an image-based character validation scene.
- **Effort:** M
- **Dependencies:** none

### GAME-008 — General gameplay save/load is absent

- **Severity:** MEDIUM
- **Category:** missing feature
- **Evidence:** The code search found serialization for `ProgressionSnapshot` and user configuration/bindings, but no general `World` or game-session save/load API. `ProgressionSnapshot` serializes a progression record, not player/world transforms, NPC state, encounter timers, inventory-in-world, or level/session state. [src: file: game/src/pickup_model.cpp:599-663; engine/input/src/input_map.cpp:132-173; game/include/ahamkara/game/world.h]
- **Impact:** Checkpointing and deterministic “save, mutate, reload, compare” loops are unavailable as a common gameplay API. This limits both player-facing persistence and AI agents’ ability to reproduce/branch test scenarios.
- **Recommended fix:** Decide the initial persistence scope (campaign checkpoint versus complete simulation snapshot), then define a versioned text/binary schema with stable entity IDs and explicit compatibility behavior. Keep configuration/progression persistence separate from world-state save semantics.
- **Effort:** L
- **Dependencies:** owner decision on initial game/session model

### GAME-009 — Input and encounter test evidence remains unverified in this pass

- **Severity:** LOW
- **Category:** risk
- **Evidence:** Test sources exist for player movement/world/pathfinding, but this specialist did not execute them. The recon report records a 67/71 CTest result on the dirty tree, including environment-restricted socket failures, a dirty asset-contract mismatch, and a UI test link failure; that result does not isolate the gameplay tests. [src: file: tests/src/player_movement_controller_tests.cpp; tests/src/world_tests.cpp; tests/src/nav_grid_tests.cpp; audit/01-recon.md:1-5]
- **Impact:** Static implementation observations are not proof of runtime behavior, and none of the issues above should be treated as already regression-tested or fixed.
- **Recommended fix:** Once a clean intended baseline is identified, run the focused movement, world, collision, AI/nav, animation, and encounter tests; add missing regression tests from GAME-001 through GAME-007.
- **Effort:** S
- **Dependencies:** RECON-004

## 5. What's Good

- The physics engine boundary provides typed body definitions, handles, collision layers and query APIs rather than exposing Jolt in the public collision headers. [src: file: engine/collision/include/ae/collision/world.h:20-25,28-70,85-151]
- Movement tuning is gathered in `MovementConfig`, and the current dirty tree threads its walk/sprint/jump/gravity values into the controller; a custom config test was added in the dirty test file. [src: file: game/include/ahamkara/game/movement.h:32-107; game/src/world.cpp:312-349; tests/src/player_movement_controller_tests.cpp:56-77]
- The path grid and A* implementation are deterministic for fixed input, with explicit corner-cut prevention and focused pathfinding tests. [src: file: game/include/ahamkara/game/ai/nav_grid.h:72-180; tests/src/nav_grid_tests.cpp:27-93]
- Camera and combat feedback have a sensible data-driven seam: recoil is drawn from the weapon’s deterministic pattern and eased back by the world tick; camera bob and landing feedback are isolated from renderer dependencies. [src: file: game/src/world_projectile.cpp:51-61; game/src/world.cpp:236-244,735-746; game/src/world_camera.cpp:12-60]
- Encounter definitions are plain C++ data structures and progression state is separately inspectable, which can support agent-generated content once execution semantics are completed. [src: file: game/include/ahamkara/game/encounter_scripting.h:40-123]
- The codebase has several tests aimed at meaningful gameplay outcomes, including movement state, world projectile/recoil feedback and nav-grid route properties; preserve these intent-level tests while fixing their execution graph and baseline ambiguity. [src: file: tests/src/player_movement_controller_tests.cpp:15-88; tests/src/world_tests.cpp:235-282; tests/src/nav_grid_tests.cpp:27-193]

## 6. Open Questions for the Owner

- Should the first game target be an FPS/shooter (current movement/combat fit) or a broader 2D/3D engine API? Recommended default: finish the demonstrable FPS vertical slice before investing in generalized traversal and AI features.
- Is the intended NPC model physical (collision-resolved characters) or abstract deterministic transforms? Recommended default: collision-resolved for authored combat NPCs because the current navigation is obstacle-aware.
- What should “save/load” mean for the first product: checkpointed campaign progress or exact simulation snapshots for replay/testing? Recommended default: versioned checkpoint plus reproducible test snapshot as separate APIs.
- Are unsupported encounter objective types planned near-term? Recommended default: either implement all declared types or reject them during encounter validation rather than silently accepting inert content.

## 7. Handoff Notes for the Next AI Agent

- Start from `audit/01-recon.md`; preserve the dirty checkout and attribute all observations to the working tree unless separately verified against a clean revision.
- For movement, trace the whole path through `World::apply_input`, `PlayerMovementController`, collision shape update, Jolt `extended_update`, state sync and camera update. Do not infer that a `MovementConfig` field is active from its presence in the struct.
- For AI, inspect both `tick_ai_combatants` and `tick_ai_combatants_movement`; the latter updates transforms through direct 2D coordinates and currently bypasses collision resolution.
- For encounters, test condition-to-action dispatch, simultaneous-wave completion and every declared objective type; a `fired` trigger is not evidence that its authored action ran.
- For persistence, distinguish game/world saves from `ProgressionSnapshot`, network snapshots, config files and input-binding files.
- No focused gameplay test run was performed by this report. Use the isolated build/test caveats in `audit/01-recon.md` before interpreting a failing result.
