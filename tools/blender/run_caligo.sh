#!/usr/bin/env bash
set -euo pipefail

# Blender 5.1.2 on some Apple Silicon/macOS combinations can crash during
# native startup, before Python or the Caligo script runs. Probe the binary
# first so asset generation remains deterministic and non-destructive.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BLENDER_BIN="${BLENDER_BIN:-}"
OUT_DIR="assets/models"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --blender)
      BLENDER_BIN="$2"
      shift 2
      ;;
    --out_dir)
      OUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "Usage: $0 [--blender PATH] [--out_dir DIR]" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$BLENDER_BIN" ]]; then
  if command -v blender >/dev/null 2>&1; then
    BLENDER_BIN="$(command -v blender)"
  elif [[ -x /Applications/Blender.app/Contents/MacOS/Blender ]]; then
    BLENDER_BIN="/Applications/Blender.app/Contents/MacOS/Blender"
  fi
fi

cd "$ROOT_DIR"

if [[ -n "$BLENDER_BIN" && -x "$BLENDER_BIN" ]]; then
  set +e
  "$BLENDER_BIN" --background --factory-startup --threads 1 \
    --python-expr 'print("AHAMKARA_BLENDER_READY")' \
    >/tmp/ahamkara_blender_probe.log 2>&1
  probe_status=$?
  set -e
  if [[ "$probe_status" -eq 0 ]] && grep -q 'AHAMKARA_BLENDER_READY' /tmp/ahamkara_blender_probe.log; then
    exec "$BLENDER_BIN" --background --factory-startup --threads 1 \
      --python tools/blender/build_caligo.py -- --out_dir "$OUT_DIR"
  fi
  echo "run_caligo: Blender probe failed (exit $probe_status); using glTF fallback" >&2
else
  echo "run_caligo: Blender not found; using glTF fallback" >&2
fi

exec python3 tools/blender/build_caligo.py --out_dir "$OUT_DIR"
