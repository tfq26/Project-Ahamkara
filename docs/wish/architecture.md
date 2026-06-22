# Wish Engine Architecture

## Role

Wish Engine is the thin gameplay/backend scaffold for Ahamkara-adjacent systems that need to stay separate from the existing Ahamkara engine. It exists to host server-oriented state, replication, session handling, admin hooks, and service integrations without pulling those concerns into the render/game runtime.

## Module boundaries

| Module | Responsibility |
|---|---|
| `wish/core` | Shared identity, versioning, and small cross-cutting primitives. |
| `wish/net` | Transport-facing configuration and wire-level helpers. |
| `wish/session` | Session lifetime, connection state, and player/session ownership data. |
| `wish/replication` | Authoritative snapshot and delta replication concepts. |
| `wish/admin` | Admin and operational command descriptors. |
| `wish/integrations/nakama` | Adapter boundary for Nakama-specific calls and data translation. |

## Separation from Nakama

Nakama is treated as an external backend, not the engine core. The `wish/integrations/nakama` area is the only intended contact point:

- engine code should not depend on Nakama types outside the integration boundary
- session and replication state stay expressed in Wish-native types
- transport and admin behavior remain usable even if the backend changes later

This keeps the project portable if the backend is replaced or if only part of the stack uses Nakama.

## Future evolution path

1. Keep the current headers as stable contracts.
2. Expand each module with explicit interfaces before adding implementation detail.
3. Add backend adapters beside `nakama` instead of threading backend logic through core modules.
4. Add tests and fixtures around replication/session behavior as soon as state shape stabilizes.
5. Promote shared utilities into `wish/core` only when two or more modules need them.

## Extension points

- `wish/core`: engine identity, feature flags, shared constants.
- `wish/net`: packet framing, address handling, protocol version checks.
- `wish/session`: join/leave lifecycle, ownership, auth state.
- `wish/replication`: snapshot builders, dirty tracking, authoritative sync.
- `wish/admin`: remote admin verbs and operational control surface.
- `wish/integrations/nakama`: API translation layer only; no game rules.

The code in this tree is intentionally thin so later agents can fill in behavior without first undoing a large prototype.
