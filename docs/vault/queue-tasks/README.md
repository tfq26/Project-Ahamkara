# Queue Tasks

This folder is the repo-local task queue between Codex and OpenCode.

Each queued task should contain the broken-down implementation plan, validation
bar, and reporting instructions for one scoped slice of work.

## For OpenCode

1. Read [OpenCode standing instructions](opencode-standing-instructions.md).
2. Read [OpenCode task queue workflow](../workflows/opencode-task-queue.md).
3. Read [Agent handoff](../../guides/agent-handoff.md).
4. Pick one task plan from `open/`.
5. Claim it in `claimed/`.
6. Implement within scope.
7. Write a report in `../../reports/subagents/`.
8. Put the task in `review-needed/` with the report linked.

Do not put a task in `completed/`; Codex does that after review.

## For Codex

Queue broken-down task plans in `open/` using
[opencode-queued-task.md](../templates/opencode-queued-task.md).

Review OpenCode work from `review-needed/`.
Use [Codex review workflow](../workflows/codex-review-workflow.md) and
[Codex review template](../templates/codex-review-template.md).

## Active Planning

- [Client Runtime Cleanup Push](../features/2026-06-15-client-runtime-cleanup.md)
- [Open task queue](open/README.md)

## States

- `open/` - ready for OpenCode
- `claimed/` - in progress
- `review-needed/` - OpenCode finished or paused and wants Codex review
- `completed/` - Codex accepted
- `blocked/` - needs user or Codex input

## Queue Invariants

Use [queue-state-invariants.md](../workflows/queue-state-invariants.md) to keep
one task in one state only.
