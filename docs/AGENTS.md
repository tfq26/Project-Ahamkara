# Documentation Instructions For Agents

These instructions apply to every file under `docs/`.

## Structure

Keep documentation organized by purpose:

- `guides/` - How to build, run, test, and operate the project.
- `systems/` - Current subsystem and architecture documentation.
- `wish/` - Wish engine protocol, runtime, and integration notes.
- `roadmap/` - Planning documents and future work.
- `reports/` - Historical reports, investigations, and subagent outputs.
- `vault/` - Obsidian-compatible agent memory. Follow `docs/vault/AGENTS.md`
  for files in that subtree.

Do not add new Markdown files directly under `docs/` unless they are indexes or
folder-level navigation files.

## Linking

- Prefer relative Markdown links.
- Link to canonical docs in `systems/` or `guides/` instead of duplicating long
  explanations.
- Historical reports may reference older paths when preserving history, but add
  current links when editing them for active use.

## Agent-Facing Docs

When adding or changing docs for agents:

- Keep the first-read path clear.
- Mark uncertain or stale information explicitly.
- Point to source files, tests, and canonical docs for verification.
