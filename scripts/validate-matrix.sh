#!/usr/bin/env sh
# validate-matrix.sh
#
# Builds all CMake presets and runs tests for testable configurations.
# Reports a pass/fail matrix summary.
#
# Usage:
#   ./scripts/validate-matrix.sh [--skip-configure]
#
# Options:
#   --skip-configure   Only build + test, skip cmake --preset configure step
#   --help             Show this help and exit

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

SKIP_CONFIGURE=0

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/validate-matrix.sh [--skip-configure]

Options:
  --skip-configure   Only build + test, skip cmake --preset configure step
  --help             Show this help and exit
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --skip-configure)
            SKIP_CONFIGURE=1
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Error: unknown option: $1" >&2
            print_usage >&2
            exit 1
            ;;
    esac
done

cd "$PROJECT_DIR"

# ---------------------------------------------------------------------------
# Matrix definition
# ---------------------------------------------------------------------------
# Format: "preset_name:requires_tests:build_only_note"
#   requires_tests = 1  -> run ctest after build
#   requires_tests = 0  -> build only (no test targets)
#   build_only_note     -> human-readable reason
MATRIX="\
debug:1:
release:0:release builds lack GLFW/OpenGL for client tests
debug-headless:1:"
MATRIX_PATH="/tmp/validate-matrix-results.$$.txt"
trap 'rm -f "$MATRIX_PATH"' EXIT INT TERM

# Always succeeds — we track results per row.
overall_pass=0

echo "============================================"
echo " Ahamkara Build Matrix Validation"
echo "============================================"
echo ""

echo "$MATRIX" | while IFS=':' read -r preset run_tests note; do
    [ -z "$preset" ] && continue
    preset=$(echo "$preset" | xargs)

    echo "---"
    echo "[$preset] Configuring..."

    if [ "$SKIP_CONFIGURE" -eq 0 ]; then
        if ! cmake --preset "$preset" 2>&1; then
            echo "[$preset] CONFIGURE FAILED" >> "$MATRIX_PATH"
            continue
        fi
    fi

    echo "[$preset] Building..."
    if ! cmake --build --preset "$preset" 2>&1; then
        echo "[$preset] BUILD FAILED" >> "$MATRIX_PATH"
        continue
    fi

    echo "[$preset] Build OK."

    if [ "$run_tests" = "1" ]; then
        echo "[$preset] Running tests..."
        build_dir="build/$preset"
        if [ -d "$build_dir" ]; then
            if ctest --test-dir "$build_dir" --output-on-failure 2>&1; then
                echo "[$preset] TESTS PASSED" >> "$MATRIX_PATH"
            else
                echo "[$preset] TESTS FAILED" >> "$MATRIX_PATH"
            fi
        else
            echo "[$preset] TESTS SKIPPED (no build directory)" >> "$MATRIX_PATH"
        fi
    else
        if [ -n "$note" ]; then
            echo "[$preset] TESTS SKIPPED ($note)" >> "$MATRIX_PATH"
        else
            echo "[$preset] TESTS SKIPPED" >> "$MATRIX_PATH"
        fi
    fi
done

# Collect results
echo ""
echo "============================================"
echo " Validation Matrix Summary"
echo "============================================"

echo "$MATRIX" | while IFS=':' read -r preset _ _; do
    [ -z "$preset" ] && continue
    preset=$(echo "$preset" | xargs)
    result=$(grep "^\\[$preset\\]" "$MATRIX_PATH" 2>/dev/null || echo "[$preset] UNKNOWN")
    echo "  $result"
done

echo "============================================"

# Check overall status from saved results
if [ -f "$MATRIX_PATH" ]; then
    while IFS= read -r line; do
        case "$line" in
            *FAILED*) overall_pass=1 ;;
        esac
    done < "$MATRIX_PATH"
fi

exit "$overall_pass"
