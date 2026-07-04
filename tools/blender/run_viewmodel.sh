#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BLENDER_BIN=${BLENDER_BIN:-/Applications/Blender.app/Contents/MacOS/Blender}
OUT_DIR=${1:-"$ROOT_DIR/assets/models"}

exec "$BLENDER_BIN" \
  --python "$ROOT_DIR/tools/blender/build_viewmodel.py" \
  -- \
  --out_dir "$OUT_DIR" \
  --open
