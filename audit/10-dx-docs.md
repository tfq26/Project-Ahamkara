# 10 — DX, Docs & Onboarding Reviewer

Audit date: 2026-10-16 (America/Chicago)
Repository state: see `audit/01-recon.md`; observed HEAD `b705d74`, branch `dev/flashback-menu-shell`, with a heavily dirty working tree.
Rating: **2/5 — useful operational documentation exists, but the repository does not provide a reliable, game-authoring onboarding path for a cold-start human or AI agent.**

## 1. Scope & Method

Reviewed `README.md`, `docs/README.md`, `docs/repo-map.md`, `docs/guides/README.md`, `docs/guides/building.md`, `docs/guides/agent-handoff.md`, `docs/api-reference/README.md`, and the Flashback sample entry point and CMake files. Searched docs and samples for tutorial, quick-start, and game-authoring material; cross-checked documented current state against the reconnaissance report and the relevant current source/build layout. Evidence references below use repository paths and line ranges. The initial repo state is already dirty (see `audit/01-recon.md`); no source files were changed.

No build, test, demo launch, or game-authoring experiment was run for this documentation-focused pass. Build/install attempts and their environment limitations are recorded in `audit/01-recon.md`. In particular, this report does not claim a measured time-to-first-game; it evaluates whether docs expose a reproducible route to authoring a new game.

## 2. Executive Summary

- **Overall DX/docs rating: 2/5.** A developer can find build, test, launch, server, and asset-import commands, but there is no demonstrated end-to-end tutorial for creating a new game.
- The root README’s `Quick Start` section is effectively empty: it immediately becomes a `Prerequisites` heading, while native client setup appears later under a differently named section (`README.md:136-148,152-190`).
- The build guide is operationally useful and documents headless and graphical builds, existing launchers, controls, and the asset importer (`docs/guides/building.md:38-82,84-106,133-223`). It teaches running existing behavior, not authoring a new game.
- The API Reference is a module/area index only. It points agents to browse headers and gives no direct links to canonical headers, examples, minimal usage, ownership/lifetime rules, or an API index generated from code (`docs/api-reference/README.md:5-39`).
- Product vocabulary is overloaded: “client” can mean the native C++ game client or the Vue web frontend; “backend” can mean Wish or the Hono service. The README itself labels native setup “Client (Frontend)” and separately identifies the web frontend (`README.md:99-106,152-155`).
- The README’s “Current Milestone” text describes an early networking skeleton and says audio/editor/full renderer are deferred (`README.md:48-55`), while the recon inventory confirms substantial render, audio, UI, animation, asset, and gameplay code exists in this checkout. This is a major orientation hazard; dirty-tree attribution still applies.
- The documentation landing page repeats its “Start Here” section with inconsistent casing and numbering (`docs/README.md:41-49,62-80`), and the README TOC advertises a Quick Start section that does not deliver a quick-start flow (`README.md:13-27,136-148`).
- No user-facing tutorial/example path was found that takes an agent from an idea to a minimal runnable custom game, then adds behavior, an asset, scoring/win state, and a verification artifact. Time-to-first-game is therefore **not established** and likely dominated by source/header exploration and build-target discovery.

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Root orientation and quick start | `README.md` | PARTIAL | 2 | Clear stack overview, but obsolete milestone claims and empty Quick Start section; naming collisions between native/web surfaces. |
| Build and run guide | `docs/guides/building.md` | IMPLEMENTED | 4 | Concrete configure/build/test/run commands for existing client/server/sandbox and importer; not a game-authoring tutorial. |
| Agent handoff guide | `docs/guides/agent-handoff.md` | IMPLEMENTED | 3 | Strong repo-orientation and workflow pointers; includes extensive agent process detail but no concise API/game-authoring path. |
| Repository map | `docs/repo-map.md` | PARTIAL | 3 | Useful folders and entrypoints; its build composition is contradicted by its own cited CMake range according to recon (`audit/01-recon.md`, RECON-002). |
| API reference | `docs/api-reference/README.md` | PARTIAL | 1 | Lists broad module names and says to inspect headers; no module pages or examples surfaced by this index. |
| Docs landing page | `docs/README.md` | PARTIAL | 2 | Broad index and useful source-of-truth guidance, but duplicated “Start Here” block and repeated numbering. |
| Example game / sample entry point | `samples/flashback/` | PARTIAL | 2 | Demonstrates bootstrapping existing Flashback module; no adjacent tutorial or minimal generic game example. |
| Inline comments / usage hints | `samples/flashback/src/main.cpp`, public headers | PARTIAL | 3 | Some actionable local usage notes exist, but they are isolated and not discoverable from the docs index. |
| Tutorial for creating a new game | Not found in `docs/guides/`, `docs/api-reference/`, or `samples/` | MISSING | 1 | No documented minimal authoring path from new game to verified playable result. |

## 4. Detailed Findings

### DX-001 — README milestone and feature status are stale against the inspected tree

- **Severity:** HIGH
- **Category:** docs
- **Evidence:** `README.md:48-55` says the current milestone is a networking skeleton, calls the renderer “tiny,” and says audio, editor tooling, and fuller renderer systems are held back. The current repository inventory includes `engine/render`, `engine/audio`, `engine/ui`, `engine/animation`, asset tooling, and a large gameplay/client surface; `audit/01-recon.md` also records this mismatch as RECON-002. The README claims are not date-stamped or marked historical.
- **Impact:** A returning owner or cold-start agent may conclude existing systems are absent, duplicate work, or choose the wrong first task. The mismatch is especially costly for agents using README as their first context anchor.
- **Recommended fix:** Replace the milestone paragraph with a dated, evidence-backed current-state summary. Distinguish demonstrably present code from verified working capabilities, and link each subsystem to a canonical status page or smoke/example command. Keep historical milestone notes under reports.
- **Effort:** S
- **Dependencies:** none

### DX-002 — There is no end-to-end tutorial for making a new game

- **Severity:** BLOCKER
- **Category:** missing feature
- **Evidence:** The README Quick Start heading is followed only by a Prerequisites heading/table and link to build instructions (`README.md:136-148`). The building guide explains building and launching existing client/server/sandbox workflows (`docs/guides/building.md:38-106,133-205`) and importing assets (`:208-223`), but does not define a smallest game project or a sequence for adding gameplay and verifying it. The API Reference only lists module families and redirects the reader to inspect headers (`docs/api-reference/README.md:5-39`). Sample `main.cpp` documents launching Flashback and its `--level`/`--autoplay` flags, not how to author another game (`samples/flashback/src/main.cpp:1-13,33-74`).
- **Impact:** The engine’s stated AI-native objective requires an agent to autonomously author and iterate a complete game. Current documentation gets an agent to an existing demo, then leaves it to infer composition, game-module registration, asset wiring, game rules, and verification. Time-to-first-game is unknown and cannot be estimated from documented steps.
- **Recommended fix:** Add a versioned “Create a minimal game” tutorial that starts from a supported template or minimal CMake target and produces a playable game. Include exact files, build/run commands, a movement example, a second entity behavior, asset import/display, score and win/lose logic, and a deterministic/headless or screenshot verification step. Link the tutorial from the README Quick Start, API Reference, and sample directory. If no stable authoring contract exists yet, state that explicitly and nominate one supported path.
- **Effort:** L
- **Dependencies:** core architecture/API stability findings from `audit/02-architecture.md`; AI-ergonomics findings from its assigned specialist report.

### DX-003 — API Reference is a directory map, not a usable API reference

- **Severity:** HIGH
- **Category:** docs
- **Evidence:** `docs/api-reference/README.md:5-35` gives module/area names and broad descriptions but no links to individual headers, types, function signatures, snippets, or usage constraints. Its closing paragraph tells the reader to find details in headers under wildcard paths (`:37-39`); it offers no header inventory or module navigation.
- **Impact:** An LLM must search and read implementation headers without knowing the intended entry point, stable contract, ownership rules, or simplest composition. This inflates context use and increases the chance it selects internal or transitional APIs.
- **Recommended fix:** For each supported game-authoring surface, provide an API landing page with direct links to canonical public headers, minimal compilable snippets, lifecycle/ownership notes, and links to tests/examples. Prefer generated symbol/signature indexes where maintainable, and label internal/unstable surfaces.
- **Effort:** M
- **Dependencies:** DX-002

### DX-004 — Product terms make the first navigation choice ambiguous

- **Severity:** MEDIUM
- **Category:** docs
- **Evidence:** The README describes a native C++ client in the project overview (`README.md:39-40`), then calls its setup “Client (Frontend)” (`:152-155`), while a separate web monorepo table names `frontend/` as the Vue application and `backend/` as Hono (`:99-106`). Wish is separately described as a backend session platform (`:43-44`).
- **Impact:** A human or agent can open the wrong “client” or “backend” docs/package, run irrelevant commands, or conflate native gameplay with the information site and Hono service.
- **Recommended fix:** Use “native game client,” “web frontend,” “Hono API,” and “Wish session platform” consistently in headings, tables, scripts, and docs. Add a short first-page routing table with task → component → canonical guide.
- **Effort:** S
- **Dependencies:** none

### DX-005 — Documentation navigation repeats and weakens its entry path

- **Severity:** LOW
- **Category:** docs
- **Evidence:** `docs/README.md:41-49` contains a “Start Here” list; lines 62-80 repeat the section as “Start here,” add items, and restart numbering at 1/2/2. The root README’s TOC lists a Quick Start anchor, but its corresponding section does not provide a quick start (`README.md:13-27,136-148`).
- **Impact:** The docs home looks partially merged or stale, making it less obvious which list is canonical. Repeated navigation reduces trust in adjacent status claims.
- **Recommended fix:** Keep one curated “Start here” list per audience (new game author, engine contributor, operator) and repair numbering and section anchors. Make root Quick Start a real runnable path or rename the section to prerequisites and link to the game tutorial.
- **Effort:** S
- **Dependencies:** DX-002

### DX-006 — Existing demo instructions do not explain authoring or verify visible outcomes

- **Severity:** MEDIUM
- **Category:** docs
- **Evidence:** `docs/guides/building.md:133-165` describes the debug view and its controls; `:167-205` describes network and offline launch workflows. It does not give expected state/output beyond broad interaction description, or a screenshot/state capture procedure. The Flashback sample comments do identify invocation and root-relative asset-path constraints (`samples/flashback/src/main.cpp:8-13`), but the sample is a launcher for the existing game module.
- **Impact:** The docs support manual operation but provide weak closed-loop verification for an autonomous agent: it cannot reliably tell from prose what success looks like or capture a portable artifact after a change.
- **Recommended fix:** Add expected outputs and one machine-checkable verification procedure to each supported sample. For game creation, provide a headless boot/step/state check or a screenshot capture path with known-good expected output, and document the asset working-directory contract.
- **Effort:** M
- **Dependencies:** DX-002; QA verification design

## 5. What’s Good

- `docs/guides/building.md` gives concrete CMake preset, build, CTest, launcher, and control instructions (`:38-106,133-205`). This is a useful operational base to keep.
- The guide documents a first-slice importer invocation and points to the asset-pipeline system doc (`docs/guides/building.md:208-223`), giving an actual command rather than only architectural prose.
- The sample entry point records the run-from-repository-root requirement and how to opt into a level or autoplay (`samples/flashback/src/main.cpp:8-13`). This is valuable practical context that should be surfaced in a canonical tutorial.
- The docs index establishes a source-of-truth ordering that prioritizes code/tests/build config for behavior and accepted architecture for intent (`docs/README.md:96-100`).
- The agent handoff guide gives a structured first-read path and canonical build/test commands (`docs/guides/agent-handoff.md:69-86,104-137`).

## 6. Open Questions for the Owner

- Which single authoring route should the future tutorial treat as supported: a game module linked into the native client, a sample target, a standalone package consumer, or another route?
- Should the first AI-first vertical slice target a 2D game, a top-down 3D game, or build on the existing first-person Flashback work? The owner mission leaves genre/platform priorities blank.
- Which execution environment should the tutorial optimize for: the macOS local GUI path, Linux headless Coder workspaces, or both?
- What should count as “verified playable” for the reference tutorial: a deterministic test/state assertion, screenshot, interactive run, or all three?

## 7. Handoff Notes for the Next AI Agent

- Start with `audit/01-recon.md` for working-tree caveats and the verified current repository map. Do not infer that the checked-out dirty state equals clean HEAD.
- Preserve the operational CMake/run commands in `docs/guides/building.md`; organize a game-authoring tutorial beside them rather than replacing them with aspirational prose.
- Before documenting APIs, confirm public headers and composition contracts from code and tests. The API index currently does not establish which interfaces are stable.
- Resolve native-vs-web naming at entry points. “Client” and “backend” are overloaded in the README.
- No measured time-to-first-game is available from this pass. Measure it only after an agreed authoring path and clean environment are selected; record setup time separately from game implementation time.
- Keep this audit report under `/audit/`; the mission prohibits source, config, and dependency edits during the audit.
