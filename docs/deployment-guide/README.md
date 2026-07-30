# Deployment Guide

Instructions for building, deploying, and running Ahamkara's frontend
website, backend services, and game server.

---

## Table of Contents

1. [Overview](#overview)
2. [Vue.js Frontend (GitHub Pages)](#2-vuejs-frontend-github-pages)
3. [Hono Backend (Local)](#3-hono-backend-local)
4. [Wish Dedicated Server](#4-wish-dedicated-server)
5. [Marketing Website](#5-marketing-website)
6. [Environment Variables & Configuration](#6-environment-variables--configuration)
7. [Troubleshooting](#7-troubleshooting)

---

## Overview

Ahamkara has several deployable components:

| Component | Type | Location | Deploy Target |
|-----------|------|----------|---------------|
| Vue.js SPA | Frontend | `frontend/` | GitHub Pages |
| Hono server | Backend | `backend/` | Local / bare metal |
| Wish dedicated server | Game server | `server/` | Local / Docker |
| Marketing website | Static site | `website/` | GitHub Pages |

---

## 2. Vue.js Frontend (GitHub Pages)

The project includes a Vue 3 SPA with Vue Router (history mode) that serves as
the Ahamkara info website. It lives under `frontend/`.

### Prerequisites

- **Node.js 20+** and **npm**
- A GitHub / Forgejo repository with Pages enabled

### Local Development

```sh
cd frontend
npm install
npm run dev
```

The dev server starts at `http://localhost:5173` by default with hot-reload.

### Building for Production

```sh
cd frontend
npm run build
```

The output goes to `frontend/dist/`. This includes:

- `index.html` — entry point
- `assets/` — compiled JS and CSS (content-hashed filenames)
- `404.html` — SPA fallback (create from index.html for client-side routing)

### SPA Routing Fallback

GitHub Pages (and Forgejo Pages) don't support SPA history-mode routing
natively. Copy `index.html` to `404.html` so GitHub Pages serves the app
on any route:

```sh
cp dist/index.html dist/404.html
```

### Deploy to GitHub Pages

A CI workflow (`.github/workflows/deploy-frontend.yml`) automates deployment
on every push to `main`:

1. Installs dependencies (`npm ci`)
2. Builds the frontend (`npm run build`)
3. Copies `index.html` to `404.html` for SPA fallback
4. Pushes the `dist/` contents to the `pages` branch

The `pages` branch serves as the GitHub/Forgejo Pages source. Configure your
repository Pages settings to serve from the `pages` branch root.

#### Manual Deploy

To deploy manually:

```sh
cd frontend
npm ci
npm run build
cp dist/index.html dist/404.html

# Switch to the pages branch
git checkout pages || git checkout --orphan pages
git rm -rf . > /dev/null 2>&1 || true
cp -r ../frontend/dist/* .
git add -A
git commit -m "Deploy frontend [skip ci]"
git push origin pages
```

---

## 3. Hono Backend (Local)

The Hono web server (`backend/`) serves the Vue.js frontend as static files
and provides the backend API for the Ahamkara project.

### Prerequisites

- **Node.js 20+** and **npm**

### Quick Start

```sh
# Install dependencies for both backend and frontend
cd backend
npm install
npm --prefix frontend install

# Build the Vue frontend (outputs to frontend/dist/)
npm run build:frontend

# Start the Hono server
npm run dev
```

The server listens on `http://localhost:3000`.

### Running in Production

```sh
# Build backend TypeScript
npm run build

# Build frontend
npm run build:frontend

# Start
npm start
```

### Configuration

Set the `PORT` environment variable to change the listen port (default `3000`):

```sh
PORT=8080 npm start
```

### Architecture

The backend (`backend/src/server.ts`) uses Hono with `serveStatic` middleware:

- Statically serves `frontend/dist/` for `/assets/*` and `/favicon.ico`
- All other routes fall back to `index.html` so the Vue SPA handles
  client-side routing

### Tests

```sh
cd backend
npm test
```

Tests use Vitest and validate that the Hono server serves HTML on `/` and
returns the SPA fallback on unknown routes.

---

## 4. Wish Dedicated Server

The dedicated game server is a native C++20 binary. See the full
[local run guide](../wish/local_run.md) for details.

### Build

```sh
cmake --preset release
cmake --build --preset release
```

### Run

```sh
./build/release/server/ahamkara_server
```

### Docker

```sh
docker compose up --build
```

The Docker setup exposes UDP `7777` (gameplay) and TCP `7778` (admin HTTP).

### Admin HTTP API

The admin server runs on the configured admin port (default `7778`):

```sh
curl http://127.0.0.1:7778/
curl http://127.0.0.1:7778/health
curl http://127.0.0.1:7778/players
```

The root path (`/`) renders an HTML info page with server status, match info,
connected players, and API endpoint links.

---

## 5. Marketing Website

A standalone static marketing page is available at `website/`. It provides a
landing page with Hero, Engine, Wish, and Inspiration sections.

### Local Viewing

Open `website/index.html` directly in a browser, or serve it with any static
file server:

```sh
npx serve website/
```

### Deploy to GitHub Pages

Push the `website/` directory contents to the `pages` branch, or add a CI
workflow similar to `deploy-frontend.yml`.

---

## 6. Environment Variables & Configuration

### Hono Backend

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | HTTP listen port |

### Wish Dedicated Server

| Variable | CLI Flag | Default | Description |
|----------|----------|---------|-------------|
| `WISH_SERVER_PORT` | `--port` | `7777` | UDP gameplay port |
| `WISH_SERVER_ADMIN_PORT` | `--admin-port` | `7778` | HTTP admin port |
| `WISH_SERVER_TICK_RATE` | `--tick-rate` | `60` | Simulation tick rate (Hz) |
| `WISH_SERVER_MAX_PLAYERS` | `--max-players` | `8` | Max connected clients |
| `WISH_SERVER_DISCONNECT_TIMEOUT_SEC` | `--disconnect-timeout` | `10` | Client timeout (seconds) |
| `WISH_SERVER_MATCH_DURATION_SEC` | `--match-duration` | `600` | Match length; `0` = no limit |
| `WISH_NAKAMA_ENABLED` | `--nakama` | `false` | Enable Nakama token validation |
| `WISH_NAKAMA_URL` | `--nakama-url` | — | Nakama account endpoint URL |
| `WISH_NAKAMA_HOST` | `--nakama-host` | `127.0.0.1` | Nakama HTTP host |
| `WISH_NAKAMA_PORT` | `--nakama-port` | `7350` | Nakama API port |
| `WISH_NAKAMA_ACCOUNT_PATH` | `--nakama-account-path` | `/v2/account` | Account endpoint path |
| `WISH_NAKAMA_TIMEOUT_MS` | `--nakama-timeout-ms` | `1500` | Nakama connect/read timeout |

Copy `.env.example` to `.env` and edit for workspace-local overrides:

```sh
cp .env.example .env
```

### Vue.js Frontend

The frontend itself uses no runtime environment variables. Build-time
configuration (e.g. API base URL) should be added to `vite.config.js`.

---

## 7. Troubleshooting

### Frontend build fails with Node version errors

Ensure Node.js 20+ is installed:

```sh
node --version
```

### SPA routes return 404 on GitHub Pages

Make sure `404.html` exists in the Pages branch root and is a copy of
`index.html`. The CI workflow handles this automatically.

### Hono server won't start (port in use)

Change the port:

```sh
PORT=8080 npm start
```

### Wish server can't bind to port

Ensure no other process is using the port. Change ports via environment
variables or CLI flags:

```sh
./build/release/server/ahamkara_server --port=7778 --admin-port=7779
```

### Docker build fails

Ensure Docker is installed and the daemon is running:

```sh
docker info
```

Then try a clean build:

```sh
docker compose build --no-cache
```
