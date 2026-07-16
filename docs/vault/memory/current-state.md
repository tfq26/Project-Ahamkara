# Current state

Status: Active
Last updated: 2026-07-13

## Direction

The accepted destination is three independent repositories: Ahamkara engine,
Flashback game, and Wish backend/session platform. Ahamkara and Wish remain
independent; Flashback consumes versioned releases and owns integration
adapters.
[src: user:taufeeqali:2026-07-13: explicit three-repository direction]

The canonical boundary is
[the repository-split document](../../architecture/repository-split.md).

## Current checkout

The root build still adds engine modules, Wish, and game in one project, with
client/server/sample/test targets selected by options.
[src: file: CMakeLists.txt:104-145]

Important transition facts:

- Flashback is still a thin launcher over `ahamkara_client_lib`.
  [src: file: samples/flashback/CMakeLists.txt:1-13]
- `ahamkara_game` still links `wish_engine`.
  [src: file: game/CMakeLists.txt:41-49]
- Wish's session runtime still imports a game command type.
  [src: file: wish/session/session_runtime.h:1-6]
  [src: file: wish/session/session_runtime.h:76-86]
- The renderer and animation targets still form a dependency cycle.
  [src: file: engine/render/CMakeLists.txt:47-55]
  [src: file: engine/animation/CMakeLists.txt:21-24]

## Operations

The engine currently has categorized logging, telemetry counters/gauges/
histograms, POSIX crash capture, and diagnostic bundles.
[src: file: engine/core/include/ae/core/log.h:11-84]
[src: file: engine/core/include/ae/core/telemetry.h:16-190]
[src: file: engine/core/include/ae/core/crash_handler.h:21-137]
[src: file: engine/core/include/ae/core/diagnostics.h:29-66]

Stable public error identities are proposed but not implemented. See
[the error design](../../design/error-system.md).

## Work tracking

GitHub Issues is the only current source of truth for state, priority,
dependencies, and acceptance criteria. The architecture initiative is indexed
by [#61](https://github.com/tfq26/Project-Ahamkara/issues/61). Do not add queue
inventories or copied issue bodies to this note.

## Verification

Do not preserve a test-count snapshot here because it becomes stale. Use the
commands in [testing and quality](../../testing-quality.md) and inspect current
CI/issue status for the revision being evaluated.
