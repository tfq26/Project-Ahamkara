# Ahamkara Vault

This folder is a shared Markdown vault for Ahamkara. It is meant to be opened
directly in Obsidian and read directly by coding agents from the repo.

The vault is not a replacement for source code, tests, build files, or the main
documentation in `docs/`. It is a navigation and memory layer: repo maps,
handoffs, decision notes, feature briefs, and durable context that helps agents
join the project quickly.

## Open In Obsidian

1. Open Obsidian.
2. Choose **Open folder as vault**.
3. Select this folder: `docs/vault`.

Obsidian settings that are safe to share live in `.obsidian/`. Machine-local
workspace state is ignored by `.gitignore`.

## Agent Entry Point

Start here:

- [[00-start-here]]
- [[01-repo-map]]
- [[02-agent-memory]]
- [[03-feature-development]]
- [[05-agent-skills]]
- [[06-frontmatter-conventions]]
- [[workflows/opencode-task-queue]]

## Main Areas

- `memory/` - Current state, open questions, decisions, and handoffs.
- `features/` - Feature briefs and planning notes.
- `systems/` - System-level maps that explain how repo areas fit together.
- `workflows/` - Project-specific workflow profiles and task flow notes.
- `queue-tasks/` - File-backed OpenCode task queue with broken-down task plans.
- `templates/` - Copyable note templates for agents and humans.
- `skills/` - Project-local copies of agent coordination skills.

## Workflow Notes

- [[workflows/feature-task-workflow]]
- [[workflows/opencode-task-queue]]
- [[workflows/codex-review-workflow]]
- [[workflows/review-escalation-policy]]
- [[workflows/model-routing]]
- [[workflows/project-model-allowlist-policy]]
- [[workflows/cross-project-langgraph-workbench]]
- [[workflows/queue-state-invariants]]
- [[workflows/revision-escalation-policy]]
- [[workflows/local-secrets-workflow]]
- [[workflows/ahamkara-reporting-profile]]
- [[skills/lean-implementation]]
- [[memory/known-good-commands]]
- [[memory/known-traps]]

## Principle

If a note says what the code does, link to the file or canonical doc. If a note
says why the project is moving in a direction, keep the reasoning here.
