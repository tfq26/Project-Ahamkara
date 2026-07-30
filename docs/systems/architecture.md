# Current engine subsystem composition

Status: Transitional monorepo implementation

The canonical system-wide boundary and target repository model are documented
in [the architecture overview](../architecture/overview.md). The layered
architecture diagram is at [architecture-overview.svg](../../assets/architecture-overview.svg).
This file is a compact map of current engine modules.

## Engine targets

| Target | Current responsibility |
|---|---|
| `ae_core` | Config, console, crash handling, diagnostics, file watching, allocation, pacing, jobs, logging, memory budgets, telemetry, and time [src: file: engine/core/CMakeLists.txt:1-15] |
| `ae_runtime` | Application mode/lifecycle, camera, runtime metrics, and performance logging [src: file: engine/runtime/CMakeLists.txt:1-13] |
| `ae_network` | UDP transport and network timing/reliability/interpolation primitives [src: file: engine/network/CMakeLists.txt:1-21] |
| `ae_platform` | Window and platform-device integration [src: file: engine/platform/CMakeLists.txt:1-16] |
| `ae_render` | Compiled resources, scene/debug/PBR rendering, effects, text, and OpenGL backend [src: file: engine/render/CMakeLists.txt:1-29] |
| `ae_animation` | Animation types, graphs, drivers, character/weapon state, recoil, IK, and render bridge [src: file: engine/animation/CMakeLists.txt:1-12] |
| `ae_input` | Input mapping/runtime [src: file: engine/input/CMakeLists.txt:1-6] |
| `ae_audio` | Audio runtime [src: file: engine/audio/CMakeLists.txt:1-6] |
| `ae_ui` | UI/menu/HUD runtime [src: file: engine/ui/CMakeLists.txt:1-17] |
| `ae_collision` | Collision world, shapes, traces, layers, and debug support [src: file: engine/collision/CMakeLists.txt:1-17] |
| `ae_physics` | Physics-world wrapper [src: file: engine/physics/CMakeLists.txt:1-14] |

## Current composition outside the engine

- `ahamkara_game` contains current gameplay and concrete activities and links
  `wish_engine`. [src: file: game/CMakeLists.txt:1-49]
- `ahamkara_client_lib` composes engine modules with `ahamkara_game`.
  [src: file: client/CMakeLists.txt:1-42]
- `flashback` is currently a launcher over that client library.
  [src: file: samples/flashback/CMakeLists.txt:1-13]
- `ahamkara_server` composes runtime/network/game/Wish into a dedicated-server
  process. [src: file: server/CMakeLists.txt:1-18]

These are transition facts, not endorsed final boundaries. See
[the repository split](../architecture/repository-split.md).

## Known boundary defects

- `ae_render` and `ae_animation` previously formed a target cycle (resolved via ae_skeleton).
  [src: file: engine/render/CMakeLists.txt:47-55]
  [src: file: engine/animation/CMakeLists.txt:21-24]
- Install rules name graphical, game, and Wish targets in the core library set
  regardless of whether all targets were built.
  [src: file: cmake/InstallRules.cmake:12-40]
- The public runtime `Application` currently exposes only mode, start,
  shutdown, and running state; the actual frame orchestration remains in the
  concrete client pipeline.
  [src: file: engine/runtime/include/ae/runtime/application.h:5-25]
  [src: file: client/include/ahamkara/client/client_frame_pipeline.h:37-124]

Executable repair work is tracked in
[GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
