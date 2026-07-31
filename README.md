# Ahamkara

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)](https://cmake.org)
[![License](https://img.shields.io/badge/license-proprietary-blue)](#license)

A custom C++20 game engine and multiplayer tech demo built from scratch.
Ahamkara is both a research platform for engine and networking techniques and an
early-stage multiplayer game (codenamed *Flashback*).

---

## Table of Contents

- [Project Overview](#project-overview)
- [Tech Stack](#tech-stack)
- [Repository Layout](#repository-layout)
- [Prerequisites](#prerequisites)
- [Setup: Client (Frontend)](#setup-client-frontend)
- [Setup: Server & Wish (Backend)](#setup-server--wish-backend)
- [Quick Start](#quick-start)
- [Testing](#testing)
- [Linting](#linting)
- [Development Workflows](#development-workflows)
- [Contribution Guidelines](#contribution-guidelines)
- [Documentation](#documentation)
- [License](#license)

---

## Project Overview

### Purpose

Ahamkara provides a complete, from-scratch multiplayer game stack:

- **Engine** — Core C++20 libraries for networking, collision detection, math,
  memory management, input handling, audio, animation, and rendering.
- **Client** — A playable game client with a GLFW3/OpenGL window, debug
  renderer, and input layer for local and networked gameplay.
- **Server** — A headless authoritative dedicated server that simulates game
  state and synchronises clients over UDP.
- **Wish** — A backend session platform managing authentication, matchmaking,
  player sessions, and admin operations (with optional Nakama integration).
- **Gameplay** — The *Flashback* game layer (movement, weapons, AI,
  inventory, progression) built on the engine.

### Current Milestone

Dedicated authoritative server and client networking skeleton. The client sends
`PlayerInputCommand` packets over UDP at ~60 Hz; the server simulates movement
and replies with `ServerSnapshot` packets. A native window + input layer
(GLFW3) and a tiny client-only debug renderer are available so movement can be
visually checked in a 3D grid scene. Audio, editor tooling, and fuller renderer
systems are kept out until the networking and platform foundations are validated.

---

## Tech Stack

| Layer | Technology | Notes |
|---|---|---|
| **Language** | C++20 | Compiler extensions disabled; GCC 11+, Clang 14+, Apple Clang 14+ |
| **Build System** | CMake ≥ 3.20 + Ninja | CMakePresets.json for debug/release/headless/package |
| **Client Graphics** | GLFW 3.3+ / OpenGL | Temporary debug renderer backend |
| **Server** | C++20, headless | Dedicated authoritative server, no GPU dependency |
| **Backend Platform (Wish)** | C++20 | Session management, Nakama auth bridge, HTTP admin surface |
| **Testing** | CTest | 48+ test targets across engine, game, and tools |
| **Linting** | clang-format, clang-tidy, ruff, shellcheck, actionlint, cmake-lint | Managed via `scripts/setup-lint.sh` / `scripts/lint.sh` |
| **CI** | Agola (self-hosted) | Build, test, lint pipelines on every PR |
| **Containerisation** | Docker / docker-compose | For server deployment |

---

## Repository Layout

```
ahamkara/
├── CMakeLists.txt          # Top-level CMake project
├── CMakePresets.json       # Build presets (debug, release, headless, package)
├── engine/                 # Engine libraries (12 modules: core, network, collision, etc.)
├── client/                 # Playable client library + executable (frontend)
├── server/                 # Headless dedicated server (backend)
├── game/                   # Gameplay library (Flashback: movement, weapons, AI)
├── wish/                   # Backend/session platform
├── backend/                # Future backend services (placeholder)
├── samples/flashback/      # Flashback engine demo
├── tests/                  # 48+ CTest test targets
├── tools/                  # Asset importer, lint runner, diagnostic tools
├── assets/                 # Game assets (models, textures, levels, materials)
├── scripts/                # Developer convenience scripts (24 scripts)
├── docs/                   # Architecture, guides, system docs, reports
└── cmake/                  # CMake package export and install rules
```

---

## Prerequisites

| Tool | Minimum Version | Notes |
|---|---|---|
| CMake | 3.20 | Build-system generator |
| Ninja | any recent | Fast parallel build tool |
| C++20 compiler | GCC 11+, Clang 14+, Apple Clang 14+ | Required for C++20 support |
| GLFW3 | 3.3+ | Client window/input and debug-render backend |
| OpenGL | System OpenGL | Temporary debug renderer backend |

See [Building from Source](docs/guides/building.md) for per-OS install instructions.

---

## Setup: Client (Frontend)

The client is a playable C++20 application with a GLFW/OpenGL window. Build it
when you want to test gameplay visually.

### Install dependencies

**Ubuntu (22.04+):**
```sh
sudo apt update
sudo apt install -y cmake ninja-build g++-12 libglfw3-dev libgl1-mesa-dev pkg-config
```

**macOS:**
```sh
brew install cmake ninja glfw
xcode-select --install
```

### Configure & build

```sh
# From the project root
cmake --preset debug
cmake --build --preset debug
```

This builds the client binary (`client/ahamkara_client`), the server, all
engine libraries, and tests.

### Run the client

```sh
# Launch the debug render view (standalone movement sandbox)
./scripts/run_debug_view.sh

# Or launch the client connected to a local server
./scripts/run_client.sh
```

---

## Setup: Server & Wish (Backend)

The dedicated server is headless and has no GPU dependency. It runs the same
engine core as the client but without the GLFW/OpenGL layer. The Wish session
platform builds as part of the server.

### Build (headless)

For server-only or test-heavy work, use the headless preset (faster, no GLFW):

```sh
cmake --preset debug-headless
cmake --build --preset debug-headless
```

This produces `server/ahamkara_server` plus all engine and Wish libraries.

### Run the server

```sh
# Default configuration (UDP 7777, admin HTTP 7778)
./scripts/run_server.sh

# With explicit environment variables
WISH_SERVER_PORT=7777 \
WISH_SERVER_ADMIN_PORT=7778 \
WISH_SERVER_MAX_PLAYERS=8 \
./build/debug-headless/server/ahamkara_server
```

### Configuration

The server reads environment variables and matching CLI flags:

| Env Var | CLI Flag | Default | Description |
|---|---|---|---|
| `WISH_SERVER_PORT` | `--port=7777` | `7777` | UDP gameplay port |
| `WISH_SERVER_ADMIN_PORT` | `--admin-port=7778` | `7778` | HTTP admin port |
| `WISH_SERVER_TICK_RATE` | `--tick-rate=60` | `60` | Fixed simulation tick rate |
| `WISH_SERVER_MAX_PLAYERS` | `--max-players=8` | `8` | Tracked client cap |
| `WISH_SERVER_DISCONNECT_TIMEOUT_SEC` | `--disconnect-timeout=10` | `10` | Client timeout window |
| `WISH_SERVER_MATCH_DURATION_SEC` | `--match-duration=600` | `600` | Match length; `0` = no limit |

Nakama-backed auth configuration is also available; see
[docs/wish/local_run.md](docs/wish/local_run.md).

### Inspect the server

```sh
curl http://127.0.0.1:7778/health
curl http://127.0.0.1:7778/match/status
curl http://127.0.0.1:7778/players
```

### Docker

```sh
docker compose up --build
```

Exposes UDP `7777` for gameplay and TCP `7778` for the admin surface.

---

## Quick Start

```sh
# One-command local debug view (configure + build + run)
./scripts/start.sh

# Or launch the local UDP server+client pair together
./scripts/start.sh network
```

The debug view opens a 1280×720 window with a ground grid, origin axes, and a
local player marker. Keyboard and controller input are both supported.

**Keyboard:** W/A/S/D move, Shift sprint, Space jump, C slide, Ctrl crouch,
F3 metrics, Esc exit.

**Controller:** left stick move, right stick look, LB sprint, A jump, B crouch,
X slide, Back metrics, Start exit.

The network client connects to the local server over UDP. Both processes print
tick and position diagnostics to stdout.

---

## Testing

```sh
# Full suite (debug preset)
ctest --test-dir build/debug --output-on-failure

# Headless preset (no GLFW/OpenGL)
ctest --test-dir build/debug-headless --output-on-failure

# Specific test
ctest --test-dir build/debug -R collision_tests --output-on-failure

# Via convenience script
./scripts/run-tests.sh
```

Tests live in `tests/src/` and are registered in `tests/CMakeLists.txt`.
Every test is a standalone C++20 executable that returns zero on success.

---

## Linting

```sh
# Setup lint tools once
./scripts/setup-lint.sh

# Lint branch diff
./scripts/lint.sh --base-ref origin/main --compile-db build/debug

# Apply safe fixes
./scripts/lint.sh --base-ref origin/main --compile-db build/debug --fix
```

The lint toolchain runs clang-format, clang-tidy, ruff (Python), shellcheck,
actionlint, and cmake-lint.

---

## Development Workflows

### Local Development

```sh
./scripts/setup-dev.sh
cmake --build --preset debug
./scripts/run-tests.sh
```

### Remote Workspace (Coder)

Use [Coder workspaces](https://dev.2helix.org) for offloaded builds and agent
execution. See [docs/guides/building.md](docs/guides/building.md) for details.

### Multi-Agent Workflow

- One agent = one workspace = one branch
- One active writer per branch
- Git is the source of truth

Branch naming convention:

```
agent/<agent-name>/<task-name>     # Agent-authored branches
feature/<feature-name>             # Feature branches
fix/<bug-name>                     # Bug fixes
```

### Safe Agent Rules

- Do not commit secrets.
- Do not modify infrastructure files unless asked.
- Do not run destructive commands.
- Do not auto-merge to `main`.
- Do not rewrite history.
- Always summarise changed files and commands run.

See [docs/guides/remote-agent-workflow.md](docs/guides/remote-agent-workflow.md)
for the full remote-agent playbook.

---

## Contribution Guidelines

We welcome contributions. Please read the full
**[CONTRIBUTING.md](CONTRIBUTING.md)** before submitting changes.

### Quick summary

1. **Create a branch** from `main`:
   ```sh
   git fetch forgejo main
   git checkout -b agent/<name>/<task> forgejo/main
   ```
2. **Make changes** following [coding standards](docs/coding-rules.md).
3. **Build and test**:
   ```sh
   cmake --preset debug && cmake --build --preset debug
   ctest --test-dir build/debug --output-on-failure
   ```
4. **Lint your changes**:
   ```sh
   ./scripts/setup-lint.sh && ./scripts/lint.sh --base-ref origin/main --compile-db build/debug
   ```
5. **Rebase and push**:
   ```sh
   git fetch forgejo main
   git rebase forgejo/main
   git push --force-with-lease forgejo HEAD
   ```
6. **Open a pull request** against `main`.

### PR requirements

- Every code change includes a test or an explanation of why testing is infeasible.
- All tests pass.
- Lint is clean on the branch diff.
- PRs should be scoped to a single concern.

### Coding standards

- **C++20** with compiler extensions disabled.
- Engine code uses `ae` or `ae::<subsystem>` namespace.
- Public headers go under `engine/<module>/include`, implementation under `engine/<module>/src`.
- Declare dependencies in CMake targets — do not add repo-root include paths.
- Use categorised logging; guard expensive debug message construction.
- Follow existing patterns and idioms in the module you're touching.
- Do not edit auto-generated files.

---

## Frontend & Deployment

The project includes a Vue.js frontend (in [`frontend/`](frontend/)) for the web dashboard, deployed to **Cloudflare Pages** on every push to `main`.

### Prerequisites for deployment

- **Node.js ≥ 18** with npm
- A **Cloudflare account** with Pages enabled
- `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` environment variables set

### Build locally

```sh
cd frontend
npm ci
npm run build     # outputs to frontend/dist/
```

The `dist/` directory contains the production build with SPA fallback routing (via `_redirects`).

### Deploy manually

```sh
bash deploy.sh    # builds and deploys to Cloudflare Pages
```

### CI/CD

- **Forgejo (Agola)**: runs `build-frontend` on every push; runs `deploy-frontend` on push to `main`.
- **GitHub Actions**: runs `build-frontend` on every push/PR; runs `deploy-frontend` on push to `main`.
- Deployment uses **Wrangler CLI** with direct upload (`wrangler pages deploy`).
- Set `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` as secrets in both CI systems.

### SPA routing

The [`frontend/public/_redirects`](frontend/public/_redirects) file enables client-side routing by serving `index.html` for all routes (Cloudflare Pages SPA fallback).

## Documentation

- [Docs index](docs/README.md)
- [Architecture overview](docs/systems/architecture.md)
- [Building from source](docs/guides/building.md)
- [Networking model](docs/systems/networking.md)
- [Client config](docs/systems/client_config.md)
- [Asset pipeline](docs/systems/asset_pipeline.md)
- [Wish session platform](docs/wish/README.md)
- [Coding rules](docs/coding-rules.md)
- [Contributing](CONTRIBUTING.md)
- [Roadmap](docs/roadmap/roadmap.md)

---

## License

Proprietary. All rights reserved.
