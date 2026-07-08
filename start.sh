#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="$SCRIPT_DIR/build/debug"
CLIENT_BIN="$BUILD_DIR/client/ahamkara_client"

# Auto-configure if build directory doesn't exist
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    echo "==> Configuring..."
    cmake --preset debug -S "$SCRIPT_DIR" -B "$BUILD_DIR"
fi

# Build
echo "==> Building..."
cmake --build "$BUILD_DIR"

# Launch
echo "==> Launching Flashback Arena..."
echo ""
echo "  Controls:"
echo "    W/A/S/D  - Move"
echo "    Mouse    - Look around"
echo "    LMB      - Fire"
echo "    R        - Reload"
echo "    Space    - Jump"
echo "    Shift    - Sprint"
echo "    Ctrl     - Crouch"
echo "    C        - Slide"
echo "    1/2/3    - Switch weapons"
echo "    Esc      - Pause / Menu"
echo "    Tab      - Scoreboard"
echo "    F3       - Performance metrics"
echo "    V        - Toggle third-person"
echo ""

exec "$CLIENT_BIN" --local "$@"