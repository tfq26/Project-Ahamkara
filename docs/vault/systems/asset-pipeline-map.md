# Asset Pipeline Map

Status: Seed

Use this map before changing importers, asset manifests, compiled formats,
materials, maps, or runtime asset loading.

## Canonical Docs

- [Asset pipeline](../../systems/asset_pipeline.md)
- [Build guide](../../guides/building.md)
- [AE level format report](../../reports/subagents/aelevel_format.md)

## Main Areas

- `assets/manifest.assets`
- `assets/`
- `tools/asset_importer.cpp`
- `tools/asset_importer_dispatch.*`
- `tools/asset_importer_level.*`
- `tools/asset_importer_pack.*`
- `engine/render/include/ae/render/compiled_*`
- `engine/render/src/compiled_*`
- `tests/src/asset_pipeline_tests.cpp`

## Agent Checks

- Importer changes should name expected generated outputs.
- Avoid committing large generated assets unless the project expects them.
- Renderer asset changes may need both importer tests and runtime/render checks.

## Related

- [Rendering map](rendering-map.md)
- [Known good commands](../memory/known-good-commands.md)
