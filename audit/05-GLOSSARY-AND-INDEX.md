# Glossary and Audit Index

## Glossary

| Term | Meaning in this audit/repository |
|---|---|
| Ahamkara | Native engine/runtime workspace and project name. Exact product boundary is still being clarified. |
| Flashback | Current FPS/action game and client/sample composition inside the repository. |
| Wish | Multiplayer/session/admin platform modules; separate from the Hono web backend. |
| Native client | C++ game executable and rendering/input path. Do not confuse with Vue web frontend. |
| Web frontend/backend | `frontend/` Vue/Vite application and `backend/` Hono/TypeScript service. |
| `IGameModule` | Versioned native lifecycle interface with initialize/tick/shutdown; not currently a complete authoring API. |
| Game MCP | Development-only file-backed bridge and MCP wrapper for controlling/observing the existing client. |
| Headless | Build/runtime configuration without the graphical client; current headless tests do not prove graphical rendering. |
| Asset manifest | Text description consumed by importer; current path confinement/runtime package behavior has known gaps. |
| APKG | Compiled asset package output format referenced by asset tooling; a general runtime package reader was not demonstrated. |
| Fixed tick | Simulation step at a fixed duration; internal use exists, but agent-facing seeded replay is not established. |
| State dump | Structured runtime game state emitted by an engine API; scratch experiment JSON was custom stdout, not an engine dump. |
| Reference game | Small complete game used to prove engine APIs, fun/playability, and verification workflow. |
| Dirty baseline | The observed checkout includes uncommitted changes; findings may not map to committed `b705d74`. Initial exact status count is disputed. |
| Status labels | `IMPLEMENTED` means verified working path; `PARTIAL` means incomplete/limited; `STUBBED` means placeholder; `PLANNED` means documented not started; `MISSING` means not found/needed; `UNKNOWN` means not verified. |
| Severity | `BLOCKER`, `HIGH`, `MEDIUM`, `LOW`, `INFO` as assigned by specialists; threat-model-dependent ratings are called out in synthesis. |

## Audit report index

| File | Area | Main finding IDs |
|---|---|---|
| [00-MASTER-AUDIT.md](00-MASTER-AUDIT.md) | Overall synthesis, scorecard, capability matrix, experiments, top findings, risks | Cross-report |
| [01-ROADMAP.md](01-ROADMAP.md) | Strategy, milestones, deferred work, owner decisions | AUD/API/LOOP/SEC/GAME/PERF/GENRE tasks |
| [02-BACKLOG.md](02-BACKLOG.md) | Ordered executable proposals with acceptance criteria | TASK-001–023 |
| [03-AGENTS.md](03-AGENTS.md) | Draft agent guide | Not findings |
| [04-OPEN-QUESTIONS.md](04-OPEN-QUESTIONS.md) | Decisions requiring owner intent | Q1–Q22 |
| [01-recon.md](01-recon.md) | Repo archaeology, build/test/install attempts | RECON-001–006 |
| [02-architecture.md](02-architecture.md) | Architecture and runtime contracts | ARCH-001–009 |
| [03-rendering.md](03-rendering.md) | Rendering and graphics | REND-001–006 |
| [04-gameplay.md](04-gameplay.md) | Gameplay systems | GAME-001–009 |
| [05-assets-audio.md](05-assets-audio.md) | Audio and asset pipeline | ASSET-001–009 |
| [06-ai-ergonomics.md](06-ai-ergonomics.md) | AI interface and five live experiments | AIER-001–009 |
| [07-qa-testing.md](07-qa-testing.md) | Testing and verification | QA-001–004 |
| [08-performance-reliability.md](08-performance-reliability.md) | Profiling, scaling, reliability | PERF-001–005 |
| [09-game-design.md](09-game-design.md) | Game design and fun-factor affordances | GDES-001–006 |
| [10-dx-docs.md](10-dx-docs.md) | Documentation/onboarding | DX-001–006 |
| [11-security.md](11-security.md) | Security and safety | SEC-001–012 |
| [12-red-team.md](12-red-team.md) | Challenges and synthesis corrections | RED-001–005 |

## Finding ID namespaces

- `RECON` repository setup/state; `ARCH` architecture; `REND` rendering; `GAME` gameplay; `ASSET` asset/audio; `AIER` AI ergonomics; `QA` verification; `PERF` performance/reliability; `GDES` game design; `DX` docs/onboarding; `SEC` security; `RED` red-team challenges.
- Backlog task IDs are distinct from finding IDs. Cross-links in the master report point to specialist finding IDs; the backlog contains proposed implementation work only.
