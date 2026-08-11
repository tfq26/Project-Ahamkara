# Headless Ahamkara and Flashback workflow

## Purpose

Ahamkara is developed and tested headlessly on the server. Flashback is the
graphical reference consumer used locally for interactive and visual testing.
The engine build exposes Flashback's headless game-module tests through
`AHAMKARA_BUILD_FLASHBACK_CONTRACT_TESTS` while the graphical executable remains
behind the client/sample build path. [src: file: CMakeLists.txt:108-186]

## Engine-side workflow

Use the server or another display-free environment for engine work:

```sh
cmake --preset debug-headless
cmake --build --preset debug-headless
ctest --test-dir build/debug-headless --output-on-failure
```

The headless preset disables the graphical client and sample executable, while
the Flashback module contract tests remain enabled. [src: file: CMakePresets.json:30-42]

The public game boundary is `ae::IGameModule`. Its API version and host
capabilities allow new engine services to be added incrementally and detected
by consumers instead of requiring a rewrite. [src: file: engine/runtime/include/ae/runtime/game_module.h:11-48]

## Local Flashback workflow

Build the graphical preset on the local machine, where a display is available:

```sh
cmake --preset debug
cmake --build --preset debug
./scripts/run_flashback_smoke.sh
```

The smoke script checks the executable and compiled level, starts Flashback in
autoplay mode, and leaves the display check to the local operator. It supports
`FLASHBACK_EXECUTABLE`, `FLASHBACK_BUILD_DIR`, and `FLASHBACK_LEVEL` overrides.
[src: file: scripts/run_flashback_smoke.sh:1-32]

## Compatibility rules

Flashback-specific gameplay stays in `samples/flashback`; reusable systems stay
in the engine. Engine APIs should be extended additively, versioned when their
contract changes, and kept compatible through adapters during migration.
Flashback's standalone consumer build remains the clean-room SDK check.
[src: file: samples/flashback/standalone/CMakeLists.txt:1-30]

Every engine change must pass the headless suite and Flashback contract tests
before review. Local graphical testing is required when the change affects
rendering, input, audio, assets, animation, or other display-facing behavior.
