# Logging Conventions

Canonical conventions for logging in Ahamkara, established by the deep-logging
epic foundation (TASK-20260623-1601).

## Levels

| Level | Semantics | Default | Per-frame safe? |
|-------|-----------|---------|-----------------|
| `Error` | Failures, unrecoverable paths | Always on | No (should be rare) |
| `Warning` | Recoverable problems, fallbacks, missing assets | Always on | No |
| `Info` | Lifecycle, state transitions, config load, connect/disconnect | Always on | No |
| `Debug` | Detailed flow, resource resolution, state-machine transitions | Off | Yes (gated) |
| `Trace` | Very verbose, per-tick/per-frame milestones | Off | Yes (gated) |

Error, Warning, and Info are always emitted (they are below or equal to the
default global minimum of `Info`). Debug and Trace are disabled by default and
require explicit opt-in.

## Category Convention

Each component defines a category via `#define AE_LOG_CATEGORY "<Name>"` in
every translation unit that logs. The defined categories are:

`Core`, `Collision`, `Physics`, `Network`, `Runtime`, `Platform`, `Render`,
`Animation`, `UI`, `Input`, `Audio`, `Game`, `Client`, `Server`, `Wish`,
`Tools`, `Config`.

Use the categorized logging helpers:

```cpp
#define AE_LOG_CATEGORY "Core"
log_info_cat(AE_LOG_CATEGORY,  "JobSystem initializing with " + std::to_string(n) + " thread(s)");
log_debug_cat(AE_LOG_CATEGORY, "Registered config var: " + key);
log_trace_cat(AE_LOG_CATEGORY, "Frame " + std::to_string(frame_number));
```

## Enabling Debug/Trace

Control verbosity via environment variables:

```sh
# Set global minimum level (Debug shows debug+trace, Trace shows all)
AE_LOG_LEVEL=debug ./ahamkara_client

# Enable per-category overrides (comma-separated)
AE_LOG=Render:trace,Core:debug ./ahamkara_client

# Combine both
AE_LOG_LEVEL=info AE_LOG=Network:trace,Render:debug ./ahamkara_client
```

Programmatic control:

```cpp
ae::set_log_level(ae::LogLevel::Debug);
ae::set_category_log_level("Network", ae::LogLevel::Trace);
```

## Guarding Expensive Messages

For messages that require expensive formatting or allocations, guard with
`log_enabled()` to avoid cost when the level is disabled:

```cpp
if (log_enabled(AE_LOG_CATEGORY, LogLevel::Debug)) {
    log_debug_cat(AE_LOG_CATEGORY, "Scene has " + std::to_string(entities.size()) + " entities");
}
```

However, simple `log_debug_cat("cat", "literal")` calls are cheap enough to use
unguarded — the function checks the level internally and returns immediately
when disabled.

## Safety Rules

1. **No Info/Warning inside deterministic fixed-timestep hot paths in steady
   state** — gate those to Debug/Trace to avoid perf regressions and timing
   non-determinism.
2. **No secrets or PII** in log messages.
3. **Disabled levels are near-zero cost**: the level is checked before the
   message string is accessed.
4. **File logging**: call `ae::init_file_logging()` at startup and
   `ae::shutdown_file_logging()` at shutdown. Logs go to `logs/ahamkara.log`
   by default.

## Call Sites Not Logged (intentionally)

- `cli_utils.h` — parse failures for CLI arguments are the caller's
  responsibility to log (avoids pulling the log dependency into a utility
  header).
- `tick.h` — `FixedTimestepAccumulator` spiral-of-death detection is a
  follow-up instrumentation task (the accumulator is header-only, so logging
  requires callback injection or a separate instrumentation slice).
