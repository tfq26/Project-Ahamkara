# 07 — QA, Testing & Verification Engineer

Audit date: 2026-10-16 (America/Chicago)
Repository: `/Users/taufeeqali/Projects/Ahamkara`
Observed HEAD: `b705d74` (`feat: add initial weapon and enemy assets (#71)`)
Observed branch: `dev/flashback-menu-shell`

## 1. Scope & Method

Reviewed the test target inventory and CTest registration, Linux and Windows CI workflows, CMake headless preset, selected gameplay/replay tests, the deterministic playtest harness, and the product-slice QA criteria. This is a source/configuration audit only. No tests or builds were run for this report. The checkout was already heavily dirty when reconnaissance was performed; the conclusions below describe the observed working tree and should not attribute its failures to clean HEAD. The recon report already records the dirty-tree validation outcomes and the headless UI-test link failure; this report does not repeat those as new findings.

## 2. Executive Summary

- **QA rating: 3/5 — useful automated coverage and CI breadth, with meaningful gaps in visual verification and test portability.**
- The native suite contains focused tests spanning engine/game/network/session/assets, with 71 CTest cases discovered during the recon configure; unit tests cover fixed-step progression and network prediction replay determinism (`tests/src/local_play_tests.cpp:21-64`; `tests/src/world_tests.cpp:387-427`).
- A deterministic playtest harness simulates input and checks movement, interaction, fire, reload, and ability outcomes (`client/src/playtest_harness.cpp:147-224`). The committed test currently exercises only the default scenario and asserts three outcomes (`tests/src/playtest_harness_tests.cpp:8-22`).
- CI builds Debug, Release, and headless configurations on Linux and Windows; the full CTest suite runs on Linux Debug and headless, and Windows Debug and headless (`.github/workflows/ci.yml:68-98`; `.github/workflows/ci-windows.yml:35-87`).
- The headless preset is explicitly intended for remote agents without GLFW/OpenGL client dependencies (`CMakePresets.json:31-41`), and headless animation tests are part of CTest (`tests/CMakeLists.txt:643-652`).
- Render unit tests exist for level rendering, LOD batching, IBL, SSAO, and color grading (`tests/CMakeLists.txt:345-405`), but CI provides no screenshot capture, golden-image comparison, or visual artifact upload in the reviewed test jobs (`.github/workflows/ci.yml:88-107`).
- Network test behavior depends on loopback sockets and fixed ports; the smoke test asserts binds on ports 18901 and 18902 (`tests/src/network_smoke_tests.cpp:342-347`). The Nakama fixture binds loopback on an ephemeral port (`tests/src/nakama_bridge_tests.cpp:74-94`).
- There is no macOS runner in the reviewed CI workflows, despite the current audit host being macOS; platform behavior is therefore exercised in Linux and Windows CI, not continuously on macOS (`.github/workflows/ci.yml:26-30`; `.github/workflows/ci-windows.yml:40-48`).
- Screenshots and a fixed traversal replay are specified as a manual milestone check in the vertical-slice definition of done, but I found no corresponding automated capture/diff step in the reviewed CI configuration (`docs/design/caligo-surface-ruins-vertical-slice.md:101-113`).

## 3. Inventory

| Area | Location | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| C++ unit/integration tests | `tests/src`, `tests/CMakeLists.txt` | IMPLEMENTED | 3 | Broad and focused target list; currently mixes pure tests with socket and packaging/build-graph integration checks. |
| Headless validation | `CMakePresets.json`, `tests/CMakeLists.txt`, CI | IMPLEMENTED | 3 | Headless configure excludes client/samples; tests cover simulation, animation, streaming and runtime boundaries. Recon identifies an existing dirty-tree target/link mismatch. |
| Gameplay playtest automation | `client/src/playtest_harness.cpp`, `tests/src/playtest_harness_tests.cpp` | PARTIAL | 3 | Reusable scenario/input model with outcome checks; committed test only exercises one default scenario and no rendered output. |
| Replay/determinism checks | `tests/src/world_tests.cpp`, `tests/src/network_boundary_validation_tests.cpp`, `tests/src/rewind_tests.cpp` | PARTIAL | 3 | Tests compare selected state fields after replay/reconciliation; not a versioned full-world replay or state-diff artifact workflow. |
| Rendering verification | `tests/src/*render*`, `tests/src/{lod_batching,ibl,ssao,color_grading}_tests.cpp` | PARTIAL | 2 | Unit/configuration coverage exists; no visual baseline capture/comparison in reviewed CI. |
| CI matrix | `.github/workflows/ci.yml`, `.github/workflows/ci-windows.yml` | IMPLEMENTED | 3 | Linux self-hosted and Windows hosted; macOS not represented; test runs use Debug and headless builds. |
| Failure and environment handling | `tests/src/network_smoke_tests.cpp`, `tests/src/nakama_bridge_tests.cpp` | PARTIAL | 2 | Loopback dependent tests can be blocked by sandbox policy; first test uses fixed ports. |

## 4. Detailed Findings

### QA-001 — Visual milestone checks are not automated in CI

- **Severity:** MEDIUM
- **Category:** quality gate
- **Evidence:** The product-slice definition of done calls for first-person screenshots and a fixed traversal replay before and after material or collision milestones (`docs/design/caligo-surface-ruins-vertical-slice.md:101-113`). Render-focused CTest targets check level render, LOD batching, IBL, SSAO, and color-grading behavior (`tests/CMakeLists.txt:345-405`). The Linux CI test steps run CTest and upload benchmark JSON only (`.github/workflows/ci.yml:88-107`); there is no capture, image-diff, or screenshot artifact step in those jobs.
- **Impact:** Regressions in actual rendered appearance, camera composition, lighting/material authoring, collision traversal feel, or HUD presentation can pass the current automated native suite. The manual acceptance criterion depends on a person or a display-equipped machine remembering to perform the check.
- **Recommended fix:** Add a stable display-capable capture job with a fixed scenario/replay, capture at named checkpoints, and compare against reviewed baselines with documented tolerance. Retain the existing data-level renderer tests as complementary checks.
- **Effort:** M
- **Dependencies:** A stable renderer/driver configuration and an agreed baseline review/update process.

### QA-002 — Playtest smoke coverage is narrow relative to the harness contract

- **Severity:** MEDIUM
- **Category:** test coverage
- **Evidence:** `PlaytestScenario` supports levels, target setup, duration, movement/interactions/fire/reload/ability requirements (`client/include/ahamkara/client/playtest_harness.h:28-44`). `run_playtest_scenario` checks each configured condition and timeout (`client/src/playtest_harness.cpp:147-225`). The current test creates the default scenario, clears its level path, runs it, and asserts only pass, interaction count, and ammo spent (`tests/src/playtest_harness_tests.cpp:8-22`).
- **Impact:** The harness has a useful extension point, but CI does not currently exercise scenario failure paths such as level-load failure, timeout, disabled requirements, reload/ability checks, or varied level-specific traversal. The playtest green signal therefore represents one small synthetic path, not broad gameplay acceptance.
- **Recommended fix:** Add a small scenario matrix with positive and negative assertions for each contract field, plus at least one actual checked-in level traversal and a serialized result artifact that can be inspected in CI.
- **Effort:** M
- **Dependencies:** Stable level fixture assets and deterministic simulation conditions.

### QA-003 — Socket tests couple the full CTest run to loopback and port availability

- **Severity:** MEDIUM
- **Category:** test portability
- **Evidence:** `test_simulator_disabled_is_passthrough` hardcodes ports 18901 and 18902 and asserts both sockets open (`tests/src/network_smoke_tests.cpp:342-347`). The Nakama fake server correctly requests an ephemeral port, but asserts socket creation, bind, and listen success (`tests/src/nakama_bridge_tests.cpp:74-94`). The CI workflows invoke the complete CTest suite without an environment-specific label/filter (`.github/workflows/ci.yml:88-98`; `.github/workflows/ci-windows.yml:79-87`).
- **Impact:** Restricted containers, concurrent local runs, or machines with loopback/socket policy differences can fail the suite before the networking behavior under test is reached. Recon observed socket bind denial in its sandbox run; that environment-specific failure is documented there and is not reclassified here as a product bug.
- **Recommended fix:** Use ephemeral ports consistently, ensure tests clean up sockets with RAII on assertion failures, and label tests that require loopback so CI and constrained local environments can report them separately while normal CI still runs them.
- **Effort:** S
- **Dependencies:** None.

### QA-004 — No continuous macOS test lane is configured

- **Severity:** MEDIUM
- **Category:** platform coverage
- **Evidence:** The Linux native workflow uses `[self-hosted, linux]` runners (`.github/workflows/ci.yml:26-30,68-76`); the second workflow uses `windows-latest` (`.github/workflows/ci-windows.yml:40-48`). Neither reviewed workflow configures a macOS runner. Platform-specific code exists for Apple, Linux, and Windows in memory tracking (`engine/core/src/memory_budget.cpp:4-15,29-69`) and the crash-handler implementation is POSIX-only with Windows stubs (`engine/core/src/crash_handler.cpp:15-20,193-223`).
- **Impact:** macOS-specific compilation and runtime behavior can regress without an automated signal. This matters especially for the Apple memory sampling branch and POSIX crash handler behavior.
- **Recommended fix:** Add a macOS headless configure/build/test job, and add a lightweight GUI smoke/capture job if a stable runner/display route is available.
- **Effort:** M
- **Dependencies:** macOS runner capacity and dependency provisioning.

## 5. What's Good

- The test suite has substantial subsystem breadth, including runtime boundaries, error identity, failure paths, network validation, rewind, streaming, animation, and package consumer checks (`tests/CMakeLists.txt:605-878`).
- C++ movement and prediction tests use controlled input and compare deterministic state values, including replayed input paths (`tests/src/local_play_tests.cpp:21-64`; `tests/src/world_tests.cpp:387-460`).
- The playtest harness separates scenario inputs from simulation and returns structured outcome fields and a summary string (`client/include/ahamkara/client/playtest_harness.h:12-55`; `client/src/playtest_harness.cpp:147-248`).
- CI runs both full GUI-enabled and headless CTest configurations on Linux and Windows; Windows also validates out-of-tree package consumers (`.github/workflows/ci-windows.yml:147-193`).
- Benchmark test labels allow CI to run performance tests as a distinct verbose lane (`tests/CMakeLists.txt:795-815`; `.github/workflows/ci.yml:96-107`).

## 6. Open Questions

- Is the vertical-slice screenshot/traversal criterion intended to be a release gate or an informal milestone checklist? The design doc states the criterion but no process owner or artifact retention policy is identified.
- Should macOS be a supported developer/ship platform? The source includes Apple-specific branches, but CI currently has no macOS job.
- Which gameplay levels and scenarios should be the canonical automated smoke paths beyond the current synthetic default autoplay?

## 7. Handoff Notes

- Keep recon’s dirty-tree limitation attached to any validation claims. Do not attribute the existing headless link or asset failures to clean HEAD without an identified baseline.
- Treat screenshots and playback as a product acceptance gap even though many rendering data-path unit tests exist; those test categories answer different questions.
- Prioritize portable loopback tests and a compact real-level playtest before expanding the total test count.
- No test/build commands were run for this report, per assignment.
