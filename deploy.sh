#!/usr/bin/env bash
# Build and deploy the frontend to Cloudflare Pages.
# Prerequisites:
#   - Node.js >= 18
#   - wrangler CLI (installed via npm in frontend/)
#   - CLOUDFLARE_API_TOKEN environment variable set
#   - CLOUDFLARE_ACCOUNT_ID environment variable set
set -euo pipefail

cd "$(dirname "$0")/frontend"

echo "==> Installing dependencies..."
npm ci

echo "==> Building frontend..."
npm run build

echo "==> Deploying to Cloudflare Pages..."
npx wrangler pages deploy dist/ \
  --project-name ahamkara \
  --branch "${CI_COMMIT_REF_NAME:-main}"

echo "==> Deployment complete."
