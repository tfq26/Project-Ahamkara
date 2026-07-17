# Wish architecture

Status: Target boundary plus transitional implementation map

## Role

Wish is an independent backend/session/activity platform. It owns identity,
admission, sessions, activities, replication/service envelopes,
administration, and backend integrations. It does not own rendering, game
simulation, Flashback commands, or Ahamkara runtime behavior.
[src: user:taufeeqali:2026-07-13: requested Wish as a completely separate repository and project]

## Target dependency rule

Wish builds, tests, and packages without Ahamkara or Flashback. Flashback
consumes Wish's versioned SDK/protocol and owns translation between Wish
session/activity data and Flashback game state. See
[the repository split](../architecture/repository-split.md).

## Transitional modules

| Current area | Intended responsibility |
|---|---|
| `wish/core` | Activity interfaces, manager/loader, identity, and small shared contracts |
| `wish/net` | Wish transport-facing configuration |
| `wish/session` | Session state and lifetime |
| `wish/replication` | Game-neutral authoritative replication envelopes |
| `wish/admin` | Operational/admin contracts and server surface |
| `wish/integrations/nakama` | Nakama-specific translation and transport |

The current `wish_engine` target builds core, network, session-model,
replication, admin-command, and Nakama sources and publicly links Ahamkara core
and networking. [src: file: wish/CMakeLists.txt:1-29] That dependency is
transitional and must be removed for repository independence.

The current session runtime directly imports `ahamkara/game/net_types.h` and
accepts a Flashback-oriented player command. This violates the target boundary.
[src: file: wish/session/session_runtime.h:1-6]
[src: file: wish/session/session_runtime.h:76-86]

## Invariants

- Public Wish headers use Wish-owned or declared third-party types only.
- Game commands and snapshots are opaque/versioned at the Wish boundary.
- Backend-specific objects remain inside their adapter.
- Authentication fails closed outside an explicit development mode.
- Remote errors use the `WS` namespace and a safe wire envelope; diagnostic
  detail stays server-side. See
  [the error-system proposal](../design/error-system.md).
- Every public protocol change has compatibility tests and a version rule.

## Extraction gates

- no repository-root private include path;
- no `ahamkara/game/*` includes;
- independent build, test, install, and consumer fixture;
- service adapters tested through interfaces/fixtures;
- Flashback integration proven from released packages rather than sibling
  source paths.

Mutable implementation status belongs in
[GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
