# Known Traps

Status: Active
Last updated: 2026-06-25

Use this note for recurring issues that future humans and agents should not
rediscover the hard way.

## Build And Runtime

- Headless work should usually use `debug-headless`; full client/render work may
  require local GLFW/OpenGL dependencies.
- A compile pass is not runtime validation. Say exactly which command ran.
- Obsidian graph edges only appear for explicit links. Isolated historical
  reports are expected unless they are linked from an index.
- In renderer files that include `engine/render/src/gl_compat.h`, direct
  `glEnableClientState` / `glVertexPointer`-style legacy calls are not real
  fixed-function behavior. They are shimmed away and must be replaced with the
  compat helpers or backend draw paths.
- In the same renderer files, `glLight*`, `GL_LIGHTING`, and related
  fixed-function lighting toggles are also shimmed away. If a shader expects
  lighting, the renderer must feed the corresponding uniforms explicitly.

## Documentation

- `docs/reports/subagents/` is historical evidence and active report output.
  Current subsystem truth should live in `docs/systems/` and
  `docs/vault/systems/`.
- Do not add new docs directly under `docs/` unless they are indexes or
  folder-level navigation files.
- Project-local imported skills live in `docs/vault/skills/`; do not use them to
  write another project's reports into Ahamkara.

## Simulation And Networking

- Deterministic gameplay changes need careful validation around fixed timestep,
  RNG, packet compatibility, and replay/prediction assumptions.
- Network packet or snapshot changes should call out compatibility risks in
  reports.

## Related

- [Agent handoff](../../guides/agent-handoff.md)
- [Networking map](../systems/networking-map.md)
- [Current state](current-state.md)
