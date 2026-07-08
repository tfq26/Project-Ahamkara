# Weapon Authoring

This folder is the artist-facing source of truth for weapon frames and perk
data. For the engine-side contract that explains how the runtime consumes
weapon viewmodels, see [docs/systems/weapon_authoring.md](../../../docs/systems/weapon_authoring.md).

## Directory structure

```
tools/blender/
├── build_weapon.py        # General-purpose mesh builder (reads JSON → exports glTF)
├── weapons/
│   ├── meshes/            # Mesh geometry specs (one per archetype)
│   │   ├── ar15.json
│   │   ├── shotgun.json
│   │   ├── pistol.json
│   │   └── rocket_launcher.json
│   ├── archetypes.json    # Gameplay archetype definitions (base stats + perk slots)
│   └── perks.json         # Perk catalog (stat modifiers + gameplay effects)
```

## Mesh specs (`meshes/*.json`)

Define geometry only. One spec per visual archetype. See any file in `meshes/` for the format.

The current AR baseline is intentionally modular-friendly:

- `body` / `receiver` should stay stable across AR variants
- `barrel` should be a swappable part
- `optic` should be attachable without reauthoring the base gun
- `magazine` should be detachable for reload animation support
- `bolt` / `hammer` / `charging_handle` should be their own moving parts
- `muzzle` effects should remain separate from the base mesh
- the starter contract lives in `meshes/ar15_modular_template.json`

Build command:
```sh
blender -b -P tools/blender/build_weapon.py -- --spec tools/blender/weapons/meshes/ar15.json
```

## Archetypes (`archetypes.json`)

Defines the gameplay identity of a weapon frame. Each archetype specifies:
- Which mesh to use
- Base stats (damage, RPM, mag size, etc.)
- Which perk slots are available
- Slot type (Primary/Secondary/Heavy)

## Perks (`perks.json`)

Catalog of all possible perks. Each perk specifies:
- Which slot it fits into (barrel, magazine, trait1, trait2)
- Stat deltas (additive or multiplicative)
- Any special gameplay behavior flags

## Runtime weapon instances

At runtime, a weapon is an archetype + a list of selected perk IDs. The combined stats are computed by applying each perk's deltas to the archetype's base stats.

## Artist Notes For AR Weapons

When building a new AR frame or a future variant:

- Keep the barrel axis along `+X`.
- Keep pivots clean for the magazine, optic, barrel, and bolt group.
- Use a detachable magazine object instead of baking the mag into the receiver.
- Treat scopes as attachments, not part of the base gun.
- Leave room for heat/smoke/overheat effects around the muzzle and handguard.
- Do not rely on one mesh doing everything; the runtime is moving toward
  modular weapon parts.
- Prefer the socket names and pivot points documented in
  `docs/systems/weapon_authoring.md`.

## Recommended AR Part Names

Use names that make animation and export easier to reason about:

- `ar_body`
- `ar_barrel`
- `ar_optic_mount`
- `ar_optic`
- `ar_magazine`
- `ar_bolt_group`
- `ar_muzzle`
- `ar_hammer`
- `ar_smoke_socket`
