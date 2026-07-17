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

The current folder layout and target destination are maintained in
[the repository map](../repo-map.md) and
[repository-split architecture](../architecture/repository-split.md). Do not
infer final ownership from the transitional folder names.

Start by reading:

- [README.md](../../README.md)
- [docs/README.md](../README.md)
- [docs/architecture/overview.md](../architecture/overview.md)
- [docs/repo-map.md](../repo-map.md)
- [docs/guides/building.md](building.md)
- [docs/guides/maintenance.md](maintenance.md)
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

Use multiple agents only when the active instructions permit delegation and the
work can be split into independent, bounded slices. The parent remains
responsible for integration and validation.

Suggested subagent output:

- files changed
- commands run
- test results
- assumptions
- risks

Historical agent reports live under `docs/reports/subagents/`; they preserve
evidence but do not represent current work status. Check GitHub Issues and run
the relevant tests instead of trusting an old handoff result.

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
