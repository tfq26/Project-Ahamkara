# Logging Conventions

Canonical conventions for the current Ahamkara logging API. Logging is one
output of the proposed error system; a log message is not itself a stable error
identity. See [the error-system design](../design/error-system.md).

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
require explicit opt-in. [src: file: engine/core/include/ae/core/log.h:11-17]
[src: file: engine/core/src/log.cpp:21-25]

## Category Convention

Each component should define a category via `#define AE_LOG_CATEGORY "<Name>"`
in every translation unit that logs. Categories are free-form strings in the
current API; there is no central category registry. [src: file:
engine/core/include/ae/core/log.h:37-46] [src: file:
engine/core/src/log.cpp:70-85]

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
# Set global verbosity threshold (Debug admits Error through Debug)
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

The environment syntax and parser are implemented in the core logger. [src:
file: engine/core/include/ae/core/log.h:65-79] [src: file:
engine/core/src/log.cpp:87-109]

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
   `ae::shutdown_file_logging()` at shutdown. Logs append to
   `logs/ahamkara.log` by default. [src: file:
   engine/core/src/log.cpp:163-188]

## Call Sites Not Logged (intentionally)

- `cli_utils.h` — parse failures for CLI arguments are the caller's
  responsibility to log (avoids pulling the log dependency into a utility
  header).
- `tick.h` — the header-only `FixedTimestepAccumulator` should remain free of a
  direct logging dependency. A caller that needs instrumentation should expose
  it at the orchestration boundary.
