# Ahamkara Agent Guide (Draft)

> Draft based on the 2026-10-16 audit of a dirty checkout at `b705d74`. Verify all commands and paths against the selected clean baseline before shipping this file. This guide does not claim the general game-authoring API or visual verification loop is complete.

## What this repository contains

Ahamkara is a C++20/CMake native engine and game workspace. `engine/` contains reusable libraries; `game/` and `client/` hold the current Flashback-oriented gameplay and client; `server/` and `wish/` contain native multiplayer/session systems; `tools/` and `assets/` contain import, authoring, and content pipelines. `frontend/` and `backend/` are a separate Vue/Vite and Hono/TypeScript product surface. Do not confuse the native client with the web frontend, or Wish with the Hono backend.

The current public `IGameModule` contract is lifecycle-oriented. Do not assume it provides general input, entity, rendering, audio, or asset services. The dev Game MCP operates the existing client/game; it is not a general authoring API. See [00-MASTER-AUDIT.md](00-MASTER-AUDIT.md) and the audit reports for verified state and caveats.

## First read

1. Select the intended branch/commit and check `git status --short --branch`. This repository may contain important uncommitted work. Preserve it; never reset, clean, stash, or overwrite it without direct instruction.
2. Read root `AGENTS.md`, then `docs/AGENTS.md`, and the smallest relevant Tier-2/system guide. Root `AGENTS.md` currently has unresolved template placeholders; validate it rather than trusting its template fields.
3. Use `docs/repo-map.md` and `docs/guides/building.md` as navigation, then verify important claims in CMake/source. README milestone statements are stale relative to the inspected tree.
4. For subsystem boundaries and open decisions, read `docs/architecture/overview.md` and relevant `docs/systems/` pages.

## Build and verification

The repository quality gate documents:

```sh
cmake --preset debug
./scripts/lint.sh --base-ref origin/main --compile-db build/debug
cmake --preset debug-headless
cmake --build --preset debug-headless
ctest --test-dir build/debug-headless --output-on-failure
```

Before using a build directory, inspect its cache and avoid overwriting an existing user build. For read-only audits, put fresh generated output in a separate temporary directory and record dependency/network failures. The audit observed a headless CMake build failure in the dirty tree: a menu test linked `ae_ui` while the client/UI target was disabled. This may not reproduce on a clean baseline. Do not run formatter auto-fixes during an audit-only task.

## Golden paths and known gaps

- **Add game behavior:** current route is C++ in `game/` or a game module. There is no proven generic entity/component registration API for third-party games. Inspect `game/src/world.cpp`, `game/include/ahamkara/game/world.h`, and `engine/runtime/include/ae/runtime/game_module.h` before choosing a seam.
- **Add an asset:** follow `docs/systems/asset_pipeline.md`, inspect `assets/manifest.assets`, and use `tools/ahamkara_asset_importer`/`tools/asset_importer*`. Import success does not prove runtime loading. Never feed untrusted manifests or models to the importer until root confinement and size limits are in place.
- **Add a feature:** locate canonical subsystem headers/sources under `engine/<module>/include` and `src`, cross-check CMake gates, and add focused tests in the corresponding test target. Avoid adding product-specific ownership to reusable engine libraries.
- **Verify a game:** native tests and asset contracts exist. The MCP bridge can control a specific development client, but deterministic reset/replay and generic headless game verification are not yet established. A captured JSON written by custom game code is not equivalent to an engine state dump.
- **Security:** native game modules and agent workers execute with host privileges in the inspected setup. Treat generated code as trusted-only unless isolated. Keep MCP disabled unless needed, and do not use real Nakama bearer tokens through the current plaintext transports.

## Conventions and traps

- Work only in the user’s intended checkout and preserve every preexisting dirty path.
- Use existing module boundaries and CMake options; headless builds may exclude `engine/ui` and graphics targets.
- Separate facts from hypotheses. Cite source path/line or command output in audit and architecture docs. Label unverified platform/runtime behavior explicitly.
- Do not call feature stubs or data schemas “implemented” unless runtime behavior and tests show the full path.
- Do not claim a demo is fun without a playtest; record controls, goal, completion, and observed friction.
- GitHub Issues are the source of truth for work state. This guide and audit backlog are not a task queue.

## Next implementation direction

Prove one small, documented reference game from clean configure through input, behavior, asset display, objective/score, deterministic state verification, and screenshot capture. Track the API, headless loop, asset hardening, and security decisions in issues before expanding genre/catalog breadth.
