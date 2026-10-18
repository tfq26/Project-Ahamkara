> Historical audit snapshot: findings below describe the Mac checkout observed on 2026-10-16. The current canonical roadmap and product boundaries are in [docs/roadmap/roadmap.md](../docs/roadmap/roadmap.md) and [docs/decisions.md](../docs/decisions.md).

# Ahamkara Master Technical & Strategic Audit

Audit date: 2026-10-16 (America/Chicago)
Observed checkout: `dev/flashback-menu-shell` at `b705d74` (`feat: add initial weapon and enemy assets (#71)`), with a heavily dirty worktree. Local `main` matched this commit; `origin/main` was three commits ahead.
Overall mission rating: **2/5 — useful native engine/game systems exist, but no verified workflow lets an AI create, run, observe, and iterate a complete game using a supported engine API.**

## One-paragraph truth

Ahamkara is currently a broad C++20/CMake native engine and FPS game prototype with a separate Wish multiplayer/session platform and Vue/Hono web product. It has real physics, input, animation, OpenGL 3D rendering, UI, asset import, networking, diagnostics, tests, and a dev-only MCP bridge for operating the existing Flashback client. Those pieces do not yet form a coherent AI game-authoring product: the public game-module interface is lifecycle-only, the MCP targets one existing shooter, sprite import does not prove runtime display, general game save/load and deterministic agent replay are absent, screenshot verification was not demonstrated, and no reference game was verified as complete or fun. Build/test findings below describe the dirty tree and restricted environment, not a clean commit. Exact initial dirty-file count is disputed because the recon note’s total and breakdown did not reconcile.

## Scorecard

| Domain | Rating | Status | Verdict |
|---|---:|---|---|
| Repository/build reproducibility | 2/5 | PARTIAL | Native CMake works with cached dependencies; first fresh fetch failed under unavailable GitHub DNS; moving dependency refs remain. |
| Core architecture | 2/5 | PARTIAL | Useful module boundaries, but game/product dependencies and parallel world models remain. |
| Runtime/game authoring API | 1/5 | PARTIAL | `IGameModule` has lifecycle/tick only and no renderer/input/entity/asset services. |
| Rendering | 2/5 | PARTIAL | Substantial OpenGL 3D path; SSAO/TAA integration incomplete, no native scene sprite/tilemap path found, and no visual output was verified. |
| Gameplay systems | 2/5 | PARTIAL | FPS movement/combat/physics/AI/animation foundations exist; objective triggers, AI movement, input configuration, save/load, and integration have gaps. |
| Audio and assets | 2/5 | PARTIAL | Asset importer and content tooling exist; package loading, asset discoverability, sprite workflow, audio event mapping, and hot reload are incomplete. |
| AI observe/act/replay | 2/5 | PARTIAL | Game MCP can issue bounded input and expose state/capture paths for one client; no general authoring API, deterministic reset/replay, or verified screenshot loop. |
| QA and verification | 3/5 | PARTIAL | A broad test suite and CI exist; visual regression, complete playthrough, and generic agent self-check gates are missing. |
| Performance/reliability | 2/5 | PARTIAL | Profiling, budgets, crash reporting, and benchmarks exist; runtime wiring and crash-path/platform coverage are weak; no timings collected. |
| Security/safety | 2/5 | PARTIAL | Useful opt-in controls and redaction exist; native execution, asset path confinement, network credential transport, and worker isolation need a stated threat boundary. |
| Game design / fun | 2/5 | UNKNOWN/PARTIAL | FPS direction and game-feel primitives are evident; no demo was played, and no genuinely fun complete game was verified. |
| DX/docs/onboarding | 2/5 | PARTIAL | Build/run guidance is useful; root AGENTS has placeholders, README state is stale, and no end-to-end game-authoring tutorial exists. |

## Architecture overview

```text
                         ┌──────────────────────────┐
                         │ Vue/Vite frontend         │
                         │ Hono backend              │
                         └────────────┬─────────────┘
                                      │ web/session surfaces
┌─────────────────────────────────────┴─────────────────────────────────────┐
│ Native CMake workspace                                                    │
│                                                                            │
│  Flashback sample/client ── client frame pipeline ── OpenGL renderer/UI   │
│          │                         │                                      │
│          ├── game::World ── gameplay/physics/collision/animation/audio     │
│          │        │                                                        │
│          │        └── EnTT entities + fixed arrays/object stores           │
│          ├── Game MCP files ── observe/input/frame capture (dev only)      │
│          └── assets + importer/Blender/levelgen tools                      │
│                                                                            │
│  engine core/runtime/network/platform + Wish/session/admin/server         │
└────────────────────────────────────────────────────────────────────────────┘
```

This is a source/build composition map, not a claim that every path is behaviorally verified. The public `IGameModule` lifecycle host is not the same thing as the Flashback client/game pipeline. See [02-architecture.md](02-architecture.md), [06-ai-ergonomics.md](06-ai-ergonomics.md), and [01-recon.md](01-recon.md).

## Full capability matrix

| Capability | Status | Evidence-based summary |
|---|---|---|
| Native build/runtime host | IMPLEMENTED | C++20/CMake modules and a versioned lifecycle interface exist; consumer path needed manual CMake setup in the live attempt. |
| General game authoring API | MISSING/PARTIAL | No machine-readable capability schema or host services for entities, rendering, input, assets, audio. |
| 3D mesh rendering | IMPLEMENTED/PARTIAL | OpenGL/glTF/compiled mesh and level submission are present; visual behavior not run. |
| Materials/PBR | PARTIAL | Material/light path exists; normal maps and some authored properties are not consumed. |
| Shadows | PARTIAL | Shadow pass and cascades exist in code; no image validation. |
| SSAO/TAA | PARTIAL | Pass/state implementations exist, but main frame path lacks SSAO hookup and TAA history resolve. |
| Sprites/2D/tilemaps | MISSING | No engine-native scene sprite/tilemap API found; importer PPM passthrough is not rendering. |
| UI/HUD/menus | IMPLEMENTED/PARTIAL | Dear ImGui-based UI/menu/HUD code exists; product integration and visual tests remain limited. |
| Camera | IMPLEMENTED | Product camera controls/state exist; general-purpose authoring contract is not established. |
| Physics/collision | IMPLEMENTED/PARTIAL | Jolt-backed physics and collision utilities exist; gameplay contact integration and AI traversal have defects/gaps. |
| Input | IMPLEMENTED/PARTIAL | Input mapping/routing exists; config semantics/validation and agent action acknowledgement need work. |
| Animation/skeleton | IMPLEMENTED/PARTIAL | glTF skeleton/clip and weapon/character systems exist; blending and runtime integration issues remain. |
| NPC AI/pathfinding | PARTIAL | Nav grid/A*/agents and behavior types exist; movement may bypass physical obstacle handling and behavior integration is incomplete. |
| Game state machine/objectives | PARTIAL | Encounter/objective schema and activity implementations exist; dispatch/closure and complete loop are not verified. |
| Audio | PARTIAL | Engine audio and event hooks exist; identifiers/event mapping are incomplete; spatial audio quality unverified. |
| Asset import | PARTIAL | TGA/glTF/level and manifest tooling exist; image/sound formats and runtime package resolution are incomplete. |
| Asset catalog/metadata | PARTIAL | Manifest/registries and fragmented metadata exist; no single discoverable AI catalog/validation contract. |
| Hot reload | PARTIAL | Menu/HUD reload paths exist; general game/asset reload not established. |
| Save/load | MISSING/PARTIAL | Config/session serialization exists; general game-world save/load is not present. |
| Networking/multiplayer | IMPLEMENTED/PARTIAL | UDP/networking, server, Wish/Nakama integration exist; security and end-to-end play remain unverified. |
| Scripting | MISSING/UNKNOWN | No gameplay scripting runtime was found; native C++ is the effective extension route. |
| Headless operation | PARTIAL | Headless presets and client simulations exist; no general authorable game stepping/render/state harness. |
| AI observe/act | PARTIAL | Dev MCP can control and observe one existing game; action acknowledgement and generic mutation are absent. |
| Deterministic replay | PARTIAL | Fixed-step/prediction machinery exists internally; no agent-level seeded reset/replay contract. |
| Screenshot/readback | PARTIAL | Feature-gated client PPM capture code exists; live capture was not reached in this environment. |
| Automated game validation | PARTIAL | Unit/integration/asset tests and vision QA wrapper exist; no general boot-N-frames, win-condition, or bot-verification gate. |
| Performance profiling | PARTIAL | Profiler/budgets/benchmarks exist but runtime sampling and representative release workload gates are incomplete. |
| Crash handling/diagnostics | PARTIAL | Diagnostics and dump paths exist; POSIX handler does unsafe signal-context work and Windows handler is stubbed. |
| Security sandbox | MISSING | In-process native code and worker processes have host privileges; no OS isolation boundary. |

## AI-readiness verdict

**Can an AI build a fun game with this engine today? Not through a supported, general engine workflow.** A competent C++ agent can build product-specific gameplay by reading the source, and the isolated experiment proved a custom `IGameModule` can tick hand-written behavior after adding include paths and CMake module configuration manually. That module moved a plain C++ `Enemy`, computed a score/win boolean, and printed JSON. It did not render a square, register an engine entity, display the imported sprite, or produce engine state/screenshot output. The importer successfully copied a 1×1 PPM. The client MCP launch/capture did not reach readiness, so visual feedback is unverified.

| Live task | Result | Meaning |
|---|---|---|
| 1. Smallest moving-square game | PARTIAL | Lifecycle module configured/built after manual setup and ticked; no square was rendered. Initial consumer build failed on missing transitive includes. |
| 2. New entity with custom behavior | PARTIAL | Plain C++ struct moved from x=0 to 1.5; no engine entity/component registration or physics/render integration. |
| 3. Add and display sprite | PARTIAL | Importer copied PPM successfully; no runtime lookup or display path exercised. |
| 4. Win/lose and score | PARTIAL | Custom module logic produced score=10/won=true; no built-in objective/HUD/persistence/loss path. |
| 5. Screenshot/state dump | PARTIAL/FAILED | Hand-authored stdout JSON was filtered and validated; no engine world dump or screenshot was created. MCP did not reach a ready bridge. |

Exact experiment setup, commands, scratch file inventory, and limitations are in [06-ai-ergonomics.md](06-ai-ergonomics.md). Build/test outputs are in [01-recon.md](01-recon.md).

## Top 20 findings by mission impact

| Rank | Root issue | Severity | Evidence IDs |
|---:|---|---|---|
| 1 | No supported generic game-authoring contract or minimal consumer path | BLOCKER | AIER-001/002/003; ARCH-002; DX-002/003 |
| 2 | No deterministic, acknowledged agent action/observe/reset/replay loop | BLOCKER | AIER-004/005/007; ARCH-008; QA-001/002 |
| 3 | No verified visual game path from authored content to screenshot | HIGH | AIER-006/007; REND-005/006; ASSET-001/002; QA-001 |
| 4 | No complete, playtested reference game; fun/balance remains unknown | HIGH | GDES-001/002/004/005; GAME-003 |
| 5 | Native generated code/worker has no OS sandbox for unattended untrusted execution | HIGH, threat-model dependent | SEC-001/011 |
| 6 | Asset import paths and external glTF resources are not confined/limited | HIGH | SEC-002/003; ASSET-004 |
| 7 | Admin and game authentication expose credentials/data on reachable networks | HIGH | SEC-004/008/009 |
| 8 | README/root agent guide misstates status and lacks the actual golden path | HIGH | RECON-001/002; DX-001/002; AIER-009 |
| 9 | Runtime package/manifest path is not a demonstrated asset-to-game load workflow | HIGH | ASSET-001/007/009; AIER-006 |
| 10 | Gameplay objective/trigger and simultaneous-wave closure are incomplete | HIGH | GAME-002/003; GDES-002 |
| 11 | Texture and glTF parsers need bounded hostile-input handling | HIGH | SEC-003/010 |
| 12 | Core renderer effects/material properties are incomplete or disconnected | HIGH | REND-001/002/003 |
| 13 | General world persistence/save-load absent | HIGH | ARCH-007; GAME-008 |
| 14 | Audio event identifiers and mappings do not cover the current event set | MEDIUM | ASSET-005 |
| 15 | Runtime asset hot reload and asset validation are narrow | MEDIUM | ASSET-006; AIER-008 |
| 16 | Movement/obstacle/animation/input contracts have integration defects | MEDIUM | GAME-004/006/007; ARCH-004/005 |
| 17 | Test results are not attributable to a clean baseline; headless target graph mismatch in dirty state | HIGH audit limitation | RECON-004/006; RED-001 |
| 18 | Dependency revisions and web lock metadata weaken reproducibility | MEDIUM | RECON-003/005; SEC-005 |
| 19 | Performance diagnostics do not yet provide trustworthy runtime budgets | MEDIUM | PERF-002/004/005 |
| 20 | Streaming history retention scales with distinct regions; bounds/impact need measurement | MEDIUM, conditional | PERF-001; RED-004 |

Specialist IDs refer to their reports. `GAME-*` are gameplay-system findings; `GDES-*` are game-design findings. The duplicate specialist themes are grouped here by root cause as advised by [12-red-team.md](12-red-team.md).

## Risk register

| Risk | Likelihood / impact | Evidence and mitigation |
|---|---|---|
| Scope risk: “AI builds any fun game” exceeds current engine/API maturity | High / High | Establish one supported game-authoring contract and one vertical slice; defer claims of generality. |
| Baseline risk: broad dirty tree prevents clean attribution | Certain / High | Preserve current work, select an agreed baseline, then validate in a clean worktree. Exact starting status count is disputed. |
| Bus factor: architecture/product reasoning is scattered across code and reports | High / High | Ship a concise agent guide and decision log; document which surfaces are engine vs Flashback vs Wish. |
| Security: untrusted generated native code and agent worker can access host privileges | Conditional on unattended/untrusted execution / Critical | Keep code trusted-only until restricted-process/container execution with limited mounts/env/network exists. |
| Security: untrusted manifests/glTF can read or overwrite host files and exhaust resources | Medium / High | Enforce canonical roots, checked sizes/depth, resource ceilings, and adversarial tests. |
| Game loop risk: product subsystems may look complete in isolation but lack a playable scenario | High / High | Build/play the reference slice and require state + screenshot + completion assertions. |
| Reproducibility: CMake moving refs, network dependency fetch, pnpm mismatch | Medium / Medium | Immutable pins, lockfile consistency, cached/CI install proof, SBOM. |
| Performance: no measured representative release workload | Unknown / Medium | Define target hardware/map/entity budgets and add stable workload benchmarks before optimization claims. |
| Security/privacy: auth tokens travel over HTTP/UDP and external QA can receive project imagery/state | High if used outside trusted dev setup / High | Disable unsafe paths, design short-lived secure tickets, document/limit QA provider transfer. |

## State when development stopped

The committed tip observed is dated 2026-08-10, while audit date is 2026-10-16. The source-control excerpt shows recent work on weapon/enemy assets, game MCP launch, runtime startup/build repair, and renderer dependency fixes. The worktree contains much more uncommitted Flashback menu, character, Caligo asset, renderer, animation, game MCP, and test work. Because the checkout is dirty and the initial path list was not retained, the uncommitted feature set cannot be treated as a known paused milestone or attributed to a person. `origin/main` is three commits ahead of the checked-out base.

Recommendations: preserve the current checkout as-is; have the owner identify the intended branch/baseline and disposition of in-progress work; then run the full quality gate from a clean isolated worktree. Do not merge/rebase or discard the dirty files as part of audit follow-up. Existing features that failed current validation require clean-baseline reruns before being called regressions.

## Validation evidence and limitations

- Recon attempted CMake configure, native build, CTest, and isolated offline pnpm installation. The first CMake fetch failed on GitHub DNS; cached-source configure succeeded; dirty-tree build failed linking a UI test because `ae_ui` was absent in the headless graph; CTest reported 67/71 passed with a dirty asset assertion, two restricted socket tests, and the missing executable; pnpm stopped on missing offline package metadata. Exact commands/output summaries are in [01-recon.md](01-recon.md).
- No GUI demo or screenshot was verified. No performance measurement was taken. Dependency CVE status is unverified.
- The pre-audit tree was already heavily dirty. The exact initial count and path-level delta cannot be reconstructed. Agents wrote audit reports only under `/audit/`, and the live scratch assets/builds were under `/private/tmp`, but the required clean path-by-path status comparison is unavailable.
- Red-team adjustments are recorded in [12-red-team.md](12-red-team.md), especially the narrowed experiment claims, conditional security/performance severity, and disputed dirty count.
