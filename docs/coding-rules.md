# Coding rules

## Global

- Prefer the smallest owner-correct change; avoid drive-by refactors.
- Every code change includes a test or an explicit explanation of why the
  behavior cannot be automated.
- Never edit generated output or the generated facts block in root
  `AGENTS.md`. [src: file: AGENTS.md:5-15]
- Do not create a second issue tracker under `docs/`; use
  [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
- Public engine code cannot include Flashback or Wish private/product types.
- Logs are observations; recoverable failures must be returned or handled, not
  converted into log-only control flow.

## C++

The project requires C++20 with compiler extensions disabled.
[src: file: CMakeLists.txt:8-19]

- Match the namespace and naming style in the owning module. Engine code uses
  `ae` or a nested `ae::<subsystem>` namespace; product code is transitional
  until the repository split.
- Put public module headers under `engine/<module>/include` and implementation
  under `engine/<module>/src`; publish only the include directory through the
  target. [src: file: engine/core/CMakeLists.txt:17-21]
- Express dependencies in the owning CMake target. Do not add repository-root
  include paths to bypass a missing public contract.
- Use categorized logging and guard expensive debug/trace message construction
  with `log_enabled`. [src: file: engine/core/include/ae/core/log.h:37-79]
- Keep native backend types behind private implementation boundaries unless the
  module's public contract explicitly owns them.
- Use fixed-width serialized types and explicit protocol versions for wire and
  asset formats.

## Shell and CMake

- Shell scripts use fail-fast behavior and resolve paths relative to the script
  or repository root, as the setup/test wrappers do.
  [src: file: scripts/setup-dev.sh:1-5]
  [src: file: scripts/run-tests.sh:1-5]
- Add configurable product variants through explicit CMake options/presets;
  never assume a client-only target exists in a headless build.
- Install only targets that were actually built and validate installed packages
  with an out-of-tree consumer.

## Errors and diagnostics

The stable error model is proposed in
[`design/error-system.md`](design/error-system.md). Until it is implemented,
new failure paths should still separate safe user messages, developer detail,
native codes, and recovery ownership so they can migrate without semantic
loss.

## Tooling status

A canonical repository-wide formatter/linter command is **NOT_FOUND**. Do not
claim formatting or lint validation unless a concrete tool/config is added and
the command is documented in [`testing-quality.md`](testing-quality.md).
