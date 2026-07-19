# Project Ahamkara roadmap

This document defines product direction, sequencing, and phase exit gates. It
does not duplicate task status. Live ownership, priority, dependencies,
acceptance criteria, validation, and agent claims are maintained in the
[Program Roadmap project](https://github.com/users/tfq26/projects/4) and its
linked GitHub Issues.

## North star

Ship three independently releasable products:

- **Ahamkara** is a reusable engine, SDK, runtime host, and toolchain.
- **Flashback** is a game that owns gameplay, presentation, content, and product
  composition while consuming released Ahamkara and Wish contracts.
- **Wish** is a game-neutral online platform for identity, sessions,
  activities, replication, persistence, and live-service integration.

Flashback is the integration proof. A phase is not complete because code exists
on a feature branch: its exit gates must be validated on the intended base
branch or from released artifacts.

## Delivery principles

1. GitHub Issues are the source of truth for executable work. Phase trackers are
   indexes and must not be claimed by agents.
2. An issue is available only when it has the `open` workflow label, no current
   claim, and no open blocker. Claims use the repository's structured 72-hour
   lease protocol.
3. Completion requires the implementation on the intended base branch, focused
   acceptance evidence, and the full validation appropriate to the boundary.
4. Reusable contracts are proven with installed, out-of-tree consumers.
   Product independence is proven with standalone configure, build, test,
   install, package, and smoke paths.
5. Headless deterministic tests prove simulation contracts. Bounded runtime or
   visual evidence supplements them where compilation cannot prove presentation.
6. Cross-product changes begin at a producer contract, then add a Flashback-owned
   adapter. Ahamkara must not import product concepts; Wish must remain
   game-neutral.
7. Performance, memory, protocol, and compatibility claims use explicit budgets
   and retained machine-readable evidence.

## Evidence model

The Program Roadmap separates implementation from validation:

- **Missing** means the required boundary or behavior is absent.
- **Partial** means useful code exists but one or more phase gates lack proof.
- **Complete** means the issue's accepted behavior is present on the intended
  base branch with recorded evidence.

This distinction is deliberate. It prevents test-only primitives, unconsumed
runtime plumbing, stubbed integrations, and develop-only commits from appearing
production-ready.

## Phase 0 — Governance and trusted delivery

[Tracker #105](https://github.com/tfq26/Project-Ahamkara/issues/105)

**Outcome:** the repository, CI, issue ledger, and agent workflow are trustworthy.

Exit gates:

- GitHub exposes product ownership, roadmap phase, workflow state, dependencies,
  validation, confidence, and claim leases;
- `main` is the completion baseline and branch promotion cannot close absent work;
- clean debug, release, headless, package, and installed-consumer jobs pass;
- duplicate, stale, and contradictory issue states are reconciled;
- failure logs distinguish configure, compile, link, test, package, runtime, and
  service boundaries.

## Phase 1 — Ahamkara engine and SDK product

[Tracker #106](https://github.com/tfq26/Project-Ahamkara/issues/106)

**Outcome:** Ahamkara is a reusable, installable engine product rather than a
collection of source-tree libraries.

Exit gates:

- engine-only configure, build, test, install, and package paths require no
  Flashback or Wish source;
- versioned `Ahamkara::` package targets cover the reusable modules needed by an
  external game and reject incompatible consumers clearly;
- an out-of-tree module is hosted through the public runtime contract;
- render/animation and physics/collision ownership remains one-directional;
- jobs, frame allocation, profiling, pacing, memory budgets, diagnostics,
  telemetry, crash capture, authoring tools, and streaming have deterministic
  contracts and focused tests.

## Phase 2 — Flashback independent game product

[Tracker #107](https://github.com/tfq26/Project-Ahamkara/issues/107)

**Outcome:** Flashback owns its executable, gameplay, presentation, content, and
server composition while consuming installed Ahamkara packages.

Exit gates:

- Flashback configures, builds, tests, and packages without repository-relative
  Ahamkara source linkage;
- its executable hosts the Flashback module through the public engine runtime;
- assets, configuration, and branding resolve through product-owned roots;
- local-client and dedicated-server smoke tests run in Flashback CI;
- no Flashback type, rule, or content leaks into Ahamkara.

## Phase 3 — Wish independent platform product

[Tracker #108](https://github.com/tfq26/Project-Ahamkara/issues/108)

**Outcome:** Wish ships a standalone, game-neutral service and SDK/protocol.

Exit gates:

- Wish configures, builds, tests, installs, and packages without Ahamkara or
  Flashback source;
- identity, session, activity, replication, and persistence contracts are
  versioned and game-neutral;
- authentication fails closed outside explicit development mode;
- stable error envelopes and structured observability support operations;
- a dummy external consumer proves the installed boundary.

## Phase 4 — Deterministic multiplayer vertical slice

[Tracker #109](https://github.com/tfq26/Project-Ahamkara/issues/109)

**Outcome:** an authoritative server and predicted client converge under adverse
network conditions with bounded, diagnosable behavior.

Exit gates:

- server tick and player/entity ownership are authoritative and deterministic;
- prediction, reconciliation, interpolation, reliable delivery, reconnect, and
  rewind have focused failure-path tests;
- a reproducible latency, jitter, loss, duplication, and reordering harness
  records convergence and recovery budgets;
- headless smoke tests exercise distinct client identities through released
  product contracts;
- divergence and lifecycle failures produce actionable diagnostics.

## Phase 5 — Combat and first-person presentation slice

[Tracker #110](https://github.com/tfq26/Project-Ahamkara/issues/110)

**Outcome:** one complete authored weapon loop is deterministic from input through
authoritative hit resolution and client presentation.

Exit gates:

- rate of fire, ammunition, reload, recoil, hits, cooldowns, and energy are
  covered by deterministic simulation tests;
- per-weapon meshes, offsets, FOV, IK, grip sockets, reload phases, ADS, sway,
  bob, and recoil are covered by a headless presentation contract;
- immutable snapshots drive animation, audio, VFX, and feedback without mutating
  simulation state;
- invalid content and missing assets fail with stable diagnostics;
- a bounded client smoke test proves the authored vertical slice.

## Phase 6 — World, content, and streaming slice

[Tracker #111](https://github.com/tfq26/Project-Ahamkara/issues/111)

**Outcome:** an authored destination compiles, loads, streams, renders, and hosts
Flashback gameplay within explicit residency and memory budgets.

Exit gates:

- level specifications compile into versioned runtime artifacts with actionable
  diagnostics;
- destination metadata, materials, sky, fog, and asset roots remain
  Flashback-owned;
- spatial partitioning, LOD, batching, sorting, and residency are integrated
  runtime systems rather than isolated primitives;
- async streaming covers cancellation, failure, retry, eviction, and budget
  pressure at a deterministic commit boundary;
- AI navigation, combatants, encounters, inventory, rewards, and progression run
  through Flashback-owned world composition.

## Phase 7 — Sensory and render fidelity

[Tracker #112](https://github.com/tfq26/Project-Ahamkara/issues/112)

**Outcome:** rendering, animation, audio, and VFX form a measured presentation
pipeline while headless simulation remains independent.

Exit gates:

- animation produces skeleton-compatible poses that the renderer consumes;
- multi-light shadows, ambient/IBL/reflections, SSAO/TAA, color grading,
  atmosphere, sky, fog, and LOD are integrated and validated on `main`;
- audio spatialization and occlusion use real world-query contracts;
- screen shake, damage flash, and other feedback have snapshot-chain regression
  tests;
- representative scenes record frame pacing, CPU/GPU timing, and memory budgets;
- legacy compatibility rendering is absent from supported runtime paths.

## Phase 8 — Live-service operations

[Tracker #113](https://github.com/tfq26/Project-Ahamkara/issues/113)

**Outcome:** Wish and Flashback compose secure sessions, activities, progression,
matchmaking, live content, and support diagnostics through released contracts.

Exit gates:

- identity, authentication, parties, matchmaking, activities, persistence, and
  live-content hooks cover failure, retry, recovery, and idempotency;
- Flashback adapters translate product data without leaking game semantics into
  Wish;
- cross-product observability correlates stable engine, game, and service error
  identities;
- version skew is detected before unsafe runtime behavior;
- integration tests retain diagnostic bundles suitable for support triage.

## Phase 9 — Cross-platform release readiness

[Tracker #114](https://github.com/tfq26/Project-Ahamkara/issues/114)

**Outcome:** versioned Ahamkara, Flashback, and Wish artifacts are reproducible and
proven compatible on supported platforms.

Exit gates:

- Linux and Windows build, test, install, package, and consumer matrices pass;
- artifact manifests record product version, commit, toolchain, ABI or protocol
  level, and checksums;
- a clean-room job composes released artifacts without producer source access;
- deterministic performance and memory budgets reject material regressions;
- release notes, migration policy, rollback procedure, and diagnostic artifacts
  are ready for a tagged candidate.

## Sequencing policy

Phases express dependency order, not a ban on parallel work. Agents may advance a
later phase when its issue is genuinely ready and its contracts will not be
invalidated by an earlier product-boundary gate. The critical path remains:

`trusted delivery → Ahamkara SDK → Flashback/Wish independence → released-artifact integration`

Multiplayer, combat, world, sensory, and service slices should advance in thin
vertical increments around that path, with every increment ending in observable
headless, runtime, package, or compatibility evidence.
