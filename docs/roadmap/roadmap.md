# Strategic roadmap

This document records product direction, sequencing, and exit gates. It is not the active work queue. Forgejo Issues own implementation status and priority; this roadmap links goals to those issues without duplicating their task state.

## Product model

Ahamkara is the engine and runtime. It owns reusable platform, rendering, audio, animation, physics, collision, asset, and low-level networking capabilities. Wish is the first-party networking and multiplayer layer built for Ahamkara. Wish consumes Ahamkara runtime and network APIs to provide session, admission, replication, and multiplayer services. Flashback is the game used to exercise Ahamkara and Wish end to end.

Keep these products in the Ahamkara monorepo while their contracts are being developed. Enforce boundaries through targets, public interfaces, and consumer tests; repository extraction is not a roadmap gate. Flashback may run locally without Wish, while multiplayer builds integrate the Wish layer. The Hono/Vue web surface is a separate website/API project and should not be confused with Wish's game networking role.

| Component | Owns | Depends on |
|---|---|---|
| Ahamkara | Runtime, platform, input, rendering, audio, animation, physics/collision, generic network transport, asset formats/import, diagnostics, developer APIs | System libraries and pinned third-party dependencies |
| Wish | Multiplayer sessions, authentication/admission, parties/activities, replication orchestration, server-side multiplayer services | Ahamkara runtime and network APIs |
| Flashback | Game rules, weapons, encounters, game AI, progression, game presentation, content, game-specific network payloads and adapters | Ahamkara; Wish for multiplayer sessions |

Within the product stack, platform APIs support Ahamkara, Wish depends on Ahamkara, and Flashback uses Ahamkara plus Wish when multiplayer is enabled. Ahamkara must not require Flashback or Wish-specific types to build its reusable core. Wish must not own Flashback gameplay rules. Flashback remains the reference game and integration test for both lower layers.

## Starting point

The live Forgejo `main` baseline used for the initial clean-checkout run was `56ed526`. Its headless configure/build/CTest run passed 45 tests; debug and release builds failed on duplicate `PbrRenderer::set_ao_texture` declarations. That blocker is recorded in [Forgejo issue #178](https://git.2helix.org/taufeeq26/Project-Ahamkara/issues/178). The Forgejo Actions cutover is in [PR #177](https://git.2helix.org/taufeeq26/Project-Ahamkara/pulls/177); artifact upload now works through the dedicated runner network, but the debug and release checks still need to pass before that baseline can be called healthy.

The local `dev/flashback-menu-shell` checkout contains extensive uncommitted changes and does not match the live Forgejo `main` history. Preserve those changes and reconcile the branch history before treating its menu, Director, Caligo, character, rendering, or animation work as committed capability. Do not reset or clean that worktree to create a baseline.

## Roadmap principles

- Establish a reproducible clean baseline before using build results to make architecture or product claims.
- Keep Ahamkara's public runtime and network contracts small and prove them through Flashback.
- Build Wish on Ahamkara's network/runtime capabilities; avoid parallel low-level transport contracts.
- Make one local Flashback loop understandable, completable, restartable, and verifiable before expanding multiplayer scope.
- Use text-first, deterministic authoring and testing paths. Add an editor only after the scene/content contracts stabilize.
- Treat imported or AI-authored content as untrusted. Path confinement, parser limits, and worker isolation must precede accepting it from untrusted sources.
- Fix rendering, animation, and audio gaps that block the reference slice; defer broad feature polish until a repeatable scenario demonstrates its value.
- Keep one source of truth for work in Forgejo Issues. Reports and roadmaps may explain direction but must not become a parallel task queue.

## Stages and exit gates

### Stage 1 — Trust the baseline and the remote workflow

Make clean-checkout validation reliable on the selected server and make Forgejo Actions the gate for changes.

**Exit gates:**

- A recorded commit from live Forgejo `main` configures, builds, and tests in debug, release, and debug-headless presets.
- The package preset produces a usable artifact and a clean temporary consumer can configure, link, and run against the exported targets.
- Debug, release, headless, package, and required lint jobs report success or a specifically documented environment limitation; no critical check is allowed to pass by swallowing an error.
- The dev server has the required compiler, CMake, Ninja, graphics libraries, and CI toolchain in repeatable setup instructions; source and build data stay on the server.
- The local Flashback work is preserved and reconciled with the live Forgejo history in a separate branch/worktree; the published baseline is unambiguous.
- The remote GUI path can launch Flashback and expose a frame to the developer without moving build outputs to the Mac.

### Stage 2 — Set the Ahamkara/Wish/Flashback contract

Make the build graph and public interfaces express the intended ownership before adding more product-specific networking behavior.

**Exit gates:**

- Ahamkara's reusable core builds without Flashback gameplay types or Wish-specific types.
- Ahamkara owns shared transport, connection, clock, sequencing, and reliability primitives; Wish uses those APIs for sessions, admission, and replication orchestration.
- Duplicate transport/address/sequence abstractions are consolidated or have an explicit versioned boundary and compatibility reason.
- Flashback game-specific payloads and adapters live at the Flashback integration edge; the sample module demonstrates behavior beyond lifecycle-only initialize/tick/shutdown.
- Current architecture and repository-split documents no longer require Wish to build independently or schedule a three-repository extraction.
- A local single-player Flashback build can run without a live Wish service; multiplayer mode composes Wish explicitly.
- Public interfaces define ownership, threading, tick semantics, errors, lifetimes, and compatibility through tests and examples.

### Stage 3 — Make the content-to-screen path safe and dependable

Use the smallest content workflow needed by Flashback to prove that source assets can be imported, resolved, and displayed predictably.

**Exit gates:**

- A clean manifest import produces the level, mesh, material, texture, and character data used by the reference slice.
- Runtime asset resolution works from a documented project/package root, not only from one developer's current directory.
- Import paths stay within approved roots; malformed or oversized glTF, texture, level, and manifest data fail with actionable errors and bounded resource use.
- Visual and collision transforms agree for imported level meshes; source-to-compiled round trips have regression coverage.
- Essential materials, shadows, lighting, and level LOD do not silently drop or misrepresent geometry. SSAO/TAA and advanced rendering remain optional until the reference slice needs them.
- Real skinned character assets and clips can drive a non-identity gameplay pose; audio events use registered asset IDs through one lifecycle-managed path.

### Stage 4 — Deliver one playable Flashback reference slice

Use Flashback to prove that Ahamkara hosts a real game loop. Start with a small combat or exploration encounter in the existing Caligo direction; build only the geometry and presentation required for that loop.

**Exit gates:**

- From a clean remote checkout, the Flashback executable launches into its menu, reaches the Director/activity selection, loads the chosen compiled level, and reports a useful error if the level cannot load.
- The player can spawn, move, look, use the primary weapon, interact with a target/objective, reach a clear win or loss result, and restart without stale simulation or UI state.
- One short scenario has explicit objective, completion, failure, and restart behavior. A human can complete it from the on-screen instructions without reading source code.
- The scenario is covered by focused gameplay tests and a deterministic headless playtest; a display-equipped run captures menu, gameplay, and result screens.
- The Caligo three-route courtyard, hero interior, electrical hazard, and traversal replay remain proposed content targets. Add them only where the first objective and playtest show they improve the game.
- A display-capable smoke checklist covers the menu, gameplay, and result flow; menu-layout success alone does not count as proof that Flashback is playable.

### Stage 5 — Close the agent observe/act/replay loop

Make the same Flashback scenario useful to an AI agent and repeatable in CI, while keeping text/CLI authoring available.

**Exit gates:**

- A headless runner boots a supported game, applies exact fixed ticks with a seed, collects structured state/errors/logs, and exits under a time/frame budget.
- Every mutating command returns an ID, validation result, and applied tick; observations identify which commands they include.
- A scenario can reset, record inputs, replay in a fresh process, and report the first divergent tick/field; repeated deterministic runs produce the same state hash.
- Tests cover both success and failure scenarios, including objectives, target outcomes, scoring, and restart—not just input consumption or ammo counters.
- A separate display-capable lane verifies readiness and stores a screenshot with matching state and logs. Screenshots supplement, not replace, headless assertions.
- Generated/native code is treated as untrusted: no unattended arbitrary execution until process, filesystem, environment, network, and resource limits are enforced.

### Stage 6 — Integrate Wish multiplayer on Ahamkara

After the local Flashback loop and Ahamkara/Wish contracts are stable, validate the multiplayer layer with the same game.

**Exit gates:**

- Two clients connect to an authoritative local server through Ahamkara transport and Wish session/admission services.
- Two authenticated clients map to distinct authoritative players/entities; snapshots and game payloads preserve that ownership.
- Join rejection, disconnect, timeout, reconnect, and one authoritative gameplay event have deterministic integration coverage.
- Authentication secrets are not sent over plaintext HTTP or reusable-token UDP payloads; production mode is fail-closed and local development bypasses are explicit.
- Flashback's networking adapter remains game-owned; Wish contracts remain usable by another future test game without Flashback types.

### Stage 7 — Harden performance, diagnostics, and delivery

Set production bars from measured workloads after the reference game and multiplayer path exist.

**Exit gates:**

- A representative Flashback scene and Wish session establish reproducible frame-time, startup, memory, asset-load, and network-latency baselines on declared hardware.
- Release-like CI enforces documented regression budgets; benchmark output identifies the workload, commit, compiler, and host class.
- Crash capture is safe on supported platforms; logs and diagnostic bundles correlate build, game, session, and failure IDs while redacting secrets.
- Long-run tests bound streaming metadata, memory, sound handles, log growth, and reconnect state.
- Forgejo tags produce versioned Ahamkara/Wish artifacts and a Flashback reference build with checksums, dependency/provenance information, installation steps, and rollback notes.
- Supported platforms, graphics drivers, and optional editor/2D capabilities are declared from actual CI and visual evidence.

## Parallel work and deferrals

After Stage 1, Ahamkara/Wish interface design can proceed alongside Flashback content planning. Secure importer work must precede accepting untrusted assets. The deterministic runner can be built in parallel with the first playable loop once the game host and state surface are stable. Wish multiplayer must wait for the single-player objective and authoritative ownership model.

Do not schedule repository extraction, a general-purpose visual editor, a second genre, broad open-world rendering, or a Wish-independent backend release as gates in this roadmap. Revisit those only after the Ahamkara + Wish + Flashback contracts work and the owner explicitly changes the product direction.
