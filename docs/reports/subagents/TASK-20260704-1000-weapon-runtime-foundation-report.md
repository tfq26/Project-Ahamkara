---
type: subagent-report
category: gameplay-runtime
status: implemented
created: 2026-07-04
agent: opencode
subsystems: [game, docs]
branch: agent/opencode/weapon-runtime-foundation
validation: [cmake --build --preset debug, ./scripts/run-tests.sh --preset debug]
---

# Subagent Report — TASK-20260704-1000-weapon-runtime-foundation

## Task

Harden the base `WeaponRuntime` seam so future weapon-specific runtime behavior can hang off it without changing presentation code, confirm the model cache is read-only from gameplay, and keep player-owned state explicit.

## Status

implemented

## Scope

In bounds:
- Audit current weapon runtime/loadout ownership model
- Harden `WeaponRuntime` doc comment with explicit subclass contract
- Add protected `reload_timer()` accessor for future subclass use
- Add virtual `on_fire()` hook for future weapon-specific per-shot behavior
- Wire `on_fire()` through `Player::notify_weapon_fired()` → `World::notify_weapon_fired()` → called from `fire_projectile()` / `fire_hitscan()`
- Clarify ownership comments on `Player` and `WeaponModelCache`

Out of bounds:
- No weapon balance, ammo, or damage changes
- No presentation or viewmodel changes
- No ECS migration or broader architectural rewrites

## Audit Results

Ownership model is already mostly clean:
- `WeaponRuntime`: pure gameplay seam with virtual hooks, no presentation concerns
- `Player`: sole owner of `WeaponRuntime`, `Loadout`, and `ArmorConfig` — `World` delegates through `Player`
- `World`: no weapon runtime state leaked beyond ephemeral input tracking (`fire_recoil_index_`, `weapon_switch_queued_` — appropriate for World's orchestration role)
- `WeaponModelCache` (`ae::render`): completely separate namespace, no gameplay includes, read-only from gameplay perspective

## Files Changed

- `game/include/ahamkara/game/weapon_runtime.h` — expanded doc comment, added `on_fire()` hook, added `reload_timer()` accessor, added `notify_fired()` public wrapper
- `game/include/ahamkara/game/player.h` — updated ownership doc, added `notify_weapon_fired()` inline delegating to `WeaponRuntime::notify_fired()`
- `game/include/ahamkara/game/world.h` — added `notify_weapon_fired()` inline delegating to `Player::notify_weapon_fired()`
- `game/src/world_projectile.cpp` — called `world.notify_weapon_fired()` after `consume_ammo()` in `fire_projectile()` and `fire_hitscan()`
- `engine/render/include/ae/render/weapon_model_cache.h` — added Runtime Boundary doc section confirming read-only from gameplay

## What Changed

1. **WeaponRuntime seam hardened**: The class doc comment now explicitly lists all virtual extension points with their purpose, plus a "What Subclasses Must NOT Do" section prohibiting render/animation header inclusion. Added `reload_timer()` protected accessor and `on_fire()` virtual hook. Added `notify_fired()` public method that calls `on_fire()`.

2. **on_fire() wired through the stack**: `Player::notify_weapon_fired()` → `World::notify_weapon_fired()` → called in both `fire_projectile()` and `fire_hitscan()` after ammo consumption. The hook is live and ready for future weapon subclasses.

3. **Ownership comments clarified**: `Player` header now explicitly states it is the "sole owner" of weapon state and that presentation lives in `ae::render`. `WeaponModelCache` header now has a `## Runtime Boundary` doc section confirming gameplay code never accesses it directly.

## Validation Run

```
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

Build: success (46 targets, pre-existing entt deprecation warnings only)
Tests: 17/17 passed (100%)

## Known Gaps

- `on_fire()` hook is wired and tested at compile/runtime level but has no dedicated unit test — existing weapon behavior is unaffected (all hooks are no-op by default in the base class)

## Runtime Risks

- Low: all changes are additive (new hooks, new doc comments) and existing behavior is preserved. The `on_fire()` hook is called from existing firing paths and is a no-op by default.

## Cross-Agent Dependencies

- TASK-20260704-1010-weapon-fire-control can now subclass `WeaponRuntime` and override `on_fire()` for weapon-specific per-shot behavior
- TASK-20260704-1020-combat-hit-resolution can depend on the runtime seam being stable

## Recommended Next Step

Queue TASK-20260704-1010-weapon-fire-control — the runtime seam is now ready for weapon-specific fire modes, ammo types, and recoil/spread.

## Confidence

`high` — code changes are additive (new hooks, new doc comments) and all 17 tests pass.
