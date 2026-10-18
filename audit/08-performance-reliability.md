# 08 — Performance & Reliability Engineer

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed HEAD: `b705d74` (`feat: add initial weapon and enemy assets (#71)`)
Observed branch: `dev/flashback-menu-shell`

## 1. Scope & Method

Reviewed the profiler, frame pacing and memory-budget implementation, performance benchmark suite and CI use, streaming residency data structures and lifecycle, logging, crash handling, and diagnostic-bundle paths. This was source and configuration inspection only; I did not run tests, builds, or performance measurements. Findings describe the observed dirty working tree. No changes were made outside the requested audit reports.

## 2. Executive Summary

- **Performance/reliability rating: 2/5 — several useful primitives exist, but runtime integration, scaling behavior, and crash-path guarantees are not yet dependable enough for production claims.**
- The engine has a frame profiler, frame pacer, process RSS budget tracker, streaming budgets, benchmark harness, file logging, crash-dump support, and diagnostic bundles (`engine/core/include/ae/core/frame_profiler.h:16-64`; `engine/core/include/ae/core/frame_pacer.h:11-30`; `engine/core/include/ae/core/memory_budget.h:10-19`; `engine/core/include/ae/core/diagnostics.h:29-42`).
- The per-section frame profiler is instantiated in the debug frontend and exposed to UI inputs, but a repository-wide caller search found no runtime `ScopedProfile` or `FrameProfiler::end_frame` use outside its unit/benchmark code. The displayed profile snapshot therefore has no production sampling path in the inspected tree (`client/include/ahamkara/client/debug_frontend_runtime.h:28-34`; `client/src/client_frame_pipeline.cpp:466`; `engine/core/src/frame_profiler.cpp:38-85`).
- The current RSS tracker is sampled in the debug-client frame update (`client/src/client_frame_pipeline.cpp:390-398`; `client/src/debug_frontend_runtime.cpp:114-125`). On Linux each sample opens and scans `/proc/self/status` (`engine/core/src/memory_budget.cpp:44-69`), creating avoidable per-frame OS/file work for a metric surfaced at roughly human monitoring cadence.
- Streaming residency enforces in-flight and resident-byte budgets (`engine/runtime/include/ae/runtime/streaming_executor.h:97-103`), but its region registry is a vector with linear search and whole-vector passes. Discarded entries are set to `None` and retained, so unique-region churn grows retained metadata and lookup/scan cost over a long session (`engine/runtime/src/streaming_executor.cpp:142-179,214-253,300-307`).
- Crash dumps and diagnostic bundles exist, but the POSIX signal handler performs allocation, symbol resolution, filesystem I/O, callbacks, and logging from signal context; the Windows implementation is a stub (`engine/core/src/crash_handler.cpp:47-107,193-223`). A crash in allocator/stdio/lock code can therefore recurse, deadlock, or fail to persist a dump.
- Benchmarks cover useful micro/meso primitives and emit JSON (`tests/src/benchmark_tests.cpp:100-369`; `.github/workflows/ci.yml:96-107`), but the benchmark configuration used in CI is `debug-headless` (a Debug preset) and includes proxies rather than actual renderer/gameplay workloads (`CMakePresets.json:31-41`; `tests/src/benchmark_tests.cpp:306-369`).
- No run-time results were collected for this report. The recon report's build/test observations are kept separate and should not be used as performance evidence.

## 3. Inventory

| Area | Location | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| CPU frame profiling | `engine/core/{include/ae/core/frame_profiler.h,src/frame_profiler.cpp}` | PARTIAL | 2 | Fixed section API and EMA statistics; runtime sampling path not wired in the inspected tree. |
| Frame pacing | `engine/core/{include/ae/core/frame_pacer.h,src/frame_pacer.cpp}` | IMPLEMENTED | 3 | 200-sample history, EMA, percentile, compliance and warning state; bounded fixed-cost bookkeeping. |
| RSS and allocator budgets | `engine/core/{include/ae/core/memory_budget.h,src/memory_budget.cpp}` | PARTIAL | 2 | Cross-platform RSS reads and pressure thresholds; Linux polling opens/scans procfs; allocator tracker is optional and not wired in the inspected client. |
| Performance regression suite | `tests/src/benchmark_tests.cpp`, `benchmarks/budgets/`, CI | PARTIAL | 2 | Broad primitive-level benchmark set, hard thresholds and JSON output; Debug-only CI timing, proxy workloads and no previous-run comparison. |
| Streaming residency | `engine/core/include/ae/core/residency_manager.h`, `engine/runtime/src/streaming_executor.cpp` | IMPLEMENTED | 2 | Async state machine and explicit budgets; flat linear registry retains cleared entries. |
| Crash handling | `engine/core/src/crash_handler.cpp` | PARTIAL | 1 | Best-effort POSIX dump path but unsafe operations in signal handler; no Windows crash capture implementation. |
| Logs and support bundle | `engine/core/src/log.cpp`, `engine/core/src/diagnostics.cpp` | IMPLEMENTED | 3 | Structured categories, runtime filtering, file logs, system info, log tail and crash summary collection. |

## 4. Detailed Findings

### PERF-001 — Streaming metadata grows with every unique region visited

- **Severity:** HIGH
- **Category:** scalability / memory reliability
- **Evidence:** `StreamingResidencyExecutor` stores all entries in `std::vector<RegionEntry>` (`engine/runtime/include/ae/runtime/streaming_executor.h:147-179`). `find_entry` scans the vector and `get_or_create_entry` appends when a coordinate is new (`engine/runtime/src/streaming_executor.cpp:240-253`). `tick`, `commit`, and count/query methods also scan the collection (`engine/runtime/src/streaming_executor.cpp:100-179,182-235`). `discard_entry` clears payload/state but never erases or reuses the entry (`engine/runtime/src/streaming_executor.cpp:300-307`), while `tracked_region_count()` returns the vector size despite documenting “any non-None state” (`engine/runtime/include/ae/runtime/streaming_executor.h:144-145`).
- **Impact:** A player exploring many unique cells causes the registry to accumulate one record per visited coordinate. The byte budget only accounts for committed payloads, not retained entry objects. Lookup and routine tick/commit passes become progressively more expensive, and the tracked count overstates active regions.
- **Recommended fix:** Reclaim or reuse `None` entries after callbacks are no longer possible, and use coordinate-indexed lookup (with deterministic iteration where needed). Add a churn test that visits and evicts many unique coordinates and asserts bounded retained metadata and stable lookup work.
- **Effort:** M
- **Dependencies:** Preserve callback/cancel lifetime guarantees while reclaiming entries.

### PERF-002 — CPU frame profiler is not wired into the runtime sampling path

- **Severity:** HIGH
- **Category:** observability / performance diagnosis
- **Evidence:** The debug frontend owns `FrameProfiler` and a `last_profile_snapshot` (`client/include/ahamkara/client/debug_frontend_runtime.h:28-34`), and the render bridge passes the snapshot pointer onward (`client/src/client_frame_pipeline.cpp:466`). The profiler can only produce measured data after `begin_section`/`end_section` and `end_frame` (`engine/core/include/ae/core/frame_profiler.h:97-123`; `engine/core/src/frame_profiler.cpp:15-85`). A search of runtime call sites found no such calls outside tests/benchmarks; `client/src/client_frame_pipeline.cpp:390-398` updates pacing/metrics but not the profiler.
- **Impact:** The profiler data model exists, but the debug UI path receives a default/stale snapshot, so it cannot identify CPU cost by render/physics/simulation/network section. This makes the per-frame display misleading or uninformative and forces manual external profiling for normal gameplay hot paths.
- **Recommended fix:** Instrument stable frame stages with RAII scopes and call `end_frame` once per frame, or remove/label the unused panel until instrumentation exists. Keep detailed instrumentation optional if its overhead is a concern.
- **Effort:** M
- **Dependencies:** Agree which pipeline boundaries constitute the stable public profile sections.

### PERF-003 — POSIX crash handler performs non-signal-safe work; Windows handler is inert

- **Severity:** HIGH
- **Category:** crash recovery / reliability
- **Evidence:** The POSIX signal trampoline calls `backtrace_symbols`, allocates `std::string`/`std::vector` data, writes through `std::filesystem`/`std::ofstream`, invokes arbitrary registered callbacks, and routes through the regular logger before re-raising (`engine/core/src/crash_handler.cpp:47-107`). The API documentation itself warns that callbacks must be async-signal-safe (`engine/core/include/ae/core/crash_handler.h:118-121`) and labels trace capture best-effort (`engine/core/include/ae/core/crash_handler.h:38-44`). On Windows, `install()` only flips a boolean and capture returns an empty vector (`engine/core/src/crash_handler.cpp:193-223`).
- **Impact:** A crash that interrupted allocation, filesystem, or logging locks can deadlock or corrupt the process while the handler tries to allocate/log/write. Windows users receive no equivalent dump despite the cross-platform build. Crash diagnostics are most likely to fail in exactly the severe cases they are intended to explain.
- **Recommended fix:** Keep the signal handler to minimal async-signal-safe operations (for example, write a small fixed record to a pre-opened descriptor and re-raise), then resolve symbols and attach logs in an external/post-crash process. Implement a native Windows minidump route and test it separately. Do not invoke general callbacks from signal context.
- **Effort:** L
- **Dependencies:** Platform-specific dump format and retention policy.

### PERF-004 — CI performance gates measure Debug primitives, not shipping workload behavior

- **Severity:** MEDIUM
- **Category:** performance validation
- **Evidence:** CI runs the benchmark label only against `build/debug-headless` (`.github/workflows/ci.yml:96-107`), and `debug-headless` sets `CMAKE_BUILD_TYPE` to `Debug` (`CMakePresets.json:31-41`). The benchmark suite exercises allocator/job/residency/pacer/memory components (`tests/src/benchmark_tests.cpp:100-299`); network “snapshot” is a sequence-tracker/profiler proxy and streaming residency is a residency-manager update/query proxy (`tests/src/benchmark_tests.cpp:306-369`). The runner uses absolute thresholds and evaluates the sample mean (`engine/core/include/ae/core/benchmark_harness.h:273-329`), while CI uploads per-run JSON but does not compare the result against a prior baseline (`.github/workflows/ci.yml:100-107`).
- **Impact:** Debug timing is useful for gross regressions but is not representative of optimized release frame costs. Primitive-level proxies do not reveal end-to-end render/simulation/asset-streaming bottlenecks, and artifact-only history leaves regressions to be noticed by a human rather than a baseline gate.
- **Recommended fix:** Keep a stable Debug smoke benchmark, and add a Release benchmark lane on a controlled runner with a few representative gameplay/render/streaming scenarios. Compare robust statistics against versioned or centrally retained baselines and report variance/metadata; avoid using a single noisy mean as the sole gate.
- **Effort:** M
- **Dependencies:** Stable benchmark host and deterministic, representative workload fixtures.

### PERF-005 — RSS sampling performs OS/file work once per debug frame

- **Severity:** MEDIUM
- **Category:** hot-path overhead
- **Evidence:** `ClientFramePipeline::stage` updates the debug metrics on the frame path (`client/src/client_frame_pipeline.cpp:390-398`), and `update_debug_metrics` calls `budget_tracker.update_rss()` before the one-second metrics sampling condition (`client/src/debug_frontend_runtime.cpp:114-125`). On Linux the RSS implementation opens `/proc/self/status`, scans lines, and builds a temporary string for `VmRSS` (`engine/core/src/memory_budget.cpp:44-69`); Apple uses `task_info` and Windows uses `GetProcessMemoryInfo` (`engine/core/src/memory_budget.cpp:29-43`).
- **Impact:** RSS sampling cost is paid every debug frame even though the visible runtime metrics are published about once per second. On Linux this includes repeated file open/read/parse work that can add jitter to the client loop. The check is in the diagnostic/debug frontend path, so scope is narrower than the core simulation path.
- **Recommended fix:** Sample RSS at the same lower cadence as displayed metrics or on an explicit profiling cadence, while preserving peak tracking and immediate checks for any critical budget policy that truly needs frame-level response.
- **Effort:** S
- **Dependencies:** Decide whether memory pressure response is advisory or required within a single frame.

## 5. What's Good

- `FramePacer` has bounded history storage, computes a smoothed frame time and compliance, and amortizes percentile sorting after the history warms (`engine/core/include/ae/core/frame_pacer.h:32-99`; `engine/core/src/frame_pacer.cpp:26-83`).
- The streaming executor has explicit limits for in-flight loads and committed payload bytes, plus an explicit commit barrier and retry lifecycle (`engine/runtime/include/ae/runtime/streaming_executor.h:87-125`).
- The benchmark harness records sample distributions and host metadata, emits JSON, and can produce budget diagnostics (`engine/core/include/ae/core/benchmark_harness.h:49-140,146-229,273-329`; `tests/src/benchmark_tests.cpp:52-94,441-490`).
- Logging supports categories and runtime levels (`engine/core/include/ae/core/log.h:11-79`), and diagnostic bundles collect system information, config values, log tails, and crash summaries (`engine/core/include/ae/core/diagnostics.h:29-59`).
- The debug client surfaces frame pacing, RSS, CPU, and FPS metrics and periodically writes performance telemetry (`client/src/debug_frontend_runtime.cpp:114-130`; `client/src/client_frame_pipeline.cpp:466-480`).

## 6. Open Questions

- What target hardware and frame-rate/memory envelopes should govern the budgets? The current defaults focus mainly on generic engine primitives, not named device classes or gameplay scenes.
- Is the debug CPU profiler intended to be usable today? It is passed to a renderer bridge but lacks a runtime sampling path in the inspected code.
- Is Windows crash capture a supported reliability requirement, or are Windows crashes intentionally delegated to the host OS?
- How long can a world session run and how many unique regions can it visit? That scale determines how urgently the linear, retained streaming registry needs replacement.

## 7. Handoff Notes

- Do not treat the declared benchmark thresholds as evidence that gameplay frame budgets are met; the suite does not measure a representative rendered game frame and this pass collected no timings.
- The most actionable reliability work is to make crash capture safe under signal conditions and bound streaming metadata over long sessions.
- Keep debug-front-end RSS sampling and the unused CPU profiler separate in triage: the former is real per-frame work; the latter is missing instrumentation rather than a measured slowdown.
- No tests, builds, or benchmarks were run for this report.
