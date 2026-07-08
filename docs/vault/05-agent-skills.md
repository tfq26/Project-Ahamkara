# Imported Agent Skills

Status: Active

This repo carries project-local copies of agent coordination skills under
`docs/vault/skills/`. They are here as readable instructions for agents working
on Ahamkara, not as globally installed Codex skills.

## Imported Skills

- [[skills/subagent-reporting/SKILL]] - Structured end-of-task reporting and
  master-log updates.
- [[skills/subagent-collaboration-protocol/SKILL]] - Multi-agent scope,
  handoff, validation, and conflict behavior.
- [[skills/supervisor-loop/SKILL]] - Supervisor review loops, task boards, and
  control-plane conventions.
- [[skills/opencode-task-queue/SKILL]] - Queue Codex-planned implementation
  work for OpenCode in `queue-tasks/` and review completion reports.
- [[skills/lean-implementation/SKILL]] - YAGNI-first implementation and review
  discipline to keep workers and reviewers from overbuilding.
- [[skills/agent-worktrees/SKILL]] - Isolated git worktrees for parallel agent
  work and cleaner handoffs.

## Ahamkara Paths

Use these project-local paths instead of any external project paths:

- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`
- Report template: `docs/vault/templates/subagent-report-template.md`
- Team/control notes: `docs/vault/control/` and `docs/vault/team/`

## Project Profile

Use [[workflows/ahamkara-reporting-profile]] for report categories, subsystem
names, default validation expectations, and frontmatter.

Use [[workflows/feature-task-workflow]] for deciding when a feature brief or
formal report is worth creating.

Use [[workflows/opencode-task-queue]] when Codex should queue a task for
OpenCode instead of asking the user to copy/paste a plan manually.

## Rule

Do not write reports for other projects into this repo. Only use these skills
for Ahamkara work unless the user explicitly says otherwise.
