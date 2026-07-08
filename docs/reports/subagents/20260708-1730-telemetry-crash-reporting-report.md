---
type: subagent-report
category: infrastructure
status: review-needed
agent: oz
date: 2026-07-08
task: TASK-20260704-1730
branch: agent/oz/telemetry-crash-reporting
---

# TASK-20260704-1730: Telemetry, Crash Reporting, and Diagnostics

## Summary

Added runtime telemetry infrastructure (counter/gauge/histogram metrics), POSIX crash reporting with signal handlers and stack trace capture, and diagnostic bundle generation for support tooling.

## Files Changed

### New
- `engine/core/include/ae/core/telemetry.h` — TelemetryCounter, TelemetryGauge, TelemetryHistogram, TelemetrySystem
- `engine/core/src/telemetry.cpp` — Implementation of telemetry types with thread-safe atomics and mutex-protected histograms
- `engine/core/include/ae/core/crash_handler.h` — CrashContext, StackFrame, CrashHandler with signal handler install/uninstall
- `engine/core/src/crash_handler.cpp` — POSIX signal handlers (SIGSEGV/SIGABRT/SIGFPE/SIGILL/SIGBUS), stack trace capture via execinfo.h, crash dump write/read
- `engine/core/include/ae/core/diagnostics.h` — SystemInfo, write_diagnostic_bundle, collect_log_tail, collect_crash_summary
- `engine/core/src/diagnostics.cpp` — Platform-specific system info collection (macOS sysctl, Linux /proc), ring-buffer log tail capture, config dump, bundle assembly
- `tools/diagnostics/diagnostics_tool.cpp` — CLI tool: collect/view/list/info commands
- `tests/src/telemetry_tests.cpp` — 7 test cases covering counter, gauge, histogram, system snapshot, CSV flush, concurrent access, clear
- `tests/src/crash_handler_tests.cpp` — 6 test cases covering stack trace capture, signal names, crash dump write/read roundtrip, list dumps, handler install/uninstall, missing file handling
- `tests/src/diagnostics_tests.cpp` — 5 test cases covering system info collection, log tail capture (missing file, ring buffer), diagnostic bundle creation, bundle listing

### Modified
- `engine/core/CMakeLists.txt` — Added crash_handler.cpp, diagnostics.cpp, telemetry.cpp to ae_core
- `tests/CMakeLists.txt` — Added ahamkara_telemetry_tests, ahamkara_crash_handler_tests, ahamkara_diagnostics_tests
- `tools/CMakeLists.txt` — Added ahamkara_diagnostics executable

## Validation

### CMake Build: debug-headless preset
- Configured successfully with FETCHCONTENT_FULLY_DISCONNECTED (pre-existing git fetch stall)
- ae_core library compiles clean
- All 3 new test targets link successfully

### Test Results
| Suite | Tests | Passed |
|---|---|---|
| ahamkara_telemetry_tests | 7 | 7 (100%) |
| ahamkara_crash_handler_tests | 6 | 6 (100%) |
| ahamkara_diagnostics_tests | 5 | 5 (100%) |
| ahamkara_logging_tests | 9 | 9 (100%, pre-existing) |

### Pre-existing Issues (not caused by this change)
- `ahamkara_utility_tests` hangs on `test_job_system_submit_after_single_child` (job system threading deadlock, pre-existing)
- `ahamkara_server` fails to link (pre-existing admin_server/Wish issues)
- `ahamkara_nakama_bridge_tests` fails to compile (pre-existing)
- CMake debug preset requires `FETCHCONTENT_FULLY_DISCONNECTED=ON` to avoid git fetch stalls

## Design Decisions

1. **Telemetry auto-registration**: Metric constructors register with TelemetrySystem singleton; destructors deregister. This avoids dangling pointers between scopes.

2. **Crash handler safety**: Signal handlers use `SA_SIGINFO` for fault address capture. `backtrace_symbols()` is called post-crash (not async-signal-safe but acceptable for crash dump generation). Default handler is restored before re-raise to ensure OS crash report generation.

3. **Diagnostic bundles are timestamped directories**: Each bundle contains system_info.txt, config_dump.txt, log_tail.txt (last 500 lines), and crash_summary.txt. Monotonic counter prevents second-level timestamp collisions.

4. **Ownership boundary**: All infrastructure lives in `engine/core/` (ae_core library). The CLI tool is in `tools/` — no gameplay dependency. This keeps the slice focused on observability, not gameplay.

## Assumptions

- macOS/Linux platform support via `execinfo.h` for stack traces (not Windows)
- ConfigRegistry is accessible for config dump
- Engine log file lives at `logs/ahamkara.log` (matching existing defaults)
- Crash output goes to `crashes/` directory (configurable via CrashHandler::set_crash_dir)

## Risks

- Signal handler uses heap allocation via `backtrace_symbols()` — could deadlock if crash occurs during malloc. Mitigation: pre-allocated frame buffer, symbol resolution best-effort.
- No out-of-process crash reporting — crash dumps are written by the crashed process itself, so severe corruption may prevent dump generation.
- Diagnostic bundle writing may fail if filesystem is corrupted during crash.

## Next Steps

- Integration: Wire CrashHandler::install() into engine startup (main.cpp / dedicated_server_main.cpp)
- Integration: Wire TelemetrySystem periodic flush into client/server frame loops
- Enhance diagnostics with GPU info capture via OpenGL if available
- Consider adding log tail capture at crash time (pre-allocated ring buffer in shared memory)

## Review Notes

- Ownership boundary is explicit: all new code lives in engine/core/ae_core (infrastructure) and tools/ (diagnostics CLI). No gameplay ownership leakage.
- Tests are self-contained: each test suite uses isolated TelemetrySystem state via clear() + auto-registration lifecycle.
- Pre-existing build/test failures are documented and not introduced by this change.
