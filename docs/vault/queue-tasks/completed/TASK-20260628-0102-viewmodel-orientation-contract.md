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
subsystems: [render, animation, tools, assets]
related_feature: features/2026-06-28-engine-assessment.md
report: reports/subagents/TASK-20260628-0102-viewmodel-orientation-contract-report.md
---

# TASK-20260628-0102-viewmodel-orientation-contract

## Goal

Make the weapon/viewmodel orientation contract explicit and reusable so new
weapons can be authored and rendered with a consistent axis convention.

## Background

The current rifle viewmodel path is working, but the orientation logic is still
partly implicit. This task follows the roadmap's first-person camera/viewmodel
direction and reduces the chance that each future weapon needs ad hoc fixes.
Keep the contract general enough for later animation/fidelity work: the authored
axis convention should survive additional weapon models, recoil layers, and
future presentation changes without another renderer rewrite.

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

- Inspect the current weapon/viewmodel orientation path.
- Make the authored axis convention explicit in code or documentation.
- Reduce per-weapon special casing where possible.
- Keep existing rifle rendering working.

Out of bounds:

- Designing new weapons.
- Changing weapon balance, fire behavior, or animations.
- Rebuilding the entire asset pipeline.

## Likely Files

- `engine/render/src/debug_renderer.cpp`
- `engine/render/include/ae/render/debug_renderer.h`
- `tools/blender/build_viewmodel.py`
- `docs/systems/asset_pipeline.md`
- `docs/systems/rendering-map.md`

## Implementation Plan

1. Locate the current viewmodel pose/orientation logic.
2. Define the canonical authored axis convention in code or docs.
3. Make the renderer consume that convention in a reusable way.
4. Confirm the rifle still renders correctly and future weapons have a clear
   contract to follow.

## Acceptance Bar

- The weapon orientation rule is explicit, not implicit.
- The rifle still renders correctly.
- Future weapons can use the same convention without new one-off fixes.
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

Codex should check that the orientation contract is reusable, not just that one
gun looks correct.
