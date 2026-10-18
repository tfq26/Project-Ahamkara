# 02 — Core Architecture Audit

Audit date: 2026-10-16 (America/Chicago)
Repository state: dirty working tree documented in [01-recon.md](01-recon.md); findings describe the inspected state, not a clean checkout.
Overall architecture rating: **2/5 — substantial working systems, but the product boundary and authoring/runtime contracts are not yet cohesive enough to call this an AI-native engine.**

## 1. Scope & Method

Read `audit/01-recon.md`, `docs/AGENTS.md`, current CMake composition, the architecture overview and repo map, and traced the public runtime/game-module interface, client frame orchestration, game `World`, EnTT usage, core allocation/lifecycle APIs, packet serialization, and engine/game target dependencies. Read-only source inspections used `sed`, `nl`, and `rg`; line references below are to the observed working tree. No build, test, GUI launch, or source/config change was performed for this specialist pass; the reconnaissance report records the validation attempts and dirty-state/environment limitations. The audit covers architecture/module boundaries, loop/tick model, object/ECS model, ownership/memory, event/messaging, serialization, determinism, extensibility/API stability. It does not re-audit graphics, gameplay feature quality, security, or performance measurements; those belong to other specialists.

## 2. Executive Summary

- The repository has useful subsystem libraries and a recognizable `engine/`, `game/`, `client/`, `server/`, and `wish/` layout, but the root build still composes product code into engine configurations. [src: file: CMakeLists.txt:131-195] [src: file: docs/architecture/overview.md:12-21]
- `ae::Application` supplies a small lifecycle host and versioned `IGameModule`; it currently passes only mode and an opaque `void*`, while tick is a caller-supplied variable delta. It is a stable seam in form, but not yet a useful engine service API. [src: file: engine/runtime/include/ae/runtime/game_module.h:11-48] [src: file: engine/runtime/src/application.cpp:50-101]
- Graphical client ordering is explicit and reviewable, but the pipeline is client/product-specific and depends on many concrete objects. It is not a reusable general engine loop. [src: file: client/include/ahamkara/client/client_frame_pipeline.h:49-157] [src: file: client/src/client_frame_pipeline.cpp:270-321]
- Game simulation combines player objects, fixed-capacity arrays, EnTT registry entities, Jolt physics, cached render projections, and a non-owning audio interface. These are workable for a bounded prototype, but create parallel state models and uneven extension costs. [src: file: game/include/ahamkara/game/world.h:338-382] [src: file: game/src/world.cpp:80-83]
- There is no general engine event/message bus visible in the inspected engine/game tree. Audio uses a fixed 16-item queue; one-shot VFX uses a monotonic event ID. Messaging is local and subsystem-specific. [src: file: game/include/ahamkara/game/world.h:334-336] [src: file: game/src/world.cpp:694-707] [src: file: game/include/ahamkara/game/world.h:243-248]
- Serialization is present for network packets and selected config/asset formats, but there is no general game-world save/load snapshot contract. Packet protocol versioning exists, while the packet writer copies trivially copyable field bytes directly, so portability/canonical endianness needs explicit review. [src: file: game/include/ahamkara/game/net_packets.h:14-25] [src: file: game/include/ahamkara/game/net_packets.h:73-101]
- Deterministic foundations exist—fixed 60 Hz subdivision in `World::tick`, tick counters/history, prediction and replay-related code—but the convenience tick accepts arbitrary float durations and slices a final fractional step, and determinism across machines/backends is unverified. [src: file: game/src/world.cpp:153-166] [src: file: game/src/client_prediction.cpp:1-103]
- The strongest near-term architectural move is to keep and prove the engine-only/package boundary before splitting repositories, then evolve a real host-services/game-authoring contract. The accepted repository-split doc makes this order explicit. [src: file: docs/architecture/overview.md:104-120]

## 3. Inventory

| Component | Location (paths) | Status | Quality (1-5) | Notes |
|---|---|---|---:|---|
| Root build composition | `CMakeLists.txt` | PARTIAL | 2 | Options separate targets, but game links Wish and is part of root composition; graphical engine modules are conditionally coupled to client option. |
| Core/runtime libraries | `engine/core`, `engine/runtime` | IMPLEMENTED | 3 | Lifecycle, error/result, clock, logging, config, diagnostics and memory utilities exist. |
| Game module contract | `engine/runtime/include/ae/runtime/game_module.h`, `application.*` | PARTIAL | 2 | Versioned interface and lifecycle work; services contract is only mode + opaque pointer and tick is not fixed-step. |
| Client loop | `client/src/client_frame_pipeline.cpp` | IMPLEMENTED | 3 | Named ordered stages; hard-wired to current graphical client and product presentation. |
| Server loop | `server/src/dedicated_server_main.cpp` | PARTIAL | 2 | Reconciliation describes polling/admission/simulation/snapshot work in a single executable loop; server-domain specialists should judge completeness. |
| World / simulation | `game/include/ahamkara/game/world.h`, `game/src/world.cpp` | PARTIAL | 2 | Functional simulation, but mixed ownership and parallel state representations remain. |
| ECS | EnTT in `game/` | PARTIAL | 2 | Used for dummies, projectiles, and AI combatants, while players and multiple gameplay objects remain non-ECS. |
| Physics boundary | `engine/collision`, `engine/physics`, `game/src/world_jolt_bridge.*` | PARTIAL | 3 | Engine owns Jolt-facing modules; product world bridge still composes physics and game state. |
| Event/messaging | `game/src/world.cpp`, `game/include/ahamkara/game/world.h` | PARTIAL | 2 | Bounded audio event queue and VFX event ID; no general-purpose event API found in inspected scope. |
| Serialization | `game/include/ahamkara/game/net_packets.h`, compiled assets/config | PARTIAL | 2 | Protocol serializers and selected loaders exist; no general world persistence contract. |
| Deterministic simulation/replay | `game/src/client_prediction.cpp`, `game/src/rewind.cpp`, `game/src/world.cpp` | PARTIAL | 2 | Fixed step and history/prediction mechanisms exist; cross-run/platform determinism not established. |
| ABI/API stability | `engine/core/include/ae/core/abi_version.h`, runtime module API | PARTIAL | 2 | Version constants/policies exist, but an ABI version alone does not validate package consumer compatibility or stabilize a comprehensive API. |

## 4. Detailed Findings

### ARCH-001 — Product dependencies leak across the engine/game boundary

- **Severity:** HIGH
- **Category:** design flaw
- **Evidence:** Root CMake adds game when `AHAMKARA_BUILD_GAME` is enabled and errors unless Wish is enabled; game target publicly links `wish_engine`, `Jolt`, and EnTT, and consumes render/collision/animation headers. [src: file: CMakeLists.txt:177-184] [src: file: game/CMakeLists.txt:38-65] The architecture doc itself labels this composition transitional and states Ahamkara must not name Flashback or Wish in the target boundary. [src: file: docs/architecture/overview.md:12-21] [src: file: docs/architecture/overview.md:35-57]
- **Impact:** A new game author or an out-of-tree engine consumer inherits product/service implementation dependencies. AI agents cannot tell which interfaces are reusable engine API and which are Flashback-specific without reading the build graph and code.
- **Recommended fix:** Before repository extraction, prove a strict engine-only configure/build/install/consumer path and move the game-to-Wish adapter out of the engine/package dependency graph. Keep product composition in a Flashback-owned layer, as the accepted dependency direction specifies.
- **Effort:** L
- **Dependencies:** none

### ARCH-002 — The runtime host contract is too small to support engine-hosted game authoring

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** `IGameModule` exposes name, API version, `initialize`, `tick`, and `shutdown`; `GameModuleHostServices` only contains `RuntimeMode` and `void* user_data`. [src: file: engine/runtime/include/ae/runtime/game_module.h:11-48] `Application::tick` accepts any nonnegative `float dt_seconds`, increments frame/time, then directly invokes the module. [src: file: engine/runtime/src/application.cpp:80-101]
- **Impact:** A game module has no typed access to asset loading, input, rendering, audio, world/entity creation, structured diagnostics, deterministic stepping, or capabilities through the advertised host interface. A future AI must reach around the contract or write product-specific scaffolding.
- **Recommended fix:** Define a narrow, versioned set of typed host service interfaces and explicit ownership/lifetime rules. Add a host-owned fixed-step simulation clock and separate presentation frame context from simulation tick context. Do this after documenting which services are engine guarantees versus optional host capabilities.
- **Effort:** L
- **Dependencies:** ARCH-001

### ARCH-003 — Client frame pipeline is explicit but not a reusable engine loop

- **Severity:** MEDIUM
- **Category:** design flaw
- **Evidence:** The pipeline constructor requires concrete window, renderer, simulation, input provider, frontend/UI, config, audio, shadow/PBR, level scene/asset, and product state references. [src: file: client/include/ahamkara/client/client_frame_pipeline.h:49-73] Its stages explicitly cover polling, menus, gameplay input, snapshots, scene build, audio, rendering, UI, present, and post-frame work. [src: file: client/include/ahamkara/client/client_frame_pipeline.h:75-102] `run_one_frame` implements those stages as a fixed sequence. [src: file: client/src/client_frame_pipeline.cpp:270-321]
- **Impact:** The client has understandable orchestration, but a new AI-authored game cannot reuse `ae::Application` to get the same gameplay/render/input/audio loop. Frame sequencing is encoded in a product-specific type with no scheduling/plugin model.
- **Recommended fix:** Retain the ordered pipeline but split generic host stages from product gameplay/presentation. Represent stages as a small typed pipeline owned by the host and pass a documented frame context to product callbacks. Keep the current Flashback pipeline as a consumer/reference during migration.
- **Effort:** L
- **Dependencies:** ARCH-002

### ARCH-004 — World state has multiple competing representations

- **Severity:** HIGH
- **Category:** design flaw
- **Evidence:** `World` owns `std::vector<Player>`, raw arrays for dummies, damage numbers, particles, decals, an EnTT registry, a physics owner, and vectors used for collider ownership and render projection. [src: file: game/include/ahamkara/game/world.h:338-380] The header calls EnTT authoritative for projectiles while exposing a vector projection and fixed-array dummies through mutable accessors. [src: file: game/include/ahamkara/game/world.h:114-125] [src: file: game/include/ahamkara/game/world.h:326-348] Construction creates dummy registry entities from the separate dummy array. [src: file: game/src/world.cpp:80-83]
- **Impact:** Every new gameplay feature requires an agent to determine its source of truth, synchronization step, capacity constraints, and presentation projection. Duplicate representations invite stale-state bugs and make save/load, generic tooling, entity inspection, and bulk operations harder.
- **Recommended fix:** Pick one authoritative representation per domain. Prefer ECS components for scalable entities; define explicit components/resources for global player/match state; make presentation snapshots generated views instead of mutable sibling stores. Migrate incrementally and add invariants at each conversion boundary.
- **Effort:** XL
- **Dependencies:** ARCH-002

### ARCH-005 — Ownership is mixed and non-owning lifetimes are implicit

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** `World` clearly owns its physics bridge and registry via `unique_ptr` and value member, but also retains non-owning collider and destination pointers and a raw `IAudioPlayer*`. [src: file: game/include/ahamkara/game/world.h:338-377] Destination metadata is explicitly marked non-owning; collider pointer/count are stored separately from an owned collider vector. [src: file: game/include/ahamkara/game/world.h:341-344] FrameAllocator’s documented ring allocation is non-thread-safe and requires manual frame advancement. [src: file: engine/core/include/ae/core/frame_allocator.h:10-31] [src: file: engine/core/include/ae/core/frame_allocator.h:34-77]
- **Impact:** These contracts can be safe when lifetimes are disciplined, but AI agents adding levels, async loading, audio backends, or parallel jobs can create dangling references or allocate from a frame arena on the wrong thread. The public game module contract does not currently make those constraints discoverable.
- **Recommended fix:** Replace pointer/count pairs with explicit spans or owned handles where appropriate; document the lifetime owner for every view; add debug assertions for frame-arena generation/thread ownership. Avoid widening raw-pointer APIs in new engine interfaces.
- **Effort:** M
- **Dependencies:** ARCH-002

### ARCH-006 — Event delivery is bespoke and bounded, with no shared event contract

- **Severity:** MEDIUM
- **Category:** missing feature
- **Evidence:** Audio events are queued into a fixed array capped at 16 per tick; additional events are silently ignored by `queue_audio_event`. [src: file: game/include/ahamkara/game/world.h:334-336] [src: file: game/src/world.cpp:694-698] The queue is flushed only if an `IAudioPlayer` exists, then count is reset. [src: file: game/src/world.cpp:700-707] VFX feedback uses a separate payload/event ID for presentation-side deduplication. [src: file: game/include/ahamkara/game/world.h:243-248]
- **Impact:** Cross-system reactions (UI, achievements, quests, audio, telemetry, network replication, AI observations) need ad hoc callbacks or copied state. Event loss at the fixed cap is invisible, and the system lacks an inspectable event trace useful to AI verification.
- **Recommended fix:** Introduce a typed, bounded, observable event queue with overflow counters and documented delivery phase/rollback semantics. Preserve subsystem-specific payloads behind typed event variants and route presentation effects through it.
- **Effort:** M
- **Dependencies:** ARCH-004

### ARCH-007 — Persistence and simulation serialization are not unified

- **Severity:** HIGH
- **Category:** missing feature
- **Evidence:** `net_packets.h` defines protocol magic/version and explicit serializer/deserializer functions for input, snapshots and handshake packets. [src: file: game/include/ahamkara/game/net_packets.h:14-25] [src: file: game/include/ahamkara/game/net_packets.h:438-498] The available `HistoricalState` records tick, player positions/alive flags, and dummy positions/alive flags only. [src: file: game/include/ahamkara/game/world.h:30-39] No general world save/load interface appears in the inspected `World` public contract; its public lifecycle is construction, ticking, queries, and game-specific mutation. [src: file: game/include/ahamkara/game/world.h:68-100] This is a bounded source inspection, not proof no persistence mechanism exists elsewhere; general world save/load is **UNVERIFIED** beyond this scope.
- **Impact:** AI cannot reliably checkpoint arbitrary game state, resume playtests, create save slots, compare state snapshots, or serialize replay fixtures using one supported contract. Rollback history is not a substitute for durable save data.
- **Recommended fix:** Specify a versioned game-state serialization API independent from network packet structs. Include schema/version migration rules, stable entity IDs, deterministic ordering, and validation errors. Add round-trip tests per game module.
- **Effort:** L
- **Dependencies:** ARCH-004

### ARCH-008 — Fixed-step behavior is partial and determinism is not guaranteed

- **Severity:** HIGH
- **Category:** risk
- **Evidence:** `World::tick` subdivides elapsed time into intervals no larger than 1/60 second, but its final interval can be fractional and it reapplies the same input each substep. [src: file: game/src/world.cpp:153-162] Server-facing APIs separate `advance_sim` and `apply_input`, while `World::tick` is a convenience path. [src: file: game/include/ahamkara/game/world.h:75-92] Client prediction and rewind-related systems exist, but their existence does not establish cross-platform deterministic replay. [src: file: game/src/client_prediction.cpp:1-103]
- **Impact:** Variable frame deltas can alter step boundaries; replaying inputs at a different grouping may not reproduce identical state. Physics backend floating-point behavior, random seeds, iteration order, and replay equivalence were not verified here. For AI, failures then become hard to reproduce and automated playtest results less trustworthy.
- **Recommended fix:** Make fixed tick index the canonical simulation input; accumulate wall time only in host presentation code; record one normalized input command per simulation tick and seed all randomness. Add deterministic state hashing/replay checks on supported platforms and explicitly scope any non-deterministic physics behavior.
- **Effort:** L
- **Dependencies:** ARCH-007

### ARCH-009 — Versioning exists, but compatibility policy has gaps

- **Severity:** MEDIUM
- **Category:** risk
- **Evidence:** Runtime module versioning requires matching major and accepts an older minor/patch relative to host. [src: file: engine/runtime/include/ae/runtime/game_module.h:11-22] Core ABI versioning declares a policy but its helper only compares consumer major/minor/patch against producer values. [src: file: engine/core/include/ae/core/abi_version.h:5-34] Root CMake installs/packages engine libraries separately, but the monorepo composition and exported API boundaries are still being established. [src: file: CMakeLists.txt:131-195] [src: file: docs/architecture/overview.md:88-103]
- **Impact:** An AI may be able to detect a coarse version mismatch, but cannot infer capability availability or safely negotiate optional services. A version constant does not by itself ensure ABI checks are invoked by consumers or that all public headers respect the ABI promise.
- **Recommended fix:** Add capability negotiation to the host contract, define consumer-vs-host version semantics in one place, and make package-consumer CI exercise older compatible and intentionally incompatible consumers. Document source API versus binary ABI support separately.
- **Effort:** M
- **Dependencies:** ARCH-002

## 5. What's Good

- Separate CMake libraries and a distinct public-header/source convention provide a useful starting structure for package boundaries. [src: file: docs/repo-map.md:36-44]
- The small `IGameModule` lifecycle and `Application` owner are understandable and testable seams to evolve, rather than requiring a game to inherit from a giant engine object. [src: file: engine/runtime/include/ae/runtime/game_module.h:35-48] [src: file: engine/runtime/include/ae/runtime/application.h:12-46]
- Client frame stages are named and sequenced explicitly, so frame behavior can be traced and later decomposed. [src: file: client/include/ahamkara/client/client_frame_pipeline.h:75-102]
- `World::advance_sim` and `World::apply_input` are separated for authoritative server progression and input application; this is the right kind of seam for network prediction/reconciliation. [src: file: game/include/ahamkara/game/world.h:75-92]
- The EnTT registry is explicitly authoritative for projectiles and output projection is centralized, offering a migration pattern for other entity domains. [src: file: game/include/ahamkara/game/world.h:326-348]
- CMake has an engine-only mode that force-disables product targets, and the architecture doc describes an out-of-tree package consumer workflow. It gives a concrete boundary to prove. [src: file: CMakeLists.txt:131-145] [src: file: docs/architecture/overview.md:88-103]
- A fixed-step subdivision, server tick number, bounded state history, client prediction and VFX event IDs provide foundations for replayable simulation, even though end-to-end determinism remains unverified. [src: file: game/src/world.cpp:153-166] [src: file: game/include/ahamkara/game/world.h:30-39] [src: file: game/include/ahamkara/game/world.h:243-248]

## 6. Open Questions for the Owner

- Should this repository first ship a reusable general-purpose engine, or ship Flashback as the only reference game while extracting reusable APIs? **Recommended default:** keep the current accepted three-project direction, but prove package boundaries before extraction. [src: file: docs/architecture/overview.md:12-21] [src: file: docs/architecture/overview.md:104-120]
- Is gameplay authoring intended to be C++ only, or should the runtime contract expose an embedded scripting/data-first layer? **Recommended default:** keep C++ as the first supported path and defer an embedded language until host services, serialization and hot reload are stable; this audit did not find enough evidence to infer the owner's choice.
- What determinism promise is required: repeatable on one machine/build, lockstep across supported machines, or only reproducible input traces with tolerant state comparisons? **Recommended default:** target same-build replay first, then explicitly document physics and platform limits.
- Should EnTT become the canonical game-object model, or remain an implementation detail for scalable entities alongside explicit player/global state? **Recommended default:** avoid a wholesale ECS rewrite; standardize ownership and state inspection first, then migrate one domain at a time.

## 7. Handoff Notes for the Next AI Agent

- Start with `audit/01-recon.md` for the observed dirty state and validation results; do not reset, stash, clean, or overwrite the pre-existing worktree.
- Read `docs/architecture/overview.md` and `docs/architecture/repository-split.md` before moving modules. The stated target direction differs from current CMake composition.
- Treat `ae::Application` as a minimal lifecycle owner, not as proof that a complete host API already exists. Its current services object exposes only mode and opaque user data.
- When editing `World`, distinguish authoritative state from projection/cache state. In particular, projectiles have a registry-backed projection, while players, dummies, particles, and decals use separate storage patterns.
- If making claims about save/load or determinism, inspect all asset/config formats and replay code and run the relevant tests. This pass establishes missing general contracts only within the inspected public interfaces; it does not prove that no project-specific persistence exists anywhere.
- Preserve the fixed-step/server-input separation and named client frame stages while reducing coupling; these are useful seams rather than accidental code to remove.
