# Building Ahamkara

This document describes how to build Ahamkara on each supported platform and
documents known platform-specific issues.

---

## Prerequisites

| Requirement  | Version         | Notes                                                  |
| ------------ | --------------- | ------------------------------------------------------ |
| CMake        | >= 3.20         |                                                        |
| C++ compiler | C++20 capable   | MSVC 2022, Apple Clang (Xcode 14+), GCC 11+, Clang 14+ |
| Ninja        | >= 1.10         | Cross-platform build system used by all presets        |

### Per-platform dependency installation

#### Linux (Debian/Ubuntu)

```sh
sudo apt update
sudo apt install -y cmake ninja-build libglfw3-dev pkg-config
```

#### macOS (Homebrew)

```sh
brew install cmake ninja glfw
```

#### Windows (vcpkg + Chocolatey)

```powershell
# Install Ninja (required by CMake presets)
choco install ninja

# Install GLFW3 via vcpkg (vcpkg is pre-installed on GitHub Actions runners;
# for local development, install it from https://vcpkg.io)
vcpkg install glfw3 --triplet x64-windows
```

When configuring CMake on Windows, pass the vcpkg toolchain file:

```powershell
cmake --preset debug `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

---

## Build

All CMake presets use **Ninja** as the generator for consistent behaviour
across platforms.

```sh
# Configure (pick one preset)
cmake --preset debug          # Debug with full symbols
cmake --preset release        # Release with optimisations
cmake --preset debug-headless # Debug without GLFW/OpenGL client (server-only)

# Build
cmake --build --preset debug

# Run tests (debug and debug-headless only)
ctest --test-dir build/debug --output-on-failure
```

### Available presets

| Preset                    | Description                                      |
| ------------------------- | ------------------------------------------------ |
| `debug`                   | Debug build, full symbols, tests enabled         |
| `release`                 | Release build, optimised                         |
| `debug-headless`          | Debug build without GUI (GLFW/OpenGL) deps       |
| `package`                 | Debug configuration for producing distribution   |
| `package-debug-headless`  | Headless debug configuration for packaging       |
| `wish-standalone`         | Engine core + Wish only; no game/client/server   |

---

## Platform-specific dependencies

### All platforms

| Dependency       | Obtained via                    | Used by                  |
| ---------------- | ------------------------------- | ------------------------ |
| GLM              | `FetchContent` (git)            | `ae_core` (math headers) |
| EnTT             | `FetchContent` (git)            | Game entity system       |
| JoltPhysics      | `FetchContent` (git, v5.0.0)   | Collision / physics      |
| miniaudio        | `FetchContent` (git)            | `ae_audio`               |

### Per-platform

| Platform  | Dependency          | Mechanism                                          |
| --------- | ------------------- | -------------------------------------------------- |
| Linux     | GLFW, OpenGL        | `find_package(glfw3)` / `find_package(OpenGL)` via pkg-config |
| macOS     | GLFW, OpenGL        | `find_package(glfw3)` (Homebrew config) / `find_package(OpenGL)` (system) |
| macOS     | CoreFoundation      | `find_library(CoreFoundation)`                     |
| macOS     | CoreText            | `find_library(CoreText)`                           |
| macOS     | CoreGraphics        | `find_library(CoreGraphics)`                       |
| Windows   | GLFW, OpenGL        | `find_package(glfw3)` (vcpkg) / `find_package(OpenGL)` (system SDK) |
| Windows   | ws2_32              | Linked automatically in `ae_network`               |

---

## Known platform-specific issues

### \_aligned_malloc / aligned_alloc (MSVC)

`engine/core/src/frame_allocator.cpp` uses `_aligned_malloc` / `_aligned_free`
under MSVC (`_MSC_VER`) and `std::aligned_alloc` / `std::free` on other
compilers. Both codepaths are tested.

### Crash handler stubs on Windows

`engine/core/src/crash_handler.cpp` provides signal-based backtraces on
POSIX systems. On Windows the handlers are stub implementations (no-op)
because structured exception handling (SEH) is the native mechanism.
This is intentional — the application should already use `__try`/`__except`
at a higher layer.

### Executable path resolution on Windows

`client/src/main.cpp` resolves the executable path via:
- macOS: `_NSGetExecutablePath`
- Linux: `readlink("/proc/self/exe")`
- Windows: `GetModuleFileNameW`

All three branches return a `std::filesystem::path` usable for locating
config files relative to the executable.

### GLAD / OpenGL loader on Windows

On Windows, `engine/render/src/render_backend_opengl.cpp` uses the
`gladLoadGL()` loader that ships in `engine/render/glad/`. On macOS and
Linux the system OpenGL loader is used directly.

### Text rasterizer

`engine/render/src/text_rasterizer.cpp` only provides a native text
rasterizer on macOS (using CoreText). On Windows and Linux the function
returns `nullptr` and text rendering falls back to a simpler path.

### fork() in ImGui (macOS)

Third-party file `engine/ui/imgui.cpp` uses `fork()`/`execvp()` in an
OS-level helper (`ImOsOpenInShell`). On Windows it uses `ShellExecuteW`;
on other POSIX systems `system()` is used. This is in ImGui library code
and is not part of the Ahamkara engine API.

### dlopen/dlsym in glad (non-MSVC)

On non-MSVC platforms, `engine/render/glad/src/gl.cpp` and
`engine/ui/imgui_impl_opengl3.cpp` use `dlopen`/`dlsym`/`dlclose` to load
OpenGL at runtime. On MSVC (Windows) they fall back to `wglGetProcAddress`.

### Network headers

POSIX networking (`<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`,
`<fcntl.h>`, `<unistd.h>`) vs Windows (`<winsock2.h>`, `<ws2tcpip.h>`) are
handled via `#ifdef _WIN32` in:
- `engine/network/src/udp_socket.cpp`
- `wish/admin/admin_server.cpp`
- `wish/integrations/nakama/src/nakama_bridge.cpp`

The `ae_network` target links `ws2_32` on Windows automatically
(see `engine/network/CMakeLists.txt`).

### MSVC runtime library

On Windows, all targets force the dynamic CRT via
`CMAKE_MSVC_RUNTIME_LIBRARY = "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"`
to avoid RuntimeLibrary mismatches with FetchContent dependencies (notably
JoltPhysics).

### macOS OpenGL deprecation warnings

`GL_SILENCE_DEPRECATION` is defined on macOS to silence deprecation
warnings from Apple's OpenGL framework. See `engine/render/CMakeLists.txt`.

### macOS nontrivial memcall warnings in Jolt

JoltPhysics v5.0.0 uses `memset`/`memcpy` on non-trivially-copyable types
which triggers `-Wnontrivial-memcall` under Apple Clang. This is suppressed
on the Jolt target via `target_compile_options(Jolt PRIVATE -Wno-nontrivial-memcall)`.

---

## CI

The CI workflow (`.github/workflows/ci.yml`) builds and tests on three
platforms:

| Platform | Runner              | Presets                     |
| -------- | ------------------- | --------------------------- |
| Linux    | `[self-hosted, linux]` | debug, release, debug-headless |
| macOS    | `macos-latest`      | debug, release              |
| Windows  | `windows-latest`    | debug, release              |

The `debug-headless` preset is Linux-only because it exercises server-side
code paths that are primarily relevant to the Linux deployment target.
Windows and macOS CI jobs only build `debug` and `release` presets.

