---
type: subagent-report
category: implementation
status: implemented
created: 2026-07-08T17:00:00Z
agent: oz
subsystems:
  - engine/core
branch: agent/oz/job-system-frame-allocator
validation:
  - cmake --build --preset debug (targeted)
  - core_tests: 20/20 passed
  - utility_tests: 17/17 passed
  - logging_tests: 9/9 passed
  - world_tests: passed
  - movement_tests: passed
  - gameplay_tests: passed
  - collision_tests: passed
  - weapon_loader_tests: passed
  - nav_grid_tests: passed
  - ai_combatant_tests: passed
  - encounter_scripting_tests: passed
---

# Subagent Report — Job System & Frame Allocator

## Task

Add a job system and frame allocator so the engine has a real foundation for multithreaded runtime work. Phase 10 production-readiness track.

## Status

Implemented and validated. Both the job system and frame allocator existed as skeleton implementations on `main`; this slice improved them with robust synchronization, new APIs, ring-buffer design, and comprehensive test coverage.

## Scope

**In bounds:**
- Job scheduling ownership (thread pool, dependencies, bulk dispatch)
- Frame allocator ownership (ring-buffer, per-frame slot rotation)
- Thread/runtime integration (condition variable wake-up, not spin-wait)
- Dedicated test suite

**Out of bounds:**
- No new game rules or combat systems
- No renderer fidelity work
- No deferred HDR resurrection

## Changes

### Job System (`engine/core/include/ae/core/job_system.h`, `engine/core/src/job_system.cpp`)

The job system was rewritten to replace spin-wait/sleep polling with condition variable-based wake-up, making worker threads sleep efficiently when idle.

**New APIs:**
- `submit_after_all(parents, fn)` — Submit a job that waits for multiple parents to complete. Supports arbitrary fan-in dependency graphs.
- `dispatch(count, fn)` — Bulk-submit `count` jobs, each receiving its index `[0, count)`. Returns a vector of handles for fine-grained waiting.

**Internal improvements:**
- `pop_ready_job_locked()` — Extracted ready-job pop into a helper.
- `execute_job(job_idx)` — Extracted job execution + dependency notification into a helper; always notifies the condition variable on completion so waiting callers can wake.

**Synchronization:**
- Worker threads wait on `jobs_cv_` with predicate `!running_ || !ready_jobs_.empty()`.
- `submit()` / `submit_after()` / `submit_after_all()` / `dispatch()` all call `notify_all()` after adding jobs.
- `execute_job()` always calls `notify_all()` on completion, so `wait()` / `wait_all()` callers can wake.

### Frame Allocator (`engine/core/include/ae/core/frame_allocator.h`, `engine/core/src/frame_allocator.cpp`)

Converted from a single-arena bump allocator to a ring-buffer design.

**Ring buffer:**
- Constructor takes `(total_bytes, num_slots)` where `num_slots` defaults to 3.
- Each frame slot gets `total_bytes / num_slots` capacity (minimum 64 bytes per slot).
- Total capacity rounded up to `alignof(max_align_t)` to satisfy `aligned_alloc` requirements.

**API changes:**
- `end_frame()` — Advances to the next ring-buffer slot, resetting its bump pointer. Call once per frame.
- `reset_all()` — Resets all slots and returns to slot 0.
- `slot_size()`, `num_slots()`, `current_slot()` — Inspection accessors.
- Old `reset()` is replaced by `end_frame()` / `reset_all()`.

**Preserved:**
- `allocate(size, alignment)` — Same signature, bumps within current slot.
- `allocate_array<T>()` / `allocate_object<T>()` — Template convenience wrappers unchanged.
- `used()`, `peak_used()`, `capacity()` — Inspection methods with updated semantics.

### Tests (`tests/src/core_tests.cpp`, `tests/CMakeLists.txt`)

New dedicated test file with 20 tests:

**Job System (12 tests):**
- `test_job_system_submit_and_wait` — Basic submit + wait for a single job
- `test_job_system_submit_after_single_child` — Single parent dependency
- `test_job_system_submit_after_multiple_children` — One parent, three children
- `test_job_system_submit_after_all` — Two parents, one merged child
- `test_job_system_submit_after_all_single_parent` — Edge case: single-item parent list
- `test_job_system_submit_after_all_empty_parents` — Edge case: empty parent list
- `test_job_system_dispatch` — Bulk dispatch of 100 jobs
- `test_job_system_wait_all` — Wait for all outstanding jobs
- `test_job_system_mixed_independent_and_dependent` — Mix of dependent and independent jobs
- `test_job_system_chained_dependencies` — Linear chain: A → B → C
- `test_job_system_stress` — 1000 jobs via dispatch with busy-work
- `test_job_system_diamond_dependency` — Diamond graph: root → left+right → merged

**Frame Allocator (8 tests):**
- `test_frame_allocator_basic_alloc` — Basic allocation across slots
- `test_frame_allocator_alignment` — Verify 16/64/128-byte alignment
- `test_frame_allocator_oom` — Out-of-memory handling per slot
- `test_frame_allocator_array_and_object` — Template wrappers and construction
- `test_frame_allocator_reset_all` — Full reset behavior
- `test_frame_allocator_peak_usage` — Peak tracking across frames
- `test_frame_allocator_slot_rotation` — 3-slot rotation cycle
- `test_frame_allocator_slot_capacity` — Per-slot capacity enforcement

## Files Changed

- `engine/core/include/ae/core/job_system.h` — Added condition variable, `submit_after_all()`, `dispatch()`, `pop_ready_job_locked()`, `execute_job()`
- `engine/core/src/job_system.cpp` — Rewrote with condition variable synchronization + new APIs
- `engine/core/include/ae/core/frame_allocator.h` — Changed to ring-buffer API: `end_frame()`, `reset_all()`, multi-slot constructor
- `engine/core/src/frame_allocator.cpp` — Rewrote for ring-buffer allocation
- `tests/src/core_tests.cpp` — New: 20 tests (12 job system + 8 frame allocator)
- `tests/CMakeLists.txt` — Registered `ahamkara_core_tests` executable

## Validation Run

```sh
cmake --build --preset debug   # targeted targets
ctest --test-dir build/debug
```

Full `run-tests.sh` is blocked by a pre-existing `admin_server` build failure (`::close` not in global namespace, `SocketHandle` private access) in `wish/admin/`. All reachable test targets were built and executed individually.

## Validation Results

| Test Suite | Result |
|---|---|
| `ahamkara_core_tests` (new) | 20/20 passed |
| `ahamkara_utility_tests` | 17/17 passed |
| `ahamkara_logging_tests` | 9/9 passed |
| `ahamkara_world_tests` | All passed |
| `ahamkara_movement_tests` | All passed |
| `ahamkara_weapon_loader_tests` | All passed |
| `ahamkara_nav_grid_tests` | All passed |
| `ahamkara_ai_combatant_tests` | All passed |
| `ahamkara_encounter_scripting_tests` | All passed |
| `ahamkara_collision_tests` | All passed |
| `ahamkara_gameplay_tests` | All passed |
| `ahamkara_session_tests` | All passed |

## Known Gaps

- Pre-existing `admin_server` build failures in `wish/admin/` block the full `run-tests.sh` path.
- `FrameAllocator::allocate()` uses a minimum slot size of 64 bytes; very small arenas are rounded up.
- Frame allocator is not thread-safe (documented). Callers must provide external synchronization or use one per thread.

## Cross-Agent Dependencies

- `engine/core/include/ae/core/job_system.h` — Any agent adding work dispatch or parallel-for loops will use this API.
- `engine/core/include/ae/core/frame_allocator.h` — Any agent adding per-frame temporary allocations will use this API.

## Confidence

`High` — All 20 new tests pass, all existing tests pass with no regressions. The condition variable synchronization eliminates the old spin-wait/sleep pattern. The ring-buffer design supports multi-frame data lifetimes for double/triple-buffered pipelines.
