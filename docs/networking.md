# Ahamkara Networking

## Model

Ahamkara uses a dedicated authoritative server model. Clients send input commands to the server,
the server simulates authoritative game state, and the server sends snapshots back to clients.

## Current Loop

- The client sends `PlayerInputCommand` packets over UDP at roughly 60 Hz.
- The server receives input, simulates authoritative state (Jolt physics via `World::tick()`),
  and records each tick into a `ServerHistoryBuffer`.
- The server sends `ServerSnapshot` packets back to the most recent client.
- The client buffers snapshots in a `SnapshotInterpolator`, interpolates between bracketing
  snapshots, and runs client-side prediction with a local `World` copy.
- The `NetworkSimulator` wraps the UDP socket on both ends for configurable packet loss and
  latency injection (CLI: `--simulate`, `--simulate-loss=X`, `--simulate-latency=Xms`, etc.).

## Implemented Tooling

### Packet Loss / Latency Simulation (`ae::NetworkSimulator`)

Location: `engine/network/include/ae/network/network_simulator.h`

A configurable degradation layer that wraps a `UdpSocket`. Integrated into both server and
client loops. Key knobs:

| Parameter         | Type   | CLI Flag                   | Description                                |
|-------------------|--------|----------------------------|--------------------------------------------|
| `loss_rate`       | float  | `--simulate-loss=0.1`      | Packet drop probability [0.0, 1.0]         |
| `latency_min_ms`  | float  | `--simulate-latency=50`    | Minimum round-trip latency (ms)            |
| `latency_max_ms`  | float  | `--simulate-latency-max=80`| Maximum round-trip latency (ms)            |
| `jitter_ms`       | float  | `--simulate-jitter=10`     | Gaussian jitter stddev per-packet (ms)     |
| `enabled`         | bool   | `--simulate`               | Master toggle                              |

Server logs simulator stats every 300 ticks (~5 seconds).

### Clock Synchronization (`ae::NetworkClock`)

Location: `engine/network/include/ae/network/network_clock.h`

EWMA-based estimator tracking offset between local time and server tick time.

### Server History Buffer (`ae::ServerHistoryBuffer<State, Capacity>`)

Location: `engine/network/include/ae/network/server_history.h`

Templated circular buffer. The server records `HistoricalState` entries (player position +
dummy states) each tick. Buffer capacity: 1024 ticks (~17 seconds at 60 Hz).

### Snapshot Interpolation (`ae::SnapshotInterpolator<Snapshot, Capacity>`)

Location: `engine/network/include/ae/network/snapshot_interpolator.h`

Client-side buffer of up to 3 `ServerSnapshot` packets. On each frame, the client computes
`render_time = now - interpolation_delay` and linearly interpolates player position, velocity,
yaw, health, and shield between the two snapshots bracketing that time. The interpolation
delay is dynamically tuned from measured snapshot arrival jitter.

### Client-Side Prediction (`ahamkara::game::ClientPredictionManager`)

Location: `game/include/ahamkara/game/client_prediction.h`, `game/src/client_prediction.cpp`

Runs a local `World` copy to apply inputs immediately. On each server snapshot:
1. Discards acknowledged inputs from the pending queue.
2. Compares predicted vs authoritative state (position threshold: 0.05 units).
3. If mismatch exceeds threshold, resets to authoritative state and replays
   unacknowledged inputs (reconciliation).
4. Logs reconciliation events with prediction/auth deltas.

## Running with Simulation

Server:
```sh
./build/debug/server/ahamkara_server --simulate --simulate-loss=0.1 --simulate-latency=50 --simulate-jitter=10
```

Client:
```sh
./build/debug/client/ahamkara_client 127.0.0.1 --simulate --simulate-loss=0.05 --simulate-latency=40
```

All simulator knobs are optional; omitting `--simulate` means zero-overhead passthrough.

## Future Work

### Protocol version negotiation
- Extend the packet header with a `supported_versions` bitmask.
- On mismatch, server sends a `VersionMismatch` packet.

### Secure connection handshake
- Client→Server: connection request with nonce.
- Server→Client: session token (HMAC-signed).
- DTLS layered on top once handshake exists.

### Anti-cheat validation hooks
- `ValidationContext` struct passed to server input handlers.
- Hooks: `validate_movement()`, `validate_fire_rate()`, `validate_position()`.
- Violations logged; trigger kick/ban.

### Server-authoritative weapon firing
- Client sends `fire_held`; server decides projectile spawn.
- Server enforces fire rate, ammo, line-of-sight via existing `World::spawn_projectile()`.

### Client prediction for projectiles/effects
- Client spawns local predicted projectile on `fire_held`.
- Server snapshot confirms or replaces; mismatches destroyed.
- Particles/decals remain cosmetic and local only.
