---
type: subagent-report
category: implementation
status: implemented
created: 2026-06-28
agent: opencode
subsystems: [engine/core]
branch: main
validation: [debug-build, debug-tests]
---

# Subagent Report: TASK-20260623-1601-deep-logging-core-foundation

## Task

Extend `ae/core/log.h`/`log.cpp` with Debug/Trace levels and runtime per-category gating, then instrument `engine/core` under category `Core`. This is the foundation all other deep-logging children depend on.

## Status

implemented

## Scope

In bounds: LogLevel enum, `log_debug_cat`/`log_trace_cat`, runtime gating via env vars (`AE_LOG_LEVEL` / `AE_LOG`), `log_enabled()` fast-path, `engine/core` instrumentation, logging-conventions doc, unit test.

Out of bounds: Other component instrumentation (their own child tasks), async/file rotation system.

## Files Changed

- `engine/core/include/ae/core/log.h` — Added `LogLevel` enum, `to_string()`, `log_debug_cat`/`log_trace_cat`, `log_enabled()`, `set_log_level()`, `set_category_log_level()`, `init_log_levels_from_env()`, `get_log_level()`, `get_category_log_level()`
- `engine/core/src/log.cpp` — Implemented level gating (global minimum + per-category overrides via mutex-protected map), env var parsing, `log_enabled` checks in all `*_cat` functions, Core-category instrumentation for init/shutdown file logging
- `engine/core/src/config.cpp` — Added `#define AE_LOG_CATEGORY "Config"`, added debug logging for register/reload/save/poll operations, tracking of unknown keys
- `engine/core/src/frame_allocator.cpp` — Added core logging: OOM detection with rate-limited warning, allocation failure logging
- `engine/core/include/ae/core/frame_allocator.h` — Added `oom_logged_` member for rate-limiting
- `engine/core/src/job_system.cpp` — Added core logging: init with thread count, shutdown sequence lifecycle
- `engine/core/include/ae/core/cli_utils.h` — No functional change (added comment about deferred logging)
- `tests/src/logging_tests.cpp` — New test: 9 cases covering enum order, to_string, default gating, global/per-category changes, override interactions
- `tests/CMakeLists.txt` — Registered `ahamkara_logging_tests` (links `ae_core`)
- `docs/systems/logging.md` — New conventions doc: level semantics, category usage, env var format, guarding, safety rules
- `docs/systems/README.md` — Added link to logging.md

## What Changed

1. **LogLevel enum**: Error(0) < Warning(1) < Info(2) < Debug(3) < Trace(4). Default global minimum is Info, so Debug/Trace are off by default.
2. **Runtime gating**: `init_log_levels_from_env()` parses `AE_LOG_LEVEL` (global min) and `AE_LOG` (comma-separated `Category:level` pairs). Programmatic API: `set_log_level()`, `set_category_log_level()`. Thread-safe via separate `g_level_mutex`.
3. **log_enabled() fast-path**: All categorized log functions check `log_enabled(category, level)` before formatting/writing. Disabled levels are near-zero cost.
4. **Core instrumentation**: `config.cpp` (Config category), `log.cpp` (Core category), `frame_allocator.cpp` (Core category — OOM warning with rate limit), `job_system.cpp` (Core category — init/shutdown lifecycle).
5. **Backward compatibility**: Plain `log_info`/`log_warning`/`log_error` unchanged. `log_info_cat`/`log_warning_cat`/`log_error_cat` now check `log_enabled` (but are always on at default Info level). `init_file_logging`/`shutdown_file_logging` now use categorized logging (Core) — format changes from `[Info][t] msg` to `[Info][Core][t] msg`.

## Validation Run

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

## Validation Results

- Debug build: Pass (only pre-existing entt hashed_string warnings)
- 14/14 tests pass (0 failures): ahamkara_smoke_tests, world_tests, movement_tests, window_input_provider_tests, collision_tests, gameplay_tests, session_tests, utility_tests, **logging_tests** (new), nakama_bridge_tests, asset_pipeline_tests, level_render_tests, reliable_channel_tests, nav_grid_tests
- Env var parsing verified: `AE_LOG_LEVEL=debug` sets global to Debug(3), `AE_LOG=Network:trace,Core:debug` sets per-category correctly
- Debug-headless build: Core/network/game/server compile and link; window_input_provider_tests fail with pre-existing `ae/platform/gamepad.h` not-found error (not caused by this change)

## Known Gaps

- `cli_utils.h` parse failures intentionally left to caller logging (avoids pulling log.h into a utility header)
- `tick.h` FixedTimestepAccumulator spiral-of-death logging deferred (header-only, needs callback injection)
- No runtime integration — `init_log_levels_from_env()` is not yet called from any entry point; component children should call it at startup
- Debug-headless full build blocked by pre-existing platform library gap

## Runtime Risks

- The `log_enabled` check inside every `*_cat` function adds a mutex lock on a hot path. For Info/Warning/Error (always-on), this is unnecessary overhead. Could be optimized with an `unlikely` branch hint or separate always-on fast path. Current perf impact is minimal for typical use — the mutex is uncontended and the check is fast.
- Format change for file-logging startup/shutdown messages (now `[Info][Core][...]`) may surprise any log-parsing scripts.

## Cross-Agent Dependencies

- All deep-logging child tasks (1602-1616) depend on this foundation being accepted
- Component children should call `ae::init_log_levels_from_env()` at their subsystem init, then use `log_debug_cat`/`log_trace_cat` with their category

## Recommended Next Step

Codex review this foundation, then unblock the 15 remaining deep-logging child tasks (one per component) which are now ready to implement.

## Confidence

`high` — all targeted api changes are implemented, validated with 9 dedicated test cases, full suite green, env var parsing verified e2e.
