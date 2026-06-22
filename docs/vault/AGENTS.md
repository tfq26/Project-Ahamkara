# Vault Instructions For Agents

This directory is a repo-local Obsidian vault and an agent-readable project
memory layer.

## Scope

These instructions apply to every file under `docs/vault/`.

## Purpose

Use this vault to preserve project understanding that helps humans and agents
work without repeatedly rediscovering the same context. The source code, tests,
build files, and canonical docs remain the source of truth. Vault notes should
link to those sources and explain orientation, intent, decisions, handoffs, and
open questions.

## Rules

- Keep notes in plain Markdown that works in GitHub, terminals, and Obsidian.
- Prefer relative Markdown links to repo files when possible.
- Use Obsidian wiki links only for links between vault notes.
- Do not store secrets, credentials, private tokens, or machine-specific paths.
- Do not claim code behavior from memory when the code can be checked.
- Mark uncertain claims with `Status: Unverified` or `Status: Assumption`.
- Update `memory/current-state.md` after substantial repo-wide or feature work.
- Add decision records to `memory/decision-log.md` when a choice affects future
  implementation direction.
- Keep agent handoff notes concise and dated.

## Agent Start Path

Before using the vault for project orientation, read:

1. `README.md`
2. `00-start-here.md`
3. `01-repo-map.md`
4. `memory/current-state.md`

Then read the canonical repo docs linked from those notes as needed.
