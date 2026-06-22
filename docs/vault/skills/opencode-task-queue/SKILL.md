---
name: opencode-task-queue
description: Queue implementation tasks from Codex to OpenCode using repo-local Markdown files, then review OpenCode completion reports without copying prompts by hand. Use when a plan discussed with Codex should become an executable OpenCode task with step-by-step implementation instructions, validation requirements, and a completion handoff back to Codex.
---

# OpenCode Task Queue

Use this project-local skill to turn a Codex-reviewed plan into an OpenCode task
without copying prompts between tools.

## Queue Paths

- Open tasks: `docs/vault/queue-tasks/open/`
- Claimed tasks: `docs/vault/queue-tasks/claimed/`
- Review-needed tasks: `docs/vault/queue-tasks/review-needed/`
- Completed tasks: `docs/vault/queue-tasks/completed/`
- Blocked tasks: `docs/vault/queue-tasks/blocked/`
- Task template: `docs/vault/templates/opencode-queued-task.md`
- Report template: `docs/vault/templates/subagent-report-template.md`
- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`

## Codex Queue Workflow

When the user asks Codex to queue work for OpenCode:

1. Create one task file in `docs/vault/queue-tasks/open/`.
2. Use the task template exactly enough that OpenCode has concrete steps.
3. Include the first-read path, subsystem maps, likely files, validation bar,
   and reporting requirements.
4. Set `escalation_tier` to `low` or `high`.
5. Keep scope small enough for one OpenCode pass.
6. Link the task from any related feature brief.

Task naming:

```text
TASK-YYYYMMDD-HHMM-short-name.md
```

## OpenCode Worker Workflow

OpenCode should:

1. Read `docs/vault/queue-tasks/README.md`.
2. Pick one task from `docs/vault/queue-tasks/open/`.
3. Move or copy it to `docs/vault/queue-tasks/claimed/` and set status to `claimed`.
4. Follow the implementation plan.
5. Run the requested validation or explain why it was not run.
6. Write a formal report in `docs/reports/subagents/`.
7. Append `docs/reports/subagents/subagent-master-log.md`.
8. Move or copy the task to `docs/vault/queue-tasks/review-needed/` with report links.

If blocked, put the task in `docs/vault/queue-tasks/blocked/` and write a
report with status `blocked`.

## Codex Review Workflow

When Codex checks OpenCode work:

1. Read `docs/vault/queue-tasks/review-needed/`.
2. Read the linked report and master-log entry.
3. Check `git status` and `git diff`.
4. Compare implementation against the task acceptance bar.
5. For `high` escalation tasks, pass successful first-pass review to a stronger
   secondary reviewer before final completion.
6. Decide: `complete`, `revise`, `verify`, or `blocked`.
7. Move or copy the task to `completed/` only when accepted.

Codex should not treat a completed report as proof. Completion requires evidence.

## Notification Reality

This queue does not wake Codex by itself. It gives OpenCode a durable place to
signal completion. Codex can be asked to check the queue, or a future external
watcher can monitor `review-needed/` and notify a Codex thread.
