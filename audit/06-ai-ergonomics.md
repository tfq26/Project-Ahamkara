# 06 — AI-Ergonomics & Agent-Interface

Audit date: 2026-10-16 (America/Chicago)
Repository baseline: dirty working tree described in `audit/01-recon.md`; observed HEAD `b705d74`.
Overall AI-ergonomics rating: **2/5 — rudimentary and mission-critical gaps remain.**

## 1. Scope & Method

Read `audit/01-recon.md` first. Inspected the root CMake configuration, runtime `IGameModule` API, Flashback module, game-MCP server and native bridge, asset pipeline guide/importer, debug frame pipeline, and the existing debug build configuration. Performed live attempts in `/private/tmp/ahamkara-ai-audit/` only; no repository source, config, dependency, or generated asset files were changed. Existing repo binaries were invoked with a temp working directory so logs/session outputs could not land in the checkout.

Live commands and outcomes:

- Wrote a minimal `CMakeLists.txt` and `main.cpp` in `/private/tmp/ahamkara-ai-audit/minigame/` implementing `ae::IGameModule` and an `Application`. Initial configure failed because the project’s `InstallRules.cmake` could not find `GenerateProductManifest`; elapsed 2.31 s. Retrying with `-DCMAKE_MODULE_PATH=/Users/taufeeqali/Projects/Ahamkara/cmake` configured successfully in 0.05 s. `cmake --build ... --target minigame -j4` failed in 2.9 s because the `ae_runtime` build lacked `ae/core/...` include paths (`fatal error: 'ae/core/error_types.h' file not found`, and equivalent `math.h`/`log.h`). This was a scratch consumer build against the existing dirty tree and cached dependencies, not a project test or clean baseline.
- Created a 1×1 PPM sprite plus a manifest under `/private/tmp/ahamkara-ai-audit/assets/` and ran `/Users/taufeeqali/Projects/Ahamkara/build/debug/tools/ahamkara_asset_importer --manifest /private/tmp/ahamkara-ai-audit/manifest.assets`. It exited 0, copied the sprite byte-for-byte, wrote a registry and reported `Imported: 1, skipped: 0, failed: 0` in under 0.001 s. The sprite was not rendered by the game.
- Tried launching the existing MCP-enabled debug client from `/private/tmp/ahamkara-ai-audit` with `AHAMKARA_GAME_MCP_ENABLED=1`, a temp session root/token, and `--mcp`. It stayed alive until an 8 s subprocess timeout, emitted macOS LaunchServices/XPC “Connection invalid” diagnostics, and created no session directory or `ready.json` / `state.json`. This did not establish whether the cause is GUI/display startup, the dirty build, or another environment issue.
- Not attempted: full rebuild of the game-MCP target, install of the optional MCP SDK, or GUI interaction. Recon already established network fetch failures and a large dirty tree; launch attempt did not reach bridge readiness. The existing binary/cache state is not a clean reproducible build.

Scratch files touched were confined to `/private/tmp/ahamkara-ai-audit/`: `minigame/CMakeLists.txt`, `minigame/main.cpp`, generated `minigame/build/`, `assets/source/sprite.ppm`, `assets/compiled/sprite.ppm`, `manifest.assets`, and importer outputs `compiled/asset_registry.tsv` and `compiled/assets.pkg`. MCP launch was attempted with temp session path but produced no files.

## 2. Executive Summary

- The repo now has a development-only local MCP feedback slice: bounded player-input commands, a JSON snapshot, optional framebuffer PPM capture, and a wrapper exposing `game_observe`, `game_step`, `game_launch`, `game_stop`, `game_capture_frame`, and `game_analyze_frame` (`tools/game_mcp_server.py`; `client/src/game_mcp_bridge.cpp`). This is real agent-facing infrastructure, but it automates play of the existing client/game rather than authoring arbitrary games.
- There is no runtime API that enumerates capabilities or exposes a machine-readable general engine schema; MCP tool definitions describe a handful of Flashback/player controls only.
- The public `IGameModule` contract is tiny and typed, but only provides lifecycle and a per-frame callback. It has no access to renderer, input, scene, asset registry, entity creation, UI, or audio (`engine/runtime/include/ae/runtime/game_module.h`).
- A scratch `IGameModule` consumer could be configured only after manually adding `CMAKE_MODULE_PATH`; its build failed on missing public core include paths. The smallest runnable visible game task therefore has no discoverable, proven golden path.
- Asset authoring is text-manifest based and repeatable for current supported formats. Sprite import is only pass-through copy, with no atlas/metadata validation and no verified runtime sprite-render path (`docs/systems/asset_pipeline.md`; importer experiment).
- Closed-loop observation/action is partial: state JSON and capture exist in implementation, but no deterministic fixed-step game-control API, reset/replay, arbitrary entity mutation, or correlated action-result response is exposed. The docs explicitly call reset/replay a later slice (`tools/game_mcp_README.md`).
- UI/HUD hot reload exists in the client frame pipeline, but this does not hot reload game code, scene logic, or general assets (`client/src/client_frame_pipeline.cpp`, `stage_*` path around `poll_hot_reload`).
- Context efficiency is poor for this mission: root `AGENTS.md` has unresolved placeholders and its claimed Tier 2 tables are absent, per recon. The useful asset pipeline guide does not cover “write a game and verify it” end to end.

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| General runtime/game API | `engine/runtime/include/ae/runtime/game_module.h`, `application.h` | PARTIAL | 2 | Stable-looking interface version, but lifecycle-only and no game-facing services. |
| Runtime capability discovery | `engine/runtime`, `tools` | MISSING | 1 | No general capability/schema enumeration API found. |
| Agent MCP protocol | `tools/game_mcp_server.py`, `client/src/game_mcp_bridge.cpp` | PARTIAL | 3 | Local token-gated command files, bounded input, observations and frame capture. Specific to existing client. |
| MCP discoverability/docs | `tools/game_mcp_README.md` | PARTIAL | 3 | Setup is documented, but model/API dependency, build and visible game workflow are incomplete; docs state replay is later. |
| Game authoring model | `game`, `samples/flashback`, `.lvl`, JSON specs | PARTIAL | 2 | C++ product-specific modules and text level specs; no minimal general sample or compact game DSL. |
| Asset import | `tools/asset_importer*`, `assets/manifest.assets`, `docs/systems/asset_pipeline.md` | PARTIAL | 3 | Manifest importer exists. Models, TGA textures, materials, levels compile; sprite/audio/data mostly copy through. |
| Runtime sprite authoring/display | `engine/render`, `client`, `assets` | UNKNOWN | 1 | Sprite import is documented, but no clear standalone sprite component/display API surfaced in searched docs/interfaces. Not proven absent from every dirty path. |
| Actionable diagnostics | `engine/core`, logs, importer | PARTIAL | 3 | Importer errors/logs and broad diagnostics exist; MCP invalid command files are removed silently. |
| Headless runtime | CMake headless preset; `client/src/headless_clients.cpp` | PARTIAL | 2 | Headless clients/network simulations exist. No general headless game authoring/step-and-observe harness. |
| Deterministic game replay | `game/src/client_prediction.cpp`, tests, MCP docs | PARTIAL | 2 | Network prediction rollback replays inputs internally; agent-level scenario reset/input-history export/replay is explicitly absent. |
| Hot reload | `client/src/client_frame_pipeline.cpp` | PARTIAL | 2 | Menu/HUD resources hot reload; game code/assets not established. |
| Game self-verification / bot | `tools/game_qa_model.py`, MCP tool | PARTIAL | 2 | Vision QA can inspect a captured frame if configured; no integrity gate, win-condition validator, or general bot/completion solver. |
| Agent guide / index | `AGENTS.md`, `docs/AGENTS.md`, `docs/guides`, `docs/systems` | PARTIAL | 1 | Several detailed subsystem docs, but unresolved root placeholders and no AI authoring golden path. |

## 4. Detailed Findings

### AIER-001 — Agent interface controls one existing game rather than exposing an engine authoring API

- **Severity:** BLOCKER
- **Category:** missing feature
- **Evidence:** `tools/game_mcp_server.py:create_mcp_server()` registers six fixed tools. `game_step` maps to movement/look and fixed buttons; `GameMcpBridge::read_command_file()` parses a fixed `PlayerInputCommand`. `publish_snapshot()` serializes player, weapon, match, dummies, projectiles, and a few events. The interface offers no project creation, entity/component schemas, scene editing, asset registration, or general game APIs. `IGameModule` in `engine/runtime/include/ae/runtime/game_module.h` only defines `name`, API version, `initialize`, `tick`, and `shutdown`.
- **Impact:** The stated product goal is an agent that creates complete games using engine APIs. The current MCP is a useful QA controller for a specific shooter but cannot create or edit arbitrary game content. The most important unique product capability is therefore not implemented at this interface.
- **Recommended fix:** Define a versioned, machine-readable authoring contract: project/game manifest, supported runtime capabilities, entity/component schema, input/action schema, asset registry, and scene/level mutation API. Publish these as MCP resources/tools plus stable on-disk formats, with one small reference game using only that contract.
- **Effort:** XL
- **Dependencies:** AIER-002, AIER-003, AIER-004

### AIER-002 — The core game module contract has no usable renderer/input/scene services

- **Severity:** HIGH
- **Category:** design flaw
- **Evidence:** `engine/runtime/include/ae/runtime/game_module.h` gives `GameModuleHostServices` only `RuntimeMode mode` and opaque `void* user_data`; `GameModuleFrameContext` only gives time, dt, frame index. `IGameModule::tick()` has no input/context result channel. Flashback’s module (`samples/flashback/game_module/src/game_module.cpp`) is effectively a lifecycle shell; its callbacks are empty. Rendering and gameplay are assembled elsewhere in the client/product code.
- **Impact:** A cold agent can implement a type satisfying the interface but cannot use it to move or draw a square. It must reverse-engineer and couple into other product layers. The module abstraction implies pluggability without offering service contracts.
- **Recommended fix:** Add explicit, versioned host services for input, entity/scene lifecycle, rendering submission, asset access, audio, and diagnostics, or document and stabilize a different supported extension model. Provide a compile-and-run minimal game sample independent of Flashback.
- **Effort:** L
- **Dependencies:** AIER-001

### AIER-003 — The minimum consumer build path is not demonstrated and scratch build failed

- **Severity:** HIGH
- **Category:** tech debt
- **Evidence:** Scratch consumer under `/private/tmp/ahamkara-ai-audit/minigame/` was written against `IGameModule` and `ae_runtime`. First configure failed at `cmake/InstallRules.cmake` because `GenerateProductManifest` was not found. Retrying with `-DCMAKE_MODULE_PATH=/Users/taufeeqali/Projects/Ahamkara/cmake` configured successfully. The build then failed compiling `ae_runtime` with missing `ae/core/error_types.h`, `ae/core/math.h`, and `ae/core/log.h`. The existing repo root `CMakeLists.txt` adds subdirectories, while root CMake configures `CMAKE_MODULE_PATH` late (after `InstallRules.cmake` inclusion); the include-path failure is observed in the scratch consumer graph and should be rechecked on a clean baseline before assigning root cause.
- **Impact:** There is no verified, short, clean starter workflow for an external game. An agent has to solve build graph/package integration before implementing gameplay; failure messages identify headers, but not the consumer setup fix.
- **Recommended fix:** Add a minimal example target that is part of supported builds and a documented out-of-tree consumer test. Export/install targets with transitive include requirements and correct module path ordering. Verify a fresh configure/build/run from a temp directory in CI.
- **Effort:** M
- **Dependencies:** AIER-002

### AIER-004 — MCP action calls do not confirm which simulation tick applied the action

- **Severity:** MEDIUM
- **Category:** design flaw
- **Evidence:** `GameMcpServer.send_action()` atomically writes a `.cmd` file and immediately returns `self.observe()` (`tools/game_mcp_server.py`). The client consumes commands later in `GameMcpBridge::poll_input()` and publishes snapshots separately. There is no command ID/result acknowledgment or wait-until-frame-applied behavior. `publish_snapshot()` includes a frame number but no last-applied command sequence.
- **Impact:** An agent can receive a stale snapshot and mistakenly conclude that an action had no effect, or associate the previous observation with the new action. This undermines reliable closed-loop iteration and makes flakiness difficult to diagnose.
- **Recommended fix:** Include command IDs and accepted/rejected results, publish `last_applied_command_id` and frame/tick, and let `game_step` wait with a timeout for a resulting observation. Return structured parse/authorization errors rather than silently discarding invalid command files.
- **Effort:** S
- **Dependencies:** none

### AIER-005 — No agent-level deterministic reset/replay despite internal network rollback

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** `tools/game_mcp_README.md` says “A later slice will add deterministic scenario reset/replay support.” Existing `game_step` only sends bounded input durations (1–600 ticks) and the bridge increments a transient sequence. Game/network code has prediction reconciliation and input replay, but MCP exposes no reset seed, scenario save/restore, recorded input export, replay command, or fixed-step switch.
- **Impact:** Agents cannot reproduce a behavior bug reliably across iterations or perform deterministic state-diff playtests. Internal multiplayer rollback is not a user-facing replay facility.
- **Recommended fix:** Add seeded scenario initialization, fixed tick stepping independent of wall time, input recording with command/result frames, reset/load, and a replay verifier that compares structured state hashes.
- **Effort:** L
- **Dependencies:** AIER-001, AIER-004

### AIER-006 — Sprite import succeeds as a copy but does not complete the AI asset workflow

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** `docs/systems/asset_pipeline.md` says sprite assets are passthrough copies, with optional metadata copied beside output; sprite-sheet packing and metadata validation are future work. Live temp experiment imported a 1×1 PPM with success and byte-identical output, but no runtime sprite lookup/display path was used or established. The manifest registry is an index, while the same guide says runtime registry resolution is a follow-up for level mesh IDs.
- **Impact:** An agent can register a file, but cannot infer how to convert that registration into a visible, validated sprite in a game. Asset presence is not asset usability; silent dimensions/metadata assumptions can survive import.
- **Recommended fix:** Define a minimal sprite metadata schema (dimensions, frame rectangles, origin, filtering/scale), validate it on import, support common generated formats, and expose a runtime lookup/render API with a documented example and image capture assertion.
- **Effort:** M
- **Dependencies:** AIER-001, AIER-002

### AIER-007 — Live visual verification was not reachable in the attempted environment

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** The existing debug CMake cache has `AHAMKARA_ENABLE_GAME_MCP:BOOL=ON`, but launching its existing `ahamkara_client --mcp` from a temp working directory with explicit bridge env remained alive until an 8 s timeout and emitted LaunchServices/XPC connection-invalid diagnostics; no `ready.json` or `state.json` appeared. `tools/game_mcp_README.md` describes frame capture via request and `frame.ppm`. This is a failed launch attempt, not proof the bridge fails in normal GUI execution.
- **Impact:** The key agent feedback loop is not independently demonstrated in this audit; screenshots/state cannot be used as evidence that a new generic game works. Existing recon also says no GUI demo was launched and build/test evidence is limited by the environment and dirty tree.
- **Recommended fix:** Add a display-independent MCP/headless runner that can deterministically step a sample scene and emit state/framebuffer artifacts. CI should invoke it from a clean build, assert the bridge ready state, issue an action, check acknowledgment/state movement, and validate captured output.
- **Effort:** M
- **Dependencies:** AIER-001, AIER-005

### AIER-008 — Hot reload and automated QA cover only narrow product slices

- **Severity:** MEDIUM
- **Category:** missing feature
- **Evidence:** `client/src/client_frame_pipeline.cpp` polls hot reload for menu/HUD systems (`menu_system_.poll_hot_reload()`, `hud_system_.poll_hot_reload()`). The MCP wrapper adds frame capture and optional `game_analyze_frame`, and `tools/game_qa_model.py` sends a captured image and state to a configured vision model. No general game-code reload, content reload contract, boot-N-frames integrity command, win/loss assertion, or solver/bot is exposed in the MCP tool definitions. `tools/game_mcp_README.md` requires external `GAME_QA_MODEL` setup and describes model analysis separately from local observation.
- **Impact:** Iteration and self-verification depend on manually managing process/build state and potentially external model setup. A screenshot analyzer does not establish that the game boots, is winnable, or has correct score rules.
- **Recommended fix:** Expose a deterministic `validate_game`/playtest interface with startup diagnostics, bounded simulation, assertions against declared goals, structured failure output, and artifact paths. Support resource hot reload for authorable data/assets and explicitly document code reload as unsupported if it remains so.
- **Effort:** L
- **Dependencies:** AIER-001, AIER-005, AIER-007

### AIER-009 — Agent onboarding is fragmented and root guide is currently misleading

- **Severity:** HIGH
- **Category:** docs
- **Evidence:** `audit/01-recon.md` documents literal `{{STACK_SUMMARY}}`, `{{TEST_CMD}}`, `{{LINT_CMD}}`, `{{DO_NOT_1}}`, and `{{DO_NOT_2}}` placeholders in root `AGENTS.md` and a reference to missing Tier 2 tables. The asset guide is detailed about formats/paths but not a minimal game, entity behavior, verification loop, or API index. MCP setup requires several environment variables, a special dev CMake flag, a separately installed optional MCP SDK, and externally configured QA model for image analysis (`tools/game_mcp_README.md`).
- **Impact:** Cold agents have to discover product ownership and build requirements across many docs/code paths, and can mistake the low-level runtime module interface for a complete game API. This spends context before implementation and increases false assumptions.
- **Recommended fix:** Ship a concise agent-facing index with a tested one-command starter and examples for add behavior, add/display asset, win/lose/score, headless run, state dump, screenshot, deterministic replay, and exact guide links. Repair root placeholders and establish a supported starting target before claiming this workflow exists.
- **Effort:** M
- **Dependencies:** AIER-001, AIER-002, AIER-003

## 5. What's Good

- The MCP bridge is development-only, explicitly opt-in, token-gated, and based on atomic local command/state files rather than a network listener (`tools/game_mcp_README.md`; `GameMcpBridge::config_from_environment`, `write_atomic`).
- Input actions have bounded duration (1–600 ticks) and the native bridge rejects malformed values and wrong tokens before applying them (`GameMcpServer.send_action`, `GameMcpBridge::read_command_file`).
- Structured observations include frame/time/FPS and useful player, weapon, match, dummy, projectile, and feedback state, which is more machine-usable than logs alone (`GameMcpBridge::publish_snapshot`).
- MCP can request a PPM framebuffer capture and optionally pass it to a vision-QA wrapper; this is a promising observe-and-analyze seam even though this launch attempt did not reach it.
- Asset manifest paths and outputs are text-based and reproducible; the isolated sprite import succeeded and generated a registry without touching project assets.
- The engine has a deliberately small module-version contract and already separates Flashback identity from the Ahamkara client runtime, which could support a clean sample-game workflow once host services are defined.

## 6. Open Questions for the Owner

- Is the intended near-term authoring interface C++ modules, data-driven scene/spec files, scripting, or a combination? **Recommended default:** text game/scene manifests for content plus C++/script extension points for custom logic.
- Is AI control meant to operate Flashback for playtesting, or to create arbitrary Ahamkara games? **Recommended default:** both, but treat the generic authoring interface and headless loop as the product’s primary differentiator.
- Which first game genre should define minimum services? **Recommended default:** a small top-down arena/puzzle game, unless owner priorities establish another genre.
- Should the general AI authoring API be available in production or dev-only? **Recommended default:** keep mutation/control tooling dev-only, with generated game artifacts and normal runtime APIs available in release builds.

## 7. Handoff Notes for the Next AI Agent

- Read `audit/01-recon.md` before relying on this report; the repo is substantially dirty and build failures must be attributed to the observed working tree, not clean HEAD.
- Do not treat the game MCP bridge as an engine authoring API. It is a bounded controller/observer for the existing player and match state.
- `IGameModule` is lifecycle-only. Inspect `engine/runtime/include/ae/runtime/game_module.h` and `samples/flashback/game_module/src/game_module.cpp` before assuming a module can draw or create entities.
- Asset import supports several useful formats, but a successful sprite pass-through does not mean runtime display/metadata validation works. Keep scratch importer manifests and outputs out of tracked `assets/`.
- `game_mcp_README.md` explicitly leaves reset/replay for later. Do not claim deterministic scenario testing based solely on client prediction rollback code.
- The scratch experiment generated only under `/private/tmp/ahamkara-ai-audit`; the repo itself was not edited by this specialist.

## Follow-up addendum — remaining live experiments (2026-10-16)

The first pass did not individually exercise all five Section 3G tasks. The following follow-up closes the entity behavior, win/lose+score, and state-dump experiments. All files and build output remained under `/private/tmp/ahamkara-ai-audit/`; no repo files outside `/audit/` were changed.

### Experiment 2 — Add a new entity type with custom behavior

- Added `struct Enemy { float x; float speed; void tick(float dt) { x += speed * dt; } }` to the scratch consumer’s `main.cpp`. `SquareGame::tick()` calls the entity behavior; its three calls through `ae::Application::tick(0.5F)` moved it from `x=0` to `x=1.5`.
- The first consumer build in the prior pass failed because the out-of-tree target did not receive the engine include directories transitively. For this follow-up, scratch `CMakeLists.txt` explicitly added `engine/core/include` and `engine/runtime/include` via `include_directories(...)`, and configured with `-DCMAKE_MODULE_PATH=/Users/taufeeqali/Projects/Ahamkara/cmake` plus the cached dependency source overrides listed in the prior experiment. `cmake -S ... -B ... && cmake --build ... --target minigame -j4` completed successfully in 4.18 s.
- Running `/private/tmp/ahamkara-ai-audit/minigame/build/minigame` completed in under 0.01 s and emitted the entity position as `1.5` in the state JSON (full output under Experiment 5).
- Friction: this is a hand-written C++ data type, not an engine entity/component registration. The `IGameModule` callback can run custom behavior, but its host services expose no entity system, input, renderer, or game-state API. The workaround depends on manually adding include directories and module path to an out-of-tree CMake consumer.
- Scratch files touched/updated: `minigame/main.cpp`, `minigame/CMakeLists.txt`; generated files under `minigame/build/`.

### Experiment 4 — Add win/lose condition and score

- In that same scratch `SquareGame::tick`, when `enemy.x >= 1.0F`, code sets `won=true` and adds 10 to `score` once. Three half-second ticks cross the threshold. This is custom game-module logic; no built-in scoring or win/loss primitive was found.
- The same configure/build/run commands above succeeded. Observed output: `{"enemy_x":1.5,"score":10,"won":true}`. The state variables are ordinary fields on the custom module; no HUD, persisted score, loss path, or assertion API was exercised.
- Time: implementation and build edits were performed in the same scratch iteration as Experiment 2; configure+build wall time 4.18 s, execution below 0.01 s. There is no useful standalone time split between the two code additions because they share one source file/build.
- Files touched: same two scratch source/build files; no repository files outside `/audit/`.

### Experiment 5 — Capture a screenshot or state dump

- **State dump succeeded, but was manually authored by the scratch game.** Ran `/private/tmp/ahamkara-ai-audit/minigame/build/minigame`, filtered its one JSON line into `/private/tmp/ahamkara-ai-audit/minigame/state.json`, and validated it with `python3 -m json.tool /private/tmp/ahamkara-ai-audit/minigame/state.json`. Exact parsed values: `enemy_x=1.5`, `score=10`, `won=true`. This verifies the custom simulation logic only; it does not demonstrate an engine-provided scene dump or renderer screenshot.
- **Built-in MCP observation/capture did not succeed.** `GameMcpServer.observe()` with a fresh temp session returned `{'ready': False, 'state': None}`. The first `capture_frame(timeout_seconds=0.25)` attempt failed with `FileNotFoundError` because `capture_frame` writes `<root>/capture.request.tmp` without first creating the root. After manually `mkdir -p /private/tmp/ahamkara-ai-audit/session`, another capture attempt returned `TimeoutError: The game did not produce a framebuffer capture`; the request file existed, but `frame.ppm` did not. The prior direct client launch also timed out after 8 s without `ready.json`.
- Commands: direct game run plus `sed -n '/^{/p' ... > .../state.json`; `python3 -m json.tool .../state.json`; and the Python `GameMcpServer` calls documented in the observed output above. State dump validation took under 0.01 s; capture waited 0.25 s after session directory creation (first failure was immediate).
- Files touched: `/private/tmp/ahamkara-ai-audit/minigame/state.json`, `run.out`, `run.err`, and `/private/tmp/ahamkara-ai-audit/session/capture.request`; no screenshot artifact was created.

### Revised interpretation of the first consumer build failure

The earlier missing-header build failure was recoverable by explicitly adding the engine core/runtime include directories globally in the scratch CMake project. Therefore it demonstrates that the consumer setup was not self-describing/transitive, not that the runtime library cannot be built at all. The consumer can compile a lifecycle-only module with this manual workaround. It still does not provide engine-managed entities, a renderer-backed square, game input, or the AI feedback APIs required by the requested complete-game workflow. The first configure’s missing `GenerateProductManifest` was likewise worked around by explicitly setting `CMAKE_MODULE_PATH`.
