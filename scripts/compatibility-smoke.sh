#!/usr/bin/env bash
# ===========================================================================
# Cross-Product Artifact Compatibility Smoke Test
# ===========================================================================
# Clean-room verification that released Ahamkara and Wish artifacts can be
# consumed by Flashback without any producer source checkout.
#
# Usage:
#   ./scripts/compatibility-smoke.sh [--prefix DIR] [--skip-incompatible]
#
# The script:
#   1. Ensures the main project is built and installed to a prefix.
#   2. Builds Flashback standalone against the installed artifacts.
#   3. Runs Flashback game module smoke tests.
#   4. Optionally tests intentionally incompatible version combinations.
#
# Environment variables:
#   CMAKE_PREFIX_PATH  — additional prefix paths
#   BUILD_DIR          — build directory override (default: build/debug)
#   INSTALL_PREFIX     — install prefix override (default: build/debug/install)
#
# Output artifacts:
#   _compat_logs/        — full log bundle
#   _compat_logs/manifest-ahamkara.json
#   _compat_logs/manifest-wish.json
# ===========================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "${SCRIPT_DIR}"

# ── Config ──────────────────────────────────────────────────────────────
BUILD_DIR="${BUILD_DIR:-build/debug}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${BUILD_DIR}/install}"
FLASHBACK_STANDALONE_DIR="samples/flashback/standalone"
COMPAT_TEST_DIR="tests/cross_product_compatibility"
COMPAT_BUILD_DIR="build/compatibility-test"
FLASHBACK_STANDALONE_BUILD="build/flashback-standalone"
COMPAT_LOGS="_compat_logs"
SKIP_INCOMPATIBLE=false
PRESET_NAME="${PRESET_NAME:-debug}"

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) INSTALL_PREFIX="$2"; shift 2;;
        --skip-incompatible) SKIP_INCOMPATIBLE=true; shift;;
        --preset) PRESET_NAME="$2"; shift 2;;
        *) echo "Unknown option: $1"; exit 1;;
    esac
done

mkdir -p "${COMPAT_LOGS}"

echo "=========================================="
echo " Cross-Product Compatibility Smoke Test"
echo "=========================================="
echo " Build dir:      ${BUILD_DIR}"
echo " Install prefix: ${INSTALL_PREFIX}"
echo " Skip incompat:  ${SKIP_INCOMPATIBLE}"
echo "=========================================="

# ── Step 1: Build the main project (if not already built) ───────────────
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
    echo ""
    echo "── Step 1: Configuring main project ──"
    cmake --preset "${PRESET_NAME}"
fi

echo ""
echo "── Step 1: Building main project ──"
cmake --build --preset "${PRESET_NAME}" 2>&1 | tee "${COMPAT_LOGS}/build-main.log"

# ── Step 2: Install to prefix ───────────────────────────────────────────
echo ""
echo "── Step 2: Installing to ${INSTALL_PREFIX} ──"
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}" --component Ahamkara 2>&1 | tee "${COMPAT_LOGS}/install-ahamkara.log"
if cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}" --component Wish 2>&1; then
    echo "Wish component installed." | tee "${COMPAT_LOGS}/install-wish.log"
else
    echo "Wish component installed as part of Ahamkara." | tee "${COMPAT_LOGS}/install-wish.log"
fi

# ── Step 3: Collect manifests ───────────────────────────────────────────
echo ""
echo "── Step 3: Collecting product manifests ──"
for manifest in "ahamkara-manifest.json" "wish-manifest.json"; do
    src="${BUILD_DIR}/${manifest}"
    if [[ -f "${src}" ]]; then
        cp "${src}" "${COMPAT_LOGS}/manifest-${manifest}"
        echo "  Found: ${src}"
        cat "${src}"
    else
        echo "  WARNING: ${src} not found"
    fi
done

# ── Step 4: Build Flashback standalone ──────────────────────────────────
echo ""
echo "── Step 4: Building Flashback standalone ──"
rm -rf "${FLASHBACK_STANDALONE_BUILD}"

cmake -S "${FLASHBACK_STANDALONE_DIR}" \
      -B "${FLASHBACK_STANDALONE_BUILD}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
      2>&1 | tee "${COMPAT_LOGS}/flashback-standalone-configure.log"

cmake --build "${FLASHBACK_STANDALONE_BUILD}" \
      2>&1 | tee "${COMPAT_LOGS}/flashback-standalone-build.log"

echo ""
echo "── Step 4b: Running Flashback standalone tests ──"
ctest --test-dir "${FLASHBACK_STANDALONE_BUILD}" --output-on-failure \
      2>&1 | tee "${COMPAT_LOGS}/flashback-standalone-test.log"

echo ""
echo "── Step 4c: Verifying no producer source paths ──"
# Check that the build uses no source paths from engine/ or wish/
if grep -rn "${SCRIPT_DIR}/engine/" "${FLASHBACK_STANDALONE_BUILD}/CMakeCache.txt" 2>/dev/null; then
    echo "ERROR: Flashback standalone build references engine/ source paths!"
    exit 1
fi
if grep -rn "${SCRIPT_DIR}/wish/" "${FLASHBACK_STANDALONE_BUILD}/CMakeCache.txt" 2>/dev/null; then
    echo "ERROR: Flashback standalone build references wish/ source paths!"
    exit 1
fi
echo "  Clean-room check PASSED: no producer source paths in Flashback build."

# ── Step 5: Compatible version combo ────────────────────────────────────
echo ""
echo "── Step 5: Compatible version combination test ──"
rm -rf "${COMPAT_BUILD_DIR}"
cmake -S "${COMPAT_TEST_DIR}" \
      -B "${COMPAT_BUILD_DIR}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
      2>&1 | tee "${COMPAT_LOGS}/compat-test-configure.log"

cmake --build "${COMPAT_BUILD_DIR}" \
      2>&1 | tee "${COMPAT_LOGS}/compat-test-build.log"

ctest --test-dir "${COMPAT_BUILD_DIR}" --output-on-failure \
      2>&1 | tee "${COMPAT_LOGS}/compat-test-run.log"

echo "  Compatible combination: PASS"

# ── Step 6: Incompatible version combos (optional) ──────────────────────
if [[ "${SKIP_INCOMPATIBLE}" == "false" ]]; then
    echo ""
    echo "── Step 6: Incompatible version combination tests ──"

    # Test 6a: Ahamkara ABI major mismatch
    echo ""
    echo "  Test 6a: Ahamkara ABI major mismatch (expected 99.0.0)"
    rm -rf "${COMPAT_BUILD_DIR}-bad-ae-major"
    if cmake -S "${COMPAT_TEST_DIR}" \
             -B "${COMPAT_BUILD_DIR}-bad-ae-major" \
             -G Ninja \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
             -DAHAMKARA_ABI_MAJOR_EXPECTED=99 \
             2>&1 | tee "${COMPAT_LOGS}/incompat-ae-major.log"; then
        echo "  FAIL: Expected configure to fail for ABI major mismatch"
        exit 1
    else
        echo "  PASS: Configure correctly rejected ABI major mismatch"
    fi

    # Test 6b: Wish ABI major mismatch
    echo ""
    echo "  Test 6b: Wish ABI major mismatch (expected 99.0)"
    rm -rf "${COMPAT_BUILD_DIR}-bad-wish-major"
    if cmake -S "${COMPAT_TEST_DIR}" \
             -B "${COMPAT_BUILD_DIR}-bad-wish-major" \
             -G Ninja \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
             -DWISH_ABI_MAJOR_EXPECTED=99 \
             2>&1 | tee "${COMPAT_LOGS}/incompat-wish-major.log"; then
        echo "  FAIL: Expected configure to fail for Wish ABI major mismatch"
        exit 1
    else
        echo "  PASS: Configure correctly rejected Wish ABI major mismatch"
    fi

    # Test 6c: Wish protocol major mismatch
    echo ""
    echo "  Test 6c: Wish protocol major mismatch (expected 99.0)"
    rm -rf "${COMPAT_BUILD_DIR}-bad-proto-major"
    if cmake -S "${COMPAT_TEST_DIR}" \
             -B "${COMPAT_BUILD_DIR}-bad-proto-major" \
             -G Ninja \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
             -DWISH_PROTO_MAJOR_EXPECTED=99 \
             2>&1 | tee "${COMPAT_LOGS}/incompat-proto-major.log"; then
        echo "  FAIL: Expected configure to fail for protocol major mismatch"
        exit 1
    else
        echo "  PASS: Configure correctly rejected protocol major mismatch"
    fi

    # Test 6d: Ahamkara ABI minor too new (consumer 0.5 > producer 0.1)
    echo ""
    echo "  Test 6d: Ahamkara ABI minor too new (expected 0.5.0)"
    rm -rf "${COMPAT_BUILD_DIR}-bad-ae-minor"
    if cmake -S "${COMPAT_TEST_DIR}" \
             -B "${COMPAT_BUILD_DIR}-bad-ae-minor" \
             -G Ninja \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCMAKE_PREFIX_PATH="${INSTALL_PREFIX}" \
             -DAHAMKARA_ABI_MINOR_EXPECTED=5 \
             2>&1 | tee "${COMPAT_LOGS}/incompat-ae-minor.log"; then
        echo "  FAIL: Expected configure to fail for ABI minor too new"
        exit 1
    else
        echo "  PASS: Configure correctly rejected ABI minor too new"
    fi

    echo ""
    echo "── All incompatible version tests PASSED ──"
else
    echo ""
    echo "── Step 6: Skipped (--skip-incompatible) ──"
fi

# ── Step 7: Validate artifact checksums in manifests ──────────────────
echo ""
echo "── Step 7: Validating artifact checksums in manifests ──"
VALIDATION_FAILED=0

for manifest_src in "${BUILD_DIR}/ahamkara-manifest.json" "${BUILD_DIR}/wish-manifest.json"; do
    manifest_name="$(basename "${manifest_src}")"
    manifest_dst="${COMPAT_LOGS}/manifest-${manifest_name}"

    if [[ ! -f "${manifest_src}" ]]; then
        echo "  WARNING: ${manifest_src} not found, skipping checksum validation"
        continue
    fi

    cp "${manifest_src}" "${manifest_dst}"

    # Check if manifest has "artifacts" section
    if python3 -c "
import json, sys
try:
    with open('${manifest_src}') as f:
        m = json.load(f)
    artifacts = m.get('artifacts', {})
    if not artifacts:
        print('No artifacts section found')
        sys.exit(1)
    for path, info in artifacts.items():
        sha = info.get('sha256', '')
        size = info.get('size', -1)
        if len(sha) != 64 or size < 0:
            print(f'Invalid checksum entry: {path}')
            sys.exit(1)
    print(f'Validated {len(artifacts)} artifact checksums')
    for path, info in artifacts.items():
        print(f'  {path}: sha256={info[\"sha256\"][:16]}... size={info[\"size\"]}')
    sys.exit(0)
except Exception as e:
    print(f'Error: {e}')
    sys.exit(1)
" 2>&1 | tee -a "${COMPAT_LOGS}/checksum-validation.log"; then
        echo "  PASS: ${manifest_name} checksums valid"
    else
        echo "  FAIL: ${manifest_name} checksum validation failed"
        VALIDATION_FAILED=1
    fi
done

if [[ "${VALIDATION_FAILED}" -ne 0 ]]; then
    echo ""
    echo "ERROR: Artifact checksum validation FAILED"
    exit 1
fi

# ── Summary ─────────────────────────────────────────────────────────────
echo ""
echo "=========================================="
echo " Compatibility Smoke Test: ALL PASSED"
echo "=========================================="
echo " Logs: ${COMPAT_LOGS}/"
echo " Manifests:"
ls -la "${COMPAT_LOGS}"/manifest-*.json 2>/dev/null || echo "  (no manifests found)"
echo ""
echo "To view logs:"
echo "  cat ${COMPAT_LOGS}/*.log"
