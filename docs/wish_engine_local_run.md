# Wish Engine local run

This repo now keeps the dedicated server easy to start, inspect, and containerize.

## Configuration keys

The dedicated server reads these env vars and matching CLI flags:

| Env var | CLI flag | Default | Notes |
|---|---|---:|---|
| `WISH_SERVER_PORT` | `--port=7777` | `7777` | UDP gameplay port |
| `WISH_SERVER_ADMIN_PORT` | `--admin-port=7778` | `7778` | HTTP admin port |
| `WISH_SERVER_TICK_RATE` | `--tick-rate=60` | `60` | Fixed simulation tick rate |
| `WISH_SERVER_MAX_PLAYERS` | `--max-players=8` | `8` | Tracked client cap |
| `WISH_SERVER_DISCONNECT_TIMEOUT_SEC` | `--disconnect-timeout=10` | `10` | Client timeout window |
| `WISH_SERVER_MATCH_DURATION_SEC` | `--match-duration=600` | `600` | Match length in seconds; `0` means no limit |

## Local build

```sh
cmake -S . -B build -G Ninja \
  -DAHAMKARA_BUILD_CLIENT=OFF \
  -DAHAMKARA_BUILD_TESTS=OFF \
  -DAHAMKARA_BUILD_SAMPLES=OFF
cmake --build build --target ahamkara_server
```

## Local run

```sh
WISH_SERVER_PORT=7777 \
WISH_SERVER_ADMIN_PORT=7778 \
WISH_SERVER_TICK_RATE=60 \
WISH_SERVER_MAX_PLAYERS=8 \
WISH_SERVER_DISCONNECT_TIMEOUT_SEC=10 \
WISH_SERVER_MATCH_DURATION_SEC=600 \
./build/server/ahamkara_server
```

Or use CLI overrides:

```sh
./build/server/ahamkara_server \
  --port=7777 \
  --admin-port=7778 \
  --tick-rate=60 \
  --max-players=8 \
  --disconnect-timeout=10 \
  --match-duration=600
```

## Inspect

```sh
curl http://127.0.0.1:7778/health
curl http://127.0.0.1:7778/match/status
curl http://127.0.0.1:7778/players
```

## Docker

```sh
docker compose up --build
```

The compose file exposes UDP `7777` for gameplay and TCP `7778` for the admin surface.
