#!/usr/bin/env sh
# run-coverage.sh
#
# Convenience script to build with code coverage instrumentation, run tests,
# and generate coverage reports (HTML + summary).
#
# Prerequisites:
#   - gcovr (install via: pip install gcovr)
#
# Usage:
#   ./scripts/run-coverage.sh [--skip-build] [--open]
#
# Options:
#   --skip-build   Skip the configure+build step, only run tests and report
#   --open         Open the HTML report after generation (macOS: open, Linux: xdg-open)

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

BUILD_DIR="${PROJECT_DIR}/build/coverage"
REPORT_DIR="${BUILD_DIR}/coverage-reports"

SKIP_BUILD=0
OPEN_REPORT=0

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/run-coverage.sh [--skip-build] [--open]

Options:
  --skip-build   Skip the configure+build step, only run tests and report
  --open         Open the HTML report after generation
  --help         Show this help
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1; shift ;;
        --open) OPEN_REPORT=1; shift ;;
        --help) print_usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; print_usage >&2; exit 1 ;;
    esac
done

cd "$PROJECT_DIR"

# Step 1: Configure and build (unless skipped)
if [ "$SKIP_BUILD" -eq 0 ]; then
    echo "=== Configuring with coverage preset ==="
    cmake --preset coverage

    echo "=== Building with coverage instrumentation ==="
    cmake --build --preset coverage
fi

# Step 2: Run tests to generate .gcda files
echo "=== Running tests ==="
ctest --test-dir "$BUILD_DIR" --output-on-failure || echo "Warning: some tests failed (coverage data may be partial)"

# Step 3: Generate coverage reports with gcovr
echo "=== Generating coverage reports ==="
mkdir -p "$REPORT_DIR"

# Check if gcovr is available
if ! command -v gcovr >/dev/null 2>&1; then
    echo "Error: gcovr not found. Install with: pip install gcovr" >&2
    exit 1
fi

# Common gcovr arguments
GCOVR_ARGS="--root \"$PROJECT_DIR\" \
    --gcov-ignore-parse-errors=negative_hits.warn \
    --filter 'engine/' \
    --filter 'client/' \
    --filter 'server/' \
    --filter 'game/' \
    --filter 'wish/' \
    --filter 'tests/' \
    --exclude 'tests/wish_consumer/' \
    --exclude 'tests/out_of_tree_consumer/'"

# Generate HTML report
eval gcovr $GCOVR_ARGS \
    --html --html-details \
    --output "${REPORT_DIR}/coverage.html"

# Generate XML report (for CI/coverage badge services)
eval gcovr $GCOVR_ARGS \
    --xml \
    --output "${REPORT_DIR}/coverage.xml"

# Print summary
echo ""
echo "=== Coverage Summary ==="
eval gcovr $GCOVR_ARGS --print-summary

echo ""
echo "HTML report: ${REPORT_DIR}/coverage.html"
echo "XML report:  ${REPORT_DIR}/coverage.xml"

if [ "$OPEN_REPORT" -eq 1 ]; then
    if command -v xdg-open >/dev/null 2>&1; then
        xdg-open "${REPORT_DIR}/coverage.html"
    elif command -v open >/dev/null 2>&1; then
        open "${REPORT_DIR}/coverage.html"
    else
        echo "Warning: cannot open browser automatically" >&2
    fi
fi
