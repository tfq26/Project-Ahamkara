#!/bin/bash
# Oz Runner wrapper — sources env and runs a single poll cycle
set -a
source /Users/taufeeqali/Projects/Ahamkara/.oz-runner/config.env 2>/dev/null || true
# Also use ~/.zshrc exported vars for API keys
set +a

cd /Users/taufeeqali/Projects/Ahamkara
exec python3 oz-runner.py --once "$@"
