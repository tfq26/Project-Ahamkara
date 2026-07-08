---
name: supervisor-loop
description: Supervise one or more implementation agents through bounded report-review-correction loops until work is complete, blocked, or ready for the user. Use when Codex is coordinating OpenCode work through the repo queue and reports.
---

# Supervisor Loop

Use this skill when Codex is acting as planner/reviewer and OpenCode is acting
as the worker.

## Codex Responsibilities

1. Queue one small task at a time.
2. Define validation and acceptance clearly.
3. Review reports against evidence, not completion claims.
4. Decide `complete`, `verify`, `revise`, or `blocked`.

## Main Files

- `docs/vault/queue-tasks/`
- `docs/reports/subagents/`
- `docs/vault/workflows/codex-review-workflow.md`

## Detailed Reference

For the fuller project-local version of this skill, see
[supervisor-loop/SKILL.md](supervisor-loop/SKILL.md).
