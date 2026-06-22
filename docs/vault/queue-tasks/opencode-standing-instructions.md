# OpenCode Standing Instructions

Use this file as the stable instruction for OpenCode when working in Ahamkara.

## Startup

Before doing implementation work, read:

- [Queue README](README.md)
- [OpenCode task queue workflow](../workflows/opencode-task-queue.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Repo map](../01-repo-map.md)
- [Agent skills](../05-agent-skills.md)

## Polling

Check `docs/vault/queue-tasks/open/` for queued tasks.

If there are no queued tasks, do not invent work. Report that the queue is
empty.

If there are queued tasks, claim exactly one task and follow it.

## Claiming

When claiming a task:

1. Copy or move it from `open/` to `claimed/`.
2. Set frontmatter `status: claimed` or `status: in_progress`.
3. Keep the original task ID and filename.

Prefer normal file edits for status and report-link updates. Use shell moves only
when the queue state actually changes.

## Completion

When implementation is done, paused, or blocked:

1. Write a report in `docs/reports/subagents/`.
2. Append `docs/reports/subagents/subagent-master-log.md`.
3. Update the task frontmatter `report:` field.
4. Move or copy the task to `review-needed/` or `blocked/`.

Do not move tasks to `completed/`. Codex does that after review.

## Reporting Discipline

Separate:

- what was implemented
- what was validated
- what was not validated
- what remains risky or blocked

Name exact validation commands and results.
