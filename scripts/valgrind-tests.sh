#!/usr/bin/env bash
#
# valgrind-tests.sh — Run test executables under Valgrind memcheck
#
# Builds the specified CMake preset (default: debug-headless), then runs
# every CTest target under Valgrind.  Tests linked to the renderer
# (ae_render) are skipped because they require a GLX/Wayland display that
# Valgrind cannot provide on CI runners.
#
# Usage:
#   ./scripts/valgrind-tests.sh [--preset <name>] [--build] [--help]
#
# Options:
#   --preset <name>   CMake preset to build/test (default: debug-headless)
#   --build           Rebuild before testing
#   --help            Show this help
#
# Environment:
#   VALGRIND_OPTS      Extra flags to pass to valgrind (optional)
#   AHAMKARA_SKIP_LEAK_CHECK  Set to "1" to skip leak checks (only check
#                             invalid accesses / undefined behaviour)
#
# Exit code:
#   0  — all tests passed Valgrind cleanly
#   1  — one or more tests failed Valgrind
#   2  — valgrind not found
#   3  — build/configure failure
#
# Requires: cmake, ninja, valgrind
# ----------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PRESET="${AHAMKARA_CMAKE_PRESET:-debug-headless}"
BUILD_FIRST=0

# ---- Argument parsing ------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --build)
            BUILD_FIRST=1
            shift
            ;;
        --help)
            cat <<'EOF'
Usage: ./scripts/valgrind-tests.sh [--preset <name>] [--build] [--help]

Run all test executables under Valgrind memcheck.

Options:
  --preset <name>   CMake preset (default: debug-headless)
  --build           Rebuild before testing
  --help            Show this help
EOF
            exit 0
            ;;
        *)
            echo "Error: unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

# ---- Dependency check ------------------------------------------------
if ! command -v valgrind &>/dev/null; then
    echo "ERROR: valgrind not found. Install it first:" >&2
    echo "  sudo apt install valgrind   # Debian/Ubuntu" >&2
    echo "  sudo dnf install valgrind   # Fedora" >&2
    echo "  brew install valgrind       # macOS (may not support latest version)" >&2
    exit 2
fi

VALGRIND_VERSION=$(valgrind --version 2>&1)
echo "Using Valgrind: ${VALGRIND_VERSION}"

# ---- Build -----------------------------------------------------------
cd "$PROJECT_DIR"

if [ "$BUILD_FIRST" -eq 1 ] || [ ! -d "build/${PRESET}" ]; then
    echo "=== Configuring preset: ${PRESET} ==="
    cmake --preset "$PRESET" || { echo "Configure failed"; exit 3; }

    echo "=== Building preset: ${PRESET} ==="
    cmake --build --preset "$PRESET" || { echo "Build failed"; exit 3; }
fi

BUILD_DIR="${PROJECT_DIR}/build/${PRESET}"
echo "Build directory: ${BUILD_DIR}"

# ---- Valgrind base flags ---------------------------------------------
VALGRIND_CMD=(valgrind)

# Base memcheck flags
VALGRIND_FLAGS=(
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,indirect,possible
    --track-origins=yes
    --errors-for-leak-kinds=definite,indirect,possible
    --suppressions="${SCRIPT_DIR}/valgrind-suppressions.txt"
    --gen-suppressions=all
    --error-exitcode=1
)

# When skip-leak-check is requested, still check for invalid accesses
if [[ "${AHAMKARA_SKIP_LEAK_CHECK:-0}" == "1" ]]; then
    VALGRIND_FLAGS=(
        --tool=memcheck
        --leak-check=no
        --track-origins=yes
        --error-exitcode=1
    )
fi

# Append user-supplied extra flags
if [[ -n "${VALGRIND_OPTS:-}" ]]; then
    IFS=' ' read -ra EXTRA_FLAGS <<< "$VALGRIND_OPTS"
    VALGRIND_FLAGS+=("${EXTRA_FLAGS[@]}")
fi

echo "=== Valgrind flags: ${VALGRIND_FLAGS[*]} ==="

# ---- Discover test executables ---------------------------------------
# Enumerate CTest tests and find their corresponding executables in the
# build tree.  We use `ctest -N` for the authoritative test list, then
# resolve executable paths via direct lookup in the build directory.
#
# Only C++ test executables are run (Python-based tests and CMake build
# regression targets are skipped).

echo "=== Discovering test executables ==="

declare -a TEST_NAMES=()
declare -a TEST_EXES=()

# Get test names from ctest -N (non-interactive listing)
CTEST_LIST=$("${BUILD_DIR}/bin/ctest" -N --test-dir "${BUILD_DIR}" 2>/dev/null || \
             ctest -N --test-dir "${BUILD_DIR}" 2>/dev/null || true)

if [[ -z "$CTEST_LIST" ]]; then
    echo "  (ctest -N failed; falling back to direct binary search)"
    for exe in "${BUILD_DIR}"/tests/ahamkara_*; do
        if [[ -x "$exe" ]] && [[ ! "$exe" == *".exe" ]]; then
            name=$(basename "$exe")
            TEST_NAMES+=("$name")
            TEST_EXES+=("$exe")
        fi
    done
else
    # Parse: "Test #X: <test_name>" lines from ctest -N output
    while IFS= read -r line; do
        if [[ "$line" =~ Test\ #[0-9]+:\ (.+) ]]; then
            test_name="${BASH_REMATCH[1]}"
            # Skip non-C++ tests
            case "$test_name" in
                agent_runner_python_tests|lint_runner_python_tests)
                    continue ;;
                ahamkara_install_rules_smoke|ahamkara_out_of_tree_package_consumer)
                    continue ;;
                ahamkara_controller_mapper_build_regression|ahamkara_server_build_regression)
                    continue ;;
            esac
            # Look for the executable — CMake typically puts test binaries
            # in the tests/ subdirectory of the build tree
            test_exe=""
            for candidate in \
                "${BUILD_DIR}/tests/${test_name}" \
                "${BUILD_DIR}/tests/${test_name}/${test_name}" \
                "${BUILD_DIR}/src/${test_name}/${test_name}" \
                "${BUILD_DIR}/tools/${test_name}/${test_name}"; do
                if [[ -x "$candidate" ]]; then
                    test_exe="$candidate"
                    break
                fi
            done
            # If not found by name, search the build tree
            if [[ -z "$test_exe" ]]; then
                test_exe=$(find "${BUILD_DIR}" -name "${test_name}" -type f -executable 2>/dev/null | head -1 || true)
            fi

            if [[ -n "$test_exe" ]] && [[ -x "$test_exe" ]]; then
                TEST_NAMES+=("$test_name")
                TEST_EXES+=("$test_exe")
            else
                echo "  [SKIP] ${test_name} — executable not found in build tree"
            fi
        fi
    done <<< "$CTEST_LIST"
fi

if [[ ${#TEST_EXES[@]} -eq 0 ]]; then
    echo "ERROR: no test executables found in build tree." >&2
    exit 1
fi

echo "Discovered ${#TEST_EXES[@]} test executables:"
for i in "${!TEST_NAMES[@]}"; do
    echo "  [${i}] ${TEST_NAMES[$i]} → ${TEST_EXES[$i]}"
done

# ---- Run each test under Valgrind ------------------------------------
echo ""
echo "============================================"
echo " Running tests under Valgrind memcheck"
echo "============================================"

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
FAILED_TESTS=""

# Tests known to require a display (ae_render) — skip in headless CI
GUI_TESTS=(
    "ahamkara_window_input_provider_tests"
    "ahamkara_level_render_tests"
    "ahamkara_lod_batching_tests"
    "ahamkara_ibl_tests"
    "ahamkara_ssao_tests"
    "ahamkara_color_grading_tests"
    "ahamkara_asset_pipeline_tests"
)

for i in "${!TEST_NAMES[@]}"; do
    test_name="${TEST_NAMES[$i]}"
    test_exe="${TEST_EXES[$i]}"

    # Skip GUI tests in headless mode
    skip_test=0
    for gui_test in "${GUI_TESTS[@]}"; do
        if [[ "$test_name" == "$gui_test" ]]; then
            echo ""
            echo "━━━ [SKIP] ${test_name} — requires display (ae_render) ━━━"
            SKIP_COUNT=$((SKIP_COUNT + 1))
            skip_test=1
            break
        fi
    done
    if [[ $skip_test -eq 1 ]]; then
        continue
    fi

    echo ""
    echo "━━━ [RUN] ${test_name} ━━━"
    echo "  Executable: ${test_exe}"

    set +e
    "${VALGRIND_CMD[@]}" "${VALGRIND_FLAGS[@]}" "$test_exe" 2>"${BUILD_DIR}/valgrind-${test_name}.log"
    exit_code=$?
    set -e

    if [[ $exit_code -eq 0 ]]; then
        echo "  ✅ PASS (exit code: ${exit_code})"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "  ❌ FAIL (exit code: ${exit_code})"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        FAILED_TESTS="${FAILED_TESTS} ${test_name}"
    fi

    # Print Valgrind summary (last ~25 lines of the log)
    if [[ -f "${BUILD_DIR}/valgrind-${test_name}.log" ]]; then
        echo "  --- Valgrind report (tail) ---"
        # Extract the ERROR SUMMARY line and any leak summaries
        grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost|still reachable|HEAP SUMMARY|LEAK SUMMARY|Invalid |Conditional )" \
            "${BUILD_DIR}/valgrind-${test_name}.log" 2>/dev/null \
            | head -20 \
            | sed 's/^/    /'
        echo "  --- End report ---"
        echo "  Full log: build/${PRESET}/valgrind-${test_name}.log"
    fi
done

# ---- Summary ---------------------------------------------------------
echo ""
echo "============================================"
echo " Valgrind Test Summary"
echo "============================================"
echo "  Passed:  ${PASS_COUNT}"
echo "  Failed:  ${FAIL_COUNT}"
echo "  Skipped: ${SKIP_COUNT}"
echo "  Total:   $((PASS_COUNT + FAIL_COUNT + SKIP_COUNT))"

if [[ -n "$FAILED_TESTS" ]]; then
    echo ""
    echo "  FAILED TESTS:${FAILED_TESTS}"
    echo ""
    echo "  To inspect a specific Valgrind log:"
    echo "    cat build/${PRESET}/valgrind-<test-name>.log"
    echo ""
    echo "  To reproduce a single test under Valgrind:"
    echo "    valgrind --tool=memcheck --leak-check=full \\"
    echo "      --suppressions=scripts/valgrind-suppressions.txt \\"
    echo "      build/${PRESET}/tests/<test-name>"
fi

# Exit with failure if any test failed
if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi

exit 0
