# Wish Engine session runtime

The dedicated server now uses a small session layer that stays separate from simulation:

- each UDP peer gets its own `ClientSession`
- the runtime tracks address/identity, last seen time, connection state, and last processed input sequence
- per-client `SequenceTracker` state is kept for snapshot envelopes
- stale clients are removed by a disconnect timeout
- snapshots are broadcast to every connected client

## Provisional pieces

- admission is address-based only; there is no auth or handshake
- the gameplay world is still single-state, so multiple clients share the same authoritative simulation
- there is no per-client entity ownership yet

## Layout

- `wish/session/session_runtime.h` — session bookkeeping and client iteration
- `server/src/dedicated_server_main.cpp` — consumes session state and broadcasts snapshots
- `tests/src/session_runtime_tests.cpp` — focused runtime coverage

