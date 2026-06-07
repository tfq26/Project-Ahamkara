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

## Runtime Boundary

The runtime should load engine-owned runtime formats, not editor-native project
files. Keep heavy import/parsing logic inside tools unless a source format is
explicitly accepted as a runtime format.

Near-term runtime formats:

- `.aemesh` for compiled model geometry, skin, and animation data.
- `.aetex` for compiled RGBA8 texture payloads.
- `.aemat` for compiled material scalars and texture asset references.
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

Future runtime formats:

- GPU-compressed textures such as KTX2/Basis.
- Packed sprite atlases with validated animation metadata.
- Material assets referencing textures and shader parameters.
- Level/scene assets compiled from editor-friendly source files.

## Subagent Rules

When extending the pipeline:

- Keep importer changes inside `tools/` unless adding a runtime loader.
- Document new manifest kinds in this file.
- Do not make `server` depend on render, audio, platform, or editor tooling.
- Do not add large third-party import SDKs without a design note.
- Prefer source-to-runtime conversion over loading editor formats directly in
  gameplay code.
