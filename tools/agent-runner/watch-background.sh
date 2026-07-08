#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="${AGENT_RUNNER_RUNTIME_DIR:-/private/tmp/ahamkara-agent-runner}"
PID_FILE="$RUNTIME_DIR/watch.pid"
LOG_FILE="$RUNTIME_DIR/watch.log"

mkdir -p "$RUNTIME_DIR"

if [[ -f "$PID_FILE" ]]; then
    existing_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
    if [[ -n "${existing_pid:-}" ]] && kill -0 "$existing_pid" 2>/dev/null; then
        echo "agent-runner watch already running: pid $existing_pid"
        echo "log: $LOG_FILE"
        exit 0
    fi
fi

nohup "$SCRIPT_DIR/agent-runner.sh" watch "$@" >>"$LOG_FILE" 2>&1 &
echo $! >"$PID_FILE"

echo "agent-runner watch started"
echo "pid: $(cat "$PID_FILE")"
echo "log: $LOG_FILE"
