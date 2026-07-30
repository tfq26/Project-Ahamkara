# Ahamkara documentation

This directory is the durable knowledge base for the current transitional
monorepo and the planned Ahamkara, Flashback, and Wish repository split.

---

## Technical Documentation

### [Architecture](architecture/README.md)
System-wide boundaries, dependency direction, and repository migrations.
Covers the product model, target dependency direction, and the three-repository
destination (Ahamkara, Flashback, Wish).

### [API Reference](api-reference/README.md)
Public API surface for the Ahamkara engine modules, Wish backend services,
and Flashback game layer.

### [Deployment Guide](deployment-guide/README.md)
Deployment instructions for local development, dedicated servers, Docker
containers, and CI/CD pipelines.

---

## All Documentation Areas

| Area | Purpose |
|------|---------|
| [Architecture](architecture/README.md) | System-wide boundaries, migrations |
| [API Reference](api-reference/README.md) | Module and service API documentation |
| [Deployment Guide](deployment-guide/README.md) | Deployment, configuration, CI/CD |
| [Design](design/README.md) | Proposed or accepted designs |
| [Systems](systems/README.md) | Current implementation truth |
| [Guides](guides/README.md) | Build, maintenance, operations |
| [Operations](operations/debug-operations.md) | Troubleshooting, error codes |
| [Wish](wish/README.md) | Wish implementation notes |
| [Roadmap](roadmap/roadmap.md) | Strategic direction |
| [Reports](reports/README.md) | Historical investigations |
| [Vault](vault/README.md) | Project memory and decisions |

## Start Here

1. [Architecture overview](architecture/overview.md) — current composition,
   target boundaries, and dependency direction.
2. [Three-repository destination](architecture/repository-split.md) — what
   belongs in Ahamkara, Flashback, and Wish.
3. [Repository map](repo-map.md) — what each current folder owns.
4. [Build and test guide](guides/building.md) — supported commands.
5. [Deployment guide](deployment-guide/README.md) — how to deploy and configure.

## Work tracking

Mutable work belongs in [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
The canonical architecture/backlog tracker during the split is
[#61](https://github.com/tfq26/Project-Ahamkara/issues/61).

Do not create local issue cards, queue-task files, status dashboards, or copies
of GitHub issue bodies under `docs/`. Documentation may link to an issue for
execution status, but it should explain durable architecture, operation, or
rationale independently of that issue.

## Source-of-truth order

For behavior, trust source code, tests, and build configuration before prose.
For intentional boundaries, trust accepted architecture and decision docs. For
priority and completion state, trust GitHub Issues. Historical reports explain
what was observed at a point in time and are not current specifications.
