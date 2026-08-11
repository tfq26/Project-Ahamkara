#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# This is intentionally a graphical local-machine smoke test. The engine's
# authoritative CI path is scripts/run-tests.sh --preset debug-headless.
BUILD_DIR="${FLASHBACK_BUILD_DIR:-build/debug}"
EXECUTABLE="${FLASHBACK_EXECUTABLE:-${BUILD_DIR}/samples/flashback/flashback}"
LEVEL="${FLASHBACK_LEVEL:-assets/compiled/levels/prototype_box.aelevel}"

cd "$PROJECT_DIR"

if [ ! -x "$EXECUTABLE" ]; then
    echo "Flashback executable not found: $EXECUTABLE" >&2
    echo "Build the graphical preset first: cmake --preset debug && cmake --build --preset debug" >&2
    exit 1
fi

if [ ! -f "$LEVEL" ]; then
    echo "Flashback level not found: $LEVEL" >&2
    echo "Set FLASHBACK_LEVEL to a compiled local level or run the asset build first." >&2
    exit 1
fi

echo "Launching Flashback smoke test"
echo "  executable: $EXECUTABLE"
echo "  level:      $LEVEL"
echo "Close the game window to complete the manual display check."

exec "$EXECUTABLE" --level "$LEVEL" --autoplay "$@"
