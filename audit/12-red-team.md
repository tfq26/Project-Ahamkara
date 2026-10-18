# 12 — Red Team: Adversarial Review

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed HEAD: `b705d74`; dirty working tree as described in [01-recon.md](01-recon.md).
Red-team assessment: **The consistent maturity verdict of 2/5 is supported; audit confidence is 3/5 because source inspection is broad, but GUI, clean-baseline, performance, and several live-experiment claims remain unverified.**

## 1. Scope & Method

Read `audit/01-recon.md` and all specialist reports `audit/02` through `audit/11`. Re-read the mission prompt’s required experiment list. Compared overlapping findings and their severities, inspected the actual scratch game source/output and imported sprite/hash under `/private/tmp/ahamkara-ai-audit/`, reviewed the MCP wrapper and native bridge preconditions, checked representative source claims for the game-module contract, profiler, streaming registry, crash handler, and CI runner set, and re-counted the current Git status. No builds, tests, game launches, or benchmarks were run for this phase. No source or configuration was changed; this report is the only file written in this pass.

The scratch experiment outputs and recon command results are audit evidence, not independent reproduction by this reviewer. The original build directory `/private/tmp/ahamkara-audit-debug-headless` was not present when I checked, so I reviewed the exact commands and outcomes preserved in the recon report rather than rerunning them. The repository was already dirty before the audit; current status cannot establish which non-audit paths changed during the audit because the initial report did not preserve a per-path status snapshot.

## 2. Executive Summary

- **Overall maturity remains 2/5 against the mission.** Across architecture, rendering, gameplay, assets, AI ergonomics, game design, DX, and security, the reports consistently describe useful native building blocks without a proven path from game idea to a complete, self-verified game.
- The reports are generally explicit about source-only evidence and the dirty baseline. The main synthesis risk is treating absence of live verification as proof that a source feature is broken, or treating a manually authored scratch workaround as proof of engine-native capability.
- The five required experiments did not establish the promised game-authoring loop: the scratch module built after manual CMake work, but no moving square was rendered; sprite import copied a PPM but did not display it; scoring was ordinary custom C++ fields; and the JSON state artifact was made by filtering program output rather than emitted by an engine state-dump API (`audit/06-ai-ergonomics.md:170-190`; `/private/tmp/ahamkara-ai-audit/minigame/main.cpp`; `/private/tmp/ahamkara-ai-audit/minigame/run.out`).
- The failed screenshot attempt is not evidence by itself that `capture_frame` is defective: the first request was made with a nonexistent root, and the second was made after creating the directory but without a confirmed running bridge. The documented game launch path is what starts the native bridge and creates its command/root directory (`tools/game_mcp_server.py:89-102,104-135`; `client/src/game_mcp_bridge.cpp:118-134`). The client launch itself did not reach readiness, which remains an environment-limited, unverified result.
- Recon has a concrete status-count inconsistency: it states 182 entries but decomposes that as 74 tracked plus 118 untracked, which totals 192 (`audit/01-recon.md:12`). Current status is 74 tracked and 109 untracked (183 total, including the 11 audit reports). Without the original path list, the exact pre-audit dirty count and any during-audit delta cannot be reconciled.
- Some high-severity items need conditional framing: the streaming registry retains historical entries but may be bounded by a finite world grid; the unbounded-native-code threat depends on whether agents will compile/run untrusted code unattended; and the unavailable screenshot attempt does not establish a product bug. These are still relevant risks or gaps, but should not be presented as unconditional confirmed failures.
- Major cross-report overlap is manageable: authoring API deficiencies recur as architecture, AI ergonomics, and docs findings; lack of replay/visual proof recurs in AI ergonomics, QA, rendering, and game design; world persistence recurs in architecture and gameplay. Synthesis should group these by root cause, while preserving security, rendering, and design consequences as distinct impacts.
- No cited build/test result was found to justify clean-HEAD claims. Recon explicitly attributes its compiler/test outcomes to the dirty working tree and environment; the specialist reports generally preserve that caveat. Keep it in every master-level validation summary.

## 3. Inventory

| Review area | Reports | Status | Quality (1-5) | Adversarial assessment |
|---|---|---|---:|---|
| Baseline/recon | `audit/01-recon.md` | PARTIAL | 2 | Useful exact commands and environment context, but its status-count arithmetic is inconsistent and no per-path starting snapshot is retained. |
| Architecture and authoring API | `audit/02-architecture.md`, `audit/06-ai-ergonomics.md`, `audit/10-dx-docs.md` | PARTIAL | 2 | Strong common evidence for a lifecycle-only module contract; merge duplicates and distinguish “API absent” from “not found in inspected scope.” |
| Graphics and assets | `audit/03-rendering.md`, `audit/05-assets-audio.md` | PARTIAL | 2 | Source evidence is detailed; renderer behavior remains unverified because no GUI/render output was tested. Dirty implementation files require baseline attribution caveats. |
| Gameplay and game design | `audit/04-gameplay.md`, `audit/09-game-design.md` | PARTIAL | 2 | Source-level system gaps are documented; fun, responsiveness, and complete playable-loop claims remain explicitly unverified. |
| AI feedback/live experiments | `audit/06-ai-ergonomics.md` | PARTIAL | 2 | Good friction evidence, but the five experiments are not five completed engine workflows; two were source-level custom logic, one was copy-only, and state dump was manually generated. |
| QA and performance | `audit/07-qa-testing.md`, `audit/08-performance-reliability.md` | PARTIAL | 2–3 | QA rating 3 is defensible as suite breadth, though weaker than mission-level self-verification. Performance risks are plausible by inspection, with no timings. |
| Security | `audit/11-security.md` | PARTIAL | 2 | High-value threat surfaces identified; severity of native-code sandbox gap depends on the intended trust boundary for generated code. |

## 4. Detailed Findings

### Challenge and Resolution Register

| Challenge | Resolution |
|---|---|
| “182 dirty entries (74 + 118)” in Recon | **Reject exact count as written.** Arithmetic gives 192. Current status is 183 including 11 reports, but this cannot reconstruct the starting set. Master audit should say “heavily dirty; exact baseline count disputed” until a preserved starting status list exists. |
| Experiment 1 proves a moving-square game | **Not established.** The scratch code starts an `Application` and ticks a custom `IGameModule`, but there is no renderer or square draw call in `/private/tmp/ahamkara-ai-audit/minigame/main.cpp`. This demonstrates lifecycle/module execution after manual CMake setup only. |
| Experiment 2 proves engine entity authoring | **Not established; custom behavior did work.** `Enemy` is a plain struct embedded in `SquareGame`, not an engine entity/component. Output verifies arithmetic movement from 0 to 1.5, not registration, rendering, collision, or engine-managed lifetime (`audit/06-ai-ergonomics.md:170-176`). |
| Experiment 3 proves sprite workflow | **Import-only success.** The manifest importer copied a 1×1 PPM; source and output SHA-256 match. No runtime lookup/display occurred, so this is not an asset-to-game end-to-end success (`audit/06-ai-ergonomics.md:14`; `/private/tmp/ahamkara-ai-audit/manifest.assets`; scratch asset hashes). |
| Experiment 4 proves built-in win/lose/score features | **Only custom C++ logic succeeded.** The scratch module sets a boolean and integer score when its custom entity crosses a threshold. No built-in objective, UI, persistence, loss path, or acceptance assertion was exercised (`audit/06-ai-ergonomics.md:178-183`). |
| Experiment 5 proves an engine state dump/screenshot | **Only manual JSON extraction succeeded; screenshot did not.** The scratch program writes one JSON line and `sed` copies it into `state.json`; this is not an engine-provided world dump. The `capture_frame` call without a session root raises `FileNotFoundError` by implementation, and after manually creating the directory it times out because no running bridge produced `frame.ppm` (`audit/06-ai-ergonomics.md:185-190`; `tools/game_mcp_server.py:89-102`). The failed client launch remains inconclusive. |
| Capture failure proves MCP capture is broken | **Overstated if read that way.** The normal `game_launch` route starts the client, whose bridge `start()` creates `<root>/commands` and therefore the root (`tools/game_mcp_server.py:104-135`; `client/src/game_mcp_bridge.cpp:118-134`). The experiment did not confirm that route reached readiness. Record this as “not demonstrated in the environment,” not “capture implementation broken.” |
| QA 3/5 conflicts with the audit-wide 2/5 verdict | **Not a hard contradiction.** QA rates test infrastructure and source coverage, where many focused tests and Linux/Windows CI exist; the broader AI self-verification loop is substantially weaker. Master scorecard should keep domain scores distinct and state the mission-level score separately. |
| PERF-001 proves an unbounded memory leak | **Narrow the claim and severity.** Entries are retained in a vector after `discard_entry`, and lookups/iterations are linear (`engine/runtime/src/streaming_executor.cpp:142-179,214-253,300-307`). However the intended `ResidencyManager` grid is finite and world dimensions may bound unique coordinates. Say “historical records are not compacted; cost scales with total distinct coordinates admitted,” and use MEDIUM absent evidence of unbounded coordinates or target world scale. |
| PERF-002 profiler absence is a high-severity runtime failure | **Claim verified; severity likely MEDIUM.** Search and call-site inspection show no frame-profile sampling path in the client; the snapshot is passed to scene construction but not populated (`client/src/client_frame_pipeline.cpp:466`; `client/include/ahamkara/client/debug_frontend_runtime.h:28-34`). This weakens a diagnostic panel but does not directly break gameplay. |
| SEC-001 is unconditionally a blocker | **Threat-model dependent.** The module is native in-process code (`engine/runtime/include/ae/runtime/game_module.h:35-48`), but the mission does not specify whether generated code is adversarial or runs automatically on a host with sensitive credentials. Keep it high/blocker only for unattended execution of untrusted generated code; otherwise document the trusted-code assumption and sandbox gap. |

### RED-001 — Recon’s dirty-tree baseline is numerically unreconciled

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** Recon says `git status --porcelain=v1` returned 182 entries, then states 74 tracked plus 118 untracked (`audit/01-recon.md:12`). Those components sum to 192. Current review-time status is 74 tracked changes and 109 untracked entries, including 11 audit reports, for 183 total. The current count cannot establish what changed after Recon because no initial per-path status output is preserved.
- **Impact:** Exact claims about which files were preexisting, changed by an experiment, or altered during the audit cannot be verified from the recorded count. This weakens blame attribution for dirty-tree compile/test failures and the mission’s “nothing outside `/audit/` changed” self-check.
- **Recommended fix:** In the master report, flag the baseline count as disputed and use Recon’s categorical limitation rather than its exact total. Before future audits, save a timestamped `git status --porcelain=v1` path list and compare it at completion; do not reset or clean this checkout to manufacture a baseline.
- **Effort:** S
- **Dependencies:** none

### RED-002 — Live experiments establish a module workaround, not the requested visual game loop

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** The scratch `main.cpp` implements `Enemy::tick`, `SquareGame::tick`, a custom score/win boolean, and stdout JSON; it uses `RuntimeMode::Tests` and has no rendering call (`/private/tmp/ahamkara-ai-audit/minigame/main.cpp`). Its output is `{"enemy_x":1.5,"score":10,"won":true}` (`/private/tmp/ahamkara-ai-audit/minigame/run.out`). The PPM source/output hashes match, but the audit says it was not rendered (`audit/06-ai-ergonomics.md:14,170-190`). The separate GUI/MCP launch timed out with no ready/state output (`audit/06-ai-ergonomics.md:15,185-190`).
- **Impact:** The experiment transcript is valuable evidence of CMake friction and of what hand-written module code can do, but it does not demonstrate “create a moving square,” “add/display a sprite,” or engine-provided state capture. A synthesized claim that the agent built a complete playable game would be false.
- **Recommended fix:** In the master experiment table, mark 1 as `PARTIAL` (lifecycle module compiled and ticked, no visible square), 2 `PARTIAL` (plain C++ behavior only), 3 `PARTIAL` (pass-through import only), 4 `PARTIAL` (custom fields/logic only), and 5 `PARTIAL/FAILED` (manual state JSON; no screenshot or engine dump). Preserve all failed setup steps and explain that no exact standalone time/file count was captured for experiment 1, while 2 and 4 shared a build/time measurement.
- **Effort:** S
- **Dependencies:** none

### RED-003 — The MCP capture attempt is an inconclusive integration test, not a confirmed capture defect

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** `GameMcpServer.capture_frame()` writes `<root>/capture.request.tmp` but does not create the root and then waits for `frame.ppm` (`tools/game_mcp_server.py:89-102`). `GameMcpBridge::start()` creates the commands directory under the root (`client/src/game_mcp_bridge.cpp:118-134`); `GameMcpServer.launch()` starts the executable but the experiment did not demonstrate a ready bridge (`tools/game_mcp_server.py:104-135`; `audit/06-ai-ergonomics.md:15,188`).
- **Impact:** Calling capture on a fresh, unlaunched session predictably fails; that precondition is not a test of the intended launched-game flow. The direct launch timeout still leaves the end-to-end flow unverified, but it does not isolate whether capture code, the client binary, GUI environment, or dirty build caused the lack of a frame.
- **Recommended fix:** Keep the result as an environment-limited failure. A future experiment should invoke `game_launch`, await `ready.json`/state, then request capture; report each stage separately and retain stdout/stderr rather than discarding launcher diagnostics.
- **Effort:** S
- **Dependencies:** none

### RED-004 — Streaming scaling severity exceeds the evidence about world bounds

- **Severity:** MEDIUM
- **Category:** performance
- **Evidence:** The executor retains `None` entries and linearly scans `regions_` (`engine/runtime/src/streaming_executor.cpp:142-179,214-253,300-307`). `ResidencyManager` allocates a finite `cols × rows` grid and clamps player region coordinates to that grid (`engine/core/include/ae/core/residency_manager.h:68-75,121-139`). No measured world size, churn rate, or retained-memory result appears in the performance report.
- **Impact:** The algorithm has an evident scaling ceiling, but “grows with every unique region” does not prove an unbounded leak in the documented finite-grid composition. Calling it HIGH without a target map size or measurement may distort prioritization.
- **Recommended fix:** Retain as a medium scalability issue and state the finite-grid bound. Raise severity only if valid transitions can contain arbitrary coordinates or product targets exceed the vector scan’s measured scale. Add a representative churn benchmark before describing user-visible slowdown as observed.
- **Effort:** M
- **Dependencies:** none

### RED-005 — Overlapping findings should be grouped by root cause in the master report

- **Severity:** LOW
- **Category:** tech debt
- **Evidence:** The lifecycle-only host-service gap is separately described in `ARCH-002`, `AIER-001/002/003/009`, and `DX-002/003` (`audit/02-architecture.md`, `audit/06-ai-ergonomics.md`, `audit/10-dx-docs.md`). Replay/state verification and visual proof overlap across `ARCH-008`, `AIER-005/007/008`, `REND-006`, `QA-001/002`, and `GDES-001` (`audit/02-architecture.md`, `audit/03-rendering.md`, `audit/06-ai-ergonomics.md`, `audit/07-qa-testing.md`, `audit/09-game-design.md`). Generic game save/load overlaps `ARCH-007` and `GAME-008` (`audit/02-architecture.md`, `audit/04-gameplay.md`).
- **Impact:** Counting every specialist ID as an independent top-level problem inflates the apparent backlog and risks assigning separate work to the same root cause. It can also bury the highest-leverage API/replay gap under repeated symptoms.
- **Recommended fix:** Keep specialist IDs as cross-references but synthesize one root issue per workstream: (1) supported authoring/runtime contract and consumer path; (2) deterministic headless action/observation/replay; (3) visual capture and screenshot verification; (4) general persistence. Preserve security threat-boundary consequences and renderer/gameplay defects as separate sub-findings where fixes differ.
- **Effort:** S
- **Dependencies:** none

## 5. What's Good

- Reports consistently qualify findings as working-tree observations and avoid attributing the dirty headless failures to clean `b705d74` (`audit/01-recon.md:30-33`; `audit/03-rendering.md:8-16`; `audit/04-gameplay.md:7-11`).
- The AI-ergonomics follow-up corrects its initial scratch build interpretation: missing includes were fixed by explicit include directories, so the failure demonstrates a non-self-describing consumer setup rather than an impossible runtime build (`audit/06-ai-ergonomics.md:192-194`).
- The experiment addendum honestly distinguishes copied assets from rendered assets and hand-authored JSON from an engine state dump (`audit/06-ai-ergonomics.md:14,185-190`).
- Security states the limitations of its scan and leaves dependency CVE status unverified instead of claiming a clean bill of health (`audit/11-security.md:8-12,79-87`).
- Rendering explicitly notes that the screenshot path is dirty-tree-only and that no GUI output was validated (`audit/03-rendering.md:8-16`).
- QA and design reports distinguish automated/source-level coverage from visual quality, responsiveness, play feel, and actual fun, which were not observed (`audit/07-qa-testing.md:8-22`; `audit/09-game-design.md:7-20`).

## 6. Open Questions for the Owner

- Will AI-generated game code be compiled and run unattended on a host that has access to credentials or private files? This decides whether native-code execution is an immediate security blocker or a trusted-local-development limitation.
- What is the largest target streamed world and expected number of distinct region coordinates? This sets the priority for replacing the retained linear registry.
- Should visual screenshots and deterministic replay be mandatory CI gates, or are they owner-reviewed milestone artifacts? The mission suggests self-verification should be automatic, but the owner has not specified render targets/platforms.
- Which audit baseline should govern follow-up work: the dirty `dev/flashback-menu-shell` state, clean `b705d74`, or the three commits ahead on `origin/main`? Recon correctly did not choose one.

## 7. Handoff Notes for the Next AI Agent

- Read `audit/01-recon.md`, but do not repeat its exact dirty-entry arithmetic; the reported total and components disagree. Preserve the checkout and mark baseline attribution uncertain.
- In the master experiment results, never describe the scratch output as a rendered game. It is a manually implemented `IGameModule` state machine that compiled only after manual CMake module/include setup.
- Treat screenshot capture as unverified in the attempted environment, not as a confirmed implementation defect. The request was tested without a confirmed running bridge.
- Group duplicate specialist findings while retaining their original IDs as references. Keep distinct fixes separate when root cause differs (for example, screenshot CI is distinct from a render shader bug).
- Do not convert unmeasured performance risks into claims of observed frame-time regressions. The current source supports algorithmic/hot-path concerns, not runtime timing conclusions.
- The audit-only requirement is not proven by the current Git status because the initial path-level snapshot is missing. Preserve every non-audit dirty path and report any inability to attribute changes rather than cleaning or reverting it.
