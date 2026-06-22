# Current State

Status: Seed
Last updated: 2026-06-14

This note captures durable project memory for agents. Verify implementation
details in source before changing code.

## Project Direction

Ahamkara is a custom C++20 game engine and multiplayer tech demo. Current work
appears focused on engine/runtime foundations, networked gameplay, asset
pipeline growth, rendering, collision/physics, animation, and remote-agent
workflows.

## Agent Workflow

The repo already has an agent handoff document and remote workflow docs. The
working model is:

- one agent per workspace and branch
- Git as source of truth
- small, reviewable changes
- summarize files changed, commands run, assumptions, and risks

Primary docs:

- [Agent handoff](../../guides/agent-handoff.md)
- [Remote agent workflow](../../guides/remote-agent-workflow.md)

## Build And Test Memory

Canonical build and test commands live in
[docs/guides/building.md](../../guides/building.md).
For headless agent work, prefer the `debug-headless` preset when UI/OpenGL work
is not required.

## Vault Bootstrap

The shared Markdown/Obsidian vault was added under `docs/vault/` on
2026-06-14. Its purpose is to help agents and humans share durable context,
feature plans, repo maps, decisions, and handoffs without duplicating source
truth.

## Verification Needed

This seed state was created from existing repo documentation and file layout.
Future agents should update this note after verifying the latest branch state,
tests, and active roadmap.
