#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

if [ ! -f ".env.example" ]; then
    echo "Warning: .env.example is missing." >&2
    exit 1
fi

if [ ! -f ".env" ]; then
    echo "Environment check: .env not present. Using defaults from .env.example and the codebase."
    exit 0
fi

required_keys='
WISH_SERVER_PORT
WISH_SERVER_ADMIN_PORT
WISH_SERVER_TICK_RATE
WISH_SERVER_MAX_PLAYERS
WISH_SERVER_DISCONNECT_TIMEOUT_SEC
WISH_SERVER_MATCH_DURATION_SEC
'

missing=0

for key in $required_keys; do
    if ! grep -Eq "^${key}=" ".env"; then
        echo "Missing env key in .env: $key" >&2
        missing=1
    fi
done

if [ "$missing" -ne 0 ]; then
    echo "Environment check failed." >&2
    exit 1
fi

echo "Environment check passed."
