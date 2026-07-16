# Strategic roadmap

This document records direction and exit gates, not executable tasks. Priority,
status, dependencies, and acceptance criteria live in
[GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues); the current
architecture initiative is indexed by
[#61](https://github.com/tfq26/Project-Ahamkara/issues/61).

## North star

Build a reusable Ahamkara engine, a separate Flashback game, and an independent
Wish backend/session platform. Flashback should demonstrate that the two lower
level products can be consumed through released contracts rather than shared
source ownership.
[src: user:taufeeqali:2026-07-13: explicit three-repository direction]

## Stage 1 — Trust the baseline

Exit gates:

- debug, release, and headless configurations build their claimed targets;
- tests and packages run from clean checkouts;
- current documentation contains concrete commands and no local task queue;
- diagnostics distinguish configure, compile, link, test, runtime, and service
  failures.

## Stage 2 — Make Ahamkara a real engine product

Exit gates:

- engine-only configure/build/install requires no Flashback or Wish source;
- exported namespaced package targets are consumable out of tree;
- render/animation and physics/collision ownership is one-directional;
- the runtime hosts a game through a documented module contract;
- stable errors, logs, telemetry, crash data, and diagnostic bundles form one
  operational path.

## Stage 3 — Make Flashback a real game product

Exit gates:

- gameplay, presentation, content, branding, configuration, and server
  composition live in the Flashback repository;
- Flashback consumes released Ahamkara packages;
- multiplayer assigns distinct authoritative player/entity ownership;
- local and dedicated-server smoke tests run in Flashback CI.

## Stage 4 — Make Wish a real independent product

Exit gates:

- Wish builds/tests/packages without Ahamkara or Flashback source;
- session, admission, activity, replication, identity, and backend contracts
  are game-neutral;
- authentication is fail-closed outside explicit development mode;
- Wish publishes a versioned SDK/protocol that Flashback can consume.

## Stage 5 — Grow capabilities through the correct owner

After separation, rendering, animation, world scale, networking, tools, and
performance improvements belong to Ahamkara; weapons, AI, encounters, content,
and presentation belong to Flashback; social, activity, identity, and live
service capabilities belong to Wish. Cross-product work begins with a contract
change in the producer and an adapter change in Flashback.
