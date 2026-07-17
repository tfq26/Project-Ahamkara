#!/usr/bin/env sh
# check-template-tokens.sh
#
# Checks that critical files do not contain unreplaced template tokens
# (e.g. {{STACK_SUMMARY}}, {{TEST_CMD}}).  This prevents stale agent
# config templates from being merged.
#
# Usage:
#   ./scripts/check-template-tokens.sh [files...]
#
# If no files are given, a built-in list of known-critical paths is used.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

if [ "$#" -gt 0 ]; then
    files="$@"
else
    # Default list of files that commonly carry template tokens.
    files="
        AGENTS.md
        .clinerules
        .cursorrules
        .windsurfrules
        .github/copilot-instructions.md
        .kiro/steering/instructions.md
        CLAUDE.md
        GEMINI.md
    "
fi

exit_code=0

# Regex matches any {{...}} token that is NOT a standard GitHub Actions
# expression (i.e. does NOT start with ${{ or contain github., matrix., env.,
# needs., steps., secrets., inputs.).
# It also allows common CMake @var@ substitutions.
token_pattern='\{\{[A-Z_][A-Z_0-9]*\}\}'

for relpath in $files; do
    f="${PROJECT_DIR}/${relpath}"
    if [ ! -f "$f" ]; then
        echo "[SKIP]  $relpath — not found"
        continue
    fi

    matches=$(grep -E "$token_pattern" "$f" 2>/dev/null || true)
    if [ -n "$matches" ]; then
        echo "[FAIL]  $relpath contains unresolved template tokens:"
        echo "$matches" | while IFS= read -r line; do
            echo "         $line"
        done
        exit_code=1
    else
        echo "[PASS]  $relpath"
    fi
done

exit "$exit_code"
