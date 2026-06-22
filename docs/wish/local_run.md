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

When you want the dedicated server to validate real Nakama session tokens, it also reads:

| Env var | CLI flag | Default | Notes |
|---|---|---:|---|
| `WISH_NAKAMA_ENABLED` | `--nakama` | `false` | Enables real Nakama token validation |
| `WISH_NAKAMA_URL` | `--nakama-url=http://127.0.0.1:7350/v2/account` | unset | Convenience URL for host, port, and account path |
| `WISH_NAKAMA_HOST` | `--nakama-host=127.0.0.1` | `127.0.0.1` | HTTP host used when `WISH_NAKAMA_URL` is not set |
| `WISH_NAKAMA_PORT` | `--nakama-port=7350` | `7350` | Nakama API port |
| `WISH_NAKAMA_ACCOUNT_PATH` | `--nakama-account-path=/v2/account` | `/v2/account` | Endpoint used for Bearer-token account lookup |
| `WISH_NAKAMA_TIMEOUT_MS` | `--nakama-timeout-ms=1500` | `1500` | Socket connect/read timeout |

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
WISH_NAKAMA_ENABLED=1 \
WISH_NAKAMA_URL=http://127.0.0.1:7350/v2/account \
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

## Nakama-backed auth flow

With `WISH_NAKAMA_ENABLED=1`, the server stops using the no-op auth validator and instead:

1. Accepts the client's `ClientHello.session_token`
2. Calls Nakama's HTTP account endpoint with `Authorization: Bearer <token>`
3. Extracts the Nakama `user.id`
4. Uses that Nakama user id as the authenticated Wish player identity

Today this bridge supports plain `http://` Nakama URLs. For local development that matches the default Docker setup on `127.0.0.1:7350`.

## Docker

```sh
docker compose up --build
```

The compose file exposes UDP `7777` for gameplay and TCP `7778` for the admin surface.
