# Contributing to Ahamkara

## Reporting Issues

File issues on [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).
Include:

- OS, compiler version, and CMake preset used
- Steps to reproduce
- Expected vs actual behavior
- Relevant log output or error messages

## Submitting Changes

### Branch naming

```
agent/<name>/<task>     # Agent-authored branches
feature/<name>          # Feature branches
fix/<description>       # Bug fixes
```

### Workflow

1. Create a branch from `main`:
   ```sh
   git fetch forgejo main
   git checkout -b agent/<name>/<task> forgejo/main
   ```

2. Make changes following [coding standards](docs/coding-rules.md).

3. Build and test:
   ```sh
   cmake --preset debug
   cmake --build --preset debug
   ctest --test-dir build/debug --output-on-failure
   ```

   For headless-only work (no GLFW/OpenGL):
   ```sh
   cmake --preset debug-headless
   cmake --build --preset debug-headless
   ctest --test-dir build/debug-headless --output-on-failure
   ```

4. Lint your changes:
   ```sh
   ./scripts/setup-lint.sh
   ./scripts/lint.sh --base-ref origin/main --compile-db build/debug
   ```

5. Rebase onto latest `main` and push:
   ```sh
   git fetch forgejo main
   git rebase forgejo/main
   git push --force-with-lease forgejo HEAD
   ```

6. Open a pull request against `main`.

### PR requirements

- Every code change includes a test or an explanation of why testing is infeasible.
- All tests pass.
- Lint is clean on the branch diff.
- PRs should be scoped to a single concern.

## Coding Standards

See [`docs/coding-rules.md`](docs/coding-rules.md) for the full standard. Key points:

- **C++20** with compiler extensions disabled.
- Engine code uses `ae` or `ae::<subsystem>` namespace.
- Public headers go under `engine/<module>/include`, implementation under `engine/<module>/src`.
- Declare dependencies in CMake targets — do not add repo-root include paths.
- Use categorized logging; guard expensive debug message construction.
- Follow existing patterns and idioms in the module you're touching.
- Do not edit auto-generated files.

## Testing

Tests live in `tests/src/` and are registered in `tests/CMakeLists.txt`.
Every test is a standalone C++20 executable that returns zero on success.

- Name test executables as `ahamkara_<subsystem>_tests`.
- Place test source files alongside the source they test, following existing naming.
- Use the convention `ninja -C build/debug && ctest --test-dir build/debug -R <test_name> --output-on-failure` when iterating.

## Linting

The lint toolchain is managed by `./scripts/setup-lint.sh` and `./scripts/lint.sh`.
It runs hygiene checks, clang-format, clang-tidy, ruff (Python), shellcheck, actionlint, and cmake-lint.

- Lint before opening a PR.
- Do not broaden lint exclusions or weaken checks to make a change pass.
- Legitimate legacy debt should be recorded in a GitHub issue.

## Communication

- Use [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues) for work state, priority, and acceptance criteria.
- Architecture decisions are documented in [`docs/decisions.md`](docs/decisions.md).
- Real-time discussion happens through issue comments and PR reviews.
