# Ahamkara documentation

This directory is the durable knowledge base for the current transitional
monorepo and the planned Ahamkara, Flashback, and Wish repository split.

## Work tracking

Mutable work belongs in [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
The canonical architecture/backlog tracker during the split is
[#61](https://github.com/tfq26/Project-Ahamkara/issues/61).

Do not create local issue cards, queue-task files, status dashboards, or copies
of GitHub issue bodies under `docs/`. Documentation may link to an issue for
execution status, but it should explain durable architecture, operation, or
rationale independently of that issue.

## Start here

1. [Architecture overview](architecture/overview.md) — current composition,
   target boundaries, and dependency direction.
2. [Three-repository destination](architecture/repository-split.md) — what
   belongs in Ahamkara, Flashback, and Wish.
3. [Repository map](repo-map.md) — what each current folder owns.
4. [Build and test guide](guides/building.md) — supported commands.
5. [Maintenance guide](guides/maintenance.md) — how to diagnose, repair, and
   validate changes.
6. [Error-system proposal](design/error-system.md) — stable error identities,
   propagation, recovery, telemetry, and extension rules.
7. [Error-code operations](operations/error-codes.md) — initial code catalog
   and support workflow.
8. [Deployment guide](deployment-guide/README.md) — frontend, backend, and
   server deployment instructions.

## Documentation layout

| Area | Purpose |
|---|---|---|
| [`architecture/`](architecture/README.md) | System-wide boundaries, dependency direction, and migrations |
| [`design/`](design/README.md) | Proposed or accepted designs that have not become simple subsystem facts |
| [`deployment-guide/`](deployment-guide/README.md) | Deployment instructions for frontend, backend, and services |
| [`systems/`](systems/README.md) | Current implementation truth for individual subsystems |
| [`guides/`](guides/README.md) | Build, maintenance, operation, and contributor workflows |
| [`operations/`](operations/debug-operations.md) | Troubleshooting, diagnostics, error codes, and runbooks |
| [`wish/`](wish/README.md) | Transitional Wish implementation notes until Wish is extracted |
| [`roadmap/`](roadmap/roadmap.md) | Strategic direction only; executable work stays in GitHub Issues |
| [`reports/`](reports/README.md) | Historical investigations and implementation evidence |
| [`vault/`](vault/README.md) | Lightweight project memory, decisions, and agent orientation |

## Source-of-truth order

For behavior, trust source code, tests, and build configuration before prose.
For intentional boundaries, trust accepted architecture and decision docs. For
priority and completion state, trust GitHub Issues. Historical reports explain
what was observed at a point in time and are not current specifications.
