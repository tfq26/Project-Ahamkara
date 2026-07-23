# Systems

Canonical current-state subsystem docs for Ahamkara. System-wide boundaries
and migrations live in [`../architecture/`](../architecture/README.md); proposed
mechanisms live in [`../design/`](../design/README.md).

## System Docs

- [Building](building.md) — Build prerequisites, presets, and known platform-specific issues
- [Architecture](architecture.md)
- [Networking](networking.md)
- [Asset pipeline](asset_pipeline.md)
- [Audio architecture](audio_architecture.md)
- [Client config](client_config.md)
- [Renderer backend](renderer_backend.md)
- [Logging conventions](logging.md)
- [Weapon authoring](weapon_authoring.md)

## Cross-cutting operations

- [Maintenance and error repair](../guides/maintenance.md)
- [Error-system proposal](../design/error-system.md)
- [Error-code catalog](../operations/error-codes.md)

## Agent Maps

- [Build and test map](../vault/systems/build-and-test-map.md)
- [Networking map](../vault/systems/networking-map.md)
- [Rendering map](../vault/systems/rendering-map.md)
- [Gameplay world map](../vault/systems/gameplay-world-map.md)
- [Asset pipeline map](../vault/systems/asset-pipeline-map.md)
- [Wish map](../vault/systems/wish-map.md)

Do not add task status or issue bodies here. Link GitHub Issues for mutable
execution state.
