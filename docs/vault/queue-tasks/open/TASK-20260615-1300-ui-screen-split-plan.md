---
type: opencode-task
status: open
created: 2026-06-15
queued_by: codex
assigned_to: opencode
priority: medium
subsystems:
  - client
  - docs
related_feature: ../../features/2026-06-15-client-runtime-cleanup.md
report:
---

# TASK-20260615-1300-ui-screen-split-plan

## Goal

Prepare a safe first code slice for splitting the current ImGui/menu code by
screen without immediately taking on the whole UI architecture rewrite.

## Background

The cleanup inventory calls for splitting UI files by screen and separating UI
rendering from UI actions, but that work needs a scoped starting point.

## First Read

- [Docs index](../../../README.md)
- [Agent handoff](../../../guides/agent-handoff.md)
- [Client Runtime Cleanup Push](../../features/2026-06-15-client-runtime-cleanup.md)
- [Feature task workflow](../../workflows/feature-task-workflow.md)
- [Rendering map](../../systems/rendering-map.md)

## Scope

In bounds:

- inspect the current UI/menu file layout
- identify one safe first extraction, such as main menu or settings screen
- if feasible, perform that first extraction with minimal behavior change
- if extraction is not yet safe, document the exact prep work needed

Out of bounds:

- full UI framework rewrite
- full action/event system redesign
- full controller navigation pass

## Likely Files

- current UI/menu implementation files in `client/src/`
- related UI controller headers or state files
- docs if a short follow-up note is needed

## Implementation Plan

1. Inspect the current UI file responsibilities and choose one narrow screen or
   helper extraction.
2. Either perform that first extraction safely or produce a very concrete prep
   result if extraction is still too tangled.
3. Report what should be the next UI cleanup slice.

## Acceptance Bar

- The report leaves the UI cleanup in a better state for the next slice.
- If code changes are made, they are small and behavior-preserving.
- If code changes are not yet safe, the report explains exactly why and what
  should happen next.

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

If local UI behavior is checked, include the exact run command and what was
observed.

## Reporting Required

When done or blocked:

1. Write a report in `docs/reports/subagents/` using
   `docs/vault/templates/subagent-report-template.md`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update this task status and `report:` frontmatter.
4. Move or copy this task to `docs/vault/queue-tasks/review-needed/` or
   `docs/vault/queue-tasks/blocked/`.

## Notes For Codex Review

Codex should make sure this stays a scoped first UI cleanup slice and does not
pretend to finish the whole UI restructuring story.
