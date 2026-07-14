#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
venv_path=${AHAMKARA_LINT_VENV:-"${repo_root}/.venv-lint"}
requirements_path="${repo_root}/tools/lint/requirements.txt"
marker_path="${venv_path}/.ahamkara-requirements.sha256"

requirements_hash=$(python3 - "${requirements_path}" <<'PY'
import hashlib
import pathlib
import sys

print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())
PY
)

installed_hash=""
if [[ -f "${marker_path}" ]]; then
    installed_hash=$(<"${marker_path}")
fi

if [[ ! -x "${venv_path}/bin/python" ]]; then
    python3 -m venv "${venv_path}"
fi

if [[ "${installed_hash}" != "${requirements_hash}" ]]; then
    "${venv_path}/bin/python" -m pip install \
        --disable-pip-version-check \
        --requirement "${requirements_path}"
    printf '%s\n' "${requirements_hash}" > "${marker_path}"
fi

printf 'Ahamkara lint tools are ready in %s\n' "${venv_path}"

