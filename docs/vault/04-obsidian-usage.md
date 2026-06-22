# Obsidian Usage

Status: Seed

This vault is intentionally plain Markdown. Obsidian is a friendly interface for
humans, while agents can read the same files directly.

## Recommended Obsidian Setup

- Open `docs/vault` as the vault root.
- Keep internal vault links as wiki links, like `[[memory/current-state]]`.
- Use normal Markdown links for files outside the vault, like
  `[README](../../README.md)`.
- Avoid plugins that require generated metadata unless the files remain useful
  without Obsidian.

## Shared Settings

The committed `.obsidian/` files should contain only settings that are safe and
useful for everyone. Local workspace state, plugin data, caches, and trash are
ignored.

## Agent Compatibility

Agents should be able to understand every note with simple filesystem reads.
Avoid note layouts that depend on canvases, Dataview queries, or embedded
plugin state.
