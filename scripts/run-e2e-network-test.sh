#!/usr/bin/env bash
#
# run-e2e-network-test.sh — End-to-End Network Integration Test Runner
#
# Builds the project (if needed), then launches the E2E network test harness.
# Usage:
#   ./scripts/run-e2e-network-test.sh [options]
#
# Options:
#   --preset <name>     CMake preset to build (default: debug)
#   --build             Force build before running
#   --no-build          Skip build step (use existing binaries)
#   --num-clients N     Number of client instances (default: 2)
#   --duration N        Test duration in seconds (default: 15)
#   --help              Show this help text
#
# All remaining options are forwarded to the Python test harness.
# See --help for the full list of test harness options.
#
# Environment variables:
#   AHAMKARA_CMAKE_PRESET   Override the default CMake preset
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PRESET="${AHAMKARA_CMAKE_PRESET:-debug}"
BUILD_FIRST="auto"  # auto = build only if binaries missing
PYTHON="${PYTHON:-python3}"

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/run-e2e-network-test.sh [options] [-- <test-harness-options>]

Options:
  --preset <name>     CMake preset to build (default: debug)
  --build             Force build before running
  --no-build          Skip build (use existing binaries)
  --num-clients N     Number of clients (default: 2)
  --duration N        Test duration in seconds (default: 15)
  --help              Show this help

Example:
  # Quick test with 2 clients for 10 seconds
  ./scripts/run-e2e-network-test.sh --num-clients 2 --duration 10

  # Force rebuild and run with custom options
  ./scripts/run-e2e-network-test.sh --build --num-clients 4 --duration 30
EOF
}

# Parse known flags; collect remaining for the Python harness
TEST_HARNESS_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --preset)
            if [ $# -lt 2 ]; then
                echo "Error: --preset requires a value." >&2
                exit 1
            fi
            PRESET="$2"
            shift 2
            ;;
        --build)
            BUILD_FIRST="yes"
            shift
            ;;
        --no-build)
            BUILD_FIRST="no"
            shift
            ;;
        --num-clients)
            if [ $# -lt 2 ]; then
                echo "Error: --num-clients requires a value." >&2
                exit 1
            fi
            TEST_HARNESS_ARGS+=("--num-clients" "$2")
            shift 2
            ;;
        --duration)
            if [ $# -lt 2 ]; then
                echo "Error: --duration requires a value." >&2
                exit 1
            fi
            TEST_HARNESS_ARGS+=("--duration" "$2")
            shift 2
            ;;
        --help)
            print_usage
            exit 0
            ;;
        --)
            shift
            # Remaining args go to test harness
            TEST_HARNESS_ARGS+=("$@")
            break
            ;;
        *)
            # Unknown flags forwarded to test harness
            TEST_HARNESS_ARGS+=("$1")
            shift
            ;;
    esac
done

cd "$PROJECT_DIR"

# ── Determine if build is needed ──────────────────────────────────────────
SERVER_BIN="build/$PRESET/server/ahamkara_server"
CLIENT_BIN="build/$PRESET/client/ahamkara_client"

check_binaries_exist() {
    [ -x "$SERVER_BIN" ] && [ -x "$CLIENT_BIN" ]
}

if [ "$BUILD_FIRST" = "yes" ]; then
    echo "[run-e2e-network-test] Building preset '$PRESET'..."
    cmake --build --preset "$PRESET" --target ahamkara_server ahamkara_client
elif [ "$BUILD_FIRST" = "auto" ] && ! check_binaries_exist; then
    echo "[run-e2e-network-test] Binaries not found, building preset '$PRESET'..."
    cmake --build --preset "$PRESET" --target ahamkara_server ahamkara_client
elif [ "$BUILD_FIRST" = "no" ] && ! check_binaries_exist; then
    echo "[run-e2e-network-test] ERROR: Binaries not found and --no-build specified." >&2
    echo "  Missing: $SERVER_BIN or $CLIENT_BIN" >&2
    exit 1
fi

# ── Run the test harness ──────────────────────────────────────────────────
echo "[run-e2e-network-test] Starting E2E network test..."
echo "[run-e2e-network-test] Preset: $PRESET"
echo "[run-e2e-network-test] Test harness args: ${TEST_HARNESS_ARGS[*]}"

exec "$PYTHON" "$PROJECT_DIR/tests/network_integration/e2e_network_test.py" \
    --build-dir "$PROJECT_DIR/build/$PRESET" \
    "${TEST_HARNESS_ARGS[@]}"
