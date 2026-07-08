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
subsystems: [assets, tools, engine/render]
related_feature: features/2026-06-28-engine-assessment.md
report:
---

# TASK-20260628-0104-textured-material-authoring-slice

## Goal

Author and compile a minimal textured asset slice that proves the asset pipeline
can carry UVs, a texture, and a material into a level without relying on HDR or
other deferred rendering features.

## Background

This is the non-visual, pipeline-first half of the textured material work.
It should stay compatible with the future HDR/post path by avoiding backbuffer
assumptions and by keeping the asset contract explicit.

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

- Author or update a small UV-mapped mesh.
- Author a basic albedo texture and a material that references it.
- Add or update manifest entries so the assets compile cleanly.
- Build the imported assets and confirm the runtime can resolve the material
  texture path.

Out of bounds:

- Runtime display confirmation of the textured mesh.
- Normal/ORM maps, tangent generation, atlasing, mipmap tuning.
- HDR or post-processing changes.

## Likely Files

- `assets/manifest.assets`
- `assets/models/*`
- `assets/textures/*`
- `assets/materials/*`
- `tools/asset_importer/*`
- `docs/systems/asset_pipeline.md`

## Implementation Plan

1. Create or update the minimum textured asset set.
2. Wire the manifest entries so the importer compiles them.
3. Verify the compiled asset output and material resolution path.
4. Keep the asset naming and conventions compatible with later render work.

## Acceptance Bar

- The importer compiles the textured asset slice without errors.
- The compiled asset resolves its material texture path.
- The work does not introduce any HDR or post-processing dependency.
- Build stays green.

## Review Tier

- `low` - primary reviewer signoff only

## Validation

Run when relevant:

```sh
cmake --build --preset debug --target ahamkara_asset_importer
./build/debug/tools/ahamkara_asset_importer --manifest assets/manifest.assets
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

This slice is intentionally about authoring and import correctness. Treat the
visual confirmation as a separate later slice.
