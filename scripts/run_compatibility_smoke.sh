#!/usr/bin/env bash
#
# run_compatibility_smoke.sh
#
# Clean-room cross-product compatibility smoke test runner.
#
# This script simulates the CI compatibility workflow locally:
#   1. Builds and installs Ahamkara + Wish to a temporary prefix.
#   2. Builds the compatibility consumer against that prefix (no source
#      checkout of Ahamkara or Wish — only the consumer test source).
#   3. Runs the smoke binaries.
#   4. Tests that incompatible version combinations are correctly rejected.
#
# Usage:
#   ./scripts/run_compatibility_smoke.sh [--preset <name>]
#
# The default preset is "debug-headless" which exercises the server/headless
# path (no GLFW/OpenGL dependency).  Pass "debug" to also exercise client
# paths if the host has display libraries.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

PRESET="${1:-debug-headless}"
BUILD_DIR="${REPO_DIR}/build/${PRETEST:-compat}"
PREFIX="${BUILD_DIR}/install-prefix"
CONSUMER_SRC="${REPO_DIR}/tests/compatibility_consumer"
CONSUMER_BUILD="${BUILD_DIR}/consumer-smoke"

echo "═══ Cross-Product Compatibility Smoke ═══"
echo "  Repo:     ${REPO_DIR}"
echo "  Preset:   ${PRESET}"
echo "  Prefix:   ${PREFIX}"
echo ""

# ----- Step 0: Clean -----
rm -rf "${BUILD_DIR}" "${PREFIX}" "${CONSUMER_BUILD}"

# ----- Step 1: Build Ahamkara + Wish from source -----
echo "--- Step 1: Build Ahamkara + Wish ---"
cmake --preset "${PRESET}" -S "${REPO_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"

# ----- Step 2: Install to prefix -----
echo "--- Step 2: Install to prefix ---"
cmake --install "${BUILD_DIR}" --prefix "${PREFIX}" --component Ahamkara

echo "Installed artifacts:"
find "${PREFIX}" -type f | head -30

# ----- Step 3: Build consumer against prefix (NO Ahamkara/Wish source) -----
echo "--- Step 3: Build compatibility consumer (clean-room) ---"
cmake -S "${CONSUMER_SRC}" \
      -B "${CONSUMER_BUILD}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="${PREFIX}"

cmake --build "${CONSUMER_BUILD}"

# ----- Step 4: Run consumer smoke -----
echo "--- Step 4: Run compatibility_consumer ---"
"${CONSUMER_BUILD}/compatibility_consumer"

# ----- Step 5: Run Flashback headless smoke -----
echo "--- Step 5: Run flashback_headless_smoke ---"
"${CONSUMER_BUILD}/flashback_headless_smoke"

# ----- Step 6: Test incompatible-version rejection -----
echo "--- Step 6: Test incompatible-version rejection ---"
MISMATCH_BUILD="${CONSUMER_BUILD}-mismatch"
rm -rf "${MISMATCH_BUILD}"

cmake -S "${CONSUMER_SRC}" \
      -B "${MISMATCH_BUILD}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="${PREFIX}" \
      -DAE_ABI_VERSION_OVERRIDE=999 \
      -DWISH_ABI_VERSION_OVERRIDE=999 \
      -DAE_NET_PROTOCOL_OVERRIDE=999 \
      -DWISH_SESSION_PROTOCOL_OVERRIDE=999

if cmake --build "${MISMATCH_BUILD}" 2>&1; then
    echo "FAIL: Incompatible build should have failed but succeeded!"
    exit 1
else
    echo "PASS: Incompatible build correctly rejected."
fi

# ----- Summary -----
echo ""
echo "═══════════════════════════════════════════"
echo "  All cross-product compatibility checks PASSED"
echo "  (compatible build + tests, incompatible rejection)"
echo "═══════════════════════════════════════════"
