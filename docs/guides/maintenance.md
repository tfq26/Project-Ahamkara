# Maintenance and error repair guide

## Purpose

Use this workflow for build failures, test failures, runtime errors,
regressions, crashes, and dependency-boundary repairs. It separates evidence,
ownership, repair, and validation so a final symptom is not mistaken for the
root cause.

## Before changing code

Record:

- revision and branch;
- operating system/toolchain;
- exact configure/build/run command and preset;
- first failing diagnostic, stable error code, and incident ID if present;
- whether the failure reproduces in a clean build directory;
- smallest target or test that reproduces it.

Do not start from a historical report's status claim. Reports are evidence from
a past revision; current source, tests, and build files are authoritative.

## Establish the failure layer

| Layer | Typical evidence | First owner to inspect |
|---|---|---|
| Configure | CMake option, target, dependency, or install-rule diagnostic | root/module CMake and `CMakePresets.json` |
| Compile | first compiler error and translation unit | owning module public/private include boundary |
| Link | undefined/duplicate symbol and target link line | module `CMakeLists.txt` and dependency direction |
| Unit/integration test | first assertion plus test setup | test and production owner together |
| Runtime initialization | error code/log before main loop | platform, runtime, resource, or service initializer |
| Runtime behavior | reproduction input plus state/log/telemetry | subsystem owning the state transition |
| Crash | crash dump, stack, last stable error/incident | highest trustworthy stack frame in owned code |
| Online/service | client and server incident IDs plus status | Wish/service boundary, then Flashback adapter |

## Reproduce narrowly

Configure once, then build the smallest relevant target:

```sh
./scripts/setup-dev.sh --preset debug
cmake --build --preset debug --target <target>
ctest --test-dir build/debug -R <test-name> --output-on-failure
```

The supported presets are `debug`, `release`, and `debug-headless`.
[src: file: CMakePresets.json:8-73]

Use the full preset after the narrow test passes:

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

CI currently builds debug, release, and debug-headless across its platform
matrix and runs tests for debug/headless jobs.
[src: file: .github/workflows/ci.yml:22-67]

The build matrix itself is under active repair; check
[#39](https://github.com/tfq26/Project-Ahamkara/issues/39) rather than copying
its mutable status into this guide.

## Diagnose using existing facilities

- Use category-specific logging before enabling global trace output. The logger
  supports per-category overrides. [src: file: engine/core/include/ae/core/log.h:37-79]
- Use telemetry for rates, totals, and distributions, not detailed error
  payloads. [src: file: engine/core/include/ae/core/telemetry.h:16-103]
- Use crash dumps for signals/stack frames and diagnostic bundles for system,
  configuration, log-tail, and crash context.
  [src: file: engine/core/include/ae/core/crash_handler.h:21-72]
  [src: file: engine/core/include/ae/core/diagnostics.h:29-66]
- Keep authentication tokens, private addresses, credentials, and sensitive
  backend responses out of logs and issue attachments.

## Repair rules

1. Fix the lowest owner that can express the correct behavior without learning
   about a higher-level product.
2. Do not solve missing contracts with repository-root private include paths.
3. Preserve public error identity while improving messages or internal detail.
4. Add a regression test that fails for the original cause, not merely the
   final symptom.
5. Avoid retries unless the operation is safe to repeat and a bounded backoff
   policy exists.
6. Do not swallow a failure after logging it; return/propagate a typed failure
   or handle it completely at the current boundary.
7. If a failure crosses a support boundary, use the proposed error system in
   [../design/error-system.md](../design/error-system.md).

## Validation ladder

Run only as far as the changed boundary requires, but do not skip a lower rung:

1. focused unit/regression test;
2. owning module target build;
3. related integration test;
4. full debug build and CTest;
5. headless build/test for server-safe code;
6. release build for ABI/optimization-sensitive code;
7. runtime smoke/playtest for presentation or device behavior;
8. out-of-tree consumer build for public package changes.

Report exactly which rungs ran and which did not.

## Documentation maintenance

- Update `systems/` when current behavior changes.
- Update `architecture/` or `decisions.md` when ownership/dependency direction
  changes.
- Update `design/` when an accepted design changes before implementation.
- Update `operations/error-codes.md` when a stable code is added or deprecated.
- Put work state and follow-up acceptance criteria in GitHub Issues, not docs.
- Preserve reports as historical evidence; do not rewrite them to pretend they
  described the current revision.

