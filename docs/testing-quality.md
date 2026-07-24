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
build-matrix was stabilized in
[#39](https://github.com/tfq26/Project-Ahamkara/issues/39) (closed).

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
