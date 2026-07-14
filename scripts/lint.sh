#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
venv_path=${AHAMKARA_LINT_VENV:-"${repo_root}/.venv-lint"}

"${repo_root}/scripts/setup-lint.sh" >/dev/null
export PATH="${venv_path}/bin:${PATH}"
exec "${venv_path}/bin/python" "${repo_root}/tools/lint/run.py" "$@"
