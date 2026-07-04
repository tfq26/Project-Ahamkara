#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

START_WATCH=1
RUN_GAME=0
GAME_MODE="local"

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/start-everything.sh [options]

Starts the queue workflow:
  - the background queue watcher

Options:
  --no-watch   Skip the queue watcher
  --run-game   Launch `./scripts/start.sh` after queue startup
  --mode NAME  Mode to pass to `start.sh` when used with `--run-game`
  --help       Show this help

Examples:
  ./scripts/start-everything.sh
  ./scripts/start-everything.sh --run-game
  ./scripts/start-everything.sh --run-game --mode flashback
EOF
}

cleanup() {
    if [ "${WATCHER_STARTED:-0}" -eq 1 ]; then
        "$SCRIPT_DIR/watch-stop.sh" >/dev/null 2>&1 || true
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --help)
            print_usage
            exit 0
            ;;
        --no-watch)
            START_WATCH=0
            shift
            ;;
        --run-game)
            RUN_GAME=1
            shift
            ;;
        --mode)
            GAME_MODE="${2:-}"
            if [ "$GAME_MODE" = "" ]; then
                echo "--mode requires a value" >&2
                exit 1
            fi
            shift 2
            ;;
        *)
            break
            ;;
    esac
done

cd "$PROJECT_DIR"

WATCHER_STARTED=0
if [ "$START_WATCH" -eq 1 ]; then
    trap cleanup EXIT INT TERM
    "$SCRIPT_DIR/../tools/agent-runner/watch-background.sh"
    WATCHER_STARTED=1
fi

"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" sync
"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" status
"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" next

if [ "$RUN_GAME" -eq 1 ]; then
    exec "$SCRIPT_DIR/start.sh" "$GAME_MODE" "$@"
fi

echo "Queue workflow started."
echo "Run ./scripts/start.sh separately when you want the engine."
