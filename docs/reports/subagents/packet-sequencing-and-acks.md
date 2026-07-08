# Task
Implement packet sequence numbers and ACK metadata (items 15, 16-prep) as the narrow first slice of low-level UDP networking improvements for a multiplayer FPS.

# Outcome

- **Fully implemented**: `PacketEnvelope` struct (seq, ack_seq, ack_bitfield) added to every UDP datagram; envelope serialization integrated into `ByteWriter`/`ByteReader`; packet size computations updated; `SequenceTracker` class with full ACK bitfield logic, loss estimation, and out-of-order handling; 6 unit tests.
- **Partially implemented**: The `SequenceTracker` is compiled into `ae_network` and has passing tests, but is NOT wired into the server or client runtime loops. The server and client manually increment `envelope.sequence++` without using `prepare_outgoing()` or `process_incoming()`. The ACK bitfield and loss estimation are therefore unused at runtime.
- **Not implemented**: Reliable/unreliable channels, retransmission, delta compression, snapshot priority, interest management, bandwidth budgeter (all scoped to future tasks).

# Files Changed

- `game/include/ahamkara/game/net_types.h` — Added `PacketEnvelope` struct (8 bytes: u16 seq, u16 ack_seq, u32 ack_bitfield) + static_assert.
- `game/include/ahamkara/game/net_packets.h` — Added `write_envelope`/`read_envelope` in `detail` namespace; added `kEnvelopeWireSize` constexpr; updated `player_input_packet_size()` and `server_snapshot_packet_size()` to include 8-byte envelope; updated all 4 serialize/deserialize function signatures to take a `PacketEnvelope&` parameter.
- `engine/network/include/ae/network/sequence_tracker.h` — **New file**: `ae::SequenceTracker` class with `prepare_outgoing()`, `process_incoming()`, `packets_sent()`, `packets_received()`, `estimated_lost()`.
- `engine/network/src/sequence_tracker.cpp` — **New file**: ACK bitfield shift/set logic, gap-based loss estimation, out-of-order correction.
- `engine/network/CMakeLists.txt` — Added `src/sequence_tracker.cpp` to `ae_network` library; added `ahamkara_game` to link libraries (for `PacketEnvelope` dependency).
- `tests/src/network_smoke_tests.cpp` — Updated 3 existing packet round-trip/corruption tests for new serializer signatures (envelope param); added 6 new `SequenceTracker` tests; includes `ae/network/sequence_tracker.h`.
- `docs/systems/networking.md` — **Note**: This file was overwritten by a parallel agent. The packet format diagram, envelope spec, wire sizes, and sequence tracker documentation I wrote are no longer present. The current version documents snapshot interpolation and client prediction without mentioning `SequenceTracker`.

# Interfaces Added Or Changed

## New struct
- `ahamkara::game::PacketEnvelope` — trivially copyable, 8 bytes on wire. Fields: `u16 sequence`, `u16 ack_sequence`, `u32 ack_bitfield`. Defined in `net_types.h`.

## New class
- `ae::SequenceTracker` — per-peer sequence bookkeeping. Public API: `prepare_outgoing()` → `PacketEnvelope`, `process_incoming(const PacketEnvelope&)`, `packets_sent() -> u32`, `packets_received() -> u32`, `estimated_lost() -> u32`. Defined in `sequence_tracker.h`.

## Changed function signatures (BREAKING)
All four packet serialize/deserialize functions in `net_packets.h` now require a `PacketEnvelope&` parameter:

Old:
```cpp
serialize_player_input_packet(command, buffer)
deserialize_player_input_packet(buffer, command)
serialize_server_snapshot_packet(snapshot, buffer)
deserialize_server_snapshot_packet(buffer, snapshot)
```

New:
```cpp
serialize_player_input_packet(envelope, command, buffer)
deserialize_player_input_packet(buffer, envelope, command)
serialize_server_snapshot_packet(envelope, snapshot, buffer)
deserialize_server_snapshot_packet(buffer, envelope, snapshot)
```

## New constexpr
- `ahamkara::game::kEnvelopeWireSize` = 8

## Changed packet sizes
- PlayerInput: 43 → 51 bytes
- ServerSnapshot: 61 → 69 bytes

## New tests
- `test_sequence_tracker_initial_state`
- `test_sequence_tracker_outgoing_increment`
- `test_sequence_tracker_incoming_without_gaps`
- `test_sequence_tracker_incoming_with_gaps`
- `test_sequence_tracker_out_of_order_fills_gap`
- `test_sequence_tracker_ack_reflected_in_outgoing`

# Behavior

At runtime, every UDP packet now carries an 8-byte envelope between the header (magic/version/type) and the type-specific payload:

```
[magic 4B][version 2B][type 2B][seq 2B][ack_seq 2B][ack_bitfield 4B][payload...]
```

The `PacketHeader` struct (`magic`, `version`, `type`) is untouched. Header validation in `read_header` works identically.

The server and client both manually manage the envelope:
- Server increments `envelope.sequence++` before each outgoing snapshot.
- Client increments `envelope.sequence++` before each outgoing input.
- Neither side reads or uses the incoming envelope's `ack_sequence` or `ack_bitfield` values.
- Neither side calls `SequenceTracker::process_incoming()`.

The `SequenceTracker` class is available for use but is not yet integrated.

# Validation

Build command:
```
cmake --build build --target ahamkara_smoke_tests
```

Result: **Compiles and links with 0 errors**. Only warnings are pre-existing EnTT `operator"" _hs` deprecation warnings.

Test run:
```
./build/tests/ahamkara_smoke_tests
```

Results:
- `test_player_input_packet_round_trip` — **PASS** (envelope round-trip included)
- `test_snapshot_packet_round_trip` — **PASS** (envelope round-trip included)
- `test_packet_validation_rejects_corruption` — **PASS**
- `test_shared_movement_simulation` — **PASS**
- All 5 server history buffer tests — **PASS**
- All 4 network simulator tests — **PASS**
- All 5 network clock tests — **PASS**
- All 6 sequence tracker tests — **PASS**
- `test_local_play_simulation_tick_progression` — **FAIL** (pre-existing; expects z=6.0f, got z=5.7125f; unrelated to networking)

Server build:
```
cmake --build build --target ahamkara_server
```
Result: **Compiles and links with 0 errors**.

Client build:
```
cmake --build build --target ahamkara_client
```
Result: **FAIL** (pre-existing; `debug_client.cpp:402` — undeclared `accumulator` variable; unrelated to networking).

# Known Gaps

1. **SequenceTracker not wired into runtime**: The class compiles and passes tests, but neither `server/src/dedicated_server_main.cpp` nor `client/src/headless_clients.cpp` includes `sequence_tracker.h` or calls `prepare_outgoing()`/`process_incoming()`. The envelope is incremented manually (`envelope.sequence++`) and ACK data is ignored. This means `estimated_lost()` always returns 0 at runtime and the ACK bitfield is always 0 on the wire.
2. **No per-client tracking on server**: The server uses a single `envelope` variable for all clients. When multi-client support arrives, it needs one `SequenceTracker` per `NetAddress`.
3. **No reliable channel**: The ACK bitfield is on the wire but no code reads it to trigger retransmission. This is intentionally deferred to a future task.
4. **Docs overwritten**: The `docs/systems/networking.md` packet format section I wrote was replaced by a parallel agent. The current doc mentions snapshot interpolation and client prediction but not sequence tracking or the envelope format. The wire format details live only in the code comments and this report.
5. **`debug_client.cpp` build failure**: Pre-existing bug blocks the full client build; only the headless client path (used in network/smoke tests) compiles.

# Risks

1. **Wire format break on packet sizes**: The 8-byte envelope makes packets incompatible with any pre-envelope v1 clients/servers. Since this is a single repo where client and server are rebuilt together, there is no deployment risk now. If wire compatibility with older builds is ever needed, bump `kPacketVersion` to 2.
2. **Envelope field is unused on receive**: The `ack_sequence` and `ack_bitfield` fields are written to the wire but never read at the application level. This wastes 6 bytes per packet until `SequenceTracker::process_incoming()` is called. The waste is negligible (6 bytes × 120 packets/sec = 720 bytes/sec).
3. **SequenceTracker header depends on ahamkara_game**: `sequence_tracker.h` includes `ahamkara/game/net_types.h` for `PacketEnvelope`. This creates a dependency from `ae_network` → `ahamkara_game`. If `PacketEnvelope` were moved to `ae/network/`, this dependency could be removed.
4. **16-bit sequence wrap**: Sequence numbers wrap every ~18 minutes at 60 Hz. The `SequenceTracker` handles this correctly via unsigned subtraction, but any code that stores raw sequence numbers without relative comparison will break at wrap boundaries.

# Next Recommended Steps

1. **Wire SequenceTracker into server**: Add `#include "ae/network/sequence_tracker.h"` to `dedicated_server_main.cpp`, create `ae::SequenceTracker seq_tracker`, call `seq_tracker.process_incoming(in_envelope)` after deserializing client input, and use `seq_tracker.prepare_outgoing()` instead of manually incrementing `envelope.sequence++`. Log `seq_tracker.estimated_lost()` periodically.
2. **Wire SequenceTracker into client**: Same for `headless_clients.cpp` — call `process_incoming()` on received snapshots and `prepare_outgoing()` for sent input. This enables RTT estimation from ACK timing.
3. **Log sequence stats**: Add periodic (every 300 ticks) logging of `packets_sent()`, `packets_received()`, `estimated_lost()` on both sides so the ACK pipeline is observable.
4. **Build reliable channel on top**: With ACK data flowing, implement a `ReliableChannel` that buffers sent packets keyed by sequence, inspects incoming ACK bitfields, and retransmits unacknowledged packets after a timeout.
5. **Move PacketEnvelope to ae::network**: Break the `ae_network` → `ahamkara_game` dependency by defining `PacketEnvelope` (or a typedef) in `ae/network/` and having `ahamkara::game` include it.
6. **Fix docs**: Restore the packet format diagram and envelope specification to `docs/systems/networking.md`.
7. **Fix `debug_client.cpp`**: The undeclared `accumulator` variable blocks the windowed client build; it's a one-line fix to declare it.

# Notes For Integration

- The `PacketEnvelope` is now a required parameter in all 4 serialize/deserialize functions. Any code that constructs packets directly (outside the server/client loops already updated) will **fail to compile** until it provides an envelope argument.
- The `ServerHistoryBuffer` on the server does NOT store envelope data — it only stores `HistoricalState` (tick + positions). This is correct; envelope metadata is per-packet, not per-tick.
- The `SnapshotInterpolator` on the client does NOT need the envelope — interpolation works on snapshot payloads only.
- If the `SequenceTracker` is wired into the server, note that `process_incoming()` only uses `envelope.sequence` from the remote peer. The `envelope.ack_sequence` and `envelope.ack_bitfield` in incoming packets represent what the PEER has received from US — these should be read by a future reliable channel, not by `process_incoming()`.
