# Phase 3: Shared Utility Consolidation

## Scope

Remove duplicated parsing, serialization, and platform-abstraction helper logic across
client, server, engine, tools, and render subsystems.  Replace anonymous-namespace
copy-pasted helpers with well-scoped shared utilities in the lowest sensible layer.

Priority areas addressed:
- Shared CLI/timing helper for client and server entrypoints
- Shared text parsing helper for config files
- Shared binary I/O helpers for compiled asset formats
- Shared GL platform include shim for debug renderer

## Status

**Complete** — 4 distinct duplication cases consolidated, 249 lines removed,
all touched targets compile and link cleanly.

## Implemented

### 1. `compute_frame_dt` (2 copies → 1 shared utility)

**Before:** Both `headless_clients.cpp` and `dedicated_server_main.cpp` defined identical
inline `compute_frame_dt` functions in their anonymous namespaces, shadowing the
already-existing `ae::compute_frame_dt` in `ae/core/time.h`.

**After:**
- Removed the local definitions from both files.
- Prefixed calls with `ae::` (they were unqualified before).
- Added `#include "ae/core/time.h"` to `dedicated_server_main.cpp` (it was missing).

**Files changed:** `client/src/headless_clients.cpp`, `server/src/dedicated_server_main.cpp`

### 2. Binary I/O helpers (3 copies → 1 shared header)

**Before:** `compiled_material.cpp`, `compiled_texture.cpp`, and `compiled_mesh.cpp`
each redefined `write_bytes`, `read_bytes`, `write_value<T>`, `read_value<T>`,
`write_string`, `read_string`, `write_vector<T>`, `read_vector<T>`, `validate_count`,
and `checked_count` in their anonymous namespaces.

A comprehensive `ae/render/binary_io.h` already existed with all these functions
in namespace `ae::render`, but none of the compiled-*.cpp files used it.

**After:**
- Removed duplicate anonymous-namespace helpers from all three files.
- Added `#include "ae/render/binary_io.h"` to all three.
- Calls resolve through normal namespace lookup (files are already in `namespace ae::render`).
- Mesh-specific functions (`write_meshes`, `read_meshes`, `write_skins`, etc.) preserved in
  `compiled_mesh.cpp`'s anonymous namespace — they are not generic I/O.
- Removed local `kMaxVectorElements` / `kMaxStringBytes` constants (now default
  parameters in `binary_io.h`).

**Files changed:** `engine/render/src/compiled_material.cpp`, `engine/render/src/compiled_texture.cpp`, `engine/render/src/compiled_mesh.cpp`

### 3. GL platform include shim (2 copies → 1 shared header)

**Before:** Both `debug_renderer.cpp` and `debug_renderer_internal.h` had identical
`#if defined(__APPLE__)` + `#else` blocks for including `GL/gl.h`, `GL/glext.h`,
and Apple-specific fallback defines (`GL_TIME_ELAPSED`, `GL_SAMPLES_PASSED`).

A `ae/render/gl_platform.h` already existed with the complete platform shim including
`GLFW/glfw3.h`, but neither file used it.

**After:**
- Replaced both manual GL include blocks with `#include "ae/render/gl_platform.h"`.
- The header provides `GLFW_INCLUDE_NONE`, `GL_GLEXT_PROTOTYPES`, platform-specific
  GL includes, and the two Apple fallback defines in one place.
- Restored `<array>`, `<cstdint>`, `<string>` includes in `debug_renderer_internal.h`
  that were between the old GL block and namespace declaration.

**Files changed:** `engine/render/src/debug_renderer.cpp`, `engine/render/src/debug_renderer_internal.h`

### 4. String trim utility (2 copies → 1 shared utility)

**Before:** `config.cpp` defined a local `trim` lambda inside `reload_from_file()`
using `std::isspace` + `erase`. `tools/asset_importer_common.cpp` had a standalone
`asset_importer::trim()` using `find_first_not_of`/`find_last_not_of`.

**After:**
- Added `ae::trim(std::string_view)` to `ae/core/cli_utils.h` — returns a
  `std::string_view` to avoid allocation.
- Updated `config.cpp` to use `ae::trim`, eliminating the local lambda and
  removing the `<cctype>` dependency.
- The `asset_importer` tool preserves its own `trim` to keep tool/runtime
  boundaries clean (different namespace, independent compilation unit).

**Files changed:** `engine/core/include/ae/core/cli_utils.h`, `engine/core/src/config.cpp`

## Files Changed

| File | Change |
|------|--------|
| `client/src/headless_clients.cpp` | Removed local `compute_frame_dt`; calls now `ae::compute_frame_dt` |
| `server/src/dedicated_server_main.cpp` | Removed local `compute_frame_dt`; added `ae/core/time.h` include |
| `engine/core/include/ae/core/cli_utils.h` | Added `ae::trim(std::string_view)` |
| `engine/core/src/config.cpp` | Replaced local `trim` lambda with `ae::trim`; removed `<cctype>` |
| `engine/render/src/compiled_material.cpp` | Replaced local binary I/O helpers with `ae/render/binary_io.h` |
| `engine/render/src/compiled_texture.cpp` | Replaced local binary I/O helpers with `ae/render/binary_io.h` |
| `engine/render/src/compiled_mesh.cpp` | Replaced local binary I/O helpers with `ae/render/binary_io.h` |
| `engine/render/src/debug_renderer.cpp` | Replaced manual GL includes with `ae/render/gl_platform.h` |
| `engine/render/src/debug_renderer_internal.h` | Replaced manual GL includes with `ae/render/gl_platform.h` |

Total: 9 files, +32/−249 lines.

## Interfaces / Contracts

### `ae::compute_frame_dt` (in `ae/core/time.h`)
```cpp
namespace ae {
inline float compute_frame_dt(std::chrono::steady_clock::time_point& previous);
}
```
Takes the previous-frame timestamp by mutable reference and updates it in-place.
Returns elapsed delta in seconds. Identical behavior to the removed local copies.

### `ae::trim` (in `ae/core/cli_utils.h`)
```cpp
namespace ae {
inline std::string_view trim(std::string_view s);
}
```
Trims leading and trailing whitespace (` \t\r\n`). Returns a non-owning view into
the original string. Empty input returns empty view.

### Binary I/O (`ae/render/binary_io.h`)
```cpp
namespace ae::render {
bool write_bytes(std::ofstream&, const void*, std::size_t);
template<T> bool write_value(std::ofstream&, const T&);
template<T> bool write_vector(std::ofstream&, const std::vector<T>&);
bool write_string(std::ofstream&, const std::string&);

bool read_bytes(std::ifstream&, void*, std::size_t);
template<T> bool read_value(std::ifstream&, T&);
template<T> bool read_vector(std::ifstream&, std::vector<T>&, std::uint32_t count,
                              std::string_view label, std::string& error,
                              std::uint32_t max_elements = 100'000'000);
bool read_string(std::ifstream&, std::string&, std::string& error,
                 std::uint32_t max_bytes = 1'048'576);
bool validate_count(std::uint32_t count, std::string_view label, std::string& error,
                    std::uint32_t max_elements = 100'000'000);
std::uint32_t checked_count(std::size_t value, std::string_view label);
}
```

### GL Platform Shim (`ae/render/gl_platform.h`)
Provides `GLFW_INCLUDE_NONE`, `GL_GLEXT_PROTOTYPES`, platform-correct GL/GLFW includes,
and Apple fallback defines (`GL_TIME_ELAPSED`, `GL_SAMPLES_PASSED`).

## Tests / Validation

- `ae_core` static library builds clean (config.cpp, cli_utils.h)
- `ae_render` static library builds clean (all 4 renderer TUs)
- `ahamkara_client` executable links clean
- `ahamkara_server` executable links clean
- Pre-existing test failures (`ahamkara_world_tests`, `ahamkara_gameplay_tests`)
  are unrelated game-logic bugs (camera yaw wrapping, match scoring).
  No regressions introduced.

## Known Issues

- The `asset_importer` tool has its own `trim` function in namespace `asset_importer`.
  This is left in place intentionally to respect tool/runtime boundary separation.
  The tool's `trim` uses `find_first_not_of`/`find_last_not_of` while `ae::trim` uses
  `std::string_view::find_first_not_of`/`find_last_not_of` — functionally equivalent.

## Next Recommended Steps

1. **Phase 4 follow-up:** If additional duplication is discovered in subsystems
   not covered by this phase (e.g., `wish/session/`, `engine/collision/`), apply
   the same pattern: identify, extract to lowest sensible layer, remove copies.
2. **Remove `compute_frame_dt` comment in time.h:** The doc comment says
   "used by both the dedicated server and headless client entrypoints so they do
   not need to duplicate this logic" — this has now been enforced.
3. **Consider unifying `ae::trim` and `asset_importer::trim`:** If tool/runtime
   boundary concerns are relaxed, the asset importer could include `ae/core/cli_utils.h`
   and use `ae::trim`.

## Notes For Integrator

- All changes are backward-compatible. No API surface was changed.
- The `ae::compute_frame_dt` utility in `time.h` was already present before this
  phase — this phase merely forced the entrypoints to use it instead of their
  shadowing anonymous-namespace copies.
- The `binary_io.h` and `gl_platform.h` headers were also pre-existing — this
  phase deleted the duplicate bodies and directed the source files to the
  canonical locations.
- `compiled_mesh.cpp` retains its anonymous namespace for mesh/skeleton/animation
  serialization functions — these are domain-specific and not suitable for the
  generic `binary_io.h`.
