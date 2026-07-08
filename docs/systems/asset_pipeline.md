# Ahamkara Asset Pipeline

## Goal

Ahamkara should support assets made in existing game-development tools without
making the runtime depend on those tools or their native project formats.

Artists and designers can work in tools such as Blender, Maya, 3ds Max,
Substance, Aseprite, Photoshop, Tiled, and audio editors. The engine pipeline
then imports common interchange files into compact runtime assets.

## Directory Model

```text
assets/
├── manifest.assets       # Import manifest consumed by ahamkara_asset_importer
├── models/               # Current source model exports
└── compiled/             # Generated runtime-ready assets + asset_registry.tsv
```

`assets/compiled/` is generated output. Do not hand-edit compiled assets. If an
asset needs to change, edit the source file and run the importer again.

## Supported First-Slice Imports

| Kind | Source | Output | Current Behavior |
|------|--------|--------|------------------|
| `model` | glTF 2.0 `.gltf` plus external buffers | `.aemesh` | Compiles mesh, skin, and animation data loaded by the existing glTF loader |
| `texture` | Uncompressed `.tga` | `.aetex` | Compiles RGBA8 texture payloads for runtime loading |
| `material` | Simple text `.mat` source | `.aemat` | Compiles material scalars and texture asset references |
| `level` | Simple text `.lvl` source | `.aelevel` | Compiles world settings, spawn points, collision boxes, and mesh instance references |
| `sprite` | Sprite sheet image | Same extension | Passthrough copy, optional metadata copied beside output |
| `audio` | `.wav`, `.ogg` | Same extension | Passthrough copy |
| `data` | Engine-readable data file | Same extension | Passthrough copy |

The sprite, audio, and data asset kinds are intentionally simple. They
establish a stable manifest workflow now, while leaving room for future texture
compression, sprite-atlas packing, audio conversion, and metadata validation.

## Generated Registry

When you run the importer with `--manifest`, it also writes
`assets/compiled/asset_registry.tsv`.

The registry is the importer-owned index of compiled assets. Each row records:

- Stable `asset_id` derived from the compiled output path.
- Asset `kind`.
- Source, compiled output, and optional metadata paths.
- Source and metadata content hashes.
- Compiled output size in bytes.

This registry is the handoff point for future work such as incremental
rebuilds, runtime asset lookups, and editor tooling.

The importer now uses the registry as a simple build cache. If an entry's kind,
source path, output path, metadata path, and source hashes still match the last
successful import, and the compiled output still exists with the same size, the
importer skips rebuilding that asset.

## Manifest Format

Each non-empty line in `assets/manifest.assets` uses whitespace-separated
tokens:

```text
<kind> <source> <output> [metadata]
```

Examples:

```text
model models/test_box.gltf compiled/models/test_box.aemesh
texture textures/wall_albedo.tga compiled/textures/wall_albedo.aetex
material materials/wall.mat compiled/materials/wall.aemat
level levels/javelin4.lvl compiled/levels/javelin4.aelevel
sprite sprites/player.png compiled/sprites/player.png sprites/player.json
audio audio/rifle_fire.wav compiled/audio/rifle_fire.wav
```

Paths are relative to the manifest file unless they are absolute.

## Running The Importer

After configuring and building the client/tools targets:

```sh
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
```

Running the same manifest a second time should skip unchanged assets and only
rebuild entries whose source files or metadata changed.

You can also compile a single glTF model directly:

```sh
./build/debug/tools/ahamkara_asset_importer --model assets/models/test_box.gltf assets/compiled/models/test_box.aemesh
```

Material source files use a simple `key=value` format. Supported keys today:

- `base_color=r g b a`
- `metallic=value`
- `roughness=value`
- `emissive_color=r g b`
- `double_sided=true|false`
- `albedo_texture=asset/id`
- `normal_texture=asset/id`
- `orm_texture=asset/id`
- `emissive_texture=asset/id`

Level source files (`.lvl`) use `key=value` lines for world settings and
`[section]` headers for structured data. Supported world settings:

- `name=string` — level name
- `sky_color=r g b` — sky color (default 0.3 0.4 0.6)
- `ambient=r g b` — ambient light (default 0.05 0.05 0.1)
- `gravity=float` — gravity strength (default 20.0)
- `skybox_material=asset/id` — optional skybox material reference
- `ground_material=asset/id` — optional ground material reference

Section `[spawn]` — one spawn point per line:
```text
pos_x pos_y pos_z yaw [team=N]
```

Section `[collision]` — one axis-aligned box per line:
```text
min_x min_z max_x max_z top_y bottom_y [wall=true|false] [jump_through=true|false] [auto_step=true|false] [surface=N]
```

Section `[mesh]` — one mesh instance per line:
```text
mesh_id=asset/id [material_id=asset/id] [pos=x y z] [yaw=N] [pitch=N] [roll=N] [scale_x=N] [scale_y=N] [scale_z=N]
```

## JSON Level Spec (Path A Authoring)

Levels can be authored as a JSON spec and converted to `.lvl` with
`tools/levelgen/spec_to_lvl.py` — the "Path A" fast iteration loop from the
authoring-stack decision (`docs/vault/memory/decision-log.md`). The same spec is
the shared contract reused by the Blender generator (Path B).

```sh
python3 tools/levelgen/spec_to_lvl.py <spec.json> <out.lvl>
python3 tools/levelgen/spec_to_lvl.py --selftest <spec.json>   # emit + parse-back roundtrip check
```

Spec schema (kept 1:1 with `.lvl` capability):

```json
{
  "name": "My Level",
  "sky_color": [0.3, 0.4, 0.6],
  "ambient": [0.05, 0.05, 0.1],
  "gravity": 20.0,
  "skybox_material": "asset/id",
  "ground_material": "asset/id",
  "spawns": [{"pos": [0, 1.5, 0], "yaw": 0.0, "team": 1}],
  "collision": [{"min": [-10, -10], "max": [10, 10], "top_y": 0.0, "bottom_y": -0.5,
                 "wall": false, "jump_through": false, "auto_step": true, "surface": 0}],
  "meshes": [{"mesh": "asset/id-or-path", "material": "asset/id",
              "pos": [0, 1, 0], "yaw": 0.0, "pitch": 0.0, "roll": 0.0, "scale": [1, 1, 1]}]
}
```

Example specs live at `assets/levels/prototype_arena.json` (movement/collision)
and `assets/levels/prototype_box.json` (a `test_box` mesh showcase), with the
generated `.lvl` beside them and manifest entries that compile to `.aelevel`.

Note: mesh ids in `meshes` are resolved at runtime by the level render scene,
which currently tries the id as a literal path first (asset-registry resolution
is a follow-up). The example uses `assets/compiled/models/test_box.aemesh` so it
resolves from the repo root.

## Blender Generator (Path B Authoring)

The same JSON spec can be turned into blockout geometry + a `.blend` + glTF +
`.lvl` with the headless Blender generator `tools/blender/build_level.py`
(requires a Blender install; tested against Blender 4.x):

```sh
blender -b -P tools/blender/build_level.py -- <spec.json> <out_dir>
```

It reuses the Path A `.lvl` writer, so both paths emit identical `.lvl` for the
same spec. Collision volumes become greybox boxes; mesh instances become
placement empties (the `.lvl` carries the real mesh reference for the engine).
Generation is one-way (the spec owns layout/semantics; Blender owns geometry
detail; `.blend` edits do not round-trip back into the spec).

The pure, `bpy`-free logic (spec load, blockout plan, `.lvl` writing) is
unit-testable without Blender:

```sh
python3 tools/blender/test_build_level.py
```

## Viewmodel Generator

`tools/blender/build_viewmodel.py` is a Blender entrypoint for the first-person
rifle viewmodel. It builds the rifle from primitives, saves the `.blend`, and
exports glTF so the asset importer can compile the runtime `.aemesh`.

```sh
blender -P tools/blender/build_viewmodel.py -- --out_dir assets/models --open
```

`--out_dir` defaults to `assets/models`. Passing `--open` reloads the saved
`.blend` in the active Blender session after export.

### Viewmodel Orientation Contract

Weapon viewmodels follow a stable axis convention shared by the asset tool, the
importer, and the runtime renderer:

- **Authored axis**: Barrel runs along **+X** in Blender (Y-up world, glTF
  `export_yup = True`).
- **Renderer conversion**: The renderer applies a single **-90° Y rotation** to
  map +X barrel direction into view-forward / -Z. This conversion is in
  `engine/render/src/debug_renderer.cpp` and is shared by all weapons.
- **Per-weapon adjustments**: Pitch/yaw/roll offsets (degrees) are defined in
  `game/include/ahamkara/game/weapon_registry.h` (canonical) and mirrored in
  the renderer. Rotation order: barrel correction → yaw → pitch → roll.
- **Matrix convention**: Column-major 4x4 matrices (`ae::gl_compat::Mat4`),
  post-multiplied (`viewmodel = viewmodel * rotation`).

New weapons authored with the same +X barrel convention will render correctly
without per-weapon renderer hacks.

For the AR-specific modeling and animation contract, see
[Weapon authoring](weapon_authoring.md) and
[`tools/blender/weapons/README.md`](../../tools/blender/weapons/README.md).
The concrete starter AR socket map is in
[`tools/blender/weapons/meshes/ar15_modular_template.json`](../../tools/blender/weapons/meshes/ar15_modular_template.json).

## Runtime Boundary

The runtime should load engine-owned runtime formats, not editor-native project
files. Keep heavy import/parsing logic inside tools unless a source format is
explicitly accepted as a runtime format.

Near-term runtime formats:

- `.aemesh` for compiled model geometry, skin, and animation data.
- `.aetex` for compiled RGBA8 texture payloads.
- `.aemat` for compiled material scalars and texture asset references.
- `.aelevel` for compiled levels with world settings, spawn points, collision boxes, and mesh instance references.
- Copied sprite/audio files for early development.
- `asset_registry.tsv` for importer metadata and asset indexing.

Runtime model loading now uses `ae::render::CompiledMeshLoader`, which loads
`.aemesh` files back into the existing `GltfModel` shape consumed by the debug
renderer path. The importer writes `.aemesh` through the matching
`save_compiled_mesh` helper so tools and runtime agree on one binary layout.

Runtime texture loading now uses `ae::render::CompiledTextureLoader`, which
loads `.aetex` files into CPU-side RGBA8 texture assets for future renderer
upload paths. The importer currently supports uncompressed 24-bit and 32-bit
TGA sources for this first texture slice.

Runtime material loading now uses `ae::render::CompiledMaterialLoader`, which
loads `.aemat` files into CPU-side material descriptions. Material texture
properties are stored as asset IDs so future renderer/editor systems can look
them up through the asset registry instead of hardcoded file paths.

Runtime level loading now uses `ae::render::CompiledLevelLoader`, which loads
`.aelevel` files into CPU-side `LevelAsset` structs. Levels reference meshes
and materials by asset ID, define AABB collision boxes with surface material
tags and behavior flags (wall, jump-through, auto-step), and include spawn
points with team assignments. The format is independent of the game library;
game code converts `LevelAsset` into runtime simulation data.

Future runtime formats:

- GPU-compressed textures such as KTX2/Basis.
- Packed sprite atlases with validated animation metadata.
- Material assets referencing textures and shader parameters.

## Subagent Rules

When extending the pipeline:

- Keep importer changes inside `tools/` unless adding a runtime loader.
- Document new manifest kinds in this file.
- Do not make `server` depend on render, audio, platform, or editor tooling.
- Do not add large third-party import SDKs without a design note.
- Prefer source-to-runtime conversion over loading editor formats directly in
  gameplay code.
