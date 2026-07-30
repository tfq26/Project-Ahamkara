#!/usr/bin/env bash
# Validate that the frontend builds correctly and produces expected output.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FRONTEND_DIR="$PROJECT_DIR/frontend"

echo "==> Installing frontend dependencies..."
cd "$FRONTEND_DIR"
npm ci

echo "==> Building frontend..."
npm run build

echo "==> Validating build output..."
DIST_DIR="$FRONTEND_DIR/dist"
if [ ! -d "$DIST_DIR" ]; then
  echo "ERROR: dist/ directory not found at $DIST_DIR"
  exit 1
fi

if [ ! -f "$DIST_DIR/index.html" ]; then
  echo "ERROR: dist/index.html not found"
  exit 1
fi

echo "==> Validating SPA routing (_redirects)..."
if [ ! -f "$DIST_DIR/_redirects" ]; then
  echo "ERROR: dist/_redirects not found"
  exit 1
fi

# Verify the redirect rule is present
if ! grep -q "/\*    /index.html   200" "$DIST_DIR/_redirects"; then
  echo "ERROR: _redirects missing SPA fallback rule"
  exit 1
fi

echo "==> All frontend validation checks passed."
