#!/usr/bin/env sh
# Dev-only repeatable visual QA pass for Flashback menus.
#
# Usage:
#   tools/menu_qa/run_menu_qa.sh [--capture] [--resolution WxH ...] [--build-dir DIR]
#
# The static checklist always runs. With --capture the flashback client is
# launched at each supported resolution, a screenshot is taken, and launch
# logs are recorded separately from visual findings.
#
# This script is development-only: it is not wired into any game build and
# never enables the game MCP bridge.
set -eu

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(dirname -- "$(dirname -- "$SCRIPT_DIR")")
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build/debug}"
OUT_DIR="${OUT_DIR:-${PROJECT_DIR}/build/menu_qa}"
MENUS_DIR="${MENUS_DIR:-${PROJECT_DIR}/assets/menus}"
CONFIG_FILE="${SCRIPT_DIR}/menu_qa_config.json"
CHECKER="${SCRIPT_DIR}/menu_qa_check.py"

CAPTURE=0
RESOLUTIONS=""
BUILD=1

print_usage() {
    cat <<'EOF'
Usage: tools/menu_qa/run_menu_qa.sh [options]

Options:
  --capture            Launch flashback and capture a screenshot per resolution
  --resolution WxH     Restrict capture to one resolution (repeatable)
  --build-dir DIR      Flashback build directory (default build/debug)
  --no-build           Do not build the flashback target first
  --help               Show this help
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --capture) CAPTURE=1 ;;
        --resolution)
            shift
            RESOLUTIONS="${RESOLUTIONS} $1"
            ;;
        --build-dir)
            shift
            BUILD_DIR=$1
            ;;
        --no-build) BUILD=0 ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            print_usage >&2
            exit 2
            ;;
    esac
    shift
done

# ── Build the flashback client (dev-only) ───────────────────────────────────
if [ "$BUILD" -eq 1 ]; then
    cmake --build "$BUILD_DIR" --target flashback >/dev/null
fi
FLASHBACK_BIN="${BUILD_DIR}/samples/flashback/flashback"

# ── Timestamped QA output ────────────────────────────────────────────────────
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
RUN_DIR="${OUT_DIR}/${TIMESTAMP}"
LOG_DIR="${RUN_DIR}/logs"
SCREENSHOT_DIR="${RUN_DIR}/screenshots"
mkdir -p "$LOG_DIR" "$SCREENSHOT_DIR"

# ── Static checklist ─────────────────────────────────────────────────────────
echo "==> Running static menu QA checklist"
FINDINGS_JSON="${RUN_DIR}/findings.json"
REPORT_MD="${RUN_DIR}/report.md"
CHECKER_LOG="${RUN_DIR}/checker.log"
set +e
python3 "$CHECKER" \
    --menus-dir "$MENUS_DIR" \
    --config "$CONFIG_FILE" \
    --json "$FINDINGS_JSON" \
    --markdown "$REPORT_MD" >"$CHECKER_LOG" 2>&1
CHECK_EXIT=$?
set -e
cat "$CHECKER_LOG"

# ── Screenshot capture (dev-only, platform-specific) ────────────────────────
if [ "$CAPTURE" -eq 1 ]; then
    if [ ! -x "$FLASHBACK_BIN" ]; then
        echo "!! Flashback binary not found at ${FLASHBACK_BIN}; skipping capture" >&2
    else
        CAPTURE_TOOL=""
        if command -v screencapture >/dev/null 2>&1; then
            CAPTURE_TOOL=screencapture
        elif command -v import >/dev/null 2>&1; then
            CAPTURE_TOOL=import
        elif command -v gnome-screenshot >/dev/null 2>&1; then
            CAPTURE_TOOL=gnome-screenshot
        fi
        if [ -z "$CAPTURE_TOOL" ]; then
            echo "!! No screenshot tool (screencapture/import/gnome-screenshot); skipping capture" >&2
        else
            resolutions=$(python3 -c "import json,sys; c=json.load(open('$CONFIG_FILE')); print(' '.join(f'{w}x{h}' for w,h in c['supported_resolutions']))")
            if [ -n "$RESOLUTIONS" ]; then
                resolutions=$RESOLUTIONS
            fi
            for res in $resolutions; do
                width=${res%x*}
                height=${res#*x}
                echo "==> Capturing ${res}"
                # Launch in the repo root so relative asset paths resolve.
                (cd "$PROJECT_DIR" && "$FLASHBACK_BIN" --window-width "$width" --window-height "$height") \
                    >"${LOG_DIR}/launch-${res}.log" 2>&1 &
                game_pid=$!
                # Give the engine time to open a window and render the menu.
                sleep 3
                shot="${SCREENSHOT_DIR}/menu-${res}.png"
                if [ "$CAPTURE_TOOL" = "screencapture" ]; then
                    screencapture -x "$shot"
                elif [ "$CAPTURE_TOOL" = "gnome-screenshot" ]; then
                    gnome-screenshot -f "$shot"
                else
                    import -window root "$shot"
                fi
                kill "$game_pid" 2>/dev/null || true
                wait "$game_pid" 2>/dev/null || true
                if [ -f "$shot" ]; then
                    echo "    -> $shot"
                else
                    echo "    !! screenshot capture failed for ${res}" >&2
                fi
            done
        fi
    fi
fi

# ── Launch / crash log recording (kept separate from visual findings) ────────
if ls "$LOG_DIR"/* >/dev/null 2>&1; then
    echo "==> Launch logs recorded under ${LOG_DIR}"
    grep -nEi "error|crash|fatal|segv|abort|assert" "$LOG_DIR"/* >"${RUN_DIR}/launch-issues.txt" 2>/dev/null || true
    if [ -s "${RUN_DIR}/launch-issues.txt" ]; then
        echo "!! Crash/error indicators found in launch logs:"
        sed 's/^/    /' "${RUN_DIR}/launch-issues.txt" | head -40
    fi
fi

echo
echo "==> QA report: ${REPORT_MD}"
echo "==> Findings JSON: ${FINDINGS_JSON}"
echo "==> Exit summary: static checker exited ${CHECK_EXIT}"

exit "$CHECK_EXIT"
