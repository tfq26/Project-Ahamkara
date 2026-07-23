# Editor Architecture

## Overview

The Ahamkara Editor (`ahamkara_editor`) is an in-engine editing tool that
launches a windowed application with a dockable ImGui interface.  It reuses
existing engine modules—`ae_platform` (GLFW windowing) and `ae_ui` (ImGui
integration)—so that the editor rides the same rendering and event pipelines
as the game client.

## Directory Layout

```
editor/
├── CMakeLists.txt                    # Build target ahamkara_editor
├── include/
│   └── ahamkara/
│       └── editor/
│           └── editor_window.h       # EditorWindow public API
└── src/
    ├── main.cpp                      # Executable entry point
    └── editor_window.cpp             # EditorWindow implementation
```

## Build Integration

The editor is registered in the root `CMakeLists.txt` under the
`AHAMKARA_BUILD_CLIENT` guard, because it depends on OpenGL, GLFW, and
ImGui—the same dependencies required by the game client.  Building without
`AHAMKARA_BUILD_CLIENT` (e.g. the `debug-headless` preset) skips the editor
automatically.

A typical debug build:

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/editor/ahamkara_editor
```

## Architecture

### `EditorWindow` (class)

| Responsibility | Implementation |
|---|---|
| Window lifecycle | Owns an `ae::PlatformWindow` (GLFW) created with an OpenGL context. |
| UI initialisation | Calls `ae::ui::initialize_ui()` to set up ImGui atop the GLFW window. |
| Event loop | Polls OS events via `window_->poll_events()`, then renders an ImGui frame. |
| Rendering | Calls `ae::ui::sync_input_to_imgu()`, `ae::ui::begin_ui_frame()` to start a frame, draws ImGui widgets, then calls `ae::ui::end_ui_frame()` and `glfwSwapBuffers()`. |

### Module dependencies

```
ahamkara_editor
├── ahamkara_editor_lib (static)
│   ├── ae_core       — logging, types, version
│   ├── ae_platform   — GLFW window, event polling
│   └── ae_ui         — ImGui context, rendering, input sync
└── main.cpp          — entry point, CLI parsing
```

### Docking layout

The editor creates a full-viewport "DockSpace" window that acts as the root
docking node.  Panels (Engine Info, Scene, Inspector, etc.) are docked into
this space using ImGui's `DockSpace()` API.  This allows users to rearrange
panels freely.

### Version info

Engine version information is currently defined as local constants in
`editor_window.cpp`.  When the engine grows a formal version API, the
editor will consume it from `ae_core` instead.

## Current capabilities

- Window creation (GLFW + OpenGL 3.3 context)
- ImGui initialisation and frame loop
- DockSpace root window for future panel docking
- Engine Info panel showing engine/editor/ImGui versions
- Quit button
- ImGui Demo window (useful during development)

## Future work

- Connect to the engine runtime (scene graph, entity list, asset browser)
- Replace local version constants with a shared engine version API
- Add tool panels: Scene Viewport, Entity Inspector, Asset Browser, Console
- Implement a menubar with File/Edit/View/Help menus
- Support runtime shader/material hot-reloading
