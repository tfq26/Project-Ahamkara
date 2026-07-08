---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [render, game, docs]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260628-0102-viewmodel-orientation-contract

## Task

Make the weapon/viewmodel orientation contract explicit and reusable so new weapons can be authored and rendered with a consistent axis convention.

## Status

implemented

## Scope

In bounds: Inspect current viewmodel orientation path, make authored axis convention explicit, reduce per-weapon special casing, keep existing rifle rendering working.

Out of bounds: Designing new weapons, changing weapon balance/fire/animations, rebuilding the asset pipeline.

## Files Changed

- `engine/render/src/debug_renderer.cpp` — Renamed `WeaponViewmodelPose`/`weapon_viewmodel_pose` to `WeaponViewmodelTransform`/`weapon_viewmodel_transform` (matching game convention). Added comprehensive orientation contract comment documenting: +X barrel axis, -90° Y view-space conversion, rotation order, column-major/post-multiplied matrix convention.
- `game/include/ahamkara/game/weapon_registry.h` — Expanded `WeaponViewmodelTransform` doc comment with full axis convention and cross-reference to renderer mirror. Added "must stay in sync" note.
- `docs/systems/asset_pipeline.md` — Added "Viewmodel Orientation Contract" subsection under Viewmodel Generator documenting the full axis convention for asset authors.

## What Changed

The weapon orientation convention was already correctly implemented but spread across comments and code. This task:

1. **Renamed** the renderer-local duplicate from `WeaponViewmodelPose` to `WeaponViewmodelTransform` (matching the game-layer canonical name). Both structs have identical fields; both return all-zeros for the three current weapons; both must stay in sync.

2. **Documented the full convention** in three canonical locations:
   - Renderer site (where the conversion happens): `engine/render/src/debug_renderer.cpp`
   - Game-layer owner: `game/include/ahamkara/game/weapon_registry.h`
   - Asset authoring doc: `docs/systems/asset_pipeline.md`

3. The convention is: +X barrel in Blender → -90° Y rotation in renderer → view-forward (-Z). Per-weapon pitch/yaw/roll adjustments apply after barrel correction. Column-major matrices, post-multiplied.

No behavioral change. The rifle renders identically (build passes, 14/14 tests pass).

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: Pass (only pre-existing entt warnings)
- 14/14 tests pass (0 failures)
- Not runtime-confirmed: no GL display in this environment; visual verification requires a display

## Known Gaps

- The duplicate `WeaponViewmodelTransform` in the renderer cannot be eliminated because `ae_render` does not (and should not) depend on `ahamkara_game`. The cross-reference comments mitigate drift risk.
- No automated test enforces the sync between the two struct definitions.
- Runtime visual confirmation of rifle rendering not done (requires GL display).

## Runtime Risks

Minimal — name changes only (no behavior change). The struct and function had zero external callers (both local to the anonymous namespace in debug_renderer.cpp). If future changes create drift between the game and renderer copies, visual bugs would occur, but the cross-reference comments mitigate this.

## Cross-Agent Dependencies

- Future weapon authors should follow the +X barrel convention documented in `docs/systems/asset_pipeline.md`.
- `WeaponAnimConfig`/`WeaponAnimState` in `engine/animation/include/ae/animation/character_weapon.h` references this convention implicitly but is not yet wired into the render path.

## Recommended Next Step

Codex review. The rifle rendering should be visually confirmed by a human with a GL display (`./scripts/start.sh local`).

## Confidence

`high` — documentation-only + rename, no behavioral change, build and tests green.
