---
type: opencode-task
status: open
created:
queued_by: codex
assigned_to: opencode
priority: normal
escalation_tier: low
primary_reviewer:
secondary_reviewer:
subsystems: []
related_feature:
report:
---

# TASK-YYYYMMDD-HHMM-short-name

## Goal

Describe the implementation outcome in one or two sentences.

## Background

Summarize the Codex/user discussion and link relevant docs or feature briefs.

## First Read

- [Docs index](../../README.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Start here](../00-start-here.md)
- [Repo map](../01-repo-map.md)
- [Feature task workflow](../workflows/feature-task-workflow.md)
- [Ahamkara reporting profile](../workflows/ahamkara-reporting-profile.md)

## Scope

In bounds:

- Item.

Out of bounds:

- Item.

## Likely Files

- `path/to/file`

## Implementation Plan

1. Step.
2. Step.
3. Step.

## Acceptance Bar

- Requirement.
- Requirement.

## Review Tier

- `low` - primary reviewer signoff only
- `high` - primary reviewer plus secondary reviewer before final completion

## Validation

Run when relevant:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
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

Codex should review the report, task diff, validation evidence, and acceptance
bar before accepting completion.
