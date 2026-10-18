# Architecture decisions

This file records durable decisions. It is not a work tracker; implementation
status belongs in [Forgejo Issues](https://git.2helix.org/taufeeq26/Project-Ahamkara/issues).

| Date | Decision | Consequence | Source |
|---|---|---|---|
| 2026-07-13 | Ahamkara, Flashback, and Wish were planned as completely separate repositories and projects; superseded 2026-10-17. | Retained as historical context. The current roadmap keeps Flashback as Ahamkara’s test/integration game and Wish as networking/multiplayer built for Ahamkara, all in the monorepo. | [src: user:taufeeqali:2026-07-13: explicit three-repository direction] |
| 2026-07-13 | GitHub Issues was the source of truth for task state, priority, dependencies, and acceptance criteria; superseded 2026-10-16. | Local queue-task files and task dashboards are removed from docs. | [src: user:taufeeqali:2026-07-13: requested removal of documentation issues after GitHub migration] |
| 2026-10-16 | Forgejo Issues and Forgejo Actions are the canonical project tracking and CI surfaces. | Existing issues and open PRs moved to Forgejo with source backlinks; future work state and CI changes stay in Forgejo. | [src: user:taufeeqali:2026-10-16: explicit Forgejo cutover] |
| 2026-10-17 | Ahamkara is the engine; Flashback is its test/integration game; Wish is the future networking and multiplayer layer built for Ahamkara. | Ahamkara owns reusable runtime and transport primitives; Wish builds higher-level multiplayer services on Ahamkara APIs; Flashback exercises both. Keep the monorepo as the development/integration home and do not plan repository extraction as a gate. | [src: user:taufeeqali:2026-10-17: clarified product relationships] |
| 2026-07-13 | Documentation is organized around architecture, design, repository structure, subsystem truth, maintenance, operations, and historical evidence. | Durable docs must remain useful after an issue closes; reports remain historical. | [src: user:taufeeqali:2026-07-13: requested documentation purpose and structure] |
| 2026-06-20 | Level layout/semantics are authored as a canonical JSON spec; Blender owns geometric detail and generation is one-way. | Spec-to-level and spec-to-Blender paths may coexist without round-tripping Blender edits into the spec. | [src: file: docs/vault/memory/decision-log.md:26-46] |
| 2026-06-25 | Keep the OpenGL core profile and migrate legacy drawing through compatibility/backend helpers. | Do not restore fixed-function client state as a shortcut. | [src: file: docs/vault/memory/decision-log.md:89-102] |

The extensible error-code model is still a proposal and therefore lives in
[`design/error-system.md`](design/error-system.md), not this accepted-decision
table.
