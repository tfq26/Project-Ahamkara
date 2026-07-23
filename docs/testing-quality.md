# Testing and quality

## Supported build presets

The repository defines `debug`, `release`, and `debug-headless` configure/build
presets. The headless preset disables client and sample targets.
[src: file: CMakePresets.json:8-73]

```sh
./scripts/setup-dev.sh --preset debug
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

For server-safe changes:

```sh
./scripts/setup-dev.sh --preset debug-headless
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

The test wrapper runs CTest from `build/<preset>` and supports `--build`.
[src: file: scripts/run-tests.sh:1-55]

## Test ownership

CTest targets are declared in `tests/CMakeLists.txt` and cover core utilities,
logging, runtime boundaries, networking, collision, gameplay, assets,
telemetry, crash handling, diagnostics, and product integration.
[src: file: tests/CMakeLists.txt:1-445]

Place a regression test at the narrowest boundary that proves the defect:

| Change | Required evidence |
|---|---|
| Core value/type/algorithm | Focused unit test linked to the owning engine target |
| Public module contract | Unit tests plus a consumer or integration compile test |
| Network protocol/state | Serialization/state-machine tests and malformed-input path |
| Renderer/platform/audio device behavior | Logic test where possible plus runtime smoke evidence |
| Game behavior | Deterministic simulation test and relevant local/server integration |
| Wish service behavior | Unit test with backend fixture plus fail-closed integration path |
| Bug fix | Regression test that fails for the original cause |
| Documentation-only change | Link/citation/path validation; compile only when commands or APIs are changed |

## CI contract

CI configures and builds its debug/release/headless platform matrix, runs CTest
for debug and debug-headless jobs, and has a separate package job.
[src: file: .github/workflows/ci.yml:22-88]

The existence of a CI job is not proof that the current branch is green. The
mutable build-matrix status is tracked in
[#39](https://github.com/tfq26/Project-Ahamkara/issues/39).

## Completion bar

- The focused regression passes.
- The owning target builds.
- Related integration tests pass.
- The full relevant preset passes, or the exact pre-existing blocker is linked.
- Runtime-only behavior is observed when automated tests cannot prove it.
- Public package changes pass an out-of-tree consumer build.
- Validation claims list the commands actually run.

## Quality tooling

A canonical repository-wide lint/format command is **NOT_FOUND** as of the
2026-07-13 documentation audit. Do not invent one. Until tooling is adopted,
match adjacent code and treat compiler warnings, tests, CMake boundaries, and
review as the enforceable checks.

## Memory leak detection

The project integrates Valgrind memcheck for detecting memory leaks and
invalid memory accesses. A CI job (`valgrind` in `.github/workflows/ci.yml`)
runs all headless test executables under Valgrind.

### Running locally

Prerequisites: [Valgrind](https://valgrind.org/) must be installed on your
system (Linux only).

```sh
# Quick run with existing build:
./scripts/valgrind-tests.sh --preset debug-headless

# Build and run:
./scripts/valgrind-tests.sh --preset debug-headless --build
```

The script:

1. Builds the specified CMake preset (default: `debug-headless`).
2. Each C++ test executable runs under `valgrind --tool=memcheck`.
3. Tests that require a display (ae_render targets) are automatically
   skipped.
4. Per-test Valgrind logs are written to
   `build/<preset>/valgrind-<test-name>.log`.
5. The script exits with a non-zero code if any test reports leaks or
   invalid accesses.

### Valgrind suppressions

Known false positives from third-party libraries (JoltPhysics, GLM, EnTT,
miniaudio, OpenGL drivers, GLFW, system audio, libstdc++) are suppressed in
[scripts/valgrind-suppressions.txt](../scripts/valgrind-suppressions.txt).

Before adding a new suppression:
1. Confirm the leak originates **outside** this project's control.
2. Verify it is not a bug in the project's usage of the library.
3. Add the suppression entry above the project-specific section marker.

### CTest integration

CTest's built-in memory checking is configured via `CTestConfig.cmake` in the
project root. Run it directly:

```sh
ctest -T memcheck --test-dir build/debug-headless
```

This produces XML output in `build/debug-headless/DynamicAnalysis/` that can
be parsed by CI tooling.

### CI non-blocking

The Valgrind CI job currently uses `continue-on-error: true`, meaning the
overall CI status is not affected by Valgrind failures. This allows
incremental leak fixes without blocking other work. Once all known leaks are
resolved and the suppressions file is stable, remove `continue-on-error` to
make the Valgrind job blocking.

### Windows (future)

Dr.Memory support for Windows CI is not yet implemented. Contributions are
welcome.
