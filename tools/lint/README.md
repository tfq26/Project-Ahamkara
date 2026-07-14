# Ahamkara lint gate

The lint gate is the agent-facing definition of a reviewable change. It checks
repository hygiene, C/C++ formatting and static analysis, Python, shell, CMake,
and GitHub Actions.

## Normal agent workflow

From the repository root:

```sh
cmake --preset debug
./scripts/lint.sh --base-ref origin/main --compile-db build/debug
```

`scripts/lint.sh` creates `.venv-lint` and installs the exact tool versions in
`requirements.txt` when necessary. Use `--fix` to apply safe C/C++ and Python
formatting fixes, then rerun without `--fix`.

The default invocation checks uncommitted and untracked files. Useful explicit
scopes are:

```sh
# Everything changed on the current branch, plus local edits
./scripts/lint.sh --base-ref origin/main

# A focused local check
./scripts/lint.sh --paths engine/core/src/example.cpp tests/src/example_tests.cpp

# Whole-repository debt audit
./scripts/lint.sh --all
```

`clang-tidy` requires `compile_commands.json`; the `debug` CMake preset writes
it to `build/debug`. A changed C/C++ source file with no compile command is a
hard failure because it is not covered by the build graph.

## Reports

Every run writes `build/lint/summary.md` and `build/lint/summary.json`, plus raw
per-tool reports. CI publishes that directory as the `ahamkara-lint-report`
artifact even when lint fails and places the Markdown summary on the Actions
run summary.

The normal gate is change-aware: formatter checks are limited to changed C/C++
lines, while semantic checks run against changed source files. `--all` is the
explicit full-tree audit and may expose legacy debt outside an agent's ticket.
Agents must fix failures in their scope rather than expanding exclusions.

