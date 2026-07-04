# Queue Daemon Contract

Status: Draft

This workflow defines the minimum file-based contract for a local daemon that
dispatches workers and prompts Codex review without manual copy/paste.

For the runner layout and command flow, see
[Queue Agent Runner Architecture](queue-agent-runner-architecture.md).

## Purpose

The daemon should not try to understand task quality. It should only react to
deterministic queue and report transitions.

## File Trigger Rules

Trigger a review prompt when all of the following are true:

1. A task file exists in `docs/vault/queue-tasks/review-needed/`.
2. The task frontmatter `status:` is `review-needed`.
3. The task has a current `report:` link to a file in `docs/reports/subagents/`.
4. The report file exists and uses the standard subagent report template.
5. The master log has a corresponding entry for that task.

Trigger a worker spawn when all of the following are true:

1. A task file exists in `docs/vault/queue-tasks/open/`.
2. The task is not deferred, blocked, or already claimed.
3. The task has a narrow enough scope for one worker slice.
4. The dispatcher can allocate a clean git worktree for that slice.

Do not infer completion from compiler output alone. Do not infer completion from
code diffs alone. The explicit queue state transition is the signal.

## Minimal Script Contract

A local orchestrator should expose these commands:

```sh
agent-runner next
agent-runner spawn <task-id>
agent-runner slices
agent-runner dispatch-slices
agent-runner phase <phase-number>
agent-runner watch
agent-runner review <task-id>
agent-runner sync
```

Expected behavior:

- `next`: pick the next eligible open task, claim it, create a worktree, and
  launch a worker.
- `spawn <task-id>`: start a worker for a specific task note.
- `slices`: list the current slice groups and their task membership.
- `dispatch-slices`: claim eligible open tasks by slice and launch one worker
  per slice.
- `phase <phase-number>`: read the roadmap phase block, group tasks by slice,
  and launch one worker per slice for that phase.
- `watch`: monitor queue folders, reports, and task frontmatter transitions.
- `review <task-id>`: surface the report, diff, and task note to the supervisor
  for acceptance or revision.
- `sync`: regenerate board/dashboard views from task files.

## Completion Stamp

Each report should include a small machine-readable summary block, either in a
frontmatter-like section or a fenced YAML block, containing:

- task id
- worker name
- branch name
- worktree path
- validation commands
- report path
- confidence

This gives the daemon a stable key without replacing the full human-readable
report.

## Supervisor Boundary

The daemon may notify the supervisor, but it must not decide acceptance on its
own. Only the supervisor updates the task to `completed/`, `blocked/`, or back
to `open/` after review.
