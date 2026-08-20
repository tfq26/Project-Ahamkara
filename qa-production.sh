#!/usr/bin/env bash
set -Eeuo pipefail

# Production QA gate for the Ahamkara repository.
# Every gate writes a log and a machine-readable result under QA_ARTIFACT_DIR.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARTIFACT_DIR="${QA_ARTIFACT_DIR:-${ROOT_DIR}/qa-artifacts}"
mkdir -p "${ARTIFACT_DIR}"

declare -a GATE_NAMES=()
declare -a GATE_STATUS=()
OVERALL_STATUS=0

run_gate() {
    local name="$1"
    shift
    local safe_name
    safe_name="${name//[^a-zA-Z0-9_-]/-}"
    local log_file="${ARTIFACT_DIR}/${safe_name}.log"
    GATE_NAMES+=("${name}")

    echo "=== QA gate: ${name} ==="
    echo "Command: $*"
    if (cd "${ROOT_DIR}" && "$@") 2>&1 | tee "${log_file}"; then
        GATE_STATUS+=("passed")
        echo "PASS: ${name}"
    else
        GATE_STATUS+=("failed")
        OVERALL_STATUS=1
        echo "FAIL: ${name}" >&2
    fi
    echo
}

cd "${ROOT_DIR}"

run_gate "repository-diff" git diff --check
run_gate "debug-build-and-tests" bash -lc 'cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure'
run_gate "headless-build-and-tests" bash -lc 'cmake --preset debug-headless && cmake --build --preset debug-headless && ctest --test-dir build/debug-headless --output-on-failure'
run_gate "release-build" bash -lc 'cmake --preset release && cmake --build --preset release'
run_gate "package-build" bash -lc 'cmake --preset package && cmake --build --preset package && cpack --preset package'
run_gate "cross-product-compatibility" bash ./scripts/compatibility-smoke.sh --skip-incompatible

QA_GATE_NAMES="$(IFS=$'\x1f'; echo "${GATE_NAMES[*]}")"
QA_GATE_STATUS="$(IFS=$'\x1f'; echo "${GATE_STATUS[*]}")"
export QA_GATE_NAMES QA_GATE_STATUS

python3 - "${ARTIFACT_DIR}/qa-report.json" <<'PY'
import json
import os
import sys
from datetime import datetime, timezone

output = sys.argv[1]
names = os.environ.get("QA_GATE_NAMES", "").split("\x1f") if os.environ.get("QA_GATE_NAMES") else []
statuses = os.environ.get("QA_GATE_STATUS", "").split("\x1f") if os.environ.get("QA_GATE_STATUS") else []
report = {
    "schema_version": 1,
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "repository": "Project-Ahamkara",
    "gates": [
        {"name": name, "status": status}
        for name, status in zip(names, statuses)
    ],
    "passed": bool(names) and all(status == "passed" for status in statuses),
}
with open(output, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2)
    handle.write("\n")
print(json.dumps(report, indent=2))
PY

exit "${OVERALL_STATUS}"
