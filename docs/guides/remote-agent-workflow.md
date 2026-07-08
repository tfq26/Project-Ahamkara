# Remote Agent Workflow

This repository is set up to work well with remote AI coding agents running in
Coder workspaces on `servlenovo1` at <https://dev.2helix.org>. The AI models
can still come from external providers such as Codex, DeepSeek, or other
provider CLIs. The important part is that the repo checkout, dependency
installs, builds, tests, indexing, and terminal execution happen inside the
remote workspace instead of on your local machine.

## Recommended Model

- One agent = one Coder workspace = one branch.
- Git is the source of truth.
- Do not let multiple active agents write to the same branch at the same time.
- For larger features, let a parent agent own the main branch/workspace and let
  subagents use separate worktrees or branches for isolated tasks.

Example workspace layout:

- `ahamkara-main`: manual work, usually `main`
- `ahamkara-agent-ui`: UI task branch
- `ahamkara-agent-netcode`: gameplay or networking task branch
- `ahamkara-agent-tests`: testing or refactor task branch

## Create A Workspace In Coder

1. Sign in to <https://dev.2helix.org>.
2. Create a new workspace from your preferred Linux template.
3. Make sure the workspace image includes:
   - `git`
   - `cmake`
   - `ninja-build`
   - `g++`
   - `libglfw3-dev`
   - `libgl1-mesa-dev`
   - `pkg-config`
   - `ccache` (recommended)
4. Open a terminal in the workspace.
5. Clone the repo:

```sh
git clone <repo-url> ~/src/Ahamkara
cd ~/src/Ahamkara
```

If you want to start from a stable manual workspace, use `main`:

```sh
git checkout main
git pull --ff-only origin main
```

## Create A Branch For An Agent

Use one branch per active task:

```sh
git fetch origin
git checkout -b agent/codex/renderer-cleanup origin/main
```

Recommended branch naming:

- `agent/<agent-name>/<task-name>`
- `feature/<feature-name>`
- `fix/<bug-name>`

Examples:

- `agent/codex/debug-ui-cleanup`
- `agent/deepseek/packet-ack-audit`
- `feature/physics-broadphase`
- `fix/client-snapshot-jitter`

## Set Up The Workspace

Run the setup script after cloning:

```sh
./scripts/setup-dev.sh
```

By default this configures the full `debug` preset. For non-UI tasks in a
minimal workspace, use the headless preset:

```sh
./scripts/setup-dev.sh --preset debug-headless
```

The headless preset disables the GLFW/OpenGL client build and is useful for
server, gameplay, data, and many test-focused tasks.

## Run The Project Inside The Workspace

Full local debug workflow:

```sh
./scripts/start.sh
```

Dedicated server and client together:

```sh
./scripts/start.sh network
```

Dedicated server only:

```sh
./scripts/run_server.sh
```

Docker server workflow:

```sh
docker compose up --build
```

Note: the current Dockerfile is focused on the dedicated server. For full
interactive client development in Coder, prefer a workspace image with the
native build dependencies installed.

## Run Tests

Use the wrapper script:

```sh
./scripts/run-tests.sh
```

Exact underlying command for the default debug preset:

```sh
ctest --test-dir build/debug --output-on-failure
```

Headless preset example:

```sh
./scripts/run-tests.sh --preset debug-headless
```

## Inspect Diffs

Before reviewing or handing work back:

```sh
git status
git diff
git diff --stat
```

To compare your agent branch against `main`:

```sh
git fetch origin
git diff --stat origin/main...HEAD
git log --oneline --decorate --graph origin/main..HEAD
```

## Commit And Push Agent Work

Once the workspace is clean, tested, and reviewed:

```sh
git add <files>
git commit -m "Describe the change"
git push -u origin HEAD
```

Agents should either commit to their own branch or leave a clean diff for human
review. Agents should not auto-merge to `main`.

## Safe Syncing

To update a clean workspace from its current branch:

```sh
./scripts/sync-main.sh
```

The sync script:

- checks for uncommitted changes
- refuses to pull if the workspace is dirty
- fetches `origin`
- pulls with `--ff-only`
- prints a clear success or failure message

## Avoid Conflicts With Multiple Agents

Default rule:

- do not run multiple active writers on the same branch

Safer patterns:

- one agent per Coder workspace
- one subagent per git worktree
- one writer per branch

If a parent agent needs subagents, use worktrees:

```sh
git fetch origin
git worktree add ../ahamkara-subagent-tests -b agent/deepseek/test-pass origin/main
git worktree add ../ahamkara-subagent-audio -b agent/antigravity/audio-pass origin/main
```

Then let the parent agent review and integrate the results by merge,
cherry-pick, or manual patch application.

## Parent Agent And Subagent Pattern

This works well for larger features:

1. Parent/frontier agent owns the primary workspace and integration branch.
2. Parent splits work into narrow tasks.
3. Each subagent gets a separate worktree or branch.
4. Each subagent reports:
   - files changed
   - commands run
   - test results
   - assumptions or risks
5. Parent agent reviews all diffs and reruns tests before commit.

The repo already contains `docs/reports/subagents/` and that folder is a good
place to keep short handoff notes from subagents when useful.

## Safe Agent Rules

- Do not commit secrets.
- Do not modify infrastructure or deployment files unless explicitly asked.
- Do not run destructive commands.
- Do not auto-merge to `main`.
- Do not rewrite history.
- Always summarize changed files and commands run.

## Environment Variables

Copy the example file if you need server configuration overrides:

```sh
cp .env.example .env
./scripts/check-env.sh
```

Current env vars:

- `WISH_SERVER_PORT`
- `WISH_SERVER_ADMIN_PORT`
- `WISH_SERVER_TICK_RATE`
- `WISH_SERVER_MAX_PLAYERS`
- `WISH_SERVER_DISCONNECT_TIMEOUT_SEC`
- `WISH_SERVER_MATCH_DURATION_SEC`

## Docker Notes

This repo already includes Docker support for the dedicated server via
`Dockerfile` and `docker-compose.yml`.

- Use Docker when you want a simple server runtime in Coder.
- Use the native workspace toolchain when you need the full client, tests, or
  GLFW/OpenGL development loop.

If you later want stronger workspace reproducibility, a dedicated Coder image or
devcontainer-like setup for the full development toolchain would be a good next
step.
