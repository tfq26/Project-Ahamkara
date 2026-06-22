# Phase 2B: Asset Importer Decomposition

## Date

2026-06-06

## Summary

Decomposed the 777-line monolithic `tools/asset_importer.cpp` into 7 focused
translation units, preserving all existing behavior, CLI contract, and asset
format compatibility.

## Files Changed

### New Files

| File | Purpose |
|------|---------|
| `tools/asset_importer_common.h` | Shared types (`ImportEntry`, `ImportStats`, `ImportedAssetRecord`, `AssetRecordMap`) and utility functions (`trim`, `parse_bool_token`, `parse_float_token`, `split_tokens`, `resolve_path`) |
| `tools/asset_importer_common.cpp` | Implementation of common utilities |
| `tools/asset_importer_manifest.h` | Manifest parsing declaration |
| `tools/asset_importer_manifest.cpp` | `read_manifest()` — parses `.assets` manifest files with comment stripping, tokenization, and path resolution |
| `tools/asset_importer_registry.h` | Registry/cache logic declaration |
| `tools/asset_importer_registry.cpp` | FNV-1a hashing, `compute_file_hash`, `make_asset_id`, `read_registry`/`write_registry`, `populate_record_identity`, `finalize_record_output`, `can_skip_import` |
| `tools/asset_importer_texture.h` | Texture import declaration |
| `tools/asset_importer_texture.cpp` | `load_tga_texture` (TGA header parsing, BGR→RGBA swizzle, top-left flip), `compile_texture` |
| `tools/asset_importer_material.h` | Material import declaration |
| `tools/asset_importer_material.cpp` | `load_material_source` (key=value parsing, scalar/texture properties), `compile_material` |
| `tools/asset_importer_dispatch.h` | Import dispatch/output declaration |
| `tools/asset_importer_dispatch.cpp` | `compile_model`, `copy_asset`, `import_entry` (dispatch table), `print_usage` |

### Modified Files

| File | Change |
|------|--------|
| `tools/asset_importer.cpp` | Reduced from 777 lines to 95 lines. Now contains only `main()` which orchestrates calls into the helper modules. Uses `using namespace asset_importer;` at file scope (the anonymous-namespace pattern was replaced with the named `asset_importer` namespace). |
| `tools/CMakeLists.txt` | Added 6 new `.cpp` source files to the `ahamkara_asset_importer` target. No new link dependencies required — all helpers use types already available through `ae_render`. |

## Decomposition Map

```
asset_importer.cpp (main)
  ├── asset_importer_common.h/.cpp      Types + utilities
  ├── asset_importer_manifest.h/.cpp    Manifest parsing
  ├── asset_importer_registry.h/.cpp    Hashing, registry I/O, cache skip logic
  ├── asset_importer_texture.h/.cpp     TGA loader + compile_texture
  ├── asset_importer_material.h/.cpp    Material source loader + compile_material
  └── asset_importer_dispatch.h/.cpp    compile_model, copy_asset, import_entry, print_usage
```

## Behavior Preservation

- The CLI contract (`--manifest <path>`, `--model <source> <output>`, `--help`)
  is unchanged.
- Manifest format (`<kind> <source> <output> [metadata]`) is unchanged.
- Asset registry format (TSV with 8 columns) is unchanged.
- All 6 asset pipeline tests pass:
  - `test_compiled_mesh_roundtrip`
  - `test_compiled_mesh_rejects_bad_magic`
  - `test_importer_writes_asset_registry`
  - `test_importer_skips_unchanged_asset`
  - `test_texture_import_roundtrip`
  - `test_material_import_roundtrip`

## Design Decisions

1. **Named namespace replaces anonymous namespace.** The original file used
   `namespace {}` for internal linkage. Since the code is now split across
   translation units, all symbols live in `namespace asset_importer`.

2. **Engine headers included in helper headers.** `asset_importer_texture.h`
   and `asset_importer_material.h` include the full `ae/render/compiled_*.h`
   headers rather than using forward declarations. Forward-declaring the
   `CompiledTextureFormat` enum with the wrong underlying type caused a
   compilation error, and the target already links `ae_render`, so the direct
   include is the correct approach.

3. **No new link dependencies.** All helpers only depend on types already
   provided by the `ae_render` library, which was already a link dependency
   of `ahamkara_asset_importer`.

4. **File size reduction.** `asset_importer.cpp` went from 777 lines to 95
   lines (88% reduction). The largest helper is `asset_importer_registry.cpp`
   at 247 lines.

## Future Work

- Texture/material loaders (`load_tga_texture`, `load_material_source`) could
  be moved into the engine library itself if they become reusable at runtime.
- The dispatch table in `import_entry()` could be replaced with a `std::map`
  or `std::unordered_map` of kind → function pointer for easier extension.
- Registry/cache logic could be generalized into a reusable asset database
  module shared by other tools.
