# Networking Map

Status: Seed

Use this map before changing server-authoritative networking, packets, clocks,
or client/server session behavior.

## Canonical Docs

- [Networking](../../systems/networking.md)
- [Remote agent workflow](../../guides/remote-agent-workflow.md)
- [Packet sequencing report](../../reports/subagents/packet-sequencing-and-acks.md)
- [FPS netcode tooling report](../../reports/subagents/fps_netcode_tooling.md)

## Main Areas

- `engine/network/`
- `game/include/ahamkara/game/net_packets.h`
- `game/include/ahamkara/game/net_types.h`
- `client/src/headless_clients.cpp`
- `server/src/dedicated_server_main.cpp`
- `tests/src/network_smoke_tests.cpp`

## Agent Checks

- Verify packet/schema changes against both client and server paths.
- Call out compatibility risks when packet formats, sequence handling, or
  snapshot contents change.
- Prefer headless tests for remote validation unless runtime client behavior is
  part of the task.

## Related

- [Known traps](../memory/known-traps.md)
- [Build and test map](build-and-test-map.md)
