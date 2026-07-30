# Deployment Guide

Instructions for deploying Ahamkara, Flashback, and Wish services.

## Local Development

For local development and testing, see the [build guide](../guides/building.md).

Quick start:

```sh
# Configure and build
cmake --preset debug
cmake --build --preset debug

# Run local sandbox
./scripts/start.sh local
```

## Dedicated Server

The dedicated server binary (`ahamkara_server`) can be deployed on Linux or
Windows machines:

```sh
cmake --preset release
cmake --build --preset release
./build/release/server/ahamkara_server
```

### Server Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 7777 | UDP | Game traffic |
| 7778 | TCP | Admin HTTP API |

## Docker Deployment

A `docker-compose.yml` is provided at the project root for containerised
deployment:

```sh
docker compose up -d
```

The Wish engine container exposes UDP port 7777 for game traffic and TCP
port 7778 for the admin HTTP API. Configure via environment variables
(see `.env.example`).

## CI/CD

The repository uses Agola CI with workflows defined in [`.agola/config.jsonnet`](../.agola/config.jsonnet). The
[`ci.yml`](../.github/workflows/ci.yml) GitHub Actions workflow provides additional validation.

## Configuration

Runtime configuration is loaded from `client/config/ahamkara.cfg` and
`client/config/controller_bindings.cfg`. Environment variables override
config file values:

| Variable | Default | Description |
|----------|---------|-------------|
| `WISH_SERVER_PORT` | 7777 | UDP game port |
| `WISH_SERVER_ADMIN_PORT` | 7778 | TCP admin port |
| `WISH_SERVER_TICK_RATE` | 60 | Server tick rate |
| `WISH_SERVER_MAX_PLAYERS` | 8 | Max connected players |
