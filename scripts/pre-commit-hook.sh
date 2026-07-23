#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ahamkara pre-commit hook
#
# Opt-in hook that runs the full lint suite (C/C++ formatter & static
# analysis, Python, Shell, CMake, GitHub Actions, and repo hygiene)
# against staged changes.  Blocks the commit when any check fails.
#
# Install (one-time):
#   ./scripts/install-pre-commit-hook.sh
#
# Skip for a single commit:
#   git commit --no-verify
#
# Cross-platform: Linux, macOS, Windows (via WSL / Git for Bash).
# ---------------------------------------------------------------------------
set -euo pipefail

# ---- preamble: find the repository root -----------------------------------
if ! git rev-parse --git-dir >/dev/null 2>&1; then
    echo "pre-commit: not inside a Git repository" >&2
    exit 1
fi

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

# ---- staged files ---------------------------------------------------------
# Use -z (NUL-delimited) to handle filenames with spaces safely.
staged_files=()
while IFS= read -r -d '' f; do
    staged_files+=("${f}")
done < <(git diff --cached --name-only --diff-filter=ACMR -z)

# If no files are staged (e.g., `git commit --allow-empty`), nothing to check.
if [ ${#staged_files[@]} -eq 0 ]; then
    exit 0
fi

# ---- availability check ---------------------------------------------------
lint_script="${repo_root}/scripts/lint.sh"
if [ ! -f "${lint_script}" ]; then
    echo "pre-commit: ${lint_script} not found -- cannot run lint check" >&2
    exit 1
fi

# ---- save / restore unstaged changes --------------------------------------
# We want to validate exactly what is staged, not the working tree.  Save
# unstaged changes and restore them after the check (even on failure).
stash_created=0
if ! git diff --quiet 2>/dev/null && ! git diff --cached --quiet 2>/dev/null; then
    # There are both staged and unstaged changes.
    git stash -q --keep-index 2>/dev/null || true
    stash_created=1
fi

# Restore unstaged changes on exit.  The trap fires for EXIT (which includes
# normal finish and error paths) and ensures we do not leave the working tree
# dirty.
# shellcheck disable=SC2329  # invoked indirectly via trap EXIT
cleanup() {
    if [ "${stash_created}" -eq 1 ]; then
        git stash pop -q 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ---- run the lint suite ---------------------------------------------------
# Pass all staged files as explicit paths so the runner checks only them.
echo "-- Pre-commit hook: running lint on ${#staged_files[@]} staged file(s) ..."
set +e
"${lint_script}" --paths "${staged_files[@]}"
result=$?
set -e

# ---- result ---------------------------------------------------------------
if [ "${result}" -ne 0 ]; then
    echo ""
    echo "!! Pre-commit hook: lint/format validation FAILED."
    echo "   Fix the errors above, stage the corrections, and retry."
    echo "   To bypass this check:  git commit --no-verify"
    exit 1
fi

echo "-- Pre-commit hook: lint/format validation passed."
exit 0
