# Architecture decisions

This file records durable decisions. It is not a work tracker; implementation
status belongs in [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues).

| Date | Decision | Consequence | Source |
|---|---|---|---|
| 2026-07-13 | Ahamkara, Flashback, and Wish will become completely separate repositories and projects. | Ahamkara and Wish cannot depend on Flashback; Flashback consumes versioned packages and owns integration adapters. | [src: user:taufeeqali:2026-07-13: explicit three-repository direction] |
| 2026-07-13 | GitHub Issues is the only source of truth for task state, priority, dependencies, and acceptance criteria. | Local queue-task files and task dashboards are removed from docs. | [src: user:taufeeqali:2026-07-13: requested removal of documentation issues after GitHub migration] |
| 2026-07-13 | Documentation is organized around architecture, design, repository structure, subsystem truth, maintenance, operations, and historical evidence. | Durable docs must remain useful after an issue closes; reports remain historical. | [src: user:taufeeqali:2026-07-13: requested documentation purpose and structure] |
| 2026-06-20 | Level layout/semantics are authored as a canonical JSON spec; Blender owns geometric detail and generation is one-way. | Spec-to-level and spec-to-Blender paths may coexist without round-tripping Blender edits into the spec. | [src: file: docs/vault/memory/decision-log.md:26-46] |
| 2026-06-25 | Keep the OpenGL core profile and migrate legacy drawing through compatibility/backend helpers. | Do not restore fixed-function client state as a shortcut. | [src: file: docs/vault/memory/decision-log.md:89-102] |

The extensible error-code model is still a proposal and therefore lives in
[`design/error-system.md`](design/error-system.md), not this accepted-decision
table.
