---
type: opencode-task
task_type: parent
status: blocked
created: 2026-06-23
queued_by: user
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/core
  - engine/collision
  - engine/physics
  - engine/network
  - engine/runtime
  - engine/platform
  - engine/render
  - engine/animation
  - engine/ui
  - engine/input
  - engine/audio
  - game
  - client
  - server
  - wish
  - tools
related_feature:
report:
children:
  - TASK-20260623-1601-deep-logging-core-foundation.md
  - TASK-20260623-1602-deep-logging-collision.md
  - TASK-20260623-1603-deep-logging-physics.md
  - TASK-20260623-1604-deep-logging-network.md
  - TASK-20260623-1605-deep-logging-runtime.md
  - TASK-20260623-1606-deep-logging-platform.md
  - TASK-20260623-1607-deep-logging-render.md
  - TASK-20260623-1608-deep-logging-animation.md
  - TASK-20260623-1609-deep-logging-ui.md
  - TASK-20260623-1610-deep-logging-input.md
  - TASK-20260623-1611-deep-logging-audio.md
  - TASK-20260623-1612-deep-logging-game.md
  - TASK-20260623-1613-deep-logging-client.md
  - TASK-20260623-1614-deep-logging-server.md
  - TASK-20260623-1615-deep-logging-wish.md
  - TASK-20260623-1616-deep-logging-tools.md
---

# TASK-20260623-1600-deep-logging-epic

## Goal

Add deep, structured, level-gated logging across every component of the codebase
so runtime behavior (init/shutdown, state transitions, resource load, errors,
key per-frame/per-tick milestones) is observable. This is a **parent/epic** task;
the actual work happens in the child tasks (one per component), executed one
OpenCode pass at a time.

## Background

Motivation: runtime-confirm and debugging are painful because most subsystems
are silent (e.g. the level-load path rendered nothing with no log line either
way — see the level-authoring cluster). The logging facility
(`engine/core/include/ae/core/log.h`) already has categorized variants
(`log_info_cat` / `log_warning_cat` / `log_error_cat`) that prepend
`[Level][Category][time]`, but it has **no debug/trace level and no runtime
filtering**, so "deep" logging would flood normal runs. Child #1 fixes that
first; the rest instrument each component against the shared standard below.

## First Read

- [Docs index](../../README.md)
- [Repo map](../01-repo-map.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Architecture](../../systems/architecture.md)
- `engine/core/include/ae/core/log.h` (the logging facility)
- [OpenCode standing instructions](opencode-standing-instructions.md)

## Shared Logging Standard (the contract every child follows)

1. **Categories.** Each component logs under one stable category string via a
   file/translation-unit `#define AE_LOG_CATEGORY "<Name>"` and the `*_cat`
   helpers. Category per component:
   Core, Collision, Physics, Network, Runtime, Platform, Render, Animation, UI,
   Input, Audio, Game, Client, Server, Wish, Tools.
2. **Levels** (after child #1 adds debug/trace):
   - `Error` — failures/unrecoverable paths. Always on.
   - `Warning` — recoverable problems, fallbacks, missing assets. Always on.
   - `Info` — significant lifecycle/state (init, shutdown, config load, level
     load, connect/disconnect, match start). On by default. **Never per-frame.**
   - `Debug` — detailed flow (resource resolves, state-machine transitions,
     counts). Off by default; enabled per category at runtime.
   - `Trace` — very verbose / per-frame / per-tick. Off by default.
3. **What to instrument** (per component, as applicable): subsystem
   init/shutdown; config/asset/resource load + save (with names/paths/counts);
   state-machine transitions; connection/session/match lifecycle; error and
   fallback branches; and key periodic milestones gated to Debug/Trace.
4. **Cost & safety.** Disabled levels must be near-zero cost (check level before
   building the message; no string construction when the level is off). No
   secrets/PII. Do **not** add Info/Warning logging inside deterministic
   fixed-timestep hot paths in steady state (gate to Debug/Trace) — avoid perf
   regressions and timing nondeterminism.
5. **Scope discipline.** Logging only — no behavior changes, no refactors beyond
   what logging requires.

## Child Tasks (one component each)

Dependency: every child below depends on **child #1 (core foundation)** landing
first (it provides `log_debug_cat`/`log_trace_cat` + runtime gating). The rest
are independent of each other and can be done in any order.

- [x] 1601 engine/core (FOUNDATION: add levels + gating, then instrument core)
- [x] 1602 engine/collision
- [x] 1603 engine/physics
- [x] 1604 engine/network
- [x] 1605 engine/runtime
- [x] 1606 engine/platform
- [x] 1607 engine/render
- [x] 1608 engine/animation
- [x] 1609 engine/ui
- [x] 1610 engine/input
- [x] 1611 engine/audio
- [x] 1612 game
- [x] 1613 client
- [x] 1614 server
- [ ] 1615 wish
- [ ] 1616 tools

Child 1601 is complete and accepted; the remaining component children are now deferred until the project is back in a working state.

Deferred status note: the remaining deep-logging children stay out of active work until the user explicitly says the project is in a working state again.

## Acceptance Bar (parent)

- The shared standard above is implemented by child #1 and followed by all
  component children.
- Every component child is accepted (in `completed/`).
- A short "logging conventions" note is documented (in child #1's docs step).
- Build (`debug` and `debug-headless`) and existing tests stay green throughout.

## Review Tier

- `low` per child (additive logging); parent closes when all children complete.

## Validation

Per child:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
cmake --build --preset debug   # for render/client/GL-linked components
```

## Reporting Required

Each child follows the standard reporting flow (report, master log, status,
move to `review-needed/`). When all children are accepted, Codex closes this
parent and moves it to `completed/`.

## Notes For Codex Review

Confirm child #1 lands the level/gating infra before instrumentation children,
that no component adds per-frame Info spam, that disabled logs are cheap, and
that determinism/perf of the fixed-timestep sim is unaffected.
