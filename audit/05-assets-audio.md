# 05 — Audio & Asset Pipeline

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Rating: **2/5 — functional first slice, not an AI-ready end-to-end content pipeline**

## 1. Scope & Method

Read `audit/01-recon.md` first. Inspected importer CLI/manifest/registry/packing, compiled mesh/material/texture/level formats and loaders, glTF support, asset manifest and directory layout, renderer asset references, audio engine and client audio player, file watcher and UI hot reload. Reviewed the audit's recorded build/test outcome; did not rerun builds or tests because recon already attempted the dirty-tree headless build and reported failures. No source/config/assets were changed. Only this report was written.

Not examined: visual quality or license completeness of each individual asset, audio playback by ear, measured import performance, platform-specific audio device behavior, or GUI authoring UX. No importer-specific clean-room end-to-end run was performed; the shared working tree is heavily dirty and importer output mutates `assets/compiled`.

## 2. Executive Summary

- There is a real content compiler: manifest-driven model, TGA texture, text material and level conversion, incremental registry, and a package writer (`tools/asset_importer.cpp:30-124`). This is valuable infrastructure, but its runtime consumer and packaging contract are incomplete.
- The engine's direct glTF contract is internally inconsistent: the public header calls the loader minimal and documents no skin/animation/material support (`engine/render/include/ae/render/gltf_loader.h:96-106`), while the implementation/model structures contain partial skin and animation handling. An agent cannot infer a reliable supported-feature matrix from the public API.
- Import formats are narrow: TGA (uncompressed true-color 24/32-bit) only (`tools/asset_importer_texture.cpp:38-55,100-105`); model import advertises glTF 2.0, with external buffer limitations stated in the header. Sprite and audio “import” are file copies (`tools/asset_importer_dispatch.cpp:86-88,94-108`).
- The package writer emits `APKG` v1, but includes only model/material/texture and lacks level/data/sprite/audio entries (`tools/asset_importer_pack.cpp:38-57`). No package reader/runtime virtual filesystem was found in the inspected paths; runtime code loads paths directly. Thus packaging is not demonstrated as a deployable runtime path.
- Audio has two overlapping implementations. `ae::audio::AudioEngine` offers loaded IDs, looping, buses and spatial controls (`engine/audio/include/ae/audio/audio_engine.h:55-87`); legacy client `AudioPlayer` resolves only `hit` and `crit`, and unknown event keys silently do nothing (`client/src/audio_player.cpp:106-112,168-179`). Current game-generated keys include reload/ability/interaction/player_hit, so much authored feedback has no mapping.
- Hot reload applies to UI JSON/config, not compiled models, textures, levels or audio. The frame pipeline polls menu/HUD and file watcher (`client/src/client_frame_pipeline.cpp:839-844`); asset re-import/reload lifecycle is absent from that path.
- Metadata exists in several ad hoc forms (manifest optional sidecar, registry hashes, rig/animation/equipment JSON), but there is no unified discoverable catalog or validation workflow tying metadata to runtime assets.
- AI asset authoring today is possible for a narrow route: create supported source files, add a manifest row, run importer, then manually reference compiled paths. There is no robust sprite atlas, asset browser, recipe schema, package runtime, or automatic feedback loop for discovering unsupported/missing content.

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Asset importer CLI | `tools/asset_importer.cpp`, `tools/asset_importer_dispatch.cpp` | IMPLEMENTED | 3 | Manifest compile, direct model compile and pack commands. |
| Import manifest | `assets/manifest.assets`, `tools/asset_importer_manifest.cpp` | IMPLEMENTED | 2 | Space-token format; no quoting/escaping apparent; limited kinds. |
| Asset registry/incremental skip | `tools/asset_importer_registry.cpp` | IMPLEMENTED | 3 | FNV-1a source/metadata hashes and output-size check; TSV metadata. |
| Model importer/runtime loader | `engine/render/src/gltf_loader.cpp`, `compiled_mesh.cpp`, `tools/asset_importer_dispatch.cpp` | PARTIAL | 2 | glTF subset and compiled `.aemesh`; docs and implementation feature set disagree. |
| Texture importer/runtime loader | `tools/asset_importer_texture.cpp`, `engine/render/src/compiled_texture.cpp` | PARTIAL | 2 | TGA to RGBA8 `.aetex`; only one compiled format. |
| Material importer/runtime loader | `tools/asset_importer_material.cpp`, `engine/render/src/compiled_material.cpp` | PARTIAL | 3 | Text `.mat` to `.aemat`; narrow PBR data model. |
| Level importer/runtime loader | `tools/asset_importer_level.cpp`, `engine/render/src/compiled_level.cpp` | IMPLEMENTED | 3 | Text level source to binary; actual current manifest lists level entries. |
| Asset package writer | `tools/asset_importer_pack.cpp` | PARTIAL | 2 | Writes aligned APKG file; limited asset kinds; no demonstrated runtime reader. |
| Sprite/atlas pipeline | `tools/asset_importer_dispatch.cpp`, `engine/render` | MISSING | 1 | Sprite is copied through; no atlas packing/import/runtime abstraction identified. |
| Audio runtime | `engine/audio`, `client/src/audio_player.cpp` | PARTIAL | 2 | Two engines/adapters and event-key path; mismatch between game event names and legacy map. |
| Hot reload | `engine/core/src/file_watcher.cpp`, `engine/ui/src/{menu_system,hud_system}.cpp`, client pipeline | PARTIAL | 2 | Generic timestamp watcher and UI reload; no compiled asset reload. |
| Asset metadata/discovery | `assets/manifest.assets`, `assets/characters/*.json`, `assets/equipment/*.json`, compiled registry | PARTIAL | 2 | Multiple local conventions, no unified browser/catalog/API. |
| Procedural authoring | `tools/blender`, `client/src/audio_player.cpp` | PARTIAL | 2 | Blender generation scripts and two procedural WAV effects; no general procedural asset interface. |

## 4. Detailed Findings

### ASSET-001 — Package output has no demonstrated runtime loading path

- **Severity:** HIGH
- **Category:** missing feature / design flaw
- **Evidence:** `tools/asset_importer.cpp:119-123` auto-packs to `assets/compiled/assets.pkg`; pack code writes APKG v1 (`tools/asset_importer_pack.cpp:14-18`) and selects only `model`, `material`, `texture` (`:38-57`). Searches of engine/render/client runtime show direct filesystem loads such as `CompiledLevelLoader::load(path)` (`engine/render/src/compiled_level.cpp:326-365`) and fixed compiled paths (`client/src/main.cpp:110`). No APKG reader was found in the inspected runtime.
- **Impact:** Agents may successfully “build” a package that the game never consumes; deployable builds still rely on source-tree path layout and omit levels/audio/data from package.
- **Recommended fix:** Define a package format contract and runtime AssetStore/VFS reader first; include all runtime asset kinds, validate bounds/version/checksums, then make runtime loads go through asset IDs and test loose-file/package parity.
- **Effort:** L
- **Dependencies:** —

### ASSET-002 — Import support is too narrow for routine AI-created content

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** Texture decoder rejects color maps and all but uncompressed true-color TGA and only 24/32-bit (`tools/asset_importer_texture.cpp:38-55`); extension check limits sources to TGA (`:100-105`). Sprite/audio/data import is passthrough copy (`tools/asset_importer_dispatch.cpp:86-88`), and help calls audio copy a “first pipeline slice” (`:106-108`).
- **Impact:** An agent cannot use common PNG/JPEG/WebP audio formats or create a sprite workflow with packing/metadata; each format conversion is external and undocumented in the golden path.
- **Recommended fix:** Specify supported source formats and deterministic import recipes. Add common raster and audio decoders plus a sprite/atlas pipeline, with metadata for filtering, color space, pivots, clips and compression. Preserve source-to-output provenance in registry.
- **Effort:** L
- **Dependencies:** ASSET-001

### ASSET-003 — glTF public support contract is stale or misleading

- **Severity:** HIGH
- **Category:** docs / risk
- **Evidence:** `engine/render/include/ae/render/gltf_loader.h:99-106` says only positions/normals/indices and explicitly says no animations, skinning, materials or textures. The same header's `GltfModel` has skins and animations (`:87-94`), and compiled mesh loader reads meshes, skins and animations (`engine/render/src/compiled_mesh.cpp:366-369`).
- **Impact:** AI authors may generate unsupported content or avoid supported features. Reproducibility and debugging suffer because behavior is discoverable only by reading implementation and importer scripts.
- **Recommended fix:** Make an executable support matrix with fixtures for supported glTF extensions/accessor types, transforms, skins, animation interpolation, material channels and unsupported cases; align header, tool help and docs to tested behavior.
- **Effort:** M
- **Dependencies:** —

### ASSET-004 — Manifest silently skips missing sources and has fragile path syntax

- **Severity:** MEDIUM
- **Category:** bug / risk
- **Evidence:** Manifest parser tokenizes on whitespace and accepts only 3–4 tokens (`tools/asset_importer_manifest.cpp:36-49`), so paths with spaces have no quoting support. The importer loop warns and continues for absent sources (`tools/asset_importer.cpp:75-80`); the manifest shown includes many assets (`assets/manifest.assets`) and can still produce a successful exit if entries were skipped (`:127-128`).
- **Impact:** An AI can report a successful build while required assets are omitted, or fail on ordinary paths containing spaces. This weakens autonomous verification.
- **Recommended fix:** Use structured manifest syntax with explicit schema validation, required/optional semantics, aggregated errors, and nonzero exit on missing required inputs. Emit machine-readable per-entry results and test paths with spaces.
- **Effort:** M
- **Dependencies:** —

### ASSET-005 — Audio identifiers do not resolve consistently across game and client

- **Severity:** HIGH
- **Category:** bug / design flaw
- **Evidence:** `client/src/audio_player.cpp:106-112` resolves only `hit` and `crit`; unknown sound keys return empty and are silently skipped. Game emits keys including `reload`, `ability`, and `interaction` (`game/src/world.cpp:406,413,867-868`), and also `player_hit` (`:944-945`). Meanwhile the separate `ae::audio::AudioEngine` exposes file-path loading and spatial playback (`engine/audio/include/ae/audio/audio_engine.h:55-87`).
- **Impact:** Gameplay feedback events can be queued and appear wired while producing no sound. AI playtesting and feel iteration receive misleading signals; choosing between two audio APIs adds integration ambiguity.
- **Recommended fix:** Establish one public sound registry/event API mapping stable IDs to paths and bus/spatial metadata; report missing IDs loudly in development and validate all emitted event IDs against the registry in a test.
- **Effort:** M
- **Dependencies:** —

### ASSET-006 — Core compiled assets do not hot reload

- **Severity:** MEDIUM
- **Category:** missing feature
- **Evidence:** Frame pipeline calls only menu/HUD hot reload and generic watched-file polling (`client/src/client_frame_pipeline.cpp:839-844`). The menu/HUD reload implementations check JSON modification times (`engine/ui/src/menu_system.cpp:1264-1273`; `engine/ui/src/hud_system.cpp:119-127`). No compiled mesh/texture/material/level reload hook appears in those paths.
- **Impact:** Agents iterating on models and textures must restart the game and reconstruct state, slowing the primary generate-observe-edit loop.
- **Recommended fix:** Build asset dependency tracking from registry IDs to live resources; debounce file changes, import off-thread, validate, then swap resources on the render/audio thread while retaining prior valid asset on failure.
- **Effort:** L
- **Dependencies:** ASSET-001

### ASSET-007 — Asset metadata is fragmented and not an agent-facing catalog

- **Severity:** MEDIUM
- **Category:** design flaw / docs
- **Evidence:** Manifest supports one optional metadata path (`tools/asset_importer_manifest.cpp:36-49`); registry stores source/output/metadata paths and hashes (`tools/asset_importer_registry.cpp:101-110`). Asset-specific metadata separately appears as rig/animation/equipment JSON (e.g. `assets/characters/operative_human.rig.json`, `assets/characters/operative_human_rifle.animation.json`, `assets/equipment/operative_carbine_tp.sockets.json`). No unified query or validation command was identified.
- **Impact:** An AI cannot ask the engine what assets exist, their semantic role, dependencies, dimensions, tags, licenses, or how they can be used. It must grep filenames and hard-coded references.
- **Recommended fix:** Create a versioned asset descriptor/catalog with stable IDs, type, source/license, import settings, dependencies, semantic tags and validation status; expose CLI query/inspect and reference IDs in levels/game data.
- **Effort:** M
- **Dependencies:** ASSET-001, ASSET-002

### ASSET-008 — Procedural audio generation writes runtime files as a side effect

- **Severity:** LOW
- **Category:** design flaw / risk
- **Evidence:** Legacy client audio code synthesizes hit/crit sample vectors (`client/src/audio_player.cpp:51-77`), then writes missing `assets/hit.wav` and `assets/crit.wav` during `AudioPlayerImpl` construction (`:94-103,130-132`).
- **Impact:** First-run behavior depends on writable working-directory assets; generated content is not represented in the importer registry and can alter the checkout during ordinary launch. Procedural generation is not exposed as a reusable tool for AI-generated audio.
- **Recommended fix:** Generate deterministic samples in memory or as an explicit build/import step into a designated cache/output directory; surface output provenance and errors rather than silently ignoring write failures.
- **Effort:** S
- **Dependencies:** —

### ASSET-009 — Runtime path resolution depends on working directory

- **Severity:** MEDIUM
- **Category:** design flaw / risk
- **Evidence:** `client/src/main.cpp:110` selects a repository-relative compiled level path; debug client logs that execution should be from repo root for paths to resolve (`client/src/debug_client.cpp:120-124`). Audio searches current and up to four parent directories for `assets` (`client/src/audio_player.cpp:80-92`).
- **Impact:** Agent launches from a different cwd can get missing content despite successful build/import. This is a common failure in subprocess-driven autonomous iteration and packaged deployment.
- **Recommended fix:** Resolve content root from explicit runtime config/executable location or mount an AssetStore; print resolved root and fail startup with a structured missing-asset diagnostic.
- **Effort:** M
- **Dependencies:** ASSET-001

## 5. What's Good

- The manifest/registry flow hashes source and optional metadata and avoids recompiling unchanged outputs when source identity and output size match (`tools/asset_importer_registry.cpp:187-207,221-247`).
- Compiled formats have explicit magic/version checks and loader error strings (`engine/render/src/compiled_mesh.cpp:346-368`; `engine/render/src/compiled_texture.cpp:74-110`; `engine/render/src/compiled_level.cpp:336-365`).
- Blender scripts are present for repeatable model/rig/level construction and are wired into a directory with concrete outputs (`tools/blender/`).
- Audio engine API has named buses, looping and 3D spatial parameters (`engine/audio/include/ae/audio/audio_engine.h:11-20,34-52,63-87`), giving a basis for real gameplay audio.
- UI layout reload supports rapid iteration (`engine/ui/src/menu_system.cpp:1264-1273`; `engine/ui/src/hud_system.cpp:119-127`).

## 6. Open Questions for the Owner

- Is the APKG/package runtime path intended for shipping, or is it an abandoned experiment? The writer auto-runs after manifest import, but runtime path loading remains visible.
- Which audio API is canonical: `ae::audio::AudioEngine` or client `AudioPlayer`? Should gameplay events be asset IDs or file paths?
- Which initial content workflow matters most for the AI-native engine: 2D sprites/atlases, 3D glTF, or both equally?
- Are assets expected to be generated or imported by external tools (Blender/image/audio tools), with the engine handling validation and conversion, or should generation be native to the engine tooling?
- Which source licenses/attribution metadata must be mandatory in the asset catalog?

## 7. Handoff Notes for the Next AI Agent

- Start with `audit/01-recon.md` and its dirty-tree warning. The findings here describe the observed dirty checkout, not a clean upstream baseline.
- For model-format behavior, inspect `engine/render/src/gltf_loader.cpp`, `compiled_mesh.cpp`, and importer fixtures/scripts alongside the public header; the header's support summary is stale relative to the data structures and compilation path.
- `ahamkara_asset_importer --manifest assets/manifest.assets` writes compiled outputs, `assets/compiled/asset_registry.tsv`, and `assets/compiled/assets.pkg`; do not rerun casually because it mutates existing generated assets.
- A missing-source warning currently does not itself fail the overall importer result. Check the per-entry output and registry before treating exit 0 as complete import.
- New audio event names need an explicit mapping in `client/src/audio_player.cpp`; otherwise they silently produce no sound. Cross-check all `AudioEvent` call sites.
- Keep audit changes restricted to `/audit`; do not repair pipeline problems during the audit task.
