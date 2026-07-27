#!/usr/bin/env sh
# promote-check.sh
#
# Validates that a branch promotion to main is complete and trustworthy.
# Exit gates (from TASK-20260718-2337 governance):
#   - Branch promotion cannot silently close work that is absent from main
#   - Work is closed only after it is on the intended base branch
#
# Usage:
#   ./scripts/promote-check.sh <branch> [<base>]
#
# Arguments:
#   branch   The feature/issue branch being promoted (default: current branch)
#   base     The target base branch (default: main)
#
# Exit codes:
#   0 - Promotion is valid
#   1 - Branch has commits not reachable from base
#   2 - Branch is not fully merged into base
#   3 - Working tree is dirty

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

BRANCH="${1:-$(git rev-parse --abbrev-ref HEAD)}"
BASE="${2:-main}"

echo "=== Branch Promotion Check ==="
echo "  Branch: $BRANCH"
echo "  Base:   $BASE"
echo ""

# Sanity: ensure we have the base branch
if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
    echo "ERROR: Base branch '$BASE' does not exist locally." >&2
    exit 2
fi

# Check 1: Working tree must be clean
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "FAIL: Working tree has uncommitted changes." >&2
    echo "      Commit or stash your work before promotion." >&2
    exit 3
fi

# Check 2: Find commits on BRANCH that are not reachable from BASE
UNMERGED=$(git log --oneline "$BASE..$BRANCH" 2>/dev/null)

if [ -n "$UNMERGED" ]; then
    COMMIT_COUNT=$(echo "$UNMERGED" | wc -l)
    echo "WARNING: $COMMIT_COUNT commit(s) on '$BRANCH' are not reachable from '$BASE':"
    echo "$UNMERGED" | sed 's/^/    /'
    echo ""

    # Check 3: Verify the branch is actually merged (merge commit exists on base)
    # Look for a merge commit on BASE that brings BRANCH's tip into history
    BRANCH_TIP=$(git rev-parse "$BRANCH" 2>/dev/null)
    if git merge-base --is-ancestor "$BRANCH_TIP" "$BASE" 2>/dev/null; then
        echo "OK: Branch tip is already an ancestor of '$BASE' (merged via squash/fast-forward)."
        echo "RESULT: Promotion valid."
        exit 0
    fi

    # Check if there is a merge commit that brings this branch in
    MERGE_BASE=$(git merge-base "$BRANCH" "$BASE" 2>/dev/null)
    if [ "$MERGE_BASE" = "$BRANCH_TIP" ]; then
        echo "OK: Branch is fully merged into '$BASE'."
        echo "RESULT: Promotion valid."
        exit 0
    fi

    echo ""
    echo "FAIL: Branch '$BRANCH' has $COMMIT_COUNT unpromoted commit(s)."
    echo "      The branch must be merged into '$BASE' before closing."
    echo "      Use 'git merge --no-ff $BRANCH' on '$BASE' to promote."
    exit 1
else
    echo "OK: All commits on '$BRANCH' are reachable from '$BASE'."
    echo "RESULT: Promotion valid."
    exit 0
fi
