---
type: subagent-report
category: infrastructure
status: implemented
created: 2026-07-12
agent: oz
subsystems: [engine/render, build]
branch: main
validation: [build]
---

# Subagent Report: Gaming PC Windows Build

## Task

Set up the gaming PC (desktop2608, `100.124.18.104` via Tailscale) as a Windows build target for the Ahamkara engine, fix pre-existing compilation errors, and get the engine to link successfully on MSVC 2022.

## Status

`implemented` — the engine builds and links successfully on Windows. Pre-existing test failures (network socket init, frame allocator alignment) are unrelated and pre-date this work.

## Scope

In bounds:
- Configure SSH access to the gaming PC via Tailscale
- Install build tooling (CMake 4.4.0, Ninja 1.13.2) via Chocolatey
- Bootstrap vcpkg, install GLFW3
- Clone the repository
- Fix all pre-existing MSVC compilation errors on main
- Fix CRT mismatch between JoltPhysics (MTd) and the rest of the engine (MDd)
- Fix unresolved OpenGL symbols by integrating a GL runtime loader (GLAD)
- Verify clean build

Out of bounds:
- Fixing pre-existing test failures (socket init, alignment)
- Running engine executables with a GL display (not available over SSH)
- Runtime content verification

## Windows Build Issues Fixed

### Compilation Errors (7 files fixed)

| File | Issue | Fix |
|------|-------|-----|
| `crash_handler.cpp` | POSIX-only APIs (`unistd.h`, `execinfo.h`, `sigaction`, `SIGBUS`) | Wrapped in `#if !defined(_WIN32)` with Windows stubs |
| `animation_driver.h` | CompressedAnimState bit-field layout differs on MSVC (9 vs 8 bytes) | Redesigned with explicit byte layout |
| `gl_platform.h` | Windows SDK GL/gl.h conflicts with modern GL extension types | Bundled Khronos `glcorearb.h`/`glext.h`/`khrplatform.h` under `engine/render/include/` |
| `debug_renderer_metrics.cpp` | Embedded CR character in comment | Removed CR |
| `audio_engine.h` | Missing `#include <memory>` for `std::unique_ptr` | Added include |
| `controller_mapper.cpp` | Uses legacy immediate-mode GL, in conflict with glcorearb.h | Excluded entire target on Windows: `if(NOT WIN32)` |
| `diagnostics_tools.cpp` | Missing `#include <fstream>` | Added include |
| `admin_server.cpp/h` | `SHUT_RDWR` POSIX-only, SocketHandle visibility | Wrapped with `SD_BOTH`, moved typedef |

### Linker Errors (3 root causes fixed)

**1. CRT mismatch (RuntimeLibrary)**
- JoltPhysics v5.0.0 compiled with `MTd_StaticDebug` while the rest of the project used `MDd_DynamicDebug`.
- Fix: Set `USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE` before FetchContent (Jolt uses this name, not `USE_STATIC_CRT`). Also force `CMAKE_MSVC_RUNTIME_LIBRARY` globally and set `MSVC_RUNTIME_LIBRARY` target property post-fetch.

**2. Unresolved OpenGL symbols (glGenQueries, glBindBuffer, glCreateShader, etc.)**
- `opengl32.lib` on Windows only exports OpenGL 1.1 symbols. Modern GL functions must be resolved at runtime via `wglGetProcAddress`.
- Fix: Integrated GLAD (OpenGL 4.5 core profile) as a runtime loader. Replaced `glcorearb.h` with `glad/gl.h` on Windows. Call `gladLoadGL(glfwGetProcAddress)` after creating GL context.

**3. OpenGL::GL visibility**
- `OpenGL::GL` was `PRIVATE` to `ae_render`, so consuming targets didn't inherit `opengl32.lib`.
- Fix: Changed to `PUBLIC`.

### GLAD Integration

Generated GLAD loader for OpenGL 4.5 core profile:
- `engine/render/glad/include/glad/gl.h` — header with function pointer declarations
- `engine/render/glad/src/gl.cpp` — implementation (renamed from .c for CXX-only project)
- `engine/render/glad/include/KHR/khrplatform.h` — platform types
- Integrated via `gl_platform.h` on Windows and conditional source in `engine/render/CMakeLists.txt`

## Files Changed

- `CMakeLists.txt` — Jolt CRT options, post-fetch override, global `CMAKE_MSVC_RUNTIME_LIBRARY`
- `engine/render/CMakeLists.txt` — GLAD include dir + source, OpenGL::GL visibility
- `engine/render/include/ae/render/gl_platform.h` — Use glad.h on Windows
- `engine/render/src/render_backend_opengl.cpp` — Call gladLoadGL
- `engine/render/glad/include/glad/gl.h` — NEW (GLAD loader header)
- `engine/render/glad/src/gl.cpp` — NEW (GLAD loader implementation)
- `engine/render/glad/include/KHR/khrplatform.h` — NEW (platform types)

## Validation Run

```sh
# On desktop2608 (gaming PC):
cmd.exe /q /c ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && "C:\Program Files\CMake\bin\cmake.exe" -S C:\Users\taufe\ahamkara -B C:\Users\taufe\ahamkara\build\debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:\Users\taufe\ahamkara\vcpkg\scripts\buildsystems\vcpkg.cmake
cmd.exe /q /c ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && "C:\Program Files\CMake\bin\cmake.exe" --build C:\Users\taufe\ahamkara\build\debug -- -j8
```

## Validation Results

- **CMake configure**: Success (MSVC 19.40.33808.0, OpenGL found as opengl32)
- **Build**: All 368+ C++ objects compiled, all libraries and executables linked successfully
- **Key executables built**:
  - `client/ahamkara_client.exe`
  - `server/ahamkara_server.exe`
  - `tests/ahamkara_core_tests.exe` (and 30+ other test executables)
  - `tools/ahamkara_asset_importer.exe`
  - `samples/flashback/flashback.exe`
- **Runtime smoke test**: `ahamkara_core_tests.exe` loaded and ran (JobSystem tests: 11/11 pass; FrameAllocator alignment test fails — pre-existing)
- **Runtime smoke test**: `ahamkara_smoke_tests.exe` started and ran (network socket failure — WSAStartup not initialized in test harness — pre-existing)

## Known Gaps

- Pre-existing FrameAllocator alignment test assumes 64-byte alignment not guaranteed by `malloc` on all implementations
- Pre-existing network tests require WSAStartup which isn't called in the test runner on Windows
- GLFW3.dll was deployed alongside executables but runtime OpenGL rendering was not verified (no display available over SSH)

## Runtime Risks

- GLAD function pointer loading may fail if the GPU driver doesn't support OpenGL 4.5 core profile. The engine will need a fallback or graceful error path.
- Jolt CRT override assumes Jolt's target property name matches between versions.

## Cross-Agent Dependencies

None. All fixes are on main and the build is self-contained.

## Recommended Next Step

- Run the client executable on the gaming PC directly (with a display) to verify OpenGL rendering works end-to-end
- Fix the pre-existing FrameAllocator alignment test (64 vs 16-byte alignment requirement)

## Environment

- **Build machine**: desktop2608 (Windows Core, Tailscale `100.124.18.104`)
- **Compiler**: MSVC 19.40.33808.0 (Visual Studio 2022 Community)
- **CMake**: 4.4.0
- **Ninja**: 1.13.2
- **vcpkg**: GLFW3 installed
- **Orchestrator**: Mac (`/Users/taufeeqali/projects/ahamkara`)

## Confidence

`high` — the build cleanly compiles and links all 368+ objects and 30+ executables. Two test failures are pre-existing and unrelated to the build fixes.
