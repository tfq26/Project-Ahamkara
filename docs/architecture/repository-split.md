# Ahamkara, Flashback, and Wish repository split

Status: Accepted destination; migration in progress

The destination is three independent repositories and projects, not three
folders that continue to rely on a shared checkout.
[src: user:taufeeqali:2026-07-13: requested complete repository and project separation]

## Repository ownership

### Ahamkara

Ahamkara owns reusable technology:

- core utilities, configuration, logging, diagnostics, telemetry, and crash
  capture;
- runtime lifecycle and generic client/server hosting contracts;
- platform, input, rendering, animation, audio, UI, physics, collision, and
  generic networking;
- asset formats, import interfaces, and engine-facing tools;
- public headers, CMake package exports, examples, and engine tests.

Ahamkara must not own Flashback weapons, game modes, activities, window titles,
default maps, content, or Wish service concepts.

### Flashback

Flashback owns the shipped game:

- game module and product entry points;
- player/world rules, weapons, abilities, AI, activities, progression, and
  presentation;
- game-specific client and dedicated-server composition;
- assets, levels, configuration, branding, and automated playtest scenarios;
- adapters that translate Flashback gameplay data to Ahamkara runtime contracts
  and Wish service contracts.

The current sample is not yet this boundary: it links the entire
`ahamkara_client_lib` and calls the shared local-client entry point.
[src: file: samples/flashback/CMakeLists.txt:4-13]
[src: file: samples/flashback/src/main.cpp:28-59]

### Wish

Wish owns backend and online-service behavior:

- identity and authentication contracts;
- sessions, admission, parties, matchmaking, and activity lifecycle;
- replication/service envelopes and operational/admin surfaces;
- backend adapters such as Nakama;
- a game-neutral client/server SDK and independent test fixtures.

Wish must not import Flashback command or snapshot types. The current session
runtime does so and is therefore transitional.
[src: file: wish/session/session_runtime.h:1-6]
[src: file: wish/session/session_runtime.h:76-86]

## Dependency and version rules

| Consumer | Allowed dependency | Forbidden dependency |
|---|---|---|
| Ahamkara | Third-party libraries declared by the engine | Flashback or Wish source/packages |
| Wish | Its declared backend/transport libraries | Ahamkara or Flashback source/packages |
| Flashback | Versioned Ahamkara and Wish releases | Sibling source paths or private headers |

- Use semantic release tags and lock exact compatible versions in Flashback.
- Consume packages through installed/exported targets or a package manager, not
  `../Ahamkara` or `../Wish` include directories.
- Cross-project protocols are versioned data contracts. Wish implements its
  side independently; Flashback owns translation to game state.
- Breaking changes require a version bump and a compatibility note in the
  producer repository.

## Extraction gates

### Gate 1 — internal boundaries

- Ahamkara configures without `game/`, `client/`, `server/`, `wish/`, or
  `samples/flashback/`.
- Wish configures without root-repository private include paths or Flashback
  game headers.
- Flashback implements a real game-module contract rather than calling a
  monolithic client function.

### Gate 2 — package contracts

- Ahamkara exports namespaced CMake targets and a package config.
- Wish exports an independent SDK/package and versioned protocol.
- Out-of-tree consumer fixtures build using installed packages only.
- Debug, release, and headless CI validates the claimed variants.

### Gate 3 — history-preserving extraction

- Extract each product with its relevant Git history.
- Establish repository-specific CI, issue templates, docs, and release tags.
- Make Flashback consume released Ahamkara and Wish artifacts.
- Remove the extracted product folders from the Ahamkara repository only after
  the external Flashback build and smoke tests pass.

## Documentation after extraction

Each repository keeps only its own architecture, design, maintenance, and
error-code catalog. Flashback documents compatible Ahamkara/Wish versions.
Ahamkara and Wish document only their public contracts; neither documents
Flashback internals as part of its own architecture.

Execution status and task breakdown remain in
[GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues) during the
transition.

