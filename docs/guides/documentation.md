# API Documentation (Doxygen)

## Overview

Ahamkara uses [Doxygen](https://www.doxygen.nl/) to generate API reference
documentation from annotated C++ headers and source files. The generated HTML
site covers all engine, game, and Wish libraries.

The configuration lives in [`Doxyfile.in`](../../Doxyfile.in) at the repository
root — a CMake template that is configured at build time.

---

## Prerequisites

| Tool | Notes |
|------|-------|
| Doxygen 1.9+ | `apt install doxygen`, `brew install doxygen`, or from [doxygen.nl](https://www.doxygen.nl/download.html) |
| Graphviz / `dot` (optional) | Enables class/collaboration/include diagrams. Install via `apt install graphviz` or `brew install graphviz` |

---

## Building the docs

### Via CMake (recommended)

```sh
# 1. Configure (any preset)
cmake --preset debug

# 2. Build the 'doc' target
cmake --build --preset debug --target doc
```

If Doxygen is found on `PATH` during CMake configure, the `doc` target is
automatically available. The generated HTML is written to:

```
build/debug/docs/doxygen/html/
```

Open `build/debug/docs/doxygen/html/index.html` in a browser.

### Via Doxygen directly (no CMake)

```sh
# CMake must have been run at least once so that Doxyfile.in is configured.
doxygen build/debug/docs/doxygen/Doxyfile
```

Or, after CMake configure, copy the configured `Doxyfile` back to the source
root and run directly:

```sh
cmake --preset debug
cp build/debug/docs/doxygen/Doxyfile Doxyfile
doxygen
```

> **Note:** Running `doxygen` directly from the source root will write output
> to `build/<preset>/docs/doxygen/` because the `OUTPUT_DIRECTORY` in the
> configured Doxyfile is an absolute path set by CMake.

---

## Viewing the docs

Open the generated HTML in any browser:

```sh
# macOS
open build/debug/docs/doxygen/html/index.html

# Linux
xdg-open build/debug/docs/doxygen/html/index.html

# Windows
start build/debug/docs/doxygen/html/index.html
```

---

## Scope

The Doxygen configuration scans these directories **recursively**:

| Directory | Contents |
|-----------|----------|
| `engine/` | Core engine libraries (core, network, runtime, render, physics, collision, skeleton, animation, platform, input, audio, ui) |
| `game/`   | Flashback gameplay library and game modules |
| `wish/`   | Wish backend (engine identity, match runtime, sessions, replication, admin, integrations) |

The following are **excluded** from documentation:

| Pattern / Directory | Reason |
|---------------------|--------|
| `tools/`            | Developer tooling, not part of the public API |
| `third_party/`      | External dependencies (excluded at top level if present) |
| `engine/ui/imgui*`  | Bundled third-party Dear ImGui library |
| `engine/ui/imstb*`  | Bundled third-party stb single-header libraries |
| `engine/ui/imconfig.h` | Dear ImGui configuration header |

---

## Writing Doxygen comments

New and modified C++ code should include Doxygen-style documentation comments.
The project uses the **Javadoc** style (`@brief`, `@param`, `@return`, etc.).

### Header example

```cpp
/**
 * @brief Represents a networked player in the game world.
 *
 * Handles input replication, state interpolation, and
 * authoritative position correction from the server.
 */
class Player {
public:
    /**
     * @brief Construct a player with a given network ID.
     * @param networkId Unique identifier assigned by the session.
     */
    explicit Player(uint64_t networkId);

    /**
     * @brief Apply a movement delta received from the client.
     * @param dx  Displacement along the X axis (world units).
     * @param dy  Displacement along the Y axis (world units).
     * @param dz  Displacement along the Z axis (world units).
     * @return True if the position was updated.
     */
    bool ApplyMovement(float dx, float dy, float dz);

    /// @return The player's current world-space position.
    glm::vec3 GetPosition() const;

private:
    uint64_t networkId_;
    glm::vec3 position_;
};
```

### Key tags

| Tag | When to use |
|-----|-------------|
| `@brief` | One-line summary of the function/class/variable |
| `@param` | Document each function parameter (name + description) |
| `@return` | Describe the return value |
| `@tparam` | Template parameter documentation |
| `@see` | Cross-reference related functions or classes |
| `@note` | Additional information the caller should know |
| `@warning` | Behaviour that may be surprising or dangerous |
| `@deprecated` | Mark an API as deprecated and suggest a replacement |

### Grouping related APIs

Use `@defgroup` / `@ingroup` to organise related classes and functions:

```cpp
/** @defgroup collision Collision Detection
 *  Low-level collision primitives and spatial queries.
 *  @{
 */

struct Ray { /* ... */ };
struct AABB { /* ... */ };
bool Intersect(const Ray& ray, const AABB& aabb);

/** @} */
```

---

## CI integration

When running in CI, the documentation is generated as a build artifact:

```yaml
# Example GitHub Actions step
- name: Generate API docs
  run: cmake --build build/debug --target doc

- name: Upload docs
  uses: actions/upload-artifact@v4
  with:
    name: api-docs
    path: build/debug/docs/doxygen/html/
```

This makes the generated HTML available for download from the workflow run page.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `cmake --build ... --target doc` fails with "No rule to make target 'doc'" | Doxygen was not found during CMake configure. Install Doxygen and re-run `cmake --preset <name>`. |
| Diagrams are missing from the output | Graphviz (`dot`) is not installed. Install it and re-run CMake configure. |
| `@param` / `@return` comments produce no output | Comments must be placed **before** the declaration. Doxygen does not parse comments after the declaration by default. |
| Documentation shows internal/private functions | Set `EXTRACT_PRIVATE = NO` in `Doxyfile.in` (currently `NO`). Re-configure CMake after changing the template. |

---

## References

- [Doxygen Manual](https://www.doxygen.nl/manual/index.html)
- [Doxygen configuration reference](https://www.doxygen.nl/manual/config.html)
