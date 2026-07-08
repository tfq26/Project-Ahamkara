# Build And Test Map

Status: Seed

This map orients agents before they touch build files, tests, or validation
claims.

## Canonical Docs

- [Build guide](../../guides/building.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Known good commands](../memory/known-good-commands.md)

## Main Files

- `CMakeLists.txt`
- `CMakePresets.json`
- `client/CMakeLists.txt`
- `engine/*/CMakeLists.txt`
- `game/CMakeLists.txt`
- `server/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tools/CMakeLists.txt`
- `scripts/setup-dev.sh`
- `scripts/run-tests.sh`
- `scripts/start.sh`

## Default Validation

For most non-UI code changes:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

For local client/render/runtime work:

```sh
cmake --build --preset debug
./scripts/start.sh
```

## Risks

- CMake target changes can silently omit files if not validated by a clean or
  relevant preset build.
- Full client runs may require local graphics/windowing dependencies.
- Report validation claims must name exact commands.
