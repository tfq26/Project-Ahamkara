# 01 — Recon & Repo Archaeologist

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed HEAD: `b705d74` (`feat: add initial weapon and enemy assets (#71)`)
Observed branch: `dev/flashback-menu-shell`

## 1. Scope & Method

Inspected root and docs agent instructions, repository layout, CMake presets and dependency declarations, JavaScript workspace manifests and lockfile, README/build guide/repo map, current git status/branches/history, and TODO-like markers. Commands and outputs below were executed on the observed working tree. This report is reconnaissance only; it makes no source/config/dependency changes.

The checkout was already heavily dirty before audit work: `git status --porcelain=v1` returned 182 entries (74 tracked files modified/deleted and 118 untracked files). The working branch and local `main` both pointed to `b705d74`; `origin/main` was three commits ahead (`0a67167`, `01d3e4a`, `baf127d`). The dirty tree includes broad C++ changes, asset additions/deletions, test/CMake changes, generated Python bytecode, and docs. Therefore build/test findings describe this working tree, not a clean `b705d74` checkout. No attempt was made to stash, reset, checkout, clean, or reconcile the changes.

### Tool versions observed

| Tool | Version/output |
|---|---|
| CMake | `cmake version 4.3.3` |
| C++ compiler | Apple Clang 21.0.0 (`clang-2100.3.34.2`), arm64 Apple Darwin |
| Python | `Python 3.14.0` |
| Node | `v22.18.0` |
| pnpm | `11.25.0` |

### Safe validation attempts

Build artifacts were directed to `/private/tmp/ahamkara-audit-debug-headless`; this avoided overwriting the existing `build/debug-headless` and `build/debug` trees. Dependency source directories were reused read-only from the existing headless build after the first configure attempt showed outbound GitHub DNS/network was unavailable.

1. `cmake --preset debug-headless -B /private/tmp/ahamkara-audit-debug-headless` — **failed, exit 1**. CMake identified AppleClang 21, then FetchContent retried cloning `https://github.com/g-truc/glm.git` three times and failed with `Could not resolve host: github.com`.
2. `cmake --preset debug-headless -B /private/tmp/ahamkara-audit-debug-headless -DFETCHCONTENT_SOURCE_DIR_GLM=/Users/taufeeqali/Projects/Ahamkara/build/debug-headless/_deps/glm-src -DFETCHCONTENT_SOURCE_DIR_ENTT=/Users/taufeeqali/Projects/Ahamkara/build/debug-headless/_deps/entt-src -DFETCHCONTENT_SOURCE_DIR_JOLT=/Users/taufeeqali/Projects/Ahamkara/build/debug-headless/_deps/jolt-src -DFETCHCONTENT_SOURCE_DIR_MINIAUDIO=/Users/taufeeqali/Projects/Ahamkara/build/debug-headless/_deps/miniaudio-src` — **configured, exit 0**. CMake reported GLM 1.0.1, Python 3.14.0, and wrote generated manifests/build files under `/private/tmp`. Warnings: GLM CMake policy deprecations, miniaudio optional vorbis/opus/SteamAudio not found, and Jolt IPO unsupported for this C compiler.
3. `cmake --build /private/tmp/ahamkara-audit-debug-headless --parallel 4` — **failed, exit 1** after compiling/linking 391 of 392 steps. Final failing target: `ahamkara_menu_layout_tests`; linker reported `ld: library 'ae_ui' not found`. In this preset the client is disabled, and root CMake only adds `engine/ui` inside `if(AHAMKARA_BUILD_CLIENT)` (`CMakeLists.txt:169-187`), while the current `tests/CMakeLists.txt` adds the UI-linked test unconditionally (`tests/CMakeLists.txt:893-904`). The test/CMake additions are part of the dirty tree, so this is not evidence about clean HEAD.
4. `ctest --test-dir /private/tmp/ahamkara-audit-debug-headless --output-on-failure` — **failed, exit 8**: 67/71 passed, four failed/not run. `caligo_asset_contract_tests` failed its exact `spawns` assertion; `ahamkara_smoke_tests` and `ahamkara_nakama_bridge_tests` aborted when socket bind calls were denied (`Operation not permitted` / assertion); `ahamkara_menu_layout_tests` could not run because step 3 did not produce its executable. Asset assertions read dirty working-tree assets, so the Caligo failure is also not attributable to clean HEAD. Socket failures are environment-restricted and do not by themselves establish product defects.
5. `pnpm --dir /private/tmp/ahamkara-audit-pnpm install --frozen-lockfile --offline` — **failed, exit 1** in an isolated temporary workspace containing copies of the package manifests and lockfiles. pnpm 11.25.0 stopped at supply-chain verification: `ERR_PNPM_NO_OFFLINE_META`, missing cached metadata for `@eslint-community/eslint-utils`. No project `node_modules` or lockfile was changed. The repo manifests show frontend and backend build scripts, but no web app build/test was run because dependencies were not installed and network access was unavailable.
6. No GUI demo was launched: the available build attempt was explicitly headless, and launching a graphical sample would require a separate display-capable build/runtime environment. No test was run against the existing project build directories.

## 2. Executive Summary

- **Recon rating: 2/5 — rudimentary onboarding state with substantial drift.** The repo has a detailed module tree and CMake presets, but the root agent guide still contains literal template placeholders and points to Tier 2 tables that are absent from `docs/AGENTS.md` (`AGENTS.md:1-19`; inspected `docs/AGENTS.md`).
- This is a C++20/CMake/Ninja native game-engine/game stack plus a separate Vue/Vite frontend and Hono/TypeScript backend (`CMakeLists.txt:8-19`; `package.json:1-12`; frontend/backend manifests).
- The project is explicitly transitional: engine, Flashback gameplay, Wish backend/session platform, client/server, tools, web site, tests, and product assets coexist in one repository (`docs/repo-map.md:15-30`).
- Current source control state prevents a clean baseline conclusion. There are 182 dirty/untracked status entries; local `main` is at the same commit as the feature-named branch, and `origin/main` is three commits ahead.
- A fresh headless configure required both network access and dependency fetches; it failed online, then succeeded when reusing dependency sources already present in the existing build tree. This makes out-of-box offline reproducibility weak.
- The headless build of the current dirty tree failed at the newly present UI test target because `ae_ui` is excluded when client is disabled. Most of the compiled tests passed, but current asset and socket tests failed as described above.
- CMake pins Jolt, GLM, EnTT, and sol2 to tags, but uses moving `master` for miniaudio and optional Lua (`CMakeLists.txt:57-94`).
- Web dependency state has visible package/lock drift: backend manifest asks for Hono `^4.6.0`, while lock importer says `^4.7.0` (`backend/package.json`; `pnpm-lock.yaml:11-15`). pnpm packageManager is `11.18.0`, while observed pnpm is `11.25.0` (`package.json:4`).
- Docs have actionable drift: repo map says modules are “always” added though CMake options gate several, README describes the renderer as a tiny debug-only view despite an `engine/render` module, and guide entry points/status need a full pass (`docs/repo-map.md:8-13`; `CMakeLists.txt:131-195`; `README.md:48-55`).

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Core native engine | `engine/core`, `engine/runtime`, `engine/network` | IMPLEMENTED | 3 | Separate CMake libraries and public-header/source layout. |
| Physics/collision/gameplay | `engine/physics`, `engine/collision`, `game` | IMPLEMENTED | 3 | Jolt-backed physics and game/world code exist; details are for specialist audits. |
| Rendering/platform/UI/audio/input/animation | `engine/render`, `engine/platform`, `engine/ui`, `engine/audio`, `engine/input`, `engine/animation`, `engine/skeleton` | IMPLEMENTED | 3 | Modules are separate; graphical targets only enter the default graph when `AHAMKARA_BUILD_CLIENT=ON`. |
| Multiplayer/session platform | `server`, `wish`, `engine/network` | IMPLEMENTED | 3 | Dedicated server and Wish modules coexist with game code. |
| Flashback product/demo | `game`, `client`, `samples/flashback`, `assets` | PARTIAL | 2 | Product-oriented code and assets are extensive; current dirty tree contains broad unfinished feature work. |
| Asset/tooling pipeline | `tools`, `assets/manifest.assets`, `tools/blender`, `tools/levelgen` | PARTIAL | 3 | Importer, Blender scripts, level generation, and asset tests exist. End-to-end status requires specialist audit. |
| Web frontend/backend | `frontend`, `backend`, root `package.json`, `pnpm-lock.yaml` | PARTIAL | 2 | Vue/Vite and Hono/TypeScript workspace; lock importer mismatch and offline install blocked. |
| Build orchestration | `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | IMPLEMENTED | 3 | Debug, release, headless, package, and Wish-standalone presets; current dirty headless graph has a UI-test target dependency mismatch. |
| Tests | `tests`, `wish/*/tests`, `backend/tests`, CTest | PARTIAL | 3 | 71 CTest cases discovered in current configure; observed 67 pass with 4 failures/not-run, under dirty state and sandbox restrictions. |
| Agent onboarding | `AGENTS.md`, `docs/AGENTS.md`, `docs/repo-map.md`, `docs/guides/` | PARTIAL | 1 | Placeholder root facts/rules and inaccurate or incomplete guide material. |
| Dependency reproducibility | root CMake FetchContent + pnpm lock | PARTIAL | 2 | CMake has moving `master` refs; pnpm frozen offline install cannot resolve policy metadata, and backend manifest/lock differ. |

## 4. Detailed Findings

### RECON-001 — Root agent guide contains unresolved template placeholders

- **Severity:** HIGH
- **Category:** docs
- **Evidence:** `AGENTS.md:1-19` literally contains `{{STACK_SUMMARY}}`, `{{TEST_CMD}}`, `{{LINT_CMD}}`, `{{DO_NOT_1}}`, and `{{DO_NOT_2}}`. It directs agents to “Tier 2 tables inside `docs/AGENTS.md`,” but the inspected `docs/AGENTS.md` consists of anti-hallucination protocol, citation grammar, docs structure/linking/work-tracking guidance and has no Tier 2 tables. `AGENTS.md:23-28` has build/lint commands, but the generated facts and core project-specific agent rules are not resolved.
- **Impact:** A cold-start coding agent receives false pointers, no stack summary in the expected section, and incomplete quality instructions. This directly increases context cost and risks unsafe or mislocated changes.
- **Recommended fix:** Replace placeholders through the documented Kronn generator/source of truth, verify `docs/AGENTS.md` references, and add a compact project workflow/placement table or update the root link to the actual source.
- **Effort:** S
- **Dependencies:** none

### RECON-002 — Current docs describe an older/simple runtime state

- **Severity:** HIGH
- **Category:** docs
- **Evidence:** `README.md:48-55` says current milestone is a networking skeleton and describes a “tiny client-only debug renderer”; `engine/render` contains a full module tree and root CMake adds render/UI/audio/input when client is enabled (`CMakeLists.txt:169-177`). `docs/repo-map.md:10-13` claims core, collision, physics, network, runtime, Wish, and game are always added, whereas `CMakeLists.txt:135-167` gates physics/collision and Wish/game by options. `docs/guides/building.md:133-144` gives the older ground-grid view description. These claims were not all revalidated behaviorally, but code layout/target conditions directly contradict the broad README/repo-map descriptions.
- **Impact:** Agents and the owner can form a stale mental model, select the wrong target, or duplicate already-present subsystems. The repo map also understates CMake option conditions needed to understand headless/package builds.
- **Recommended fix:** Refresh README current state and repo-map build composition against current CMake and shipped targets. Mark speculative/future ownership plans separately from implemented state.
- **Effort:** M
- **Dependencies:** none

### RECON-003 — CMake has moving FetchContent revisions

- **Severity:** HIGH
- **Category:** risk
- **Evidence:** `CMakeLists.txt:57-62` pins miniaudio to `GIT_TAG master`; optional Lua uses `master` at `CMakeLists.txt:78-84`. Jolt/GLM/EnTT/sol2 use version tags at `CMakeLists.txt:34-55, 88-92`. In the fresh temp configure, GitHub name resolution failed and CMake could not fetch GLM; only reuse of existing source trees allowed configure to complete.
- **Impact:** New checkouts can get changing dependency contents over time and builds depend on network/reusable local caches. This impairs audit reproducibility and autonomous-agent setup.
- **Recommended fix:** Pin moving dependencies to immutable commits/releases and provide a documented cache/mirror strategy for offline or constrained CI setup.
- **Effort:** S
- **Dependencies:** none

### RECON-004 — Dirty tree blocks attribution of current test/build failures

- **Severity:** HIGH
- **Category:** risk
- **Evidence:** Initial git status recorded 182 entries (74 tracked modified/deleted, 118 untracked). Current branch and local `main` point at `b705d74`, while `origin/main` is 3 commits ahead. The temporary build's UI test linker failure maps directly to dirty `tests/CMakeLists.txt:893-904` linking `ae_ui` while `CMakePresets.json:31-41` sets client OFF and `CMakeLists.txt:169-187` defines `ae_ui` only inside the client condition. CTest also reads dirty test/assets; Caligo contract failed on expected spawns. These are working-tree observations only.
- **Impact:** It is not possible to say whether current failures are committed regressions, intentional in-progress edits, or mismatches between the owner’s unstaged work and branch base. Subsequent auditors can accidentally treat work-in-progress as authoritative product state.
- **Recommended fix:** Keep this limitation explicit in all report conclusions. Before implementation work, have the owner identify the intended disposition of the current working tree; after preservation/commit, rerun validation on an identified baseline.
- **Effort:** S
- **Dependencies:** none

### RECON-005 — pnpm package metadata and lockfile disagree

- **Severity:** MEDIUM
- **Category:** tech debt
- **Evidence:** `backend/package.json` declares Hono `^4.6.0`; the `backend` importer in `pnpm-lock.yaml:11-15` records specifier `^4.7.0`. Root `package.json:4` declares pnpm `11.18.0`, while the installed tool reports `11.25.0`. Isolated `pnpm install --frozen-lockfile --offline` stopped before a lock consistency result because pnpm lacked cached policy metadata for `@eslint-community/eslint-utils`.
- **Impact:** The lockfile does not faithfully describe the backend manifest and frozen installation may fail or prompt lock changes in a provisioned environment. Agents cannot assume an offline reproducible web setup from the checked-in metadata.
- **Recommended fix:** Regenerate/update the lock with the repository-declared pnpm version (or deliberately update the declared version), then verify frozen install/build/test in CI with network/cache available.
- **Effort:** S
- **Dependencies:** none

### RECON-006 — Headless CTest environment includes network-dependent tests

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** The CTest run in `/private/tmp/ahamkara-audit-debug-headless` aborted two tests when `bind()` was denied by the execution environment: `ahamkara_smoke_tests` at `tests/src/network_smoke_tests.cpp:346` and `ahamkara_nakama_bridge_tests` at `tests/src/nakama_bridge_tests.cpp:86`. The run reports 67/71 passed, with those two socket tests, the dirty Caligo asset contract, and the missing UI-linked executable accounting for four failures/not-run.
- **Impact:** Headless validation can be reported as a product failure even when the execution environment prevents loopback networking; the current full-suite result is not portable evidence until separated into environment-dependent and code/asset failures.
- **Recommended fix:** Document the loopback requirement for these tests and provide an explicit test label/filter or a test server abstraction that is usable in restricted environments. Preserve the full network tests in normal CI.
- **Effort:** M
- **Dependencies:** RECON-004

## 5. What's Good

- The source tree has recognizable boundaries: engine modules use `engine/<module>/include` and `src`, while `game`, `client`, `server`, `wish`, `tools`, and `samples` are distinct top-level areas (`docs/repo-map.md:15-57`).
- CMake already offers a headless preset and multiple package/standalone configurations; its headless preset explicitly excludes client and samples (`CMakePresets.json:31-41`).
- CMake uses version tags for Jolt, GLM, EnTT, and sol2, and exposes a package test suite including out-of-tree consumers (`CMakeLists.txt:34-55,88-94`; the CTest run passed package-consumer tests).
- The engine has existing test coverage for engine/game/network/session/asset/telemetry/diagnostics behavior; current configure discovered 71 CTest cases.
- A workspace lockfile is present for the Vue/Hono monorepo and standard root scripts provide a basic web build/typecheck interface (`package.json:5-12`).
- The working tree contains a substantial in-progress feature set and assets; preserve it intact while other agents inspect.

## 6. Open Questions for the Owner

- The mission template leaves target platforms/languages, 2D-vs-3D scope, timeline, and first genres blank. Which platforms and game genres should govern prioritization? Recommended default for the audit: assess what is demonstrably implemented, and defer platform/genre roadmap choices until the owner answers.
- Is `dev/flashback-menu-shell` intentionally the current development base, or should the three `origin/main` commits be considered for the baseline? Recommended default: do not merge/rebase during the audit; cite the observed commit and remote delta.
- What is the intended disposition of the 182 preexisting dirty/untracked entries? Recommended default: preserve byte-for-byte and treat them as uncommitted work-in-progress until the owner identifies their source/state.

## 7. Handoff Notes for the Next AI Agent

- Read this report first, then inspect the source tree at the exact dirty working state. Do not assume any reported test failure belongs to committed HEAD.
- Do not run commands that reset, clean, stash, checkout, regenerate assets, or build inside existing `build/`; all build outputs in this pass were isolated under `/private/tmp/ahamkara-audit-debug-headless`.
- CMake root options control which modules exist: `engine/ui`, render, platform, input, audio and client targets are in the client conditional; collision/physics and animation/skeleton are gated separately by `AHAMKARA_ENGINE_ONLY`; Wish/game/server have their own conditions (`CMakeLists.txt:131-195`).
- There are multiple distinct product surfaces: native C++ game engine, Flashback game/client/server, Wish backend/session platform, and Vue/Hono web workspace. Avoid treating web package tests as coverage of the native engine or vice versa.
- Treat README and `docs/repo-map.md` as orientation hints only until cross-checked against current CMake/code. Root `AGENTS.md` has unresolved placeholders; `docs/AGENTS.md` sets strict citation expectations.
- TODO-like census: a broad repository search returned 453 raw marker matches, heavily inflated by vendored Dear ImGui and false positives such as the game’s “speed hack” log string and `XXX` protocol examples. A narrower search over engine/game/client/server/wish/tools/tests/backend/frontend/scripts/docs still returned 20 raw matches, including vendored stb comments; this is not a verified count of actionable project TODOs.
