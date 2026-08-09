# Ahamkara

A custom C++20 game engine and multiplayer tech demo built from scratch.

## Current Milestone

Dedicated authoritative server and client networking skeleton. The client sends
`PlayerInputCommand` packets over UDP at ~60 Hz; the server simulates movement
and replies with `ServerSnapshot` packets. A native window + input layer
(GLFW3) and a tiny client-only debug renderer are available so movement can be
visually checked in a 3D grid scene. Audio, editor tooling, and fuller renderer
systems are kept out until the networking and platform foundations are validated.

## Landing Page

The project's public landing page and technical documentation site live in a
separate repository,
[`ahamkara-info-site`](https://git.2helix.org/taufeeq26/ahamkara-info-site),
and are deployed to Vercel. The landing page is a Vue 3 single-page
application built with Vite.

### Purpose

The landing page introduces Ahamkara to visitors and links to technical
documentation for developers. It is the public-facing entry point for the
project.

### Features

- Marketing landing page at `/` (see `src/views/Home.vue`) with the project
  title, tagline, and a "Read the docs" call-to-action.
- Technical documentation page at `/docs` (see `src/views/Docs.vue`).
- Client-side navigation via Vue Router with a shared navigation bar
  (`src/components/NavBar.vue`) that highlights the active route.
- Static production build output to `dist/`.

### File Locations (ahamkara-info-site repository)

| Path | Purpose |
|---|---|
| `index.html` | SPA entry point |
| `src/main.js` | App bootstrap (Vue + router) |
| `src/App.vue` | Root component |
| `src/router/routes.js` | Route table (`/` and `/docs`) |
| `src/views/Home.vue` | Marketing landing page |
| `src/views/Docs.vue` | Technical documentation page |
| `src/components/NavBar.vue` | Shared navigation bar |
| `src/components/DocsSidebar.vue` | Docs page sidebar |
| `src/components/SyntaxHighlight.vue` | Code block syntax highlighting |
| `vite.config.js` | Vite build and Vitest configuration |
| `package.json` | npm scripts and dependencies |

### Local Development

```sh
git clone https://git.2helix.org/taufeeq26/ahamkara-info-site.git
cd ahamkara-info-site
npm install
npm run dev       # dev server at http://localhost:5173
npm run build     # production build into dist/
npm run preview   # preview the production build
```

Run the Vitest unit test suite:

```sh
npm test          # run the full unit test suite once
npm run test:watch
```

### Deploying to Vercel

The site is a static Vite build, so deploying to Vercel is a standard Vite
deployment:

1. Import the `ahamkara-info-site` repository in the Vercel dashboard.
2. Vercel auto-detects the **Vite** framework preset. Confirm the settings:
   - Build command: `npm run build`
   - Output directory: `dist`
3. Deploy. Every push to `main` triggers a new deployment automatically.
4. To update the live site, push changes to `main`, or redeploy from the
   Vercel dashboard/CLI:

```sh
npx vercel --prod   # deploy the current build directly
```

## Repository Layout

```
ahamkara/
├── CMakeLists.txt          # Top-level CMake project
├── CMakePresets.json       # Build presets (debug / release)
├── .editorconfig           # Editor settings
├── assets/                 # Game assets (future)
├── backend/                # Platform backends (future)
├── build/                  # Generated build trees (git-ignored)
├── client/                 # Playable client executable
│   └── src/client_main.cpp
├── docs/                   # Guides, system docs, roadmaps, reports, vault
│   ├── README.md
│   ├── guides/
│   ├── systems/
│   ├── roadmap/
│   ├── reports/
│   └── vault/
├── editor/                 # Editor tooling (future)
├── engine/                 # Engine libraries (core, network, platform, render, runtime)
├── game/                   # Game-facing types & logic
├── scripts/                # Developer convenience scripts
├── server/                 # Headless dedicated server
│   └── src/dedicated_server_main.cpp
├── tests/                  # Test targets
└── tools/                  # Misc tooling
```

## Quick Start

### Prerequisites

- **CMake ≥ 3.20**
- **Ninja** (build tool)
- **C++20 compiler** (GCC 11+, Clang 14+, or Apple Clang 14+)
- **GLFW 3.3+** (`libglfw3-dev` on Ubuntu, `glfw` via Homebrew on macOS)

See [`docs/guides/building.md`](docs/guides/building.md) for per-OS install instructions.

### Build & Run

```sh
# One-command local debug view
./scripts/start.sh

# Or launch the local UDP server+client pair together
./scripts/start.sh network
```

The debug view opens a 1280×720 window with a ground grid, origin axes, and a
local player marker. Keyboard and controller input are both supported. Current
controller mapping uses left stick to move, right stick to look, `LB` to
sprint, `A` to jump, `B` to crouch, `X` to slide, `Back` to toggle metrics,
and `Start` to exit.

The network client connects to the local server over UDP. Both processes
print tick and position diagnostics to stdout.

For server-only local run and Docker notes, see
[`docs/wish/local_run.md`](docs/wish/local_run.md).

### Universal Start Script

Use the universal launcher if you want one entrypoint for configure/build/run:

```sh
./scripts/start.sh
./scripts/start.sh local
./scripts/start.sh network
./scripts/start.sh sandbox --skip-configure --skip-build
```

### Local Sandbox

If you want a local, no-network movement sandbox right now:

```sh
./scripts/run_sandbox.sh
```

Then use commands like:

```sh
step 60 w sprint
step 30 a
status
quit
```

## Development Workflows

### Local Zed Workflow

Use local Zed development when you want the tightest edit loop on a single
task.

```sh
./scripts/setup-dev.sh
cmake --build --preset debug
./scripts/run-tests.sh
```

For the common full-client debug path:

```sh
./scripts/start.sh
```

### Coder Remote Workspace Workflow

Use Coder workspaces on `servlenovo1` when you want builds, tests, indexing,
and agent execution to happen off your laptop.

1. Create a workspace in <https://dev.2helix.org>.
2. Install the native build dependencies in the workspace image:
   `git`, `cmake`, `ninja-build`, `g++`, `libglfw3-dev`, `libgl1-mesa-dev`,
   `pkg-config`, and optionally `ccache`.
3. Clone the repo and create an isolated task branch:

```sh
git clone <repo-url> ~/src/Ahamkara
cd ~/src/Ahamkara
git fetch origin
git checkout -b agent/codex/my-task origin/main
./scripts/setup-dev.sh
./scripts/run-tests.sh
```

For non-UI tasks in lighter remote workspaces, the headless preset avoids the
GLFW/OpenGL client build:

```sh
./scripts/setup-dev.sh --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

### Multi-Agent Workflow

Recommended model:

- one agent = one workspace = one branch
- one active writer per branch
- Git is the source of truth
- review diffs before merging manually

Recommended branch naming:

- `agent/<agent-name>/<task-name>`
- `feature/<feature-name>`
- `fix/<bug-name>`

Warning: multiple agents should not work on the same branch at the same time.

For larger features, let a parent agent own the workspace and integration
branch, and let subagents work in separate branches or worktrees. Example:

```sh
git fetch origin
git worktree add ../ahamkara-subagent-ui -b agent/deepseek/ui-pass origin/main
git worktree add ../ahamkara-subagent-tests -b agent/antigravity/test-pass origin/main
```

The parent agent should review and integrate subagent changes, then rerun the
build and tests before commit.

### Safe Agent Rules

- do not commit secrets
- do not modify infrastructure files unless asked
- do not run destructive commands
- do not auto-merge to `main`
- do not rewrite history
- always summarize changed files and commands run

### Sync, Diff, Commit, Push

Safe sync for a clean workspace:

```sh
./scripts/sync-main.sh
```

Inspect diffs:

```sh
git status
git diff
git diff --stat origin/main...HEAD
```

Commit and push:

```sh
git add <files>
git commit -m "Describe the change"
git push -u origin HEAD
```

See [`docs/guides/remote-agent-workflow.md`](docs/guides/remote-agent-workflow.md) for the
full remote-agent playbook.

## Documentation

- [Docs index](docs/README.md)
- [Architecture overview](docs/systems/architecture.md)
- [Networking model](docs/systems/networking.md)
- [Building from source](docs/guides/building.md)
- [Client config](docs/systems/client_config.md)
- [Asset pipeline](docs/systems/asset_pipeline.md)
- [Remote agent workflow](docs/guides/remote-agent-workflow.md)
