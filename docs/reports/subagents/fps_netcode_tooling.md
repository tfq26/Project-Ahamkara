# Task
Implement FPS-grade client/server netcode tooling: packet loss/latency simulation, clock sync, server history buffer, snapshot interpolation, and client-side prediction — wired into the existing Ahamkara UDP loop.

# Outcome

**Fully implemented:**
- `ae::NetworkSimulator` — configurable packet loss, latency, and jitter wrapper around `UdpSocket`. Wired into both `ahamkara_server` and `ahamkara_client` with CLI flags (`--simulate`, `--simulate-loss=X`, `--simulate-latency=X`, `--simulate-latency-max=X`, `--simulate-jitter=X`).
- `ae::NetworkClock` — EWMA-based clock offset and RTT estimator.
- `ae::ServerHistoryBuffer<State, Capacity>` — templated circular buffer. Server records `HistoricalState` per tick (1024-tick capacity).
- `ae::SnapshotInterpolator<Snapshot, Capacity>` — buffers up to 3 `ServerSnapshot` packets, linearly interpolates player position/velocity/yaw/health/shield between bracketing snapshots using a dynamically tuned interpolation delay.
- `ahamkara::game::ClientPredictionManager` — runs a local `World` copy for immediate input response, reconciles against authoritative snapshots (0.05-unit position threshold), replays unacknowledged inputs on mismatch.

**Partially implemented:**
- Clock sync (`NetworkClock`) is instantiated and fed snapshots in the client loop, but its offset estimate is not yet used to correct the interpolation timestamp (interpolation uses local arrival-time bracketing).

**Not implemented:**
- Multi-client support (server still single-client).
- Reliable UDP retransmission channels.
- Protocol version negotiation handshake.
- Secure connection (DTLS/nonce exchange).
- Anti-cheat validation hooks.
- Server-authoritative weapon firing (projectile spawning exists in `World::spawn_projectile()` but fire-authority is still client-side).
- Prediction for projectiles/effects (projectile prediction is designed but not coded).

# Files Changed

- `engine/network/include/ae/network/network_simulator.h` — **New.** `SimulatorConfig`, `SimulatorStats`, `NetworkSimulator` class.
- `engine/network/src/network_simulator.cpp` — **New.** Loss/delay/jitter implementation, TTL expiry, stat counters.
- `engine/network/include/ae/network/network_clock.h` — **New.** `NetworkClock` struct with EWMA offset + RTT.
- `engine/network/src/network_clock.cpp` — **New.** Clock sync implementation.
- `engine/network/include/ae/network/server_history.h` — **New.** `ServerHistoryBuffer<State, Capacity>` template.
- `engine/network/include/ae/network/snapshot_interpolator.h` — **New.** `SnapshotInterpolator<Snapshot, Capacity>` template with LERP logic.
- `engine/network/CMakeLists.txt` — Added `network_simulator.cpp`, `network_clock.cpp`, `sequence_tracker.cpp` (pre-existing file); added `ahamkara_game` link dep (needed by `sequence_tracker.cpp` which references `PacketEnvelope`).
- `game/include/ahamkara/game/client_prediction.h` — **New.** `ClientPredictionManager` class.
- `game/src/client_prediction.cpp` — **New.** Prediction + reconciliation implementation.
- `game/CMakeLists.txt` — Added `client_prediction.cpp`.
- `server/src/dedicated_server_main.cpp` — Rewrote server loop: added `NetworkSimulator` wrapping socket, `ServerHistoryBuffer<HistoricalState, 1024>`, simulator CLI parsing, periodic stats logging, `PacketEnvelope` on outgoing snapshots.
- `client/src/headless_clients.cpp` — Rewrote network client loop: added `NetworkSimulator`, `NetworkClock`, `SnapshotInterpolator`, `ClientPredictionManager`, interpolated state logging every 60 ticks, simulator CLI parsing.
- `client/src/main.cpp` — Updated `run_network_client` forward declaration and call signature to pass `argc/argv`.
- `tests/CMakeLists.txt` — Added `ae_network` link dep for smoke tests.
- `tests/src/network_smoke_tests.cpp` — Added 14 tests (5 history buffer, 4 simulator, 5 clock) plus `#include` for `ae/network/sequence_tracker.h`.
- `docs/systems/networking.md` — Rewritten with all implemented tooling, CLI usage examples, future paths.

# Interfaces Added Or Changed

**New public types (engine layer, namespace `ae`):**
- `struct SimulatorConfig` — `loss_rate`, `latency_min_ms`, `latency_max_ms`, `jitter_ms`, `enabled`
- `struct SimulatorStats` — `packets_received`, `packets_dropped`, `packets_delayed`, `packets_sent`, `packets_expired`, `bytes_received`, `bytes_sent`, `reset()`
- `class NetworkSimulator` — `configure()`, `send_to()`, `receive_from()`, `update(dt)`, `reset_stats()`, `config()`, `stats()`, `socket()`, `is_open()`
- `struct NetworkClock` — `record_snapshot()`, `estimate_server_time()`, `record_rtt()`, `reset()`, `smoothed_offset_seconds()`, `estimated_rtt_seconds()`
- `template ServerHistoryBuffer<State, Capacity>` — `record()`, `get()`, `newest_tick()`, `oldest_tick()`, `size()`, `capacity()`, `reset()`
- `template SnapshotInterpolator<Snapshot, Capacity>` — `push()`, `interpolate()`, `get_bracketing_snapshots()`, `suggest_delay_seconds()`, `reset()`, `size()`

**New public types (game layer, namespace `ahamkara::game`):**
- `class ClientPredictionManager` — `apply_input()`, `reconcile()`, `world()`, `pending_count()`, `last_acknowledged()`, `reset()`, `kMaxPendingInputs`

**Changed function signatures:**
- `run_network_client(const std::string&, int argc, char** argv)` — added `argc/argv` for CLI parsing.
- `parse_float_arg()` and `parse_bool_arg()` — local helpers added in both server and client mains (duplicated; should be consolidated into a utility header later).

**New CLI flags (server and client):**
- `--simulate` — enable simulator
- `--simulate-loss=<float>` — drop probability [0.0, 1.0]
- `--simulate-latency=<float>` — min RTT in ms
- `--simulate-latency-max=<float>` — max RTT in ms
- `--simulate-jitter=<float>` — Gaussian jitter stddev in ms

**New tests (in `network_smoke_tests`):**
- `test_server_history_basic`, `record_and_retrieve`, `gap_fill`, `wraparound`, `reset`
- `test_simulator_disabled_is_passthrough`, `full_loss`, `latency_delays_packets`, `stats_counters`
- `test_network_clock_initial_state`, `snapshot_offset`, `smoothing`, `rtt_tracking`, `reset`

**Build dependency change:**
- `ae_network` now depends on `ahamkara_game` (needed by pre-existing `sequence_tracker.cpp` which references `PacketEnvelope` from game layer).

# Behavior

- Server with `--simulate --simulate-loss=0.1 --simulate-latency=50` will drop ~10% of packets and delay all sends by ~25ms one-way. Server logs history buffer range and simulator stats every 300 ticks.
- Client with same flags logs interpolated position, raw snapshot position, and predicted position every 60 ticks, e.g.: `[Client] tick=60 interp_delay=0.084s | interp_pos=(-12, 0) | snap_pos=(-12, 0) | snap_tick=174 | pred_pos=(-12, 0)`.
- Client prediction reconciliation events are logged with position deltas when the server disagrees beyond 0.05 units.
- Without `--simulate`, behavior is identical to before — zero overhead, pure passthrough.

# Validation

| Command | Result |
|---------|--------|
| `cmake --build build/debug --target ahamkara_smoke_tests -j10` | Build pass |
| `cmake --build build/debug --target ahamkara_world_tests -j10` | Build pass |
| `cmake --build build/debug --target ahamkara_server -j10` | Build pass |
| `cmake --build build/debug --target ahamkara_client -j10` | Build pass |
| `./build/debug/tests/ahamkara_smoke_tests` | All 28 tests pass (14 new + 14 existing) |
| `./build/debug/tests/ahamkara_world_tests` | All 4 tests pass |
| Integration: server `--simulate-loss=0.1 --simulate-latency=50 --simulate` + client `--simulate-loss=0.05 --simulate-latency=40 --simulate` | Both processes start, exchange packets, log interpolated/predicted state, no crashes |

No build warnings from new code. Pre-existing EnTT deprecation warnings (`operator"" _hs`) are unrelated.

# Known Gaps

- `parse_float_arg` / `parse_bool_arg` are duplicated in `server/src/dedicated_server_main.cpp` and `client/src/headless_clients.cpp`. Should be extracted to a shared `ae/core/cli_utils.h`.
- `ClientPredictionManager::reset()` uses placement-new to reconstruct `World` because its copy assignment is deleted. This works but is fragile; a `World::reset()` method would be cleaner.
- `SnapshotInterpolator::lerp_player` hardcodes field access (`.position.x`, `.yaw`, etc.) — works for `ReplicatedPlayerState` but is not truly generic.
- Clock sync is collected but not wired into interpolation timestamp correction — interpolation uses raw local arrival times.
- Server is still single-client (tracks `last_client`).
- `sequence_tracker.cpp` exists in `engine/network/src/` and is compiled but creates a layering violation (engine depends on game). It was pre-existing and was added to `CMakeLists.txt` concurrently; this forced adding `ahamkara_game` as a dep of `ae_network`.

# Risks

- **Layering violation:** `ae_network` now links `ahamkara_game` because `sequence_tracker.cpp` includes `ahamkara/game/net_types.h`. This should be resolved by moving `SequenceTracker` to the game layer or `PacketEnvelope` to the engine layer. The dependency is PUBLIC, so anything linking `ae_network` now transitively depends on all of `ahamkara_game` (Jolt, EnTT, etc.).
- **Placement-new in reset():** If `World` gains a non-trivial destructor or virtual methods in the future, the placement-new pattern in `ClientPredictionManager::reset()` will break subtly.
- **Interpolation timestamp:** Using arrival-time bracketing rather than clock-sync-corrected server time means interpolation is accurate only when network jitter is low relative to the interpolation delay.
- **No graceful degradation:** If the server stalls or stops sending snapshots, the client interpolator will hold the last known state forever with no timeout/disconnect.

# Next Recommended Steps

1. **Extract CLI utils** — Move `parse_float_arg`/`parse_bool_arg` into `engine/core/include/ae/core/cli_utils.h` to eliminate duplication.
2. **Wire clock sync into interpolation** — Use `NetworkClock::estimate_server_time()` to convert the target render time to estimated server time, then bracket by `server_tick` in the interpolator instead of arrival time. This corrects for clock drift.
3. **Fix layering** — Move `SequenceTracker` from `engine/network/` to `game/` (or move `PacketEnvelope` to the engine layer). Remove the `ahamkara_game` dependency from `ae_network`.
4. **Multi-client server** — Replace `last_client` with a `std::unordered_map<NetAddress, ClientState>` in the server loop.
5. **Snapshot timeout** — Add a staleness check in `SnapshotInterpolator` (e.g., drop snapshots older than 1 second, log warning).
6. **World::reset()** — Add a proper reset method to `World` so `ClientPredictionManager` doesn't need placement-new.
7. **Server-authoritative firing** — Move fire-rate enforcement to the server side. The projectile spawning infrastructure already exists in `World::spawn_projectile()`.

# Notes For Integration

- The `ae_network` → `ahamkara_game` dependency is a **build-system concern**. If downstream targets that only need UDP sockets start pulling in Jolt/EnTT, extract `SequenceTracker` first.
- The `SnapshotInterpolator` and `ClientPredictionManager` are active in the headless network client but NOT in the windowed/debug render client (`debug_client.cpp` / `run_windowed_client`). The render client still runs locally with no networking.
- All simulator flags are parsed in `main()` bodies, not via config files. If you add `ClientConfig` file-based config for these knobs, update both `client/src/main.cpp` and `client/src/headless_clients.cpp`.
- The `PacketEnvelope` struct in `net_types.h` has `sequence`, `ack_sequence`, `ack_bitfield` fields. The server and client both track a simple incrementing envelope now; the `SequenceTracker` class (pre-existing) is not wired in. It is compiled as part of `ae_network` but unused in current main loops.
