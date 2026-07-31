# API Reference

This section documents the public API surface of Ahamkara, Flashback, and Wish.

## Ahamkara Engine API

| Module | Header | Description |
|--------|--------|-------------|
| Core | `<ae_core/...>` | Utilities, configuration, logging, diagnostics |
| Runtime | `<ae_runtime/...>` | Lifecycle, scheduling, clock, host contracts |
| Network | `<ae_network/...>` | Transport, sequence/ack, interpolation |
| Platform | `<ae_platform/...>` | OS abstraction, window, device input |
| Render | `<ae_render/...>` | GPU backends, resources, rendering pipeline |
| Animation | `<ae_animation/...>` | Pose evaluation, state machines, IK |
| Audio | `<ae_audio/...>` | Playback, mixing, streaming |
| UI | `<ae_ui/...>` | Widgets, menu primitives |
| Physics | `<ae_physics/...>` | Physics simulation, collision queries |
| Collision | `<ae_collision/...>` | Collision detection, broadphase |

## Wish Backend API

| Area | Description |
|------|-------------|
| Sessions | Match lifecycle, player join/leave |
| Activities | Game-mode runtime, tick dispatch |
| Replication | State sync, delta compression |
| Admin | HTTP admin server, health, metrics, heartbeat |

## Flashback Game API

| Area | Description |
|------|-------------|
| Gameplay | World simulation, game modes, weapons, AI |
| Presentation | Visual effects, HUD, camera management |
| Config | Product configuration, input bindings |

Detailed API documentation for each module is maintained alongside the source
code in the respective header files under `engine/*/include/`, `wish/include/`,
and `game/include/`.
