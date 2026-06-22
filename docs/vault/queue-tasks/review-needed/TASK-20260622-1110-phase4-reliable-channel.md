---
type: opencode-task
status: review-needed
created: 2026-06-22
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - engine/network
related_feature:
report: ../../../reports/subagents/TASK-20260622-1110-phase4-reliable-channel-report.md
---

# TASK-20260622-1110-phase4-reliable-channel

## Goal

Add a transport-agnostic `ae::ReliableChannel` that buffers reliable outgoing
packets by sequence, consumes incoming ACK metadata (`ack_sequence` +
`ack_bitfield`, matching `SequenceTracker`'s encoding), and reports packets that
must be retransmitted after a timeout. Roadmap **Phase 4** (#1, transport
reliability).

## Verify First (done)

`PacketEnvelope` (seq/ack_seq/ack_bitfield) and `SequenceTracker` (ack bitfield
logic) already exist, but `sequence_tracker.h` states it does NOT buffer or
retransmit, and the packet-sequencing report confirms no reliable channel reads
the ack bitfield to trigger resends. Confirmed real gap.

## Scope

In bounds:
- New header-only `engine/network/include/ae/network/reliable_channel.h`:
  - `on_send(seq, data, len, now)` — buffer a reliable packet.
  - `on_ack(ack_sequence, ack_bitfield)` — remove acked packets (bit i acks
    `ack_sequence - 1 - i`, matching SequenceTracker; 16-bit wraparound-safe).
  - `collect_retransmits(now, timeout)` — return unacked sequences past timeout
    (and refresh their send time).
  - `payload(seq)`, `pending_count()`, `clear()`.
- A dedicated headless unit test (`ahamkara_reliable_channel_tests`).
- Clock-injected (no real time) and socket-agnostic → fully unit-testable.

Out of bounds:
- Wiring it into the live client/server loops (a separate integration task),
  unreliable/ordered channels, bandwidth budgeting.

## Acceptance Bar

- `on_ack` removes directly-acked and bitfield-acked sequences (incl. wraparound).
- `collect_retransmits` returns only unacked, timed-out sequences and refreshes them.
- New unit test passes; build + existing tests stay green.

## Validation

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

## Reporting Required

Standard: report, master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
