# Dependency Management

This document describes how Ahamkara's dependencies are tracked, updated, and
audited for security vulnerabilities.

## Quick reference

| Dependency type | Tool | Update method | Vulnerability alerts |
|---|---|---|---|
| Docker base images | Dependabot | Automatic PR | GitHub Security > Dependabot alerts |
| GitHub Actions | Dependabot | Automatic PR | GitHub Security > Dependabot alerts |
| CMake FetchContent | Manual | Developer updates `GIT_TAG` in `CMakeLists.txt` | Manual review / OSV tooling |
| System apt packages | Manual | Developer updates `Dockerfile` / `Dockerfile.build` | Manual review / `apt-get upgrade` awareness |

---

## Dependabot-managed dependencies

Dependabot is configured in [`.github/dependabot.yml`](../../.github/dependabot.yml)
[src: file: .github/dependabot.yml].

### What Dependabot tracks

1. **Docker** — The `ubuntu:24.04` base image in both `Dockerfile` and
   `Dockerfile.build`. Dependabot opens PRs when Canonical publishes new image
   digests (e.g. security patches). It does _not_ track individual `apt-get`
   packages.
2. **GitHub Actions** — All actions used in `.github/workflows/ci.yml`
   (`actions/checkout`, `actions/upload-artifact`, etc.).

### How to handle a Dependabot PR

1. Review the PR description — Dependabot includes release notes and a
   changelog summary.
2. Check the **compatibility score** badge (if available).
3. Verify the change applies cleanly by inspecting the diff.
4. If the PR updates a Docker base image:
   - Rebuild locally: `docker build -f Dockerfile -t ahamkara-test .`
   - Smoke-test the server: `./scripts/start.sh local`
5. If the PR updates a GitHub Action:
   - Check the action's release notes (linked in the PR body).
   - No local build is needed — CI will validate on merge.
6. Merge via the normal PR workflow (squash-merge recommended).

### Viewing security alerts

- Navigate to **GitHub repository → Security → Dependabot alerts**.
- Alerts are categorised by severity (Critical, High, Moderate, Low).
- Each alert links to the affected dependency and the Dependabot PR that fixes
  it (if one exists).

---

## Manually managed dependencies

### CMake FetchContent dependencies

The following dependencies are fetched at configure time via CMake's
`FetchContent` module. Dependabot cannot parse CMake `FetchContent_Declare`
blocks, so these must be updated manually.

| Dependency | Repository | git tag in root `CMakeLists.txt` | Used by |
|---|---|---|---|
| Jolt Physics | https://github.com/jrouwe/JoltPhysics.git | `v5.0.0` | Collision, Physics |
| GLM | https://github.com/g-truc/glm.git | `1.0.1` | Core engine, Render |
| EnTT | https://github.com/skypjack/entt.git | `v3.13.0` | Core ECS |
| miniaudio | https://github.com/mackron/miniaudio.git | `master` (tip) | Audio |
| Lua (optional) | https://github.com/marovira/lua.git | `master` (tip) | Wish admin (optional) |
| sol2 (optional) | https://github.com/ThePhD/sol2.git | `v3.3.0` | Wish admin (optional) |

[src: file: CMakeLists.txt:34-94]

#### Update workflow for a FetchContent dependency

1. **Check for new releases**
   - Visit the dependency's GitHub Releases page (e.g.
     `https://github.com/jrouwe/JoltPhysics/releases`).
   - Review the changelog for breaking changes, security fixes, and new
     features relevant to Ahamkara.

2. **Update the `GIT_TAG` in `CMakeLists.txt`**
   - Open the root `CMakeLists.txt`.
   - Locate the `FetchContent_Declare` block for the dependency (lines 34-94).
   - Change `GIT_TAG` to the new release tag (e.g. `v5.1.0`).
   - **Do not use a branch name** (like `master`) for production dependencies;
     pin to a specific tag or commit SHA. The only exceptions are `miniaudio`
     and `lua`, which intentionally track `master` because their upstream
     projects do not publish stable tags [src: file: CMakeLists.txt:58-62].

3. **Update project-specific options if needed**
   - If the new version deprecates or renames CMake options, update the
     `set(... CACHE BOOL "" FORCE)` lines above the `FetchContent_Declare`.
   - If the new version requires new compiler flags, update the
     platform-specific workarounds (e.g. the Apple Clang
     `-Wno-nontrivial-memcall` suppression for Jolt at line 100-102).

4. **Rebuild and test**
   ```sh
   cmake --preset debug --fresh   # re-configure from scratch
   cmake --build --preset debug
   ctest --test-dir build/debug --output-on-failure
   ```
   Run both debug and debug-headless presets if the dependency is used across
   client and server targets.

5. **Commit and open a PR**
   - Include the dependency name and old/new tag in the commit message.
   - Example: `chore(deps): bump JoltPhysics from v5.0.0 to v5.1.0`

#### Optional: Using OSV-Scanner locally

For offline vulnerability scanning of FetchContent dependencies, you can use
[osv-scanner](https://github.com/google/osv-scanner):

```sh
# Install osv-scanner (one-time)
go install github.com/google/osv-scanner/cmd/osv-scanner@latest

# Scan the CMakeLists.txt for known vulnerabilities in declared dependencies
osv-scanner --lockfile CMakeLists.txt
```

> **Note**: OSV-Scanner's CMake support is experimental and may not detect all
> FetchContent dependencies. Use it as a supplementary check, not a replacement
> for upstream release monitoring.

### System packages (apt)

System packages are installed in `Dockerfile` and `Dockerfile.build` via
`apt-get install`. Dependabot does not track individual apt packages.

#### Update workflow

1. **Check for available updates**
   ```sh
   docker build -f Dockerfile -t ahamkara-check-updates .
   docker run --rm ahamkara-check-updates bash -c \
     "apt-get update && apt-get --just-print upgrade"
   ```

2. **Update package versions**
   - Edit the `apt-get install` line in the relevant `Dockerfile`.
   - Bump versioned packages (`libglfw3-dev`, etc.) to the desired version.
   - Rebuild and test as described above.

3. **For locally installed toolchains**
   - The project's internal build environments (e.g. agent-runner,
     self-hosted CI runners) should run `apt-get update && apt-get upgrade`
     periodically to pick up security fixes.

---

## Adding a new dependency

### Adding a FetchContent dependency

1. Add a `FetchContent_Declare` block in the root `CMakeLists.txt`:
   ```cmake
   FetchContent_Declare(
       mylib
       GIT_REPOSITORY https://github.com/example/mylib.git
       GIT_TAG        v1.0.0
   )
   ```
2. Set any required options before `FetchContent_MakeAvailable`.
3. Add `FetchContent_MakeAvailable(mylib)` at the appropriate point.
4. Link the target in the consuming module's `CMakeLists.txt`.
5. Add the dependency to the table in this document.

### Adding a system package

1. Add the package to the `apt-get install` line in the relevant Dockerfile(s).
2. If the package is required for building, add it to `Dockerfile.build`.
3. If the package is required at runtime, add it to the runtime `Dockerfile`
   stage.
4. Add the dependency to the table in this document.

---

## Security vulnerability response

1. **Dependabot alert** — If Dependabot opens an alert for a tracked
   dependency:
   - Follow the Dependabot PR or manual update workflow above.
   - For Critical/High severity, treat as a P0 and resolve within 48 hours.

2. **Manual discovery** — If a vulnerability is discovered in a FetchContent or
   system dependency:
   - Open a GitHub Issue with the `security` label.
   - Include the CVE identifier (if available), affected version, and fix
     version.
   - Follow the manual update workflow for that dependency type.
   - Reference the issue in the commit message.

3. **Documentation** — Update this file's version table when a dependency
   version changes, so the guide stays in sync with the repository.
