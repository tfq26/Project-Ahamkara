# Current State

Status: Active
Last updated: 2026-07-08

This note captures durable project memory for agents. Verify implementation
details in source before changing code.

## Project Direction

Ahamkara is a custom C++20 game engine and multiplayer tech demo. All phases
(3–8) and viewmodel refinement (tasks 1900–1940) have been merged into `main`.
Current focus is stabilizing the integrated codebase.

Cleanup priorities:
- Collapse debug-era seams into cleaner gameplay paths
- Gameplay/UI separation
- Continue OpenGL compatibility reduction (Apple-specific VAO extensions
  and legacy `<OpenGL/gl.h>` header are now retired on macOS)

Deferred items (not in active queue):
- HDR/offscreen render targets — last slice, path-preserving only
- Deep-logging epic — all children remain in `blocked/` until re-queued

## Recent Merge (2026-07-08)

Merged uncommitted changes from phase4 and phase6 git worktrees into the
integration branch, then merged to `main`. Summary:

- **Phase4** (agent/phase4/netcode): Lag-compensated hit validation
  (`validate_hit()` with ray-vs-dummy detection via server history buffer),
  snapshot delta compression (`SnapshotDelta` per-client), and
  `RemoteInterpolator` template class for per-remote-player interpolation.
- **Phase6** (agent/phase6/render-fidelity): Legacy-GL retirement docs
  (subagent report added, task moved to `review-needed/`).
- **Cleanup**: Removed duplicate git worktrees (`ahamkara-phase4`,
  `ahamkara-phase5`, `ahamkara-phase6`).
- **Changelog**: Added `CHANGELOG.md` at project root.

## Build & Test Status

- **17/20 tests pass** (debug preset, macOS)
- 3 tests not run — pre-existing missing binaries (pre-client-link issues):
  `ahamkara_playtest_harness_tests`, `ahamkara_ai_combatant_tests`,
  `ahamkara_encounter_scripting_tests`
- Pre-existing client/server build errors remain:
  - `debug_client.cpp`: `ClientSimulationSnapshot` scope/namespace
  - `threaded_local_runtime.cpp`: `std::lock_guard<std::mutex>` with const mutex
  - `admin_server.h/cpp`: POSIX `::close` / `SocketHandle` access

## Agent Workflow

The repo uses the standard agent workflow:

- One agent per workspace and branch
- Git as source of truth
- Small, reviewable changes with summaries
- Worktrees for parallel agent work

Primary docs:

- [Agent handoff](../../guides/agent-handoff.md)
- [Remote agent workflow](../../guides/remote-agent-workflow.md)

## Queue-Task Inventory

| State | Count |
|---|---|
| Blocked | 8 |
| Review-needed | 15 |
| Completed | 41 |
| Claimed | 3 |
| **Total** | **67** |

Note: `docs/vault/queue-tasks/open/` has been absorbed (task
TASK-20260704-1350-legacy-gl-retirement moved to `review-needed/`).

## Vault Health

- `current-state.md` — **Active**, updated 2026-07-08
- `open-questions.md` — Status: Seed, last updated 2026-06-14 (stale)
- `decision-log.md` — Status: Active, should be reviewed for recent decisions
- `known-good-commands.md` — Should be reviewed for current build commands
- `known-traps.md` — Should be reviewed for current pitfalls
