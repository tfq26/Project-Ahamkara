#!/usr/bin/env bash
set -euo pipefail

RUNTIME_DIR="${AGENT_RUNNER_RUNTIME_DIR:-/private/tmp/ahamkara-agent-runner}"
PID_FILE="$RUNTIME_DIR/watch.pid"

if [[ ! -f "$PID_FILE" ]]; then
    echo "No watch pidfile found at $PID_FILE"
    exit 0
fi

pid="$(cat "$PID_FILE" 2>/dev/null || true)"
if [[ -z "${pid:-}" ]]; then
    echo "Watch pidfile is empty"
    rm -f "$PID_FILE"
    exit 0
fi

if kill -0 "$pid" 2>/dev/null; then
    kill "$pid"
    echo "Stopped agent-runner watch: pid $pid"
else
    echo "No running process for pid $pid"
fi

rm -f "$PID_FILE"
