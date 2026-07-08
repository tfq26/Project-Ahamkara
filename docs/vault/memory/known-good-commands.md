# Known Good Commands

Status: Active
Last updated: 2026-06-14

This note records commands that agents have either verified recently or should
try first for Ahamkara work. Update it with date, environment, result, and any
important caveat after meaningful validation.

## Default Agent Checks

### Headless Configure

```sh
./scripts/setup-dev.sh --preset debug-headless
```

Use for remote or non-UI agent work where GLFW/OpenGL is not needed.

### Headless Build

```sh
cmake --build --preset debug-headless
```

Use before claiming C++ compile validation for shared engine/game/server code.

### Headless Tests

```sh
./scripts/run-tests.sh --preset debug-headless
```

Use before claiming test validation for non-rendering changes.

### Full Local Debug Run

```sh
./scripts/start.sh
```

Use for local client/runtime/render verification when a visible window is part
of the behavior under test.

## Log Format

```md
## YYYY-MM-DD - short context

Environment:
Command:
Result:
Notes:
```

## Related

- [Build guide](../../guides/building.md)
- [Build and test map](../systems/build-and-test-map.md)
- [Ahamkara reporting profile](../workflows/ahamkara-reporting-profile.md)
