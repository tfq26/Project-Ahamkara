# Debug and operations

## Baseline commands

Configure and verify the development environment:

```sh
./scripts/setup-dev.sh --preset debug
./scripts/setup-dev.sh --preset debug-headless
```

The setup script verifies `git`, `cmake`, `ninja`, and a C++ compiler before
running the selected CMake preset. [src: file: scripts/setup-dev.sh:1-89]

Build and test:

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug

cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

`run-tests.sh` invokes CTest in `build/<preset>` and can build first with
`--build`. [src: file: scripts/run-tests.sh:1-55]

Run supported modes:

```sh
./scripts/start.sh local
./scripts/start.sh flashback
./scripts/start.sh network -- 127.0.0.1
./scripts/start.sh server
./scripts/start.sh sandbox
```

The complete mode list and argument forwarding are defined by `start.sh`.
[src: file: scripts/start.sh:1-119]

## Increase logging

```sh
AE_LOG_LEVEL=debug ./build/debug/client/ahamkara_client
AE_LOG=Network:trace,Render:debug ./build/debug/client/ahamkara_client
```

The logger supports global and per-category runtime levels through
`AE_LOG_LEVEL` and `AE_LOG`. [src: file: engine/core/include/ae/core/log.h:48-79]
File logging writes `logs/ahamkara.log` after initialization.
[src: file: engine/core/src/log.cpp:168-178]

Do not leave trace logging enabled as a permanent fix. Capture the failing
window, retain the relevant incident/error identity, and return to a normal
level after diagnosis.

## Diagnostic artifacts

| Artifact | Default location | Contents |
|---|---|---|
| Runtime log | `logs/ahamkara.log` | Categorized log stream |
| Crash dump | `crashes/` | Signal, fault address, timestamp, and stack frames |
| Diagnostic bundle | `diagnostics/` | System info, config dump, log tail, and crash summary |

Crash and diagnostic defaults are defined by the core APIs.
[src: file: engine/core/include/ae/core/crash_handler.h:46-72]
[src: file: engine/core/include/ae/core/diagnostics.h:29-66]

Build the diagnostics tool and run it from the repository root when a failure
needs a shareable bundle:

```sh
cmake --build --preset debug --target ahamkara_diagnostics
./build/debug/tools/ahamkara_diagnostics
```

The target calls `write_diagnostic_bundle()`.
[src: file: tools/CMakeLists.txt:50-55]
[src: file: tools/diagnostics/diagnostics_tool.cpp:105-132]

Review a bundle before sharing it. The proposed error system requires explicit
redaction tests, but the current diagnostic implementation should not be
assumed to remove every sensitive value.

## Triage order

1. Record the exact command, preset, revision, platform, visible error code,
   and incident ID.
2. Check whether configuration failed, compilation failed, a test failed, or a
   runtime path failed; do not mix these layers.
3. Reproduce with the smallest owning target or test.
4. Enable only the relevant log categories.
5. Inspect the first causal failure rather than the final cascade.
6. Make the smallest owner-correct fix and add a regression test.
7. Run the narrow test, then the full relevant preset.
8. If the failure represents a reusable support boundary, add or refine an
   error code according to [error-codes.md](error-codes.md).

See [the maintenance guide](../guides/maintenance.md) for the full repair
workflow.
