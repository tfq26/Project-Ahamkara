# Ahamkara Architecture

## Current Milestone

The first milestone is a clean dedicated server and client networking skeleton
with a native window + input layer plus a deliberately small client debug
rendering path. Audio, editor tooling, and fuller platform/rendering backends
stay out so we can validate the networking, movement, and platform foundations
first.

## Module Split

- `engine/core` contains small portable foundation utilities such as types, timing, and logging.
- `engine/network` contains low-level networking utilities that can be reused by multiple runtime
  targets.
- `engine/platform` wraps the native window system and input behind a GLFW3 implementation.
  Game code never includes GLFW headers.
- `engine/runtime` provides lightweight application lifecycle state shared by executables.
- `engine/render` owns the client-only debug renderer. It currently draws fixed primitives with
  OpenGL, builds UI glyph atlases from platform text rasterizers, and intentionally avoids
  materials, shaders, assets, or a render graph.
- `game` contains game-facing types and logic that build on engine APIs.
- `client` owns the playable runtime executable.
- `server` owns the headless dedicated server executable.

## Dependency Direction

- Engine code does not depend on game code.
- Game code depends only on engine APIs.
- Platform-specific implementations should stay isolated inside future backend or platform modules.
- Dedicated server code stays headless and should not depend on renderer, UI, audio, or editor
  modules.
- Debug rendering attaches to the platform window API and is linked only into the client target.
- Text rasterization stays behind renderer-facing backend interfaces so future macOS, Windows, and
  console implementations can feed the same atlas-driven UI path.

## Near-Term Direction

This skeleton is intentionally minimal. It establishes the boundaries that let us add prediction,
reconciliation, interpolation, lag compensation, fuller rendering, and richer platform layers later
without forcing the game layer to talk directly to platform APIs.
