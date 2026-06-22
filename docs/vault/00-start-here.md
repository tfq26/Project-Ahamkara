# Start Here

Status: Active

This vault gives humans and agents a fast path into Ahamkara without pretending
to replace code review. Use it to understand the project shape, current memory,
and likely next steps before opening implementation files.

## What This Repo Is

Ahamkara is a custom C++20 game engine and multiplayer tech demo. The current
center of gravity is engine foundation work: authoritative server simulation,
client/runtime loops, rendering, collision, animation, asset tooling, and
agent-friendly development workflows.

Canonical entry points outside the vault:

- [Project README](../../README.md)
- [Build instructions](../guides/building.md)
- [Architecture overview](../systems/architecture.md)
- [Remote agent workflow](../guides/remote-agent-workflow.md)
- [Agent handoff](../guides/agent-handoff.md)

## How Agents Should Use This Vault

1. Read [[01-repo-map]] for the repo layout.
2. Read [[memory/current-state]] for the latest durable project memory.
3. Check [[memory/open-questions]] before making architecture assumptions.
4. For new feature work, create a brief from [[templates/feature-brief]].
5. For meaningful decisions, append an entry to [[memory/decision-log]].
6. For implementation discipline, check [[skills/lean-implementation]].

## What Belongs Here

- Repo maps and system orientation.
- Agent handoff summaries.
- Open questions and design constraints.
- Feature planning notes.
- Decision records and rationale.

## What Does Not Belong Here

- Secrets or credentials.
- Large generated logs.
- Duplicated code-level documentation.
- Claims about current behavior that have not been checked against source,
  tests, or canonical docs.
