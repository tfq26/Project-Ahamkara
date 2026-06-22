# Repo Map

Status: Seed

This map is a high-level orientation layer. Use it to decide where to look next,
then verify details in source and canonical docs.

## Top-Level Areas

- `engine/` - Engine libraries and subsystems.
- `game/` - Gameplay-facing types, world simulation, modes, and game logic.
- `client/` - Playable client executable, local play, debug UI, and input.
- `server/` - Dedicated server executable.
- `wish/` - Wish integration/runtime layer.
- `tools/` - Asset importer and support tooling.
- `tests/` - CTest-backed test targets.
- `scripts/` - Setup, build, test, and run helpers.
- `assets/` - Source and compiled game assets.
- `docs/` - Canonical architecture, workflow, and subsystem docs.
- `docs/vault/` - This Obsidian-compatible agent memory vault.

## Engine Areas

- `engine/core/` - Shared core utilities, config, logging, timing, jobs, and CLI
  helpers.
- `engine/network/` - Network transport, packets, clocks, and session-facing
  networking primitives.
- `engine/runtime/` - Runtime application scaffolding and camera/runtime helpers.
- `engine/render/` - Debug and PBR rendering, compiled assets, maps, text, and
  backend boundaries.
- `engine/collision/` - Collision world, traces, debug helpers, and backend
  integration.
- `engine/physics/` - Physics world layer.
- `engine/animation/` - Animation graph, IK, weapon animation, recoil, and
  render bridge.
- `engine/audio/` - Audio engine layer.

## Important Docs

- [docs/guides/building.md](../guides/building.md) - Configure, build, test, and run commands.
- [docs/systems/architecture.md](../systems/architecture.md) - Engine foundation overview.
- [docs/systems/networking.md](../systems/networking.md) - Server-authoritative networking.
- [docs/systems/asset_pipeline.md](../systems/asset_pipeline.md) - Asset importer and runtime
  asset boundary.
- [docs/guides/remote-agent-workflow.md](../guides/remote-agent-workflow.md) - Multi-agent
  remote workspace rules.
- [docs/reports/subagents/master_summary.md](../reports/subagents/master_summary.md)
  - Historical subagent summary.

## Common Commands

Configure:

```sh
./scripts/setup-dev.sh
./scripts/setup-dev.sh --preset debug-headless
```

Build:

```sh
cmake --build --preset debug
cmake --build --preset debug-headless
```

Test:

```sh
./scripts/run-tests.sh
./scripts/run-tests.sh --preset debug-headless
```

Run:

```sh
./scripts/start.sh
./scripts/start.sh network
./scripts/start.sh sandbox
```

## Navigation Rule

Use this map to find the right neighborhood. Before changing behavior, inspect
the relevant source, tests, and CMake target definitions.
