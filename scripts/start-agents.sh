#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

print_usage() {
    cat <<'EOF'
Usage:
  ./scripts/start-agents.sh [--no-watch]

Starts the local queue workflow:
  - background queue watcher
  - task board/dashboard sync
  - queue status summary

Options:
  --no-watch   Skip the background watcher and only sync + print status
  --help       Show this help
EOF
}

START_WATCH=1

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
        *)
            break
            ;;
    esac
done

cd "$PROJECT_DIR"

if [ "$START_WATCH" -eq 1 ]; then
    "$SCRIPT_DIR/../tools/agent-runner/watch-background.sh"
fi

"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" sync
"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" status
"$SCRIPT_DIR/../tools/agent-runner/agent-runner.sh" next
