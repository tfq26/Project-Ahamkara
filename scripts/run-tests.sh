#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

PRESET="${AHAMKARA_CMAKE_PRESET:-debug}"

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/run-tests.sh [--preset <name>] [--build]

Options:
  --preset <name>   CMake preset/build directory to test (default: debug)
  --build           Build the preset before running tests
  --help            Show this help
EOF
}

BUILD_FIRST=0

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
        --build)
            BUILD_FIRST=1
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

cd "$PROJECT_DIR"

if [ "$BUILD_FIRST" -eq 1 ]; then
    cmake --build --preset "$PRESET"
fi

ctest --test-dir "build/$PRESET" --output-on-failure
