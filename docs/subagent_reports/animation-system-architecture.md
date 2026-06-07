# Task
Design and begin the animation system architecture for FPS characters and weapons (focus areas 71–80), creating a new `engine/animation/` module with state machines, blend trees, weapon/character animation, IK, aim offsets, procedural recoil, networking compression plan, and debug tools.

# Outcome

**Fully implemented:**
- New `engine/animation/` static library with 8 header files and 8 source files
- `StateMachine` class with trigger-driven transitions, crossfade blending, exit-time transitions, 1D blend space integration, and animation event/notify firing
- `AnimationGraph` class that evaluates state machine output into blended joint matrices using the existing `evaluate_animation()` from `ae_render`
- `JointTransform` with TRS decomposition and `JointTransform::blend()` using quaternion slerp
- `BlendSpace1D` (threshold-bracketing) and `BlendSpace2D` (3-nearest inverse-distance weighting)
- `evaluate_weapon_animation()` for first-person viewmodel sway, movement bob, ADS blend, and fire kick
- `fire_recoil()` impulse and `apply_recoil()` spring-damper recovery with ADS multiplier
- Analytical two-bone IK solver (`IKSolver::solve_two_bone`) using law of cosines
- `AnimationDriver` that translates gameplay state (`AnimGameplayInput`) into state machine triggers, blend parameters, recoil, and weapon animation
- `AnimationDebugger` for overlay text, trigger logging, and skeleton bone-line extraction
- `CompressedAnimState` bitfield struct (8 bytes) with quantize/dequantize helpers for networked animation replication
- Pre-configured locomotion state machine in `AnimationDriver::init_locomotion()` (idle/walk/sprint/jump/slide/crouch/land with all transition mappings)
- Pre-configured upper-body state machine in `AnimationDriver::init_upper_body()` (idle/fire/reload/ads_idle/ads_fire)

**Partially implemented / architecture-documented only:**
- `StateMachine` 2D blend space evaluation: `BlendSpace2D` data and `get_barycentric_blend()` exist, but `StateMachine::evaluate_active_clips()` falls back to the default clip for 2D blend states (placeholder for future integration)
- `CharacterAnimInstance::tick()` skeleton exists but copies `last_pose_` identity rather than driving clip evaluation from its internal state machines (intended for external `AnimationGraph` integration)
- IK solver `solve_two_bone` is implemented but assumes a hardcoded "arm hanging down" default direction; the `root_global_world` matrix parameter is unused (simplified for the initial pass)
- The animation system is not wired into `DebugRenderer` or `World::tick()` — it compiles and links but has no runtime call site yet

**Not implemented:**
- Actual clip-driven rendering in the debug renderer (an existing `ProceduralAnimState` is available but not piped through the new driver)
- Network packet serialization of `CompressedAnimState` into `ServerSnapshot`
- glTF animation asset examples (the system expects `GltfAnimation`/`GltfSkin` data from `GltfLoader`)

# Files Changed

- `engine/animation/CMakeLists.txt` — new build file for the `ae_animation` static library (C++20, links `ae_render` + `ae_core`)
- `engine/animation/include/ae/animation/types.h` — core data types: `JointTransform`, `AnimationPose`, `AnimationClip`, `ClipInstance`, `BlendSpace1D`, `BlendSpace2D`, `AnimationState`, `AnimationTransition`, `AnimationEvent`
- `engine/animation/include/ae/animation/state_machine.h` — `StateMachine` class header
- `engine/animation/include/ae/animation/animation_graph.h` — `AnimationGraph` class header (includes `state_machine.h` for `ActiveClip`)
- `engine/animation/include/ae/animation/ik.h` — `IKChain`, `IKTarget`, `IKSolveResult`, `IKSolver` class header
- `engine/animation/include/ae/animation/aim_recoil.h` — `AimOffsetConfig`, `AimOffsetState`, `RecoilConfig`, `RecoilState`, `fire_recoil()`, `apply_recoil()`
- `engine/animation/include/ae/animation/character_weapon.h` — `CharacterAnimInstance` class header, `WeaponAnimConfig`, `WeaponAnimState`, `evaluate_weapon_animation()`
- `engine/animation/include/ae/animation/animation_driver.h` — `AnimMovementState` enum, `AnimGameplayInput` struct, `AnimationDriver` class header, `CompressedAnimState` struct, `quantize_normalized()`, `dequantize_normalized()`
- `engine/animation/include/ae/animation/debug.h` — `AnimationDebugger` class header, `AnimationDebugOverlay`, `AnimDebugLine`, `JointLine`
- `engine/animation/src/types.cpp` — implementation of `JointTransform`, `AnimationPose`, `ClipInstance`, `BlendSpace1D`, `BlendSpace2D`
- `engine/animation/src/state_machine.cpp` — full `StateMachine` implementation (states, transitions, triggers, crossfade, events)
- `engine/animation/src/animation_graph.cpp` — `AnimationGraph` multi-clip evaluation with matrix-lerp blending, additive layer, procedural fallback
- `engine/animation/src/animation_driver.cpp` — `init_locomotion()`, `init_upper_body()`, `tick()`, `compute_aim_offset()`
- `engine/animation/src/character_weapon.cpp` — `CharacterAnimInstance` methods and `evaluate_weapon_animation()` function
- `engine/animation/src/aim_recoil.cpp` — `fire_recoil()`, `apply_recoil()` with spring-damper physics
- `engine/animation/src/ik.cpp` — `IKSolver::add_chain()`, `set_target()`, `solve_two_bone()` analytical two-bone IK
- `engine/animation/src/debug.cpp` — `build_overlay()`, `log_trigger()`, `log_state_change()`, `extract_skeleton_lines()`
- `CMakeLists.txt` — added `add_subdirectory(engine/animation)` under the `AHAMKARA_BUILD_CLIENT` conditional block (line 78)

# Interfaces Added Or Changed

**New public namespace:** `ae::animation`

**New public types:**
- `JointTransform` — decomposed TRS with `to_mat4()`, `blend()`, `identity()`
- `AnimationPose` — local transforms + global matrices with `compute_globals()`
- `AnimationClip` — named clip with `GltfAnimation*` source, duration, looping flag
- `ClipInstance` — playback state with `advance()`, `reset()`, `normalized_time()`
- `BlendSample`, `BlendSpace1D` (1D threshold bracketing), `BlendSpace2D` (2D barycentric)
- `AnimationState`, `AnimationTransition` — state machine configuration types
- `AnimationEvent` — timed event with name/payload/has_fired
- `AnimationEventCallback` — `std::function<void(string, string)>`
- `AnimStateId` — type alias for `std::string`
- `AnimMovementState` — enum (Idle, Walking, Sprinting, Sliding, Jumping, OnLadder, LedgeGrab, Mantling, Crouching)
- `AnimGameplayInput` — per-frame gameplay-to-animation bridge struct
- `CompressedAnimState` — 8-byte bitfield for network replication
- `IKChain`, `IKTarget`, `IKSolveResult` — IK data types
- `AimOffsetConfig`, `AimOffsetState` — aim offset configuration/runtime
- `RecoilConfig`, `RecoilState` — recoil configuration/runtime
- `WeaponAnimConfig`, `WeaponAnimState` — first-person weapon animation config/runtime
- `AnimDebugLine`, `AnimationDebugOverlay` — debug overlay types
- `AnimationDebugger::JointLine` — skeleton visualization line

**New public classes:**
- `StateMachine` — animation state machine with trigger/crossfade/events
- `AnimationGraph` — clip evaluation and blending engine
- `IKSolver` — two-bone IK chain management and solving
- `CharacterAnimInstance` — third-person character animation container
- `AnimationDriver` — gameplay-to-animation translation layer
- `AnimationDebugger` — debug inspection and visualization

**New public free functions:**
- `evaluate_weapon_animation()` — FP weapon viewmodel animation
- `fire_recoil()` — recoil impulse
- `apply_recoil()` — recoil recovery
- `quantize_normalized()`, `dequantize_normalized()` — network quantization helpers

**Build system changes:**
- New CMake target `ae_animation` (STATIC library, C++20, depends on `ae_render` PUBLIC and `ae_core` PUBLIC)
- Root `CMakeLists.txt`: `add_subdirectory(engine/animation)` added after `add_subdirectory(engine/render)`, gated by `AHAMKARA_BUILD_CLIENT`
- No new external dependencies

**No changes to:**
- Render backend API (`RenderBackend` unchanged)
- GPU shader skinning (shader uses existing `uJointMatrices[8]`, `aJoints`, `aWeights` uniforms — unchanged)
- `GltfAnimation`, `GltfSkin`, `GltfModel`, `GltfLoader` (consumed, not modified)
- `game::World`, `game::MovementState`, `game::PlayerInputCommand`, `game::ReplicatedPlayerState` (not yet integrated)
- `DebugRenderer` (not yet calling animation driver)

# Behavior

The animation module is a **passive library** — it compiles and links but has no runtime call sites in the existing game loop or renderer. When integrated:

1. **Third-person characters:** `AnimationDriver::tick()` takes `AnimGameplayInput` (populated from `World`/ECS), fires triggers on the locomotion `StateMachine`, evaluates `AnimationGraph` with the active clips, and produces `std::vector<Mat4>` joint matrices for GPU skinning via existing `draw_gpu_model()`.

2. **First-person weapons:** `evaluate_weapon_animation()` produces a `Mat4` weapon transform (sway + bob + ADS + recoil) that the renderer applies as a model matrix before drawing the weapon mesh in view space.

3. **Recoil:** Each shot fires an impulse via `fire_recoil()`, and spring-damper recovery runs each frame via `apply_recoil()`, producing a quaternion offset applied to the weapon transform.

4. **Animation events:** Timed `AnimationEvent` entries fire callbacks during clip playback (e.g., "footstep" at t=0.3, "fire" at t=0.1), enabling audio triggers, particle spawns, etc.

5. **Networking:** `CompressedAnimState` (8 bytes) is defined but not serialized. When integrated, the server would compress animation state and replicate it per character per snapshot.

# Validation

**Build:**
- `cmake --preset debug` — configured cleanly
- `cmake --build build/debug --target ae_animation` — compiled 7 translation units, linked `libae_animation.a` (0 errors, 0 warnings from animation code)
- `cmake --build build/debug --target ahamkara_client` — linked `ahamkara_client` successfully
- `cmake --build build/debug --target all` — full project build succeeded (0 errors; all warnings are pre-existing from EnTT, Jolt, and existing test code)

**Tests:**
- `ctest --test-dir build/debug` — all 6 test suites passed:
  - `ahamkara_smoke_tests` — PASSED
  - `ahamkara_world_tests` — PASSED
  - `ahamkara_movement_tests` — PASSED
  - `ahamkara_collision_tests` — PASSED
  - `ahamkara_gameplay_tests` — PASSED
  - `ahamkara_asset_pipeline_tests` — PASSED (includes `test_compiled_mesh_roundtrip` which exercises `generate_humanoid_mesh` with skinning data)

**No test regressions.** Existing skeletal animation code, humanoid mesh generation, and glTF loading continue to compile and pass.

# Known Gaps

1. **No runtime integration:** The animation driver has no call site. `DebugRenderer::render()` still renders static humanoid models without animation. The existing `ProceduralAnimState` in `DebugRenderer::Impl` is unused.

2. **2D blend evaluation incomplete:** `BlendSpace2D::get_barycentric_blend()` computes nearest-3 weights, but `StateMachine::evaluate_active_clips()` falls back to the default clip for 2D blend states. The 2D blend output is not consumed.

3. **IK solver assumptions:** `solve_two_bone()` assumes a default bone direction of (0, -1, 0) for "arm hanging down". The `root_global_world` parameter is accepted but not used. The implementation is a simplified analytical solver suitable for prototype use but will need refinement for production.

4. **Matrix lerp approximation:** `AnimationGraph::evaluate()` blends joint matrices using per-element linear interpolation rather than proper TRS decomposition → slerp → recomposition. This is fast but can produce shear artifacts at extreme blend factors.

5. **No animation assets:** The system expects `GltfAnimation` data from glTF files, but the repo has only `test_box.gltf` (no animation tracks). Procedural animation is the only currently usable output.

6. **Event loop-detection:** `StateMachine` event handling resets fired flags based on a 0.1s wrap heuristic. This works for looping clips but may miss events in non-looping or very short clips.

7. **`CharacterAnimInstance::tick()` stub:** Copies identity pose rather than evaluating its state machines through an `AnimationGraph`. This class is an architectural placeholder.

8. **No `CompressedAnimState` serialization:** The 8-byte struct and quantize helpers are defined but not integrated into `ServerSnapshot` or network serialization.

# Risks

- **Matrix lerp blending quality:** The per-element matrix lerp in `AnimationGraph` will produce non-orthogonal intermediate matrices during crossfades. Visible artifacts may appear at extreme blend values. A follow-up should implement proper TRS decomposition + slerp blending.
- **Binary compatibility:** No ABI concerns (static library). Header-only consumption is possible but not tested.
- **Build dependency chain:** `ae_animation` → `ae_render` → `ae_core` + `ae_platform` + OpenGL + GLFW. This introduces no new link dependencies beyond what already exists for `ahamkara_client`.
- **No platform-conditioned code:** The animation module has no platform-specific paths and should compile uniformly on macOS, Linux, and Windows.
- **State machine memory:** `StateMachine` uses `std::unordered_map` with `std::string` keys for states and clips. This is fine for the prototype scale (< 50 states) but may need arena allocation or string hashing in production.

# Next Recommended Steps

1. **Wire procedural idle animation through the DebugRenderer.** The lowest-risk integration: in `DebugRenderer::render()`, create an `AnimationGraph` + `AnimationDriver`, call `evaluate_procedural_animation()` on the existing `ProceduralAnimState`, and pass the result to `draw_gpu_model()`. This makes existing humanoid models visibly animate with breathing/sway.

2. **Populate `AnimGameplayInput` from `World::tick()`.** In `LocalPlaySimulation::tick()` or `World::tick()`, populate an `AnimGameplayInput` from `ReplicatedPlayerState` + `MovementSimState` + `PlayerInputCommand` and feed it to `AnimationDriver::tick()`. This wires locomotion state machines to actual gameplay.

3. **Add a test glTF model with animation tracks.** Create or source a simple animated humanoid `.gltf` + `.bin` with walk/idle/jump clips. Register clips with `AnimationGraph`, bind to the state machine, and render skinned animation from asset data.

4. **Implement proper TRS decomposition blending.** Replace the per-element matrix lerp in `AnimationGraph::evaluate()` with `JointTransform::blend()` (slerp for rotation, lerp for translation/scale). This requires decomposing the output `Mat4` matrices from `evaluate_animation()` into TRS components.

5. **Integrate first-person weapon rendering.** Add a weapon mesh draw call in the first-person render path that applies `evaluate_weapon_animation()` output as the weapon model matrix. Wire recoil impulses from the existing `fire_held` input.

6. **Add animation debug overlay.** In `DebugRenderer::draw_scene_overlay()`, call `AnimationDebugger::build_overlay()` and render the animation state text alongside existing debug panels.

7. **Serialize `CompressedAnimState` into network packets.** In `net_packets.h`, add the 8-byte `CompressedAnimState` to either `ReplicatedPlayerState` or a new `ReplicatedAnimState` struct that the server populates and the client decompresses to drive remote character animation.

# Notes For Integration

- The animation module is a **leaf consumer** of `ae_render` types (`Mat4`, `GltfAnimation`, `GltfSkin`, `GltfModel`, `GltfLoader`, `ProceduralAnimState`). It does **not** modify any render types.
- `AnimationGraph` holds **non-owning pointers** to `GltfAnimation*` and `GltfSkin*`. The caller must keep the `GltfModel` alive for the lifetime of the graph.
- `StateMachine` uses `std::string` for state and clip identifiers. These are expected to be short-lived configuration strings; no string interning or hashed string IDs are used yet.
- The `Init_locomotion()` and `init_upper_body()` methods in `AnimationDriver` populate a `StateMachine` with placeholder clip names (`"anim_idle"`, `"anim_walk"`, etc.). These names must match registered `AnimationClip` names in the `AnimationGraph` for evaluation to succeed.
- `AimOffsetConfig` uses joint indices that must align with the skeleton layout (current humanoid mesh uses: 0=Root, 1=Hips, 2=Spine, 3=Head, 4=LeftArm, 5=RightArm, 6=LeftLeg, 7=RightLeg). Set `spine_joint=2`, `neck_joint=3` for aim offsets.
- The shader uniform `uJointMatrices` is currently sized for **8 matrices** in the GLSL vertex shader (line 1615 of `debug_renderer.cpp`). The humanoid mesh has 8 joints. Any skeleton with more than 8 joints requires a shader constant increase.
- No `#include` of animation headers exists in any client/game/render source file yet. Integration will require adding `#include "ae/animation/animation_driver.h"` (or the specific needed header) to the relevant consumer.
