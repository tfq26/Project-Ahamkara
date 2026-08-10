#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

cmake --build --preset debug

# Compiled assets are generated output and intentionally excluded from git.
# Keep the standard build/start workflow runnable from a clean checkout.
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
