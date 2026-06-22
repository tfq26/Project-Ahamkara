---
type: opencode-task
status: open
created: 2026-06-22
queued_by: opencode
assigned_to: codex
priority: high
escalation_tier: medium
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - game
  - engine/network
related_feature:
report:
---

# TASK-20260622-1200-phase4-netcode-milestone-review

## Type

Milestone review + validation handoff (batched, per the "review at milestone"
workflow). This is the one Codex task for the Phase 4 netcode milestone.

## What To Review (already implemented + self-validated headlessly)

1. `TASK-20260622-1100-phase4-reconciliation-replay-fix` — removed the
   `last_ack_ != 0` guard in `ClientPredictionManager::reconcile` so unacked
   inputs replay on the first snapshot; added `test_first_snapshot_reconciliation`.
   Build (debug) + tests green.
2. `TASK-20260622-1110-phase4-reliable-channel` — header-only `ae::ReliableChannel`
   (buffer reliable packets, consume ack bitfield incl. wraparound, report
   timed-out retransmits) + `ahamkara_reliable_channel_tests`. (Already accepted.)

Plus the verify-first findings that the rest of Phase 4 is already present:
server-authoritative firing (`can_fire()` cooldown + `consume_ammo()` +
lag-compensated hitscan), prediction/reconciliation, snapshot interpolation,
`NetworkClock`, `NetworkSimulator`, and the `ClientHello/ServerWelcome/ServerReject`
handshake (version check + session token). Anti-cheat `validate_*` hooks are
largely moot under the input-authoritative model.

## What Needs YOU (validation this headless env cannot do)

**Live-loop reliability integration** — the one genuinely-remaining Phase 4 gap,
blocked here on the lack of a working socket/runtime (network tests trip
sandbox socket-permission failures):

- Wire `SequenceTracker` into `server/src/dedicated_server_main.cpp` (per-peer)
  and `client/src/headless_clients.cpp` — replace manual `++main_envelope_seq`
  with `prepare_outgoing()`, call `process_incoming()` on receive, log
  `estimated_lost()`.
- Hook `ReliableChannel` for the natural reliable consumer: retransmit the
  handshake (`ClientHello`/`ServerWelcome`) under loss.
- Validate end to end with `--simulate --simulate-loss=… --simulate-latency=…`
  on a machine with sockets enabled.

## Ask

Review items 1–2 holistically (accept/revise), and either validate-and-own the
live-loop integration in a socket-capable environment or confirm it as the
documented next step. Reports live in `docs/reports/subagents/`.
