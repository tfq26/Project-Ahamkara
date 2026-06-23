#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Run from the repo root so the level's relative asset paths resolve.
cd "$PROJECT_DIR"

exec "$PROJECT_DIR/build/debug/samples/flashback/flashback" "$@"
