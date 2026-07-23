#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Install the Ahamkara pre-commit hook (opt-in)
#
# Usage:
#   ./scripts/install-pre-commit-hook.sh
#
# This copies scripts/pre-commit-hook.sh to .git/hooks/pre-commit and makes
# it executable.  Subsequent `git commit` invocations will automatically
# run the lint suite on staged changes.
#
# To uninstall:
#   rm .git/hooks/pre-commit
#
# To skip the hook for a single commit:
#   git commit --no-verify
#
# Prerequisites:
#   - The lint tooling must be available (run ./scripts/setup-lint.sh first)
#     or the hook will auto-install it on first invocation.
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
HOOK_SOURCE="${SCRIPT_DIR}/pre-commit-hook.sh"
HOOK_TARGET="${PROJECT_DIR}/.git/hooks/pre-commit"

# --- preamble: must be inside a Git repository ------------------------------
if [ ! -d "${PROJECT_DIR}/.git" ]; then
    echo "Error: ${PROJECT_DIR}/.git does not exist." >&2
    echo "Make sure you are running this script from within a cloned repository." >&2
    exit 1
fi

if [ ! -d "${PROJECT_DIR}/.git/hooks" ]; then
    mkdir -p "${PROJECT_DIR}/.git/hooks"
fi

# --- check source exists ----------------------------------------------------
if [ ! -f "${HOOK_SOURCE}" ]; then
    echo "Error: hook source not found: ${HOOK_SOURCE}" >&2
    exit 1
fi

# --- install ----------------------------------------------------------------
cp "${HOOK_SOURCE}" "${HOOK_TARGET}"
chmod +x "${HOOK_TARGET}"

echo "✅ Pre-commit hook installed at: ${HOOK_TARGET}"
echo ""
echo "The hook will run the full lint suite on staged changes before each commit."
echo "To uninstall: rm ${HOOK_TARGET}"
echo "To skip once: git commit --no-verify"
echo ""
echo "Note: The lint tools are installed lazily by scripts/lint.sh on first use."
echo "      The first commit may take a little longer while dependencies are"
echo "      resolved in .venv-lint."
