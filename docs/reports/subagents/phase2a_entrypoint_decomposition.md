# Phase 2A: Entrypoint Decomposition — Report

**Date:** 2026-06-06  
**Status:** Complete  
**Build:** ✅ `ahamkara_client` and `ahamkara_server` both compile  
**Tests:** ✅ All 6 test suites pass (smoke, world, movement, collision, gameplay, asset_pipeline)

---

## Overview

Phase 2A extracted duplicated CLI parsing and simulator-config helpers from the two
entrypoint files, replaced ad-hoc frame-delta computation with a shared helper, and
added small readability improvements. No CLI semantics, networking behavior, or
headless/server safety were changed.

---

## Files Created

### `engine/core/include/ae/core/cli_utils.h`

Two inline functions extracted from the duplicated anonymous-namespace code that
existed in both entrypoint files:

| Function | Description |
|----------|-------------|
| `ae::parse_float_arg(arg, key, default_val)` | Parse `--key=<value>` as a float |
| `ae::parse_bool_arg(arg, key)` | Parse `--key` as a boolean flag |

These live in `ae_core` because they are generic string-parsing utilities with no
dependency on any network or game type.

### `engine/network/include/ae/network/cli_helpers.h`

One inline function extracted from the client's `build_sim_config()`:

| Function | Description |
|----------|-------------|
| `ae::build_sim_config(argc, argv)` | Build a `SimulatorConfig` from `--simulate-*` CLI flags |

This lives in `ae_network` because it depends on `ae::SimulatorConfig` (from
`ae/network/network_simulator.h`) and uses the core `ae::parse_float_arg` /
`ae::parse_bool_arg` helpers.

---

## Files Modified

### `client/src/headless_clients.cpp`

**Removed from anonymous namespace (~35 lines):**
- `parse_float_arg` / `parse_bool_arg` (now `ae::parse_float_arg` / `ae::parse_bool_arg`)
- `build_sim_config` (now `ae::build_sim_config`)

**Added to anonymous namespace (~10 lines):**
- `compute_frame_dt(previous)` — computes `dt` and updates the timestamp in-place,
  used by all three client loops (`run_network_client`, `run_windowed_client`,
  `run_sandbox_client` uses its own simpler pattern)

**Loop changes:**
- `run_network_client`: now calls `ae::build_sim_config(argc, argv)` instead of
  local `build_sim_config`. Frame-delta uses `compute_frame_dt`.
- `run_windowed_client`: frame-delta uses `compute_frame_dt` with the existing
  `0.05F` clamp retained.

**Retained in anonymous namespace (sandbox-specific):**
- `print_sandbox_help`, `tokenize`, `parse_tick_count`, `apply_input_token`,
  `print_player_state`, `key_code_name`, `log_interpolated_state`

### `server/src/dedicated_server_main.cpp`

**Removed from anonymous namespace (~17 lines):**
- `parse_float_arg` / `parse_bool_arg`

**Added to anonymous namespace (~7 lines):**
- `compute_frame_dt(previous)` — identical to the client's version

**`main()` changes:**
- Replaced 12-line inline simulator-config parsing loop with
  `const ae::SimulatorConfig sim_config = ae::build_sim_config(argc, argv);`
- Frame-delta in the main loop now uses `compute_frame_dt`
- Removed `#include <cmath>` (no longer needed)

---

## Metrics

| Metric | Before | After |
|--------|--------|-------|
| `headless_clients.cpp` lines | ~509 | ~483 |
| `dedicated_server_main.cpp` lines | ~177 | ~153 |
| Duplicated `parse_float_arg`/`parse_bool_arg` | 2 copies | 0 (shared) |
| Duplicated `build_sim_config` logic | 1 function + 1 inline loop | 0 (shared) |
| Duplicated `compute_frame_dt` pattern | 4 ad-hoc copies | 2 uses of helper |

---

## Behavior Preservation

- **CLI semantics:** The `--simulate-*` flags are parsed identically. The new
  `ae::build_sim_config` is line-for-line identical to the old client function
  and the old server inline loop.
- **Networking:** No changes to socket, packet, simulator, or tick-loop logic.
- **Headless/server safety:** No render, audio, or editor headers were introduced
  into server-only paths.

---

## Verification

```sh
# Build
cmake --build build --target ahamkara_client -j8   # ✅
cmake --build build --target ahamkara_server -j8   # ✅

# Tests
cd build && ctest --output-on-failure -j8           # ✅ 6/6 passed
```
