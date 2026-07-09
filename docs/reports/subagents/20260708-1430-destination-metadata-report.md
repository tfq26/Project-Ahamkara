# Destination Metadata — Implementation Report

## Scope

Add destination metadata for regions, landing zones, and ambient population so large spaces can be authored as destinations. Complements the SpatialGrid (1400) and ResidencyManager (1410) foundation.

## Status

**Complete** — 18/18 test suites pass including new `ahamkara_destination_metadata_tests` (13/13).

## Implemented

### 1. New header: `game/include/ahamkara/game/destination_metadata.h`

Pure-data types (no runtime logic, no platform/rendering deps):

| Type | Purpose |
|------|---------|
| `RegionType` | Enum: Combat, Social, Exploration, Boss, Dungeon, Hub, Transition |
| `RegionBounds` | Axis-aligned world-space bounding volume (x/z/y) |
| `LandingZoneDefinition` | Spawn point with position, yaw, primary/unlock flags |
| `AmbientPopulationSpawn` | NPC spawn params: npc_id, min/max count, respawn time, radius, group |
| `RegionDescriptor` | Named zone within a destination: bounds, type, level range, population references, landing zone references |
| `DestinationMetadata` | Top-level descriptor: world bounds, grid config, region array, landing zones, default population |
| `is_valid_region_type()` | Compile-time validation helper |

### 2. Modified: `game/include/ahamkara/game/worlds/world_definition.h`

Added `const DestinationMetadata* destination {nullptr}` to `WorldDefinition`. When null (default), existing level path behaviour is unchanged.

### 3. New test suite: `tests/src/destination_metadata_tests.cpp`

13 tests covering:
- Standard layout traits for all structs
- Default values for every type
- Static definition with 3 regions, 2 landing zones, 2 population entries
- Roundtrip field access on all types
- WorldDefinition integration (null default, pointer assignment)
- `is_valid_region_type()` validation

### 4. `tests/CMakeLists.txt`

Added `ahamkara_destination_metadata_tests` target (links `ahamkara_game`).

## Files Changed

| File | Change |
|------|--------|
| `game/include/ahamkara/game/destination_metadata.h` | **New** — all metadata types |
| `game/include/ahamkara/game/worlds/world_definition.h` | Added optional destination pointer |
| `tests/src/destination_metadata_tests.cpp` | **New** — 13 tests |
| `tests/CMakeLists.txt` | Added test target |

## Interfaces / Contracts

### `WorldDefinition::destination`

- Type: `const DestinationMetadata*`
- Default: `nullptr`
- When null: existing level path works unchanged
- When set: carries region, landing zone, and population metadata for gameplay/streaming systems

All string fields are `const char*` pointers to static/compiled data — never owned or freed by these structs.

## Tests / Validation

18/18 built test suites pass:

| Test suite | Status |
|-----------|--------|
| `ahamkara_destination_metadata_tests` | **Pass** (13/13, new) |
| `ahamkara_world_tests` | Pass |
| `ahamkara_streaming_residency_tests` | Pass (12/12) |
| `ahamkara_movement_tests` | Pass |
| `ahamkara_collision_tests` | Pass |
| `ahamkara_session_tests` | Pass |
| `ahamkara_utility_tests` | Pass |
| `ahamkara_core_tests` | Pass |
| `ahamkara_gameplay_tests` | Pass |
| `ahamkara_player_movement_controller_tests` | Pass |
| `ahamkara_logging_tests` | Pass |
| `ahamkara_console_tests` | Pass |
| `ahamkara_file_watcher_tests` | Pass |
| `ahamkara_nav_grid_tests` | Pass |
| `ahamkara_ai_combatant_tests` | Pass |
| `ahamkara_encounter_scripting_tests` | Pass |
| `ahamkara_reliable_channel_tests` | Pass |
| `ahamkara_weapon_loader_tests` | Pass |

Build command: `cmake --build --preset debug` (GUI config on macOS/AppleClang).

## Pre-existing Issues (not caused by this slice)

- `ahamkara_nakama_bridge_tests` and `ahamkara_server` targets fail to compile due to pre-existing syntax error (`|};` pipe character) in `tests/src/nakama_bridge_tests.cpp:72` and a `close()` namespace issue in `wish/admin/admin_server.h:70`.
- `ahamkara_smoke_tests`, `ahamkara_playtest_harness_tests` (depend on render targets), and `ahamkara_lod_batching_tests`, `ahamkara_level_render_tests`, `ahamkara_asset_pipeline_tests` were not built (require `ae_render` target, not available in headless mode).
- `ahamkara_window_input_provider_tests` requires `ae_platform` (skipped in headless).

## Assumptions

- Destination metadata is pure data (no runtime logic) — actual runtime spawning/streaming integration will use these types when the destination system is wired in.
- The `const char*` string convention matches existing patterns in `MapDefinition`, `WorldDefinition`, `InteractionTargetDefinition`.
- The null-default on `WorldDefinition::destination` ensures backward compatibility with all existing world definitions.

## Risks

- String fields use raw `const char*` pointers — callers must ensure pointed-to data outlives the struct. Matches existing codebase convention.
- Region bounds are axis-aligned; future work may require oriented or polygonal bounds for non-rectangular regions.
