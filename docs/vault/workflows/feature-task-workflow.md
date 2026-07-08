# Feature Task Workflow

Status: Active

Use this workflow for Ahamkara work that spans multiple files, multiple agents,
or more than one session.

## First Read Path

1. [Docs index](../../README.md)
2. [Agent handoff](../../guides/agent-handoff.md)
3. [Start Here](../00-start-here.md)
4. [Repo Map](../01-repo-map.md)
5. [Agent Skills](../05-agent-skills.md)
6. Relevant [system map](../systems/README.md)
7. Relevant source, tests, and CMake targets

## Task Note

Create a feature or task note in `docs/vault/features/` when work is large
enough that another agent might inherit it.

Use [feature brief template](../templates/feature-brief.md).

If Codex is handing a specific implementation slice to OpenCode, create an
OpenCode queue task with [OpenCode task queue workflow](opencode-task-queue.md).

## Report Triggers

Write a formal report only at meaningful boundaries:

- task completed
- task paused or blocked
- non-obvious validation failure found
- another agent needs to inherit context
- supervisor accepts or rejects a worker slice

Use [Ahamkara reporting profile](ahamkara-reporting-profile.md) for the report.

## Completion Bar

A task is not complete just because files changed. The final handoff should say:

- what changed
- what was validated
- what was not validated
- what risks remain
- what the next agent should do, if anything

## Related

- [Known good commands](../memory/known-good-commands.md)
- [Known traps](../memory/known-traps.md)
- [Decision log](../memory/decision-log.md)
