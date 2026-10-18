#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

if [ ! -x "$PROJECT_DIR/build/debug/samples/flashback/flashback" ]; then
    echo "Flashback is not built. Run: cmake --build --preset debug --target flashback" >&2
    exit 1
fi

exec "$PROJECT_DIR/build/debug/samples/flashback/flashback" "$@"
