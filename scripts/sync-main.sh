#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Sync aborted: uncommitted changes detected. Commit or stash your work first." >&2
    exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [ "$CURRENT_BRANCH" = "HEAD" ]; then
    echo "Sync aborted: detached HEAD state is not supported." >&2
    exit 1
fi

echo "Fetching origin..."
if ! git fetch origin; then
    echo "Sync failed: could not fetch origin." >&2
    exit 1
fi

echo "Fast-forwarding branch '$CURRENT_BRANCH' from origin/$CURRENT_BRANCH..."
if git pull --ff-only origin "$CURRENT_BRANCH"; then
    echo "Sync complete: '$CURRENT_BRANCH' is up to date."
else
    echo "Sync failed: fast-forward pull was not possible for '$CURRENT_BRANCH'." >&2
    exit 1
fi
