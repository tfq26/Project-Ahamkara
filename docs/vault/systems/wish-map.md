# Wish Map

Status: Seed

Use this map before changing Wish protocol, runtime, local run, test client, or
Nakama integration boundaries.

## Canonical Docs

- [Wish architecture](../../wish/architecture.md)
- [Wish local run](../../wish/local_run.md)
- [Wish protocol](../../wish/protocol.md)
- [Wish session runtime](../../wish/session_runtime.md)
- [Wish test client](../../wish/test_client.md)
- [Nakama boundary](../../wish/nakama_boundary.md)

## Main Areas

- `wish/`
- `wish/include/wish/`
- `wish/core/src/`
- `wish/integrations/nakama/`
- `tools/wish-test-client/`
- `tests/src/nakama_bridge_tests.cpp`

## Agent Checks

- Keep protocol/runtime docs synchronized with code when changing behavior.
- Be explicit about local-only behavior versus external integration behavior.
- Nakama boundary work should name what is mocked, real, or not validated.

## Related

- [Build and test map](build-and-test-map.md)
- [Known traps](../memory/known-traps.md)
