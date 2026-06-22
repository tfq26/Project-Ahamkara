# Gameplay World Map

Status: Seed

Use this map before changing world simulation, gameplay state, movement,
projectiles, modes, items, weapons, or activity/world definitions.

## Canonical Docs

- [Architecture](../../systems/architecture.md)
- [Networking](../../systems/networking.md)
- [Gameplay types scaffolding report](../../reports/subagents/gameplay_types_scaffolding.md)
- [Movement system upgrade report](../../reports/subagents/movement_system_upgrade.md)

## Main Areas

- `game/include/ahamkara/game/`
- `game/src/`
- `client/include/ahamkara/client/local_play.h`
- `client/src/local_play.cpp`
- `tests/src/gameplay_tests.cpp`
- `tests/src/world_tests.cpp`
- `tests/src/local_play_tests.cpp`

## Agent Checks

- Simulation changes should be deterministic unless explicitly visual-only.
- Networked gameplay changes may require both gameplay and networking tests.
- Movement/camera behavior should distinguish compile validation from runtime
  feel.

## Related

- [Networking map](networking-map.md)
- [Known traps](../memory/known-traps.md)
