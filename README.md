# Ahamkara

A custom C++20 game engine and multiplayer FPS tech demo (Flashback) built from scratch. Three products in one transitional monorepo:

| Product | Description |
|---|---|
| **Ahamkara** | Reusable engine: ECS, networking, physics, rendering, animation, audio, UI, input, platform |
| **Flashback** | Multiplayer FPS game consuming Ahamkara and Wish (deathmatch, AI combatants, weapons, levels) |
| **Wish** | Backend/session/activity/replication platform with a game-neutral protocol |

The goal is to split into three independent repositories. See [Repository split](docs/architecture/repository-split.md).

## Tech Stack

- **Language**: C++20 (`CMAKE_CXX_EXTENSIONS OFF`)
- **Build**: CMake ≥ 3.20, Ninja
- **Compilers**: GCC 11+, Clang 14+, Apple Clang 14+
- **Rendering**: OpenGL, GLFW 3.3+
- **Physics**: Jolt Physics v5.0.0
- **ECS**: EnTT v3.13.0
- **Math**: GLM 1.0.1
- **Audio**: miniaudio
- **Scripting** (optional, Wish admin): Lua + sol2 v3.3.0
- **Testing**: Google Test / Google Mock
- **Lint/Format**: clang-format, clang-tidy, ruff (Python), shellcheck, actionlint, cmake-lint

## Repository Layout

```
├── CMakeLists.txt              # Top-level CMake project (v0.1.0)
├── CMakePresets.json           # Build presets (debug / debug-headless / release)
├── engine/                     # Reusable engine libraries (→ Ahamkara repo)
│   ├── core/                   #   ae_core: logging, config, time, math, jobs, ECS types
│   ├── network/                #   ae_network: UDP transport, reliable channel, interpolation
│   ├── physics/                #   ae_physics: Jolt Physics integration
│   ├── render/                 #   ae_render: OpenGL renderer, debug renderer, SSAO, LOD, IBL
│   ├── animation/              #   ae_animation: skeletal animation, IK, clip player
│   ├── audio/                  #   ae_audio: 3D spatialization (miniaudio)
│   ├── ui/                     #   ae_ui: Dear ImGui debug UI
│   ├── input/                  #   ae_input: keyboard, mouse, gamepad routing
│   ├── platform/               #   ae_platform: GLFW window / OS integration
│   ├── runtime/                #   ae_runtime: application lifecycle, frame pacing
│   ├── collision/              #   ae_collision: spatial queries, AABB, ray casting
│   └── skeleton/               #   ae_skeleton: pose evaluation, skinning, clip data
├── game/                       # Gameplay layer (→ Flashback repo)
├── client/                     # Client application (GLFW + OpenGL)
├── server/                     # Dedicated authoritative server (UDP)
├── wish/                       # Backend platform (→ Wish repo)
├── tests/                      # ~46 CTest targets
├── samples/flashback/          # Thin Flashback launcher
├── tools/                      # Asset importer, diagnostics, controller mapper, lint
├── assets/                     # Models, textures, materials, levels, sounds
├── scripts/                    # Developer convenience scripts (20+)
├── docs/                       # Architecture, guides, systems, wish, reports, vault
├── client/config/              # Client configuration files
├── .clang-format               # C++ formatting (LLVM-based, 4-space indent)
├── .clang-tidy                 # Static analysis checks
├── ruff.toml                   # Python lint config
└── .editorconfig               # Editor settings (LF, UTF-8, spaces)
```

## Quick Start

### Prerequisites

Install build dependencies:

**Ubuntu (22.04+)**
```sh
sudo apt update && sudo apt install -y cmake ninja-build g++-12 libglfw3-dev libgl1-mesa-dev
```

**macOS**
```sh
brew install cmake ninja glfw && xcode-select --install
```

### One-command launch

```sh
# Configure, build, and run local debug view
./scripts/start.sh

# Or launch dedicated server + client pair
./scripts/start.sh network
```

The debug view opens a 1280×720 window with a ground grid, XYZ axes, and a local player marker. Keyboard and controller input are both supported.

### Manual build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

See [Building from source](docs/guides/building.md) for per-OS details and troubleshooting.

## Setup Instructions

### Client (Frontend)

The client is a GLFW/OpenGL application with debug rendering, input handling, and menu system.

```sh
# Configure and build the debug preset (includes client)
cmake --preset debug
cmake --build --preset debug

# Run the client debug view
./scripts/run_client.sh --debug-view

# Or connect a client to a running server
./scripts/run_client.sh                # localhost
./scripts/run_client.sh 192.168.6.28   # remote server
```

Keyboard controls: `W/A/S/D` move, `Shift` sprint, `Space` jump, `C` slide, `Ctrl` crouch, `F3` metrics, `Esc` exit.

### Server (Backend)

The dedicated server runs headlessly over UDP at ~60 Hz tick rate. It handles session management, authentication, activity routing, and snapshot broadcast.

```sh
# Configure and build the headless preset (no GLFW/OpenGL dependency)
cmake --preset debug-headless
cmake --build --preset debug-headless

# Run the dedicated server
./scripts/run_server.sh
```

Server listens on UDP port `7777`. Configuration via environment variables (`WISH_SERVER_PORT`, `WISH_SERVER_ADMIN_PORT`, `WISH_SERVER_TICK_RATE`, `WISH_SERVER_MAX_PLAYERS`, `WISH_SERVER_MATCH_DURATION_SEC`). See [.env.example](.env.example) and [Wish local run](docs/wish/local_run.md).

### Full Stack (Server + Client)

```sh
# Build both
cmake --preset debug
cmake --build --preset debug

# Terminal 1: start server
./scripts/run_server.sh

# Terminal 2: start client
./scripts/run_client.sh
```

Or use the universal launcher:

```sh
./scripts/start.sh network
```

### Headless (Remote / Test-Only)

For remote workspaces or CI without a display:

```sh
./scripts/setup-dev.sh --preset debug-headless
cmake --build --preset debug-headless
ctest --test-dir build/debug-headless --output-on-failure
```

### Docker

```sh
# Build headless server image
docker build -t ahamkara-server .

# Run the server container
docker run -p 7777:7777/udp ahamkara-server
```

## Contribution Guidelines

### Branching Strategy

- `main` — stable, production-ready
- `develop` — integration branch
- `feature/<name>` — new features
- `fix/<name>` — bug fixes
- `agent/<agent-name>/<task-name>` — agent work branches

One agent/writer per branch. Do not share branches across writers.

### Pull Request Process

1. Create a branch from `develop` (or `main` for hotfixes).
2. Implement your change with tests.
3. Run the full validation gate:
   ```sh
   cmake --preset debug
   ./scripts/lint.sh --base-ref origin/main --compile-db build/debug
   ./scripts/lint.sh --base-ref origin/main --compile-db build/debug --fix
   cmake --preset debug-headless && cmake --build --preset debug-headless && ctest --test-dir build/debug-headless --output-on-failure
   ```
4. Open a pull request targeting `develop`.
5. Ensure CI passes (lint, build matrix, tests, package).
6. Request review. Do not self-merge without approval.
7. Once approved, merge via rebase or squash.

### Code Style

- **C++**: Follow [coding rules](docs/coding-rules.md). Use C++20, `ae` namespace for engine code. Match adjacent code style. Public headers in `include/`, implementation in `src/`. Express dependencies in CMake target, not root include paths.
- **Formatting**: clang-format (LLVM-based, 4-space indent) — run `./scripts/lint.sh --base-ref origin/main --compile-db build/debug --fix` before committing.
- **Linting**: clang-tidy, ruff (Python), shellcheck, cmake-lint — all checked via `./scripts/lint.sh`.
- **Shell scripts**: Fail-fast, resolve paths relative to script or repo root.
- **Python**: Follow ruff.toml config (Py311 target).

### Testing

- Every code change requires a test or an explicit explanation of why it cannot be automated.
- Place regression tests at the narrowest boundary that proves the defect.
- See [Testing and quality](docs/testing-quality.md) for the full test ownership table and CI contract.
- Run tests with: `ctest --test-dir build/debug --output-on-failure`

### Commit Messages

Follow existing conventions:
- Keep the first line under 72 characters.
- Use present-tense imperative ("Add feature", "Fix crash", not "Added" or "Fixed").
- Reference issue numbers when applicable.

## Documentation

- [Docs index](docs/README.md)
- [Architecture overview](docs/systems/architecture.md)
- [Repository split plan](docs/architecture/repository-split.md)
- [Networking model](docs/systems/networking.md)
- [Building from source](docs/guides/building.md)
- [Testing and quality](docs/testing-quality.md)
- [Coding rules](docs/coding-rules.md)
- [Asset pipeline](docs/systems/asset_pipeline.md)
- [Remote agent workflow](docs/guides/remote-agent-workflow.md)
- [Changelog](CHANGELOG.md)
- [Glossary](docs/glossary.md)
