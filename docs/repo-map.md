# Repository map

Status: Transitional monorepo, audited 2026-07-13

This map describes where code lives today. The destination ownership is defined
in [architecture/repository-split.md](architecture/repository-split.md).

## Build composition

The root project uses CMake and C++20. [src: file: CMakeLists.txt:1-19] It
always adds core, collision, physics, network, runtime, Wish, and game targets;
graphical, server, sample, and test areas are controlled by root options.
[src: file: CMakeLists.txt:104-145]

## Top-level areas

| Path | What it does now | Destination |
|---|---|---|
| `engine/` | Reusable static libraries for core, runtime, networking, platform, rendering, animation, input, audio, UI, physics, and collision | Ahamkara |
| `game/` | Current world simulation, gameplay types, weapons, AI, modes, and concrete activities | Flashback |
| `client/` | Shared client loop, local/network play, presentation, debug frontend, config, and executable | Split generic host to Ahamkara; product behavior to Flashback |
| `server/` | Dedicated-server executable that composes Ahamkara networking, Wish, and concrete game activities | Flashback composition plus Wish services |
| `wish/` | Session/activity/replication/admin/backend implementation | Wish |
| `samples/flashback/` | Thin Flashback launcher over `ahamkara_client_lib` | Flashback |
| `assets/` | Game activities, levels, materials, models, textures, menus, and compiled outputs | Mostly Flashback; reusable format fixtures may remain in Ahamkara |
| `tools/` | Controller mapper, asset importer, diagnostics tool, generators, and Wish test client | Split by the product whose contract the tool exercises |
| `tests/` | CTest executables spanning engine, game, client, server, and Wish behavior | Split with their owning code and add package-consumer tests |
| `scripts/` | Configure, build, test, and run wrappers | Split or replace with repository-specific scripts |
| `cmake/` | Install and packaging rules | Ahamkara initially; product-specific packaging moves with each product |
| `docs/` | Architecture, designs, systems, maintenance, operations, and historical reports | Split with document ownership |

Engine modules are separate CMake libraries named `ae_core`, `ae_runtime`,
`ae_network`, `ae_platform`, `ae_render`, `ae_animation`, `ae_input`,
`ae_audio`, `ae_ui`, `ae_physics`, and `ae_collision`.
[src: file: engine/core/CMakeLists.txt:1]
[src: file: engine/runtime/CMakeLists.txt:1]
[src: file: engine/network/CMakeLists.txt:1]
[src: file: engine/render/CMakeLists.txt:1]

## Primary entry points

| Program | Entry point | Target |
|---|---|---|
| Current client | `client/src/main.cpp` | `ahamkara_client` [src: file: client/CMakeLists.txt:44-53] |
| Dedicated server | `server/src/dedicated_server_main.cpp` | `ahamkara_server` [src: file: server/CMakeLists.txt:1-14] |
| Flashback sample | `samples/flashback/src/main.cpp` | `flashback` [src: file: samples/flashback/CMakeLists.txt:4-13] |
| Asset importer | `tools/asset_importer.cpp` | `ahamkara_asset_importer` [src: file: tools/CMakeLists.txt:8-18] |
| Diagnostics CLI | `tools/diagnostics/diagnostics_tool.cpp` | `ahamkara_diagnostics` [src: file: tools/CMakeLists.txt:50-55] |
| Wish test client | `tools/wish-test-client/main.cpp` | `wish_test_client` [src: file: tools/wish-test-client/CMakeLists.txt:1-15] |

## Engine module placement

Every engine module keeps its public headers under
`engine/<module>/include/` and implementation under `engine/<module>/src/`.
The module CMake target publishes the include directory when the types are part
of its API; for example, `ae_core` exposes `engine/core/include`.
[src: file: engine/core/CMakeLists.txt:17-21]

Use the owning module rather than a generic utility folder:

- lifecycle, clocks, scheduling, config, logging, diagnostics: `engine/core`
  or `engine/runtime`;
- transport, sequence/ack, interpolation, network clock: `engine/network`;
- body ownership and queries: `engine/physics` or `engine/collision` according
  to the public contract;
- GPU/backend/resource code: `engine/render`;
- pose evaluation, state machines, IK: `engine/animation`;
- OS window/device input: `engine/platform` and `engine/input`;
- playback/mixing primitives: `engine/audio`;
- engine-owned widgets and menu primitives: `engine/ui`.

## Tests

CTest targets are declared in `tests/CMakeLists.txt`; the file contains engine,
game, networking, asset, telemetry, crash, diagnostics, and runtime-boundary
executables. [src: file: tests/CMakeLists.txt:1-445] Add a regression test to the
owning target or create a narrowly named target when no existing suite owns the
boundary.

## Generated and runtime output

Do not edit or commit build output, compiled assets, or logs. The repository
ignores `/build/`, `/assets/compiled/`, and `logs/`.
[src: file: .gitignore:1-6]

The root `AGENTS.md` contains a generated facts section; fix its generator or
source rather than editing that section manually.
[src: file: AGENTS.md:5-8]
