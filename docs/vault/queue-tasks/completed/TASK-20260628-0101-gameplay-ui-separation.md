---
type: opencode-task
status: completed
created: 2026-06-28
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer:
secondary_reviewer:
subsystems: [client, ui, render, game]
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0101-gameplay-ui-separation-report.md
---

# TASK-20260628-0101-gameplay-ui-separation

## Goal

Separate gameplay presentation from menu presentation so the crosshair/HUD/viewmodel
only appear during active gameplay, while menus own their own UI state.

## Background

The engine currently still leaks debug-era state across the gameplay/menu boundary.
This task follows the roadmap's Phase 0 cleanup direction and the engine
assessment note's recommendation to collapse ambiguity around UI ownership.
Keep this slice compatible with later HUD/fidelity work: do not bake in
crosshair or overlay assumptions that would block future post-processing or a
different HUD composition path.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Start here](../00-start-here.md)
- [Repo map](../01-repo-map.md)
- [Feature task workflow](../workflows/feature-task-workflow.md)
- [OpenCode task queue](../workflows/opencode-task-queue.md)
- [Engine assessment](../features/2026-06-28-engine-assessment.md)

## Scope

In bounds:

- Review the current menu/gameplay state flow in the client.
- Make the gameplay overlay explicit instead of relying on menu flags as a proxy.
- Keep the crosshair hidden in menus and visible in gameplay.
- Preserve current menu behavior and gameplay behavior otherwise.

Out of bounds:

- Redesigning the menu system.
- Changing gameplay balance or camera feel.
- Adding new UI screens.

## Likely Files

- `client/src/client_frame_pipeline.cpp`
- `client/src/debug_scene_bridge.cpp`
- `client/src/debug_ui_controller.cpp`
- `engine/ui/src/ahamkara_ui.cpp`
- `engine/ui/include/ae/ui/ahamkara_ui.h`

## Implementation Plan

1. Trace where menu visibility and gameplay presentation are set.
2. Introduce or clarify the gameplay overlay condition if needed.
3. Ensure the crosshair is only drawn when gameplay is active.
4. Verify menus still render and gameplay still shows the overlay.

## Acceptance Bar

- Crosshair does not appear in menus.
- Crosshair appears during active gameplay.
- Existing menu rendering still works.
- Build passes.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

Run when relevant:

```sh
cmake --build --preset debug
./scripts/run-tests.sh --preset debug
```

If validation is skipped or fails, explain why in the report.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should verify the task against the gameplay/menu boundary in code, not
just the worker's summary.
