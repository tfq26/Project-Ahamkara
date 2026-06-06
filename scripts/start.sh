#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

MODE="local"
SKIP_CONFIGURE=0
SKIP_BUILD=0

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/start.sh [mode] [options] [-- extra args]

Modes:
  local        Configure, build, and launch the local debug view (default)
  debug-view   Same as local
  window       Launch the windowed input diagnostics client
  sandbox      Launch the offline text sandbox
  network      Launch the dedicated server and client together
  server       Launch only the dedicated server
  client       Launch only the network client
  configure    Run configure only
  build        Run build only

Options:
  --skip-configure   Skip the configure step
  --skip-build       Skip the build step
  --help             Show this help

Examples:
  ./scripts/start.sh
  ./scripts/start.sh local
  ./scripts/start.sh network -- 127.0.0.1
  ./scripts/start.sh sandbox --skip-configure --skip-build
EOF
}

run_configure() {
    "$SCRIPT_DIR/configure_debug.sh"
}

run_build() {
    "$SCRIPT_DIR/build_debug.sh"
}

cleanup_server() {
    if [ "${SERVER_PID:-}" != "" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --help)
            print_usage
            exit 0
            ;;
        --skip-configure)
            SKIP_CONFIGURE=1
            shift
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        --)
            shift
            break
            ;;
        local|debug-view|window|sandbox|network|server|client|configure|build)
            MODE="$1"
            shift
            ;;
        *)
            break
            ;;
    esac
done

cd "$PROJECT_DIR"

if [ "$SKIP_CONFIGURE" -eq 0 ] && [ "$MODE" != "build" ]; then
    run_configure
fi

if [ "$SKIP_BUILD" -eq 0 ] && [ "$MODE" != "configure" ]; then
    run_build
fi

case "$MODE" in
    configure)
        exit 0
        ;;
    build)
        exit 0
        ;;
    local|debug-view)
        exec "$SCRIPT_DIR/run_local.sh" "$@"
        ;;
    window)
        exec "$SCRIPT_DIR/run_windowed_client.sh" "$@"
        ;;
    sandbox)
        exec "$SCRIPT_DIR/run_sandbox.sh" "$@"
        ;;
    server)
        exec "$SCRIPT_DIR/run_server.sh" "$@"
        ;;
    client)
        exec "$SCRIPT_DIR/run_client.sh" "$@"
        ;;
    network)
        trap cleanup_server EXIT INT TERM
        "$SCRIPT_DIR/run_server.sh" &
        SERVER_PID=$!
        sleep 1
        "$SCRIPT_DIR/run_client.sh" "$@"
        ;;
    *)
        print_usage
        exit 1
        ;;
esac
