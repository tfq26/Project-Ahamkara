#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Out-of-tree consumer smoke test
#
# Builds the Ahamkara engine SDK (AHAMKARA_ENGINE_ONLY=ON), installs it to a
# temporary prefix, then configures, builds, and runs a standalone consumer
# project that uses only the installed Ahamkara:: targets.
#
# Usage:
#   ./tests/run-out-of-tree-consumer.sh [--preset <name>] [--prefix <path>]
#
#   --preset   CMake configure preset for the engine build (default: engine-only)
#   --prefix   Install prefix (default: /tmp/ahamkara-sdk-prefix)
# -----------------------------------------------------------------------------
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

PRESET="${PRESET:-engine-only}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/tmp/ahamkara-sdk-prefix}"
CONSUMER_BUILD_DIR="/tmp/ahamkara-consumer-build"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --preset)   PRESET="$2";             shift 2 ;;
        --prefix)   INSTALL_PREFIX="$2";      shift 2 ;;
        --help)     echo "Usage: $0 [--preset <name>] [--prefix <path>]"; exit 0 ;;
        *)          echo "Unknown: $1" >&2;   exit 1 ;;
    esac
done

echo "=== Step 1: Configure Ahamkara engine (preset: ${PRESET}) ==="
cmake --preset "${PRESET}" -S "${PROJECT_DIR}"

echo ""
echo "=== Step 2: Build engine ==="
cmake --build --preset "${PRESET}"

echo ""
echo "=== Step 3: Install engine to ${INSTALL_PREFIX} ==="
rm -rf "${INSTALL_PREFIX}"
cmake --install "${PROJECT_DIR}/build/${PRESET}" --prefix "${INSTALL_PREFIX}"

echo ""
echo "=== Step 4: Verify install has no product-owned Flashback/Wish headers ==="
LEAKS=0
for PRODUCT_DIR in game wish samples flashback; do
    if [ -d "${INSTALL_PREFIX}/include/${PRODUCT_DIR}" ]; then
        echo "LEAK: ${INSTALL_PREFIX}/include/${PRODUCT_DIR} should not be in an engine-only install" >&2
        LEAKS=1
    fi
done
# Ensure collision and physics are NOT installed in engine-only mode
if [ -d "${INSTALL_PREFIX}/include/ae/collision" ] || [ -d "${INSTALL_PREFIX}/include/ae/physics" ]; then
    echo "LEAK: collision or physics headers present in engine-only install" >&2
    LEAKS=1
fi
if [ "$LEAKS" -eq 1 ]; then
    echo "FAIL: Install boundary check failed" >&2
    exit 1
fi
echo "Install boundary check: PASS"

echo ""
echo "=== Step 5: Configure out-of-tree consumer against installed SDK ==="
rm -rf "${CONSUMER_BUILD_DIR}"
cmake -S "${SCRIPT_DIR}/out_of_tree_consumer" \
      -B "${CONSUMER_BUILD_DIR}" \
      -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
      -DCMAKE_BUILD_TYPE=Debug

echo ""
echo "=== Step 6: Build consumer ==="
cmake --build "${CONSUMER_BUILD_DIR}"

echo ""
echo "=== Step 7: Run consumer ==="
"${CONSUMER_BUILD_DIR}/ahamkara_consumer"

echo ""
echo "=== All steps passed ==="
