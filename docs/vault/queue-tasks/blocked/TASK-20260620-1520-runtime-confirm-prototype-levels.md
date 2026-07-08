---
type: opencode-task
status: blocked
created: 2026-06-20
queued_by: codex
assigned_to: opencode
priority: high
escalation_tier: low
primary_reviewer: codex
secondary_reviewer:
subsystems:
  - client
  - engine/render
related_feature:
report:
requires_display: true
---

# TASK-20260620-1520-runtime-confirm-prototype-levels

## Goal

On a machine with a GL display, run the client against the prototype levels and
confirm the world-render slices actually work end to end. This closes the
runtime-confirmation gap left by TASK-20260620-1200 (level meshes),
TASK-20260620-1330 (UV/textures), and TASK-20260620-1400 (level spec/emitter).

## IMPORTANT

This task REQUIRES a GL display. It cannot be completed in a headless agent
environment. Assign it to a worker or human on a machine with a window/display.

## First Read

- [Building / running](../../../guides/building.md)
- [Level world meshes report](../../../reports/subagents/TASK-20260620-1200-level-driven-world-meshes-report.md)
- [Level spec emitter report](../../../reports/subagents/TASK-20260620-1400-level-spec-and-lvl-emitter-report.md)

## Scope

In bounds:

- Build and run the client against `assets/compiled/levels/prototype_box.aelevel`
  (e.g. `./scripts/start.sh local`, pointing the client `level_path` at it).
- Confirm and record:
  - the two `test_box` mesh instances render at the expected positions/scales,
  - the HUD / crosshair / menu are NOT overwritten by the meshes (validates the
    TASK-20260620-1200 render-order fix),
  - spawns / collision behave (movement against the level),
  - if TASK-20260620-1500 has landed, the albedo texture is visible.
- Capture a screenshot or concise observed-behavior notes.

Out of bounds:

- Any code change beyond what is needed to point the client at the level.

## Acceptance Bar

- A written confirmation (with screenshot or precise notes) that meshes render
  and overlays are intact, or a precise bug report if not.

## Review Tier

- `low`.

## Validation

```sh
./scripts/start.sh local   # or the exact client invocation with the level path
```

Report observed behavior. If run headless and unable to display, move to
`blocked/` noting the display requirement.

## Reporting Required

Standard: write report, append master log, update task `report:`/status, move to
`review-needed/` or `blocked/`.
