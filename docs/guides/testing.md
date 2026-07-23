# Testing and Code Coverage

This guide covers how to run tests, measure code coverage, and interpret
coverage reports in the Ahamkara project.

## Running Tests

Tests are built and run via CTest. The project has several CMake presets that
include test targets:

```sh
# Build and run tests for the debug preset
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure

# Build and run tests for the headless preset (no GLFW/OpenGL client)
cmake --preset debug-headless
cmake --build --preset debug-headless
ctest --test-dir build/debug-headless --output-on-failure
```

You can also use the convenience script:

```sh
./scripts/run-tests.sh                     # debug preset
./scripts/run-tests.sh --preset debug-headless
```

## Code Coverage

Code coverage measures which lines of source code are exercised by the test
suite. The project uses **GCC/Clang's `--coverage` instrumentation** combined
with [gcovr](https://gcovr.com/) to produce HTML and XML reports.

### Prerequisites

- **GCC or Clang** compiler (Apple Clang also works)
- **gcovr** ≥ 6.0 (install via `pip install gcovr`)

### Quick Start

The simplest way to build with coverage, run tests, and generate reports is:

```sh
./scripts/run-coverage.sh
```

This will:
1. Configure CMake with the `coverage` preset (`CODE_COVERAGE=ON`)
2. Build all targets with `--coverage` instrumentation
3. Run the full test suite (generates `.gcda` profile data)
4. Generate an **HTML report** at `build/coverage/coverage-reports/coverage.html`
5. Generate an **XML report** at `build/coverage/coverage-reports/coverage.xml`
6. Print a **line-coverage summary** to the terminal

Open the HTML report in your browser to browse coverage by file:

```sh
./scripts/run-coverage.sh --open
```

### Manual Workflow

If you prefer to run the steps separately:

```sh
# 1. Configure with coverage enabled
cmake --preset coverage

# 2. Build (instrumented binaries)
cmake --build --preset coverage

# 3. Run tests (generates .gcda files)
ctest --test-dir build/coverage --output-on-failure

# 4. Generate reports with gcovr
mkdir -p build/coverage/coverage-reports

# Use a common set of gcovr arguments to reduce repetition
GCOVR_ARGS="--root . \
    --gcov-ignore-parse-errors=negative_hits.warn \
    --filter 'engine/' \
    --filter 'client/' \
    --filter 'server/' \
    --filter 'game/' \
    --filter 'wish/' \
    --filter 'tests/' \
    --exclude 'tests/wish_consumer/' \
    --exclude 'tests/out_of_tree_consumer/'"

# HTML report (detailed, per-file browsable pages)
eval gcovr $GCOVR_ARGS \
    --html --html-details \
    --output build/coverage/coverage-reports/coverage.html

# XML report (consumable by CI and badge services)
eval gcovr $GCOVR_ARGS \
    --xml \
    --output build/coverage/coverage-reports/coverage.xml

# Terminal summary only
eval gcovr $GCOVR_ARGS --print-summary
```

> **Why the `--filter` and `--exclude` flags?**
> The `--coverage` flag instruments *all* compiled code including external
> dependencies (JoltPhysics, glm, EnTT, etc.). The `--filter` flags restrict
> the report to first-party Ahamkara source directories. The `--exclude` flags
> skip test consumer fixtures that are not part of the main project.

### CI Coverage Job

The CI pipeline (`.github/workflows/ci.yml`) contains a `coverage` job that:

1. Configures and builds with the `coverage` preset
2. Runs the test suite
3. Generates HTML and XML coverage reports via `gcovr`
4. Uploads the reports as a **build artifact** named `ahamkara-coverage-report`

You can download the coverage artifact from the GitHub Actions page for any
pull request or push to `main`/`develop`.

### Coverage Badge

A coverage badge is displayed in the repository `README.md`. The badge uses a
dynamic shields.io endpoint that can be pointed at a hosted XML coverage report
or a CI summary output. See the badge URL in `README.md` for the current
configuration.

### Interpreting Coverage Reports

- **Line coverage**: percentage of executable source lines that were hit during
  the test run
- **Branch coverage**: percentage of decision points (if/else, ternary, loop
  conditions) that took both true/false paths
- **Function coverage**: percentage of functions that were invoked

Focus on increasing coverage in areas with complex logic (movement, collision,
networking, game systems). Aim for at least 70-80% line coverage in new code.

### Troubleshooting

**`gcovr: command not found`**
→ Install with `pip install gcovr`.

**Coverage report shows 0% for all files**
→ The `--coverage` flag may not have been applied. Verify that
  `CODE_COVERAGE=ON` was set during CMake configuration and that the compiler
  is GCC or Clang.

**`.gcda` files are missing after running tests**
→ The test executable may have crashed or been killed before it could flush
  coverage data. Run tests individually to isolate the issue. Ensure the
  working directory is writable.

**Report includes third-party code (Jolt, glm, etc.)**
→ Adjust the `--filter` arguments to `gcovr` to restrict coverage to first-party
  directories only.
