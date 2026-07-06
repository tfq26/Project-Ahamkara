---
type: feature-brief
status: draft
created: 2026-07-04
last_verified:
subsystems: [client, game, server, tests, tools, docs]
source_of_truth: [README.md, docs/roadmap/roadmap.md, docs/roadmap/autonomous-scale-roadmap.md, docs/vault/workflows/phase-slice-map.md]
---

# Autonomous Validation and Playtest Harness

Status: Draft
Owner:
Created: 2026-07-04

## Goal

Build a reusable validation layer that lets frontier agents and automated
workers play the game well enough to test real gameplay flows without manual
input. The harness should be able to:

- move characters through the world
- interact with objects and UI affordances
- use weapons and abilities
- exercise respawn and encounter loops
- report pass/fail evidence from logs, snapshots, and scripted assertions

## Non-Goals

- Shipping a general-purpose bot framework for players
- Training a learned policy before deterministic automation exists
- Replacing human playtesting for feel/polish decisions
- Building content tooling that only serves the harness and no gameplay path

## Affected Systems

- `client` input injection and scenario execution
- `game` interaction, movement, combat, and simulation hooks
- `server` authoritative validation and headless multiplayer runs
- `tests` scenario verification and regression coverage
- `tools` automation runners and report capture
- `docs` roadmap, queue, and reporting workflow

## Current Understanding

The repo already has a deterministic simulation spine, local input providers,
navigation tests, and a server/client networking skeleton. That is enough to
start a harness, but not enough to let agents reliably "play" the game end to
end. The missing pieces are a stable action API, scenario orchestration, object
interaction hooks, and a verification contract that produces evidence instead
of just claiming success.

The core harness is now implemented. The remaining work is breadth: more
scenario coverage, richer artifact capture, server-session validation, and
making autonomous validation the default path for gameplay checks.

## Implementation Sketch

1. Define a compact action vocabulary for automated agents: move, look, jump,
   crouch, interact, fire, reload, use ability, and pause/report.
2. Add a scenario runner that can execute scripted actions against local or
   headless sessions.
3. Expose object and encounter affordances through the gameplay layer so the
   harness can trigger real interactions instead of screen scraping.
4. Capture deterministic state, replay inputs, and collect failure artifacts
   such as logs, snapshots, and screenshots.
5. Add smoke scenarios for locomotion, item pickup, door/use interaction,
   combat, and respawn loops.
6. Route validation output into the existing report/queue workflow so agents can
   hand off evidence cleanly.

## Verification

- Headless build and test pass
- Scenario runner can complete scripted movement and interaction loops
- Server-authoritative and local runs both produce deterministic evidence
- Failure cases emit enough state to debug without rerunning manually

## Open Questions

- Should the harness live entirely in `tools/` or partly in `tests/`?
- Which interactions must be first-class gameplay hooks versus generic
  automation helpers?
- How much of the harness should be reusable by future QA or benchmark flows?

## Handoff Notes

This feature is the prerequisite layer for the larger roadmap. The follow-up is
to keep expanding the scenario library and wire every new gameplay surface into
machine-driven validation before it becomes a manual-only test.

See:

- [Consolidated roadmap](../../roadmap/roadmap.md)
- [Autonomous scale roadmap](../../roadmap/autonomous-scale-roadmap.md)
- [Phase slice map](../workflows/phase-slice-map.md)
