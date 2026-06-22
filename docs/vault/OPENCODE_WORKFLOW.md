# OpenCode Workflow

This is the canonical OpenCode workflow file for Ahamkara.

OpenCode cannot currently invoke the project-local files in `docs/vault/skills/`
as true callable skills in this setup. Treat this file and the queue-task notes
as the effective workflow source of truth.

## Core Role

OpenCode is the worker. Codex is the planner and reviewer.

OpenCode should:

1. Read the queue.
2. Claim exactly one task.
3. Implement only that scoped task.
4. Run the requested validation or explain why it was not run.
5. Write a report.
6. Update the master log.
7. Move the task to `review-needed/` or `blocked/`.

OpenCode should not:

- invent unrelated work when the queue is empty
- work multiple queued tasks at once
- move tasks directly to `completed/`
- claim validation it did not actually run

## Start Path

Before working, read:

- [Docs index](../README.md)
- [Queue tasks README](queue-tasks/README.md)
- [OpenCode standing instructions](queue-tasks/opencode-standing-instructions.md)
- [OpenCode task queue workflow](workflows/opencode-task-queue.md)

## Queue Paths

- Open: `docs/vault/queue-tasks/open/`
- Claimed: `docs/vault/queue-tasks/claimed/`
- Review needed: `docs/vault/queue-tasks/review-needed/`
- Completed: `docs/vault/queue-tasks/completed/`
- Blocked: `docs/vault/queue-tasks/blocked/`

## Reporting Paths

- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`
- Report template: `docs/vault/templates/subagent-report-template.md`

## Claiming A Task

When OpenCode starts work:

1. Read `docs/vault/queue-tasks/open/`.
2. Pick one task.
3. Move or copy it to `claimed/`.
4. Update its frontmatter to `status: claimed` or `status: in_progress`.

## Finishing A Task

When done, paused, or blocked:

1. Write a report in `docs/reports/subagents/`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update the task `report:` field.
4. Move or copy the task to:
   - `review-needed/` if ready for Codex review
   - `blocked/` if it cannot continue

## Claim Hygiene

Always separate:

- implemented
- build-validated
- test-validated
- runtime-confirmed

If runtime behavior was not observed, say so explicitly.

## If The Queue Is Empty

Say that the queue is empty and stop. Do not invent the next task.
