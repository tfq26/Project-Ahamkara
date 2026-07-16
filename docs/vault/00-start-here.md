# Start here

Status: Active

## Project direction

The current checkout is a transitional monorepo containing Ahamkara engine,
Flashback game/client/server code, and Wish backend/session code. The accepted
destination is three independent repositories and projects.
[src: user:taufeeqali:2026-07-13: explicit three-repository direction]

Read:

1. [Architecture overview](../architecture/overview.md)
2. [Repository split](../architecture/repository-split.md)
3. [Repository map](../repo-map.md)
4. [Build guide](../guides/building.md)
5. [Maintenance guide](../guides/maintenance.md)
6. [Current state](memory/current-state.md)

## Work and design

- Search or update
  [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues) for mutable
  work state.
- Put system-wide boundaries in `docs/architecture/`.
- Put proposed mechanisms in `docs/design/` with an explicit status.
- Put current subsystem truth in `docs/systems/`.
- Put meaningful decisions in [[memory/decision-log]].
- Use reports only for historical evidence and handoff context.

Do not recreate the retired file-backed task queue.
