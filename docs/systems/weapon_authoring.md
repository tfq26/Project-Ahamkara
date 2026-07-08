# Weapon Authoring

This doc defines the contract for first-person weapon art and animation in
Ahamkara. It is the target shape for the AR-15 baseline and the pattern future
AR variants should follow.

## Goal

Weapons should be built as modular, data-driven assets:

- a stable base weapon body
- interchangeable attachments
- animated mechanical sub-parts
- effect hooks for smoke, heat, muzzle flash, and future overheat states

The engine can already render a viewmodel and drive a first test animation path.
The authoring goal is to make future AR variants interchangeable without
rewriting the renderer or hardcoding one-off special cases.

## Canonical Conventions

- Authored barrel direction: `+X`
- Up axis in Blender: `+Y`
- glTF export setting: `export_yup = True`
- Runtime view-space conversion: `-90° Y`
- Matrix convention: column-major, post-multiplied

These conventions match the current viewmodel orientation contract in
[asset_pipeline.md](asset_pipeline.md) and
`game/include/ahamkara/game/weapon_registry.h`.

## Required Weapon Parts

The AR baseline should be authored as separate pieces, not as one baked mesh:

- `body` - receiver, stock, main frame
- `barrel` - removable barrel assembly
- `optic_mount` - base mount or rail segment
- `optic` - scope / sight attachment
- `magazine` - detachable magazine or clip
- `bolt_group` - bolt, hammer, charging handle, or equivalent moving assembly
- `muzzle_device` - optional compensator / flash hider / suppressor slot

If a part needs to move during reload, it should be its own object with a clear
origin and pivot.

## Future State Layers

These are the visual state hooks the engine expects to support:

- `reload` - magazine removed and replaced, with mechanical action
- `fire` - recoil, muzzle flash, slide/bolt motion
- `aim` - optic alignment / ADS pose
- `heat` - smoke, heat shimmer, color shift, emissive burn
- `overheat` - stronger smoke/emissive state, if gameplay later supports it

Do not bake these effects into the base mesh. Keep them as separate geometry,
materials, or animation-driven effects so they can be adjusted independently.

## Attachment Rules

- Attachments should be swappable by slot.
- Every attachment needs a stable socket name and pivot.
- Attachments should inherit the weapon's root transform, not author-space world
  transforms.
- A future scope should not require rebuilding the base AR.
- A future barrel should not change the magazine or optic pivots.

Suggested slot names:

- `optic`
- `barrel`
- `magazine`
- `muzzle`
- `underbarrel`

## AR Socket Map

This is the current starter map for the AR baseline. It is the contract that
future AR variants should preserve even if the visual design changes.

Local-space anchor points are approximate and should be kept stable across
variants:

- `socket_root` at `[0.00, 0.00, 0.00]` - weapon origin and parent for all
  attachments.
- `socket_barrel` at `[0.28, 0.03, 0.00]` - barrel assembly attach point.
- `socket_muzzle` at `[0.93, 0.03, 0.00]` - muzzle device / suppressor slot.
- `socket_optic_mount` at `[0.32, 0.095, 0.00]` - top rail or mount base.
- `socket_optic` at `[0.34, 0.105, 0.00]` - sight or scope attachment point.
- `socket_magazine` at `[0.05, -0.16, 0.00]` - removable magazine / clip.
- `socket_bolt_group` at `[-0.04, 0.085, 0.00]` - charging handle / bolt
  carrier motion.
- `socket_hammer` at `[-0.03, -0.03, 0.00]` - optional internal motion point
  for cutaway or exposed-mechanism variants.
- `socket_smoke` at `[0.90, 0.03, 0.00]` - muzzle smoke / vent effect emitter.

Ownership rules:

- `body` stays static.
- `barrel`, `optic`, `muzzle`, and `magazine` are swap candidates.
- `bolt_group`, `hammer`, and `charging_handle` are animation candidates.
- `smoke` and `heat` remain effect layers, not baked geometry.

The artist-facing starter spec for this map lives at
[`tools/blender/weapons/meshes/ar15_modular_template.json`](../../tools/blender/weapons/meshes/ar15_modular_template.json).

## Animation Beats For The AR Baseline

The first production AR should support these visible beats:

- idle sway
- fire kick
- reload start
- magazine release
- magazine out
- magazine in
- bolt / charging action
- reload settle

The exact timing can be stylized, but the state machine should have room for
each beat. Reload should not be a single opaque snap if the mesh parts can be
animated separately.

## Blender Authoring Rules

- Keep transforms clean. Apply scale/rotation before export when practical.
- Keep pivots intentional. A moving part should rotate around the point the
  player expects.
- Use separate objects for detachable components.
- Keep mesh density reasonable for first-person use.
- Avoid unnecessary bevel complexity on tiny parts that will be close to the
  camera.
- Name objects by function, not by Blender default names.
- Preserve the `+X` barrel convention for every new AR frame.

## Smoke And Heat

Smoke and overheat effects should be planned as layered runtime effects:

- smoke should emit from the muzzle or vent location
- heat should be a shader/material state, not a mesh-only trick
- future overheat should be a gameplay value, not just a cosmetic toggle

That keeps the visual and gameplay systems aligned when overheat becomes real.

## How To Add A New AR Variant

1. Start from the AR baseline.
2. Keep the same socket names and barrel convention.
3. Swap attachments by slot rather than by rebuilding the whole gun.
4. Tune the reload/firing motion after the model is complete.
5. Verify the weapon still renders correctly in the current first-person path.

## What The Engine Currently Expects

The runtime can already consume a single first-person viewmodel transform for
the AR baseline. That makes this authoring model the right target even before
full per-part runtime attachment support lands.

The next engine step is to expose named sub-part transforms so the magazine,
bolt, optic, and barrel can be animated or swapped independently at runtime.

Runtime ownership should keep a shared weapon-viewmodel presentation cache alive
in the client presentation layer so future base-weapon code can request modular
parts by path without reloading or re-uploading them on every swap. The renderer
should consume the resolved `GpuModel*` for the active weapon rather than own
weapon asset lifetime itself.
