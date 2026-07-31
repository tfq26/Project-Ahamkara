# Deployment Guide

This guide covers building, packaging, and deploying Ahamkara for both local
development and production. It also explains the CI/CD pipeline and how to
publish project documentation to GitHub Pages.

---

## Table of Contents

1. [Local Development](#1-local-development)
2. [Building for Production](#2-building-for-production)
3. [Docker Deployment](#3-docker-deployment)
4. [CI/CD Pipeline](#4-cicd-pipeline)
5. [Package Distribution](#5-package-distribution)
6. [GitHub Pages Documentation](#6-github-pages-documentation)
7. [Environment Variables](#7-environment-variables)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Local Development

For day-to-day development, see the full [Building guide](building.md).

### Quick Start

```sh
# One-command local debug view (configure + build + run)
./scripts/start.sh

# Network mode (server + client pair)
./scripts/start.sh network

# Offline movement sandbox
./scripts/start.sh sandbox
```

### Manual Steps

```sh
# Configure
cmake --preset debug

# Build
cmake --build --preset debug

# Test
ctest --test-dir build/debug --output-on-failure

# Run server
./scripts/run_server.sh

# Run client (separate terminal)
./scripts/run_client.sh
```

### Headless Build (no GLFW/OpenGL)

For remote agents or server-only environments:

```sh
cmake --preset debug-headless
cmake --build --preset debug-headless
ctest --test-dir build/debug-headless --output-on-failure
```

---

## 2. Building for Production

### Release Build

```sh
cmake --preset release
cmake --build --preset release
```

The release preset enables optimisations and omits debug symbols
[src: file: CMakePresets.json:20-29].

### Package Build

The package preset uses a debug configuration structured for distribution:

```sh
cmake --preset package
cmake --build --preset package
cpack --preset package
```

This produces TGZ and ZIP archives under `build/package/` containing the
engine libraries, headers, and binaries.

For a headless server package:

```sh
cmake --preset package-debug-headless
cmake --build --preset package-debug-headless
cpack --preset package-debug-headless
```

Reference: [CMakePresets.json](file: CMakePresets.json:44-55) and
[CMakeLists.txt packaging section](file: CMakeLists.txt:193-206).

---

## 3. Docker Deployment

The project includes a Dockerfile that builds and runs the headless dedicated
server inside a container [src: file: Dockerfile:1-41].

### Build the Image

```sh
docker build -t ahamkara-server .
```

This multi-stage build:
1. **Stage 1 (build)**: Uses `ubuntu:24.04` with CMake, GCC, Ninja. Configures
   with `debug-headless` preset and builds only the `ahamkara_server` target.
2. **Stage 2 (runtime)**: A minimal `ubuntu:24.04` image containing only the
   server binary and CA certificates.

### Run the Container

```sh
docker run -d \
  --name ahamkara-server \
  -p 7777:7777/udp \
  -p 7778:7778/tcp \
  -e WISH_SERVER_PORT=7777 \
  -e WISH_SERVER_ADMIN_PORT=7778 \
  -e WISH_SERVER_TICK_RATE=60 \
  -e WISH_SERVER_MAX_PLAYERS=8 \
  -e WISH_SERVER_DISCONNECT_TIMEOUT_SEC=10 \
  -e WISH_SERVER_MATCH_DURATION_SEC=600 \
  ahamkara-server
```

### Docker Compose

For convenience, a `docker-compose.yml` is provided
[src: file: docker-compose.yml:1-15]:

```sh
docker compose up -d
```

This exposes the same ports and environment variables. Create a `.env` file
from `.env.example` to override defaults:

```sh
cp .env.example .env
# Edit .env to customise
docker compose up -d
```

### Verify the Container

```sh
docker logs ahamkara-server
# Expected: "DedicatedServer application started. ... listening on UDP 7777."
```

---

## 4. CI/CD Pipeline

The CI workflow runs on every push to `main`, `develop`, and `agent/automerge/**`
branches, and on every pull request targeting `main` or `develop`
[src: file: .github/workflows/ci.yml:10-17].

### Pipeline Stages

```
Lint → Build & Test (matrix) → Package → (Auto-merge)
```

| Stage | Description |
|---|---|
| **Lint** | Generates compilation database, runs `scripts/lint.sh` with change-aware diff, publishes summary and reports |
| **Build & Test** | Matrix build across `debug`, `release`, `debug-headless` presets; runs CTest for `debug` and `debug-headless` |
| **Package** | Configures the `package` preset and runs `cpack` to produce distributable archives; uploads artifact |
| **Auto-merge** | Only for `agent/automerge/**` branches; merges into `develop` on success |

### Self-Hosted Runner

The pipeline runs on a self-hosted Linux runner (`servlenovo1`). This runner
has the full toolchain installed: CMake ≥ 3.20, Ninja, GCC/Clang, GLFW3 dev
libraries, and OpenGL headers.

### Viewing CI Results

1. Navigate to the repository's **Actions** tab.
2. Select the **CI** workflow.
3. Click a specific run to see job logs, lint summaries, and artifacts.

Lint reports are uploaded as the `ahamkara-lint-report` artifact. Package
archives are uploaded as `ahamkara-package`.

---

## 5. Package Distribution

### Creating a Release

1. Tag the release:
   ```sh
   git tag -a v0.1.0 -m "v0.1.0"
   git push origin v0.1.0
   ```

2. Build the package:
   ```sh
   cmake --preset package
   cmake --build --preset package
   cpack --preset package
   ```

3. The archives are in `build/package/`:
   ```
   build/package/
   ├── Ahamkara-0.1.0-Linux.tar.gz
   └── Ahamkara-0.1.0-Linux.zip
   ```

4. Upload these to a GitHub Release:
   ```sh
   gh release create v0.1.0 \
     build/package/Ahamkara-0.1.0-Linux.tar.gz \
     build/package/Ahamkara-0.1.0-Linux.zip \
     --title "v0.1.0" \
     --notes "Release notes here"
   ```

### Consuming the Package

Out-of-tree consumers use `find_package`:

```cmake
find_package(Ahamkara CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE Ahamkara::Core)
```

The engine-only package excludes Wish, Flashback, client, server, and samples.
Configure with `-DAHAMKARA_ENGINE_ONLY=ON` for a minimal distribution
[src: file: docs/architecture/overview.md:169-177].

---

## 6. GitHub Pages Documentation

The `docs/` directory can be published as a GitHub Pages site to host the
project documentation online.

### Setup (one-time)

1. In the repository on GitHub, go to **Settings → Pages**.
2. Under **Source**, select **GitHub Actions**.

### Deployment Workflow

The following GitHub Actions workflow publishes the documentation on every
push to `main`:

```yaml
# .github/workflows/docs.yml
name: Docs

on:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup Pages
        uses: actions/configure-pages@v4
      - name: Upload artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: docs/
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

This workflow:
- Checks out the repository
- Configures GitHub Pages
- Uploads the `docs/` directory as a static site artifact
- Deploys it to GitHub Pages

### Jekyll Configuration

To enable Jekyll rendering (optional), create `docs/_config.yml`:

```yaml
title: Ahamkara Engine
description: A custom C++20 game engine and multiplayer tech demo
markdown: kramdown
theme: jekyll-theme-cayman
```

GitHub Pages also serves plain Markdown files without Jekyll — they are
rendered as static HTML automatically.

### Accessing the Site

Once deployed, the documentation site is available at:

```
https://<org>.github.io/Project-Ahamkara/
```

Replace `<org>` with the GitHub organisation or username.

---

## 7. Environment Variables

The dedicated server and Docker containers respect these environment variables
[src: file: .env.example:1-7]:

| Variable | Default | Description |
|---|---|---|
| `WISH_SERVER_PORT` | `7777` | UDP port for game traffic |
| `WISH_SERVER_ADMIN_PORT` | `7778` | TCP port for admin HTTP API |
| `WISH_SERVER_TICK_RATE` | `60` | Simulation ticks per second |
| `WISH_SERVER_MAX_PLAYERS` | `8` | Maximum connected players |
| `WISH_SERVER_DISCONNECT_TIMEOUT_SEC` | `10` | Seconds before a disconnected player is removed |
| `WISH_SERVER_MATCH_DURATION_SEC` | `600` | Match duration in seconds |

For local development without Docker, create a `.env` file in the project root:

```sh
cp .env.example .env
# Edit values as needed
```

The `docker-compose.yml` reads these automatically
[src: file: docker-compose.yml:7-12].

---

## 8. Troubleshooting

### Docker

**`docker: command not found`**
→ Install Docker from [docs.docker.com](https://docs.docker.com/engine/install/).

**Port already in use**
```sh
# Check what is using the port
sudo lsof -i :7777
# Or change the port via environment variable
WISH_SERVER_PORT=7778 docker compose up -d
```

**Container exits immediately**
```sh
# Check logs
docker logs ahamkara-server
# Verify the binary exists inside the container
docker run --rm ahamkara-server ls -la /app/
```

### CI/CD

**Workflow fails on lint**
→ Read the lint summary published in the workflow run. Fix formatting or
warnings and push again.

**Workflow fails on build**
→ Check the build log for compiler errors. Common issues:
- Missing dependencies (GLFW, OpenGL)
- Outdated CMake version
- Self-hosted runner toolchain mismatch

**Package artifact not generated**
→ Verify `cpack --preset package` succeeds locally first. Check the workflow
step output for packaging errors.

### GitHub Pages

**Site not showing after deployment**
→ Go to **Settings → Pages** and verify the source is set to **GitHub
Actions**. Check the **Actions** tab for the Docs workflow run status.

**404 on documentation pages**
→ GitHub Pages serves the uploaded content as-is. Ensure all internal links
use relative paths. If using Jekyll, verify `docs/_config.yml` is valid YAML.

---

## See Also

- [Building guide](building.md) — detailed local build instructions
- [Maintenance guide](maintenance.md) — error diagnosis and repair
- [Architecture overview](../architecture/overview.md) — system boundaries
- [CI workflow](../../.github/workflows/ci.yml) — pipeline configuration
- [Dockerfile](../../Dockerfile) — container build definition
- [docker-compose.yml](../../docker-compose.yml) — Compose service definition
