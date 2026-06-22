# Wish Engine Test Client

`wish_test_client` is a headless UDP harness for exercising Wish Engine without the full game client.

## Protocol flow

The client performs a 2-phase connection:

1. **Handshake**: Sends `ClientHello` (protocol version + session token), waits for `ServerWelcome`.
   - If `ServerReject` is received (version mismatch or server busy), the client exits.
   - Handshake times out after ~60 ticks (~1s at 60 Hz).
2. **Gameplay**: After welcome, sends `PlayerInput` every tick and receives `ServerSnapshot`.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ahamkara_server wish_test_client -j
```

## Run

Start the server:

```sh
./build/server/ahamkara_server &
```

Run a single client (handshakes, then sends forward movement):

```sh
./build/tools/wish_test_client --server=127.0.0.1 --port=7777 --name=bot --duration=5 --move-y=1
```

With fire held:

```sh
./build/tools/wish_test_client --server=127.0.0.1 --name=bot --duration=8 --move-y=1 --fire
```

Suppress input logs, show only snapshots:

```sh
./build/tools/wish_test_client --server=127.0.0.1 --name=bot --duration=5 --no-events
```

Lower tick rate for sparser output:

```sh
./build/tools/wish_test_client --server=127.0.0.1 --rate=20 --duration=3
```

### Multi-client

The server currently accepts only one connected client. Multiple clients in one process
(`--count=N`) will succeed only for the first; additional clients receive `ServerReject`
with `server_busy`. Multi-client testing requires server-side session support (see Gaps).

## Output events

Each line is a `key=value` event:

| Event       | Description                                           |
|-------------|-------------------------------------------------------|
| `connect`   | Client socket opened, config summarized               |
| `hello`     | `ClientHello` sent (printed once)                     |
| `welcome`   | `ServerWelcome` received, handshake complete           |
| `reject`    | `ServerReject` received, client exits                  |
| `timeout`   | Handshake timed out                                   |
| `input`     | `PlayerInput` sent (suppressed with `--no-events`)    |
| `connected` | First `ServerSnapshot` received after welcome          |
| `snapshot`  | `ServerSnapshot` data (pos, vel, health, shield, ...) |

## CLI options

```
--server=<ip>           Server IP (default: 127.0.0.1)
--port=<n>              Server UDP port (default: 7777)
--name=<label>          Client label in logs (default: bot)
--count=<n>             Spawn n clients in one process (default: 1)
--duration=<seconds>    Run time before exit (default: 5)
--rate=<hz>             Tick rate for input sends (default: 60)
--move-x=<float>        Horizontal move input (default: 0)
--move-y=<float>        Vertical move input (default: 1)
--look-x=<float>        Look input X delta (default: 0)
--look-y=<float>        Look input Y delta (default: 0)
--fire                  Hold fire input on every tick
--no-events             Suppress input logs
--no-snapshots          Suppress snapshot logs
--simulate*             Forwarded to the network simulator
```

## Server features expected

- UDP listener on port 7777.
- `ClientHello`/`ServerWelcome` handshake with protocol version check.
- `ServerReject` for version mismatch or duplicate client.
- Authoritative snapshot loop at the configured tick rate.
- `PlayerInputCommand` and `ServerSnapshot` serialization (with envelope).

## Gaps

### Server: single-client only

The current server accepts only one connected client. Additional clients are rejected.
To test multiple concurrent clients, the server needs multi-session support.

### No reconnect / ping / liveness

Once connected, there is no disconnect/reconnect, ping, or liveness protocol.
The client runs for a fixed duration and exits.

### Fire input: no weapon effects visible in snapshots

Fire input is transmitted, but snapshot position/velocity data doesn't reflect weapon
behavior — server-side weapon handling is not yet visible in snapshots.

### Auth is stubbed

`session_token` is hardcoded to `0`. The server uses no-op auth validators.
