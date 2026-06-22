# OpenCode Task Queue Workflow

Status: Active

This workflow lets Codex queue implementation work for OpenCode through
Markdown files in the repo, then review OpenCode's completion report later.

## Why This Exists

The goal is to avoid copy/pasting a Codex plan into OpenCode. Codex writes a
broken-down task file. OpenCode reads the queue, executes the task, writes a
report, and places the task into review.

## Queue State Machine

```text
open -> claimed -> review-needed -> completed
                 -> blocked
                 -> open       # if Codex asks for revision
```

## Queue Folders

- `docs/vault/queue-tasks/open/` - tasks ready for OpenCode.
- `docs/vault/queue-tasks/claimed/` - tasks OpenCode is actively working.
- `docs/vault/queue-tasks/review-needed/` - tasks OpenCode says are ready for
  Codex.
- `docs/vault/queue-tasks/completed/` - tasks Codex accepted.
- `docs/vault/queue-tasks/blocked/` - tasks that need user/Codex input.

## Codex Responsibilities

- Queue one small, executable task at a time.
- Include step-by-step implementation guidance.
- Link relevant system maps and feature briefs.
- Define validation and acceptance clearly.
- Set `escalation_tier` to `low` or `high`.
- Review reports against evidence, not just completion claims.

## OpenCode Responsibilities

- Use [OpenCode standing instructions](../queue-tasks/opencode-standing-instructions.md)
  as the stable startup prompt for this repo.
- Claim one task before editing.
- Keep the task status accurate.
- Implement within scope.
- Run validation or explain why it was not run.
- Write a formal subagent report.
- Put completed work in `review-needed/`, not `completed/`.

## Review Trigger

OpenCode signals Codex by moving or copying a task into
`docs/vault/queue-tasks/review-needed/` and linking its report. Codex can then be
asked to "check the OpenCode queue."

Automatic wakeups require an external watcher or automation. The repo queue is
the durable handoff layer.

## Related

- [OpenCode task queue skill](../skills/opencode-task-queue/SKILL.md)
- [OpenCode queued task template](../templates/opencode-queued-task.md)
- [Ahamkara reporting profile](ahamkara-reporting-profile.md)
- [Feature task workflow](feature-task-workflow.md)
- [Review escalation policy](review-escalation-policy.md)
