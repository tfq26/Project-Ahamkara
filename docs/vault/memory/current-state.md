# Current State

Status: Seed
Last updated: 2026-06-25

This note captures durable project memory for agents. Verify implementation
details in source before changing code.

## Project Direction

Ahamkara is a custom C++20 game engine and multiplayer tech demo. Current work
appears focused on engine/runtime foundations, networked gameplay, asset
pipeline growth, rendering, collision/physics, animation, and remote-agent
workflows.

Current cleanup focus is to collapse debug-era seams into a cleaner gameplay
path while still following the roadmap: gameplay/UI separation, reusable
viewmodel orientation contracts, and continued OpenGL compatibility reduction.
The roadmap priorities still stand: stabilize the playable loop first, then
push deeper rendering fidelity and world-scale features.
HDR/offscreen render targets remain explicitly deferred to the very last slice
so earlier work can preserve a path toward future post-processing without
locking the engine into backbuffer-only assumptions. HDR is tracked in the
roadmap and memory notes, but it is intentionally not kept in the active queue
until a concrete trigger re-queues it.

The deep-logging epic is also deferred for now. Keep the remaining deep-logging
children out of active work until the user says the project is back in a
working state and the deferred slice should be resumed.

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

## Renderer Upgrade Path

The client/debug renderer is in an OpenGL 3.3 core-profile setup, but several
older draw sites still exist. The migration direction is to keep the core
profile and route legacy-style draws through `engine/render/src/gl_compat.*`
or explicit backend helpers, rather than reintroducing raw fixed-function
client-state calls. In `gl_compat.h`, direct `glEnableClientState`-style calls
are intentionally no-ops. Fixed-function lighting is also no longer a source
of truth; the main renderer now feeds lighting values to shader uniforms
explicitly.

The overlay cleanup is now following the same pattern: the shared UI primitive
helpers in `debug_renderer.cpp` draw through a small shader/VBO path, while the
remaining text and more complex debug shapes are still in transition.

## Vault Bootstrap

The shared Markdown/Obsidian vault was added under `docs/vault/` on
2026-06-14. Its purpose is to help agents and humans share durable context,
feature plans, repo maps, decisions, and handoffs without duplicating source
truth.

## Verification Needed

This seed state was created from existing repo documentation and file layout.
Future agents should update this note after verifying the latest branch state,
tests, and active roadmap.
