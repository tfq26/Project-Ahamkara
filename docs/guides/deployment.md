# Deployment Guide

How to build, package, and deploy Ahamkara client and server artifacts.

## Prerequisites

Follow the [building guide](building.md) to install CMake, Ninja, and a C++20
compiler. Docker is optional and only needed for containerised server
deployment.

## Building

### Debug (development)

```sh
cmake --preset debug
cmake --build --preset debug
```

Binaries are placed under `build/debug/`:

| Binary                    | Path                                       |
|---------------------------|--------------------------------------------|
| Client                    | `build/debug/client/ahamkara_client`       |
| Dedicated server          | `build/debug/server/ahamkara_server`       |
| Asset importer            | `build/debug/tools/ahamkara_asset_importer`|

### Headless server-only (no GLFW client)

```sh
cmake --preset debug-headless
cmake --build --preset debug-headless
```

Use this for CI workers, remote build agents, or any host without a display.

### Release (optimised)

```sh
cmake --preset release
cmake --build --preset release
```

## Packaging the Client

CPack produces a tarball containing the client, server, and asset importer:

```sh
cmake --preset package
cmake --build --preset package
cpack --preset package
```

The output is `build/package/ahamkara-*.tar.gz`.

This is the same workflow the CI pipeline uses (see
[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)). The artifact is
self-contained — unpack it on a target machine with the runtime dependencies
installed (GLFW3, OpenGL).

## Running the Server

### Local (bare-metal)

From the [local run guide](../wish/local_run.md):

```sh
./build/debug/server/ahamkara_server
```

Default listen address is `0.0.0.0:7777` (UDP gameplay) and `0.0.0.0:7778`
(TCP admin HTTP).

Health and status endpoints on the admin port:

```sh
curl http://127.0.0.1:7778/health
curl http://127.0.0.1:7778/match/status
curl http://127.0.0.1:7778/players
```

### Docker

A multi-stage `Dockerfile` is provided at the repo root. It compiles the
headless server from source in an Ubuntu 24.04 build image and copies the
binary into a minimal runtime image.

```sh
docker compose up --build
```

Docker Compose publishes UDP `7777` (gameplay) and TCP `7778` (admin) to the
host. The compose file interpolates values from a `.env` file if present (see
[Configuration](#configuration) below).

To build the image standalone:

```sh
docker build -t ahamkara-server .
docker run -p 7777:7777/udp -p 7778:7778/tcp ahamkara-server
```

## Configuration

The dedicated server reads the following environment variables.
Copy `.env.example` to `.env` and adjust values for your deployment.

### Server settings

| Variable                        | CLI flag                  | Default | Description                |
|---------------------------------|---------------------------|---------|----------------------------|
| `WISH_SERVER_PORT`              | `--port=7777`             | `7777`  | UDP gameplay port          |
| `WISH_SERVER_ADMIN_PORT`        | `--admin-port=7778`       | `7778`  | TCP admin HTTP port        |
| `WISH_SERVER_TICK_RATE`         | `--tick-rate=60`          | `60`    | Fixed simulation tick rate |
| `WISH_SERVER_MAX_PLAYERS`       | `--max-players=8`         | `8`     | Tracked client cap         |
| `WISH_SERVER_DISCONNECT_TIMEOUT_SEC` | `--disconnect-timeout=10` | `10`    | Client timeout window      |
| `WISH_SERVER_MATCH_DURATION_SEC`| `--match-duration=600`    | `600`   | Match length; `0` = no limit |

### Nakama (auth) integration

When `WISH_NAKAMA_ENABLED=1`, the server validates session tokens against
a Nakama instance instead of using no-op auth.

| Variable                     | CLI flag                         | Default                      | Description                    |
|------------------------------|----------------------------------|------------------------------|--------------------------------|
| `WISH_NAKAMA_ENABLED`        | `--nakama`                       | `false`                      | Enable real Nakama token validation |
| `WISH_NAKAMA_URL`            | `--nakama-url=...`               | unset                        | Convenience URL (host, port, path) |
| `WISH_NAKAMA_HOST`           | `--nakama-host=127.0.0.1`        | `127.0.0.1`                  | Nakama HTTP host               |
| `WISH_NAKAMA_PORT`           | `--nakama-port=7350`             | `7350`                       | Nakama API port                |
| `WISH_NAKAMA_ACCOUNT_PATH`   | `--nakama-account-path=/v2/account` | `/v2/account`             | Account lookup endpoint        |
| `WISH_NAKAMA_TIMEOUT_MS`     | `--nakama-timeout-ms=1500`       | `1500`                       | HTTP connect/read timeout      |

## CI Pipeline

Pushes to `main`, `develop`, and `agent/automerge/**` branches trigger a CI
workflow (self-hosted runner):

1. **Lint** — clang-tidy/clang-format change-aware gate
2. **Build & test** — debug, release, and debug-headless presets
3. **Package** — CPack tarball, uploaded as a build artifact

Pull requests against `main` and `develop` run the same lint + build + test
jobs.

## Connecting a Client

After deploying the server, connect with:

```sh
./build/debug/client/ahamkara_client --server 127.0.0.1
```

Or use the convenience script:

```sh
./scripts/run_client.sh 127.0.0.1
```

Replace `127.0.0.1` with the server's hostname or IP when connecting remotely.
Ensure UDP port `7777` is reachable through any firewalls.

See the [building guide](building.md) for keyboard/controller controls and
local-run alternatives.
