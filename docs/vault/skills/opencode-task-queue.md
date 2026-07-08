---
name: opencode-task-queue
description: Queue implementation tasks from Codex to OpenCode using repo-local Markdown files, then review OpenCode completion reports without copying prompts by hand. Use when a Codex plan should become one executable OpenCode task with step-by-step implementation instructions, validation requirements, and a completion handoff back to Codex.
---

# OpenCode Task Queue

Use this skill when working from the Ahamkara repo-local queue.

## Read First

- [Queue task workflow](../workflows/opencode-task-queue.md)
- [OpenCode standing instructions](../queue-tasks/opencode-standing-instructions.md)
- [Queue tasks README](../queue-tasks/README.md)

## Queue Paths

- Open tasks: `docs/vault/queue-tasks/open/`
- Claimed tasks: `docs/vault/queue-tasks/claimed/`
- Review-needed tasks: `docs/vault/queue-tasks/review-needed/`
- Completed tasks: `docs/vault/queue-tasks/completed/`
- Blocked tasks: `docs/vault/queue-tasks/blocked/`
- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`

## Worker Rules

1. Claim exactly one task from `open/`.
2. Stay inside the queued scope.
3. Run the requested validation or explain why it was not run.
4. Write a formal report.
5. Move the task to `review-needed/` or `blocked/`.

Do not move tasks directly to `completed/`.

## Detailed Reference

For the fuller project-local version of this skill, see
[opencode-task-queue/SKILL.md](opencode-task-queue/SKILL.md).
