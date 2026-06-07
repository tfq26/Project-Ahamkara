# Building Ahamkara

## Required Tools

| Tool   | Minimum Version | Notes                           |
|--------|-----------------|---------------------------------|
| CMake  | 3.20            | Build-system generator          |
| Ninja  | any recent      | Fast parallel build tool        |
| C++20 compiler | Clang 14+, GCC 11+, or Apple Clang 14+ | Required for C++20 support |
| GLFW3 | 3.3+            | Temporary client window/input and debug-render backend |
| OpenGL | System OpenGL   | Temporary debug renderer backend |

### Installing on Ubuntu (22.04+)

```sh
sudo apt update
sudo apt install -y cmake ninja-build g++-12 libglfw3-dev libgl1-mesa-dev
```

If the system `cmake` is too old, install the latest via the [Kitware APT
repo](https://apt.kitware.com/) or use `pip install cmake`.

### Installing on macOS

```sh
brew install cmake ninja glfw
```

Xcode Command Line Tools are also required (usually installed alongside
Homebrew):

```sh
xcode-select --install
```

---

## Configure

From the project root:

```sh
# Debug (symbols, no optimisation, compile_commands.json)
cmake --preset debug

# Headless debug (remote agents, server/test-heavy work, no GLFW/OpenGL client)
cmake --preset debug-headless

# Release (optimised)
cmake --preset release
```

The `debug` preset enables `CMAKE_EXPORT_COMPILE_COMMANDS` so you can point
your LSP/clangd at `build/debug/compile_commands.json`.

## Build

```sh
cmake --build --preset debug
# or
cmake --build --preset debug-headless
# or
cmake --build --preset release
```

## Test

```sh
ctest --test-dir build/debug --output-on-failure
```

For the headless preset:

```sh
ctest --test-dir build/debug-headless --output-on-failure
```

Alternatively, invoke `ninja` directly inside the build directory:

```sh
ninja -C build/debug
```

## Run

### Universal Launcher

For the quickest workflow, use the universal launcher:

```sh
./scripts/start.sh
```

Common modes:

```sh
./scripts/start.sh local
./scripts/start.sh network
./scripts/start.sh sandbox
./scripts/start.sh client -- 192.168.6.28
```

You can also skip configure/build when iterating:

```sh
./scripts/start.sh local --skip-configure --skip-build
```

### Debug Render View

Launch the client-only debug renderer:

```sh
./scripts/run_debug_view.sh
# or
./scripts/run_client.sh --debug-view
```

The window shows a ground grid, red/green/blue XYZ axes at the origin, and a
yellow local player marker driven by the movement simulation.

Keyboard controls:

- `W/A/S/D` move
- `Shift` sprint
- `Space` jump
- `C` slide
- `Ctrl` crouch
- `F3` metrics
- `Esc` exit

Controller controls:

- left stick move
- right stick look
- `LB` sprint
- `A` jump
- `B` crouch
- `X` slide
- `Back` metrics
- `Start` exit

### Networked Client + Server

Start the **dedicated server** first:

```sh
./scripts/run_server.sh
```

In another terminal, launch the **client**:

```sh
./scripts/run_client.sh
```

> The client sends UDP input commands to the server at ~60 Hz. The server
> echoes snapshot ticks and player positions back. Both processes print
> diagnostic output to stdout.

`./scripts/run_client.sh` also accepts an optional server IP, for example:

```sh
./scripts/run_client.sh 192.168.6.28
```

### Offline Sandbox

To launch the local offline movement sandbox:

```sh
./scripts/run_sandbox.sh
```

Useful sandbox commands:

```sh
step 60 w sprint
step 30 d
status
quit
```

### Asset Importer

Build and run the first-slice asset importer:

```sh
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
```

The importer currently compiles glTF models to Ahamkara `.aemesh` files,
compiles uncompressed TGA textures to `.aetex`, compiles text-based materials
to `.aemat`, and copies early sprite, audio, and data assets into
`assets/compiled/`. It also writes `assets/compiled/asset_registry.tsv` so the
pipeline has a stable index of compiled outputs. See `docs/asset_pipeline.md`
for the manifest format and runtime boundary.

## Expected Output

### Debug Render View

```
[Info][...] Platform window created (1280x720).
[Info][...] DebugRenderer initialized with GLFW/OpenGL debug backend.
[Info][...] Client application started.
[Info][...] Windowed debug view running — use WASD/arrows to move, Shift to sprint, Escape to quit.
```

### Server (typical)

```
[Info][...] DedicatedServer application started.
[Info][...] Ahamkara dedicated server listening on UDP 7777.
```

### Client (typical)

```
[Info][...] Client application started.
[Info][...] Client sending input to 127.0.0.1:7777.
[Info][...] Snapshot tick=1 position=(0, 0, 0.133333)
```

The tick counter advances and the position drifts as the server applies
movement.

## Troubleshooting

**`cmake: command not found`**
→ CMake is not installed or not on `PATH`. Follow the install instructions
above.

**`CMake Error: Could not find Ninja`**
→ Install `ninja-build` (Ubuntu) or `ninja` (Homebrew) and verify `ninja
--version` works.

**Linker errors about unresolved symbols**
→ Make sure you are building with a C++20-capable compiler. Run `cmake
--preset debug` again; CMake prints which compiler it selected.

**`ahamkara_server: No such file or directory`**
→ You need to build first: `./scripts/build_debug.sh`.

**Firewall / connection refused**
→ The server listens on UDP port `7777` and currently binds to `INADDR_ANY`.
Local testing against `127.0.0.1` does not usually require firewall changes,
but remote clients do require that UDP port `7777` is reachable.

**`glfw3`: package not found (CMake configure)**
→ Install the GLFW3 development package: `libglfw3-dev` on Ubuntu or
`glfw` via Homebrew on macOS. See the install instructions above.

**Window opens but shows a blank / black surface**
→ This is expected. The current client has no renderer — the window exists
solely to capture input and the per-frame diagnostics appear on stdout.
