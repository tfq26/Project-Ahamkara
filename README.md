# Ahamkara

A custom C++20 game engine and multiplayer tech demo built from scratch.

## Current Milestone

Dedicated authoritative server and client networking skeleton. The client sends
`PlayerInputCommand` packets over UDP at ~60 Hz; the server simulates movement
and replies with `ServerSnapshot` packets. A native window + input layer
(GLFW3) and a tiny client-only debug renderer are available so movement can be
visually checked in a 3D grid scene. Audio, editor tooling, and fuller renderer
systems are kept out until the networking and platform foundations are validated.

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
├── docs/                   # Architecture & build docs
│   ├── architecture.md
│   ├── building.md
│   └── networking.md
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

See [`docs/building.md`](docs/building.md) for per-OS install instructions.

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

## Documentation

- [Architecture overview](docs/architecture.md)
- [Networking model](docs/networking.md)
- [Building from source](docs/building.md)
- [Client config](docs/client_config.md)
