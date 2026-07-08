---
type: subagent-report
category: implementation
status: implemented_not_validated
created: 2026-07-06
agent: opencode
subsystems: [client, engine/render, engine/animation]
branch: agent/viewmodel/1920-ik
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260704-1920-viewmodel-arm-hand-ik

## Task

Add a first-person arm and hand rig with IK so the player sees hands gripping
the weapon at correct positions, staying attached during idle sway, movement
bob, and recoil.

## Status

implemented_not_validated — cmake is not available in the current PowerShell
environment, so build + test validation could not be run. Code changes are
syntactically consistent with existing patterns.

## Scope

### In bounds
- Per-weapon grip socket positions (right hand grip, left hand foregrip)
  defined in viewmodel-local space
- Viewmodel-specific two-bone IK solver (shoulder→elbow→hand) in the
  presentation layer
- Arm IK wired into the first-person render path via modified joint matrices
- Debug visualization fields for IK target and arm chain positions
- Arms originate from viewmodel bone positions and terminate at grip sockets
- IK stays in the presentation layer — no WeaponRuntime or gameplay changes

### Out of bounds
- No full-body IK or 3rd-person animation changes
- No procedural reload animation
- No new arm/hand mesh authoring — reuses viewmodel_arms assets
- No left-hand IK (only one arm in the viewmodel_arms skeleton)
- No changes to weapon runtime, gameplay code, or firing mechanics

## Files Changed

- `client/include/ahamkara/client/weapon_viewmodel_data.h` — Added
  `WeaponGripSockets` struct and per-weapon array `kWeaponGripSockets`
- `client/include/ahamkara/client/weapon_presentation.h` — Declared
  `apply_viewmodel_arm_ik()` free function
- `client/src/weapon_presentation.cpp` — Implemented analytical two-bone IK
  solver + `apply_viewmodel_arm_ik()`
- `client/src/client_frame_pipeline.cpp` — Wired IK call after joint matrix
  copy in `stage_build_scene()`; populated debug IK fields
- `engine/render/include/ae/render/debug_renderer.h` — Added IK debug
  visualization fields (`show_ik_target`, `ik_target_position`,
  `show_arm_chain`, `arm_*_pos`)

## What Changed

1. **Grip socket data** (`weapon_viewmodel_data.h`): New `WeaponGripSockets`
   struct with right/left hand grip positions (float x/y/z each). Per-weapon
   array defines plausible grip points for AR-15, Shotgun, and Rocket Launcher
   based on the viewmodel_arms skeleton bind pose (~0, 0.75, 0 for
   weapon_attach bone).

2. **Analytical two-bone IK** (`weapon_presentation.cpp`): Inline
   `solve_two_bone_arm()` function using law of cosines to compute shoulder
   and elbow rotation corrections. Default arm direction is +Y (matching the
   viewmodel_arms skeleton hierarchy). Produces quaternion corrections that
   are converted to rotation matrices and multiplied into the existing joint
   matrices.

3. **IK integration** (`weapon_presentation.h/.cpp`): Free function
   `apply_viewmodel_arm_ik()` reads grip socket positions from
   `weapon_grip_sockets()`, extracts shoulder position from the joint matrix
   array, computes target relative to shoulder, solves IK, and applies
   rotation corrections to shoulder (joint 2) and elbow (joint 3) joint
   matrices via matrix multiplication.

4. **Pipeline wiring** (`client_frame_pipeline.cpp`): IK runs on the scene
   copy of joint matrices (not the cache originals) after the memcpy from
   the animation clip player. Existing weapon sway/bob/recoil transform is
   preserved — IK targets are in model space and the weapon animation
   transform is applied as the model matrix by the renderer on top.

5. **Debug visualization** (`debug_renderer.h`): Added `show_ik_target`,
   `ik_target_position`, `show_arm_chain`, `arm_shoulder_pos`,
   `arm_elbow_pos`, `arm_hand_pos` fields. Populated in
   `client_frame_pipeline.cpp` after the IK solve for runtime debugging.

## Joint Indices (viewmodel_arms.gltf skin)

| Index | Node    | Purpose             |
|-------|---------|---------------------|
| 0     | Bone    | Root root bone      |
| 1     | root    | Armature root       |
| 2     | shoulder| IK chain root       |
| 3     | elbow   | IK chain mid        |
| 4     | wrist   | (not IK-adjusted)   |
| 5     | hand    | IK chain end effector|
| 6     | weapon_attach | Grip point  |

IK chain: shoulder (idx 2) → elbow (idx 3) → hand (idx 5)
Upper bone: 0.35m | Lower bone: 0.34m

## Validation

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Results

Validation was skipped — cmake is not on PATH in this PowerShell environment.
Code changes are structurally consistent with existing patterns (identical
namespace usage, type conventions, and matrix operations used elsewhere in the
codebase).

## Known Gaps

- Grip socket values are estimated from bind pose (weapon_attach position) and
  will need visual tuning once runtime display is available.
- The left hand grip socket is defined in data but not yet IK-solved (the
  viewmodel_arms skeleton has only one arm chain).
- IK does not account for pole vector (elbow twist direction) — uses default
  plane determined by default direction and target direction.
- No runtime visual confirmation (no GL display in this environment).
- Validation (build + tests) could not be run.

## Runtime Risks

- If the animation clip player produces joint matrices that include the root
  node rotation (a 90-degree quaternion), the IK solver's +Y convention may
  not align and will need tuning.
- The grip socket right-hand positions (all ~0, 0.70, 0) assume the weapon
  attach point is at roughly that location; actual weapon models may differ
  and require per-weapon adjustment.

## Cross-Agent Dependencies

- Grip socket values should be updated once actual weapon viewmodel meshes
  (viewmodel_ar15, viewmodel_rocket_launcher, etc.) are used in place of the
  shared viewmodel_arms placeholder path.
- The left-arm IK chain will need a second ik chain and a left-arm skeleton
  extension when dual-arm rendering is added.

## Recommended Next Step

- Verify build + tests in an environment with cmake available
- Runtime visual confirmation (requires GL display)
- Tune grip socket positions per weapon based on visual feedback

## Confidence

`medium` — the code follows established patterns and the analytical IK
math is well-understood, but build + runtime validation could not be
performed in this environment.
