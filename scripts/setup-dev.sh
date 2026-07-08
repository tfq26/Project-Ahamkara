#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

PRESET="${AHAMKARA_CMAKE_PRESET:-debug}"

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/setup-dev.sh [--preset <name>] [--skip-configure]

Options:
  --preset <name>     CMake preset to configure (default: debug)
  --skip-configure    Only verify dependencies and environment
  --help              Show this help
EOF
}

SKIP_CONFIGURE=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --preset)
            if [ "$#" -lt 2 ]; then
                echo "Error: --preset requires a value." >&2
                exit 1
            fi
            PRESET="$2"
            shift 2
            ;;
        --skip-configure)
            SKIP_CONFIGURE=1
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Error: unknown argument: $1" >&2
            print_usage >&2
            exit 1
            ;;
    esac
done

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1" >&2
        exit 1
    fi
}

cd "$PROJECT_DIR"

need_cmd git
need_cmd cmake
need_cmd ninja
need_cmd c++

echo "Verified toolchain commands: git, cmake, ninja, c++"

UNAME="$(uname -s)"
if [ "$PRESET" != "debug-headless" ] && [ "$UNAME" = "Linux" ]; then
    need_cmd pkg-config
    if ! pkg-config --exists glfw3; then
        echo "Missing required development package: glfw3 (pkg-config lookup failed)." >&2
        echo "Install libglfw3-dev before configuring preset '$PRESET'." >&2
        exit 1
    fi
    echo "Verified client dependency: glfw3"
fi

if [ -f ".env.example" ] && [ ! -f ".env" ]; then
    echo "Note: .env not found. Copy .env.example to .env if you need local overrides."
fi

"$SCRIPT_DIR/check-env.sh"

if [ "$SKIP_CONFIGURE" -eq 1 ]; then
    echo "Skipped CMake configure."
    exit 0
fi

echo "Configuring preset: $PRESET"
cmake --preset "$PRESET"
echo "Setup complete for preset: $PRESET"
