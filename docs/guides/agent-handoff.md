# Agent Handoff

Use this file to onboard Codex CLI, Antigravity CLI, OpenCode, or any other AI
coding agent before it starts making changes in this repository.

## Project Summary

Ahamkara is a custom C++20 game engine and multiplayer tech demo. The current
focus is networking, authoritative server simulation, collision/runtime
foundations, and developer tooling rather than a fully featured shipped game.

The build system is CMake + Ninja. The repo supports local development and a
remote Coder-based agent workflow.

## Primary Goals For Agents

- Understand the repo before making structural changes.
- Keep Git as the source of truth.
- Work in isolated branches and, when needed, isolated workspaces/worktrees.
- Prefer small, reviewable changes over sweeping churn.
- Leave a clear summary of files changed, commands run, and assumptions made.

## Remote Workspace Model

The preferred execution environment for AI agents is a Coder workspace on
`servlenovo1` at <https://dev.2helix.org>.

Important:

- The AI model provider is external/cloud-hosted.
- The repo checkout, indexing, builds, tests, and terminal execution happen in
  the Coder workspace.
- One agent = one workspace = one branch.
- Multiple active writers should not share the same branch.

See [remote-agent-workflow.md](remote-agent-workflow.md) for the fuller operational guide.

## Branching Rules

Recommended branch names:

- `agent/<agent-name>/<task-name>`
- `feature/<feature-name>`
- `fix/<bug-name>`

Examples:

- `agent/codex/render-cleanup`
- `agent/deepseek/netcode-audit`
- `feature/physics-broadphase`
- `fix/session-timeout-regression`

Rules:

- Do not force-push.
- Do not rewrite history.
- Do not auto-merge to `main`.
- Do not work on the same branch concurrently from multiple agents.

## Safe Agent Rules

- Do not commit secrets.
- Do not modify infrastructure, deployment, or server topology files unless
  explicitly asked.
- Do not run destructive commands.
- Do not assume the local laptop is the main execution environment.
- Always summarize changed files and commands run.

## Repo Orientation

High-level layout:

- `engine/`: engine libraries such as core, network, runtime, collision, render
- `game/`: gameplay-facing logic and types
- `client/`: playable client executable
- `server/`: dedicated server executable
- `tests/`: CTest test targets
- `tools/`: supporting tools and utilities
- `docs/`: guides, system docs, roadmaps, reports, and the vault
- `scripts/`: helper scripts for setup, build, test, and local run

Start by reading:

- [README.md](../../README.md)
- [docs/README.md](../README.md)
- [docs/guides/building.md](building.md)
- [docs/guides/remote-agent-workflow.md](remote-agent-workflow.md)
- [CMakePresets.json](../../CMakePresets.json)
- [CMakeLists.txt](../../CMakeLists.txt)

## Environment And Tooling

Expected workspace packages for Coder:

- `git`
- `cmake`
- `ninja-build`
- `g++`
- `pkg-config`
- `ccache`
- `libglfw3-dev`
- `libgl1-mesa-dev`

Docker is available for the dedicated server path, but the current primary
developer workflow is native CMake/Ninja inside the workspace.

## Canonical Commands

Setup full debug workspace:

```sh
./scripts/setup-dev.sh
```

Setup headless agent workspace:

```sh
./scripts/setup-dev.sh --preset debug-headless
```

Build:

```sh
cmake --build --preset debug
cmake --build --preset debug-headless
```

Run tests:

```sh
./scripts/run-tests.sh
./scripts/run-tests.sh --preset debug-headless
```

Local launcher:

```sh
./scripts/start.sh
./scripts/start.sh network
```

Safe sync:

```sh
./scripts/sync-main.sh
```

## Parent Agent / Subagent Pattern

For large features:

1. A parent agent owns the integration branch and workspace.
2. Subagents work on narrow scoped tasks.
3. Writing subagents should use separate branches or git worktrees.
4. The parent agent reviews, integrates, and reruns tests.

Suggested subagent output:

- files changed
- commands run
- test results
- assumptions
- risks

The repo already contains `docs/reports/subagents/` and it is a reasonable place
to store structured handoff notes when helpful.

## Known Current State

At the time this handoff was written:

- The remote-agent workflow docs and helper scripts were added on branch
  `codex/remote-agent-workflow`.
- A local compile fix was also applied in
  [engine/collision/include/ae/collision/world.h](/Users/taufeeqali/Projects/Ahamkara/engine/collision/include/ae/collision/world.h:1)
  to add the missing `#include <memory>` required by `std::unique_ptr`.
- After that compile fix, the `debug-headless` path builds far enough for tests
  to execute.

Known remaining test failures after the compile issue is fixed:

- [tests/src/world_tests.cpp](/Users/taufeeqali/Projects/Ahamkara/tests/src/world_tests.cpp:81)
  in `test_world_camera_yaw_wraps`
- [tests/src/gameplay_tests.cpp](/Users/taufeeqali/Projects/Ahamkara/tests/src/gameplay_tests.cpp:424)
  in `test_match_state_add_score`

These two are real assertion failures, not workspace bootstrap problems.

## How An Agent Should Start

1. Read the docs listed above.
2. Check the current branch and working tree state:

```sh
git status
git branch --show-current
```

3. If needed, sync a clean workspace:

```sh
./scripts/sync-main.sh
```

4. Configure and build the appropriate preset.
5. Run the relevant test command before making behavioral changes.
6. Make small changes, then rerun the affected build/tests.

## When Reorganization Is Allowed

Do not reorganize the repo aggressively just to make it easier for AI to read.

Prefer these first:

- better docs
- repo maps
- safer scripts
- clearer naming
- explicit build/test instructions

Only do deeper structural reorganization if there is a clear human
maintainability benefit and the migration cost is justified.
