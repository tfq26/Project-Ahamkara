# Feature Development

Status: Seed

Use this note as the agent-friendly feature workflow. It complements the main
repo workflow docs and keeps feature intent discoverable in Markdown.

## Before Changing Code

1. Read [[00-start-here]] and [[01-repo-map]].
2. Check [[memory/current-state]] and [[memory/open-questions]].
3. Read the canonical docs for the subsystem being changed.
4. Inspect existing source and tests before designing new abstractions.

## Feature Briefs

Create a feature brief in `features/` for work that spans multiple files,
multiple agents, or multiple sessions. Start from [[templates/feature-brief]].
For the full flow, use [[workflows/feature-task-workflow]].

If OpenCode should implement the next slice, queue a task with
[[workflows/opencode-task-queue]] and [[templates/opencode-queued-task]].

Good feature briefs capture:

- goal
- non-goals
- affected systems
- implementation sketch
- tests or verification path
- open risks

## During Implementation

- Keep one branch/workspace responsible for one active line of work.
- Prefer small changes that can be reviewed independently.
- Update the feature brief if the implementation direction changes.
- Link to relevant source files, tests, and docs instead of copying large
  details into the vault.

## After Implementation

- Add a short handoff note if another agent may continue the work.
- Append meaningful decisions to [[memory/decision-log]].
- Update [[memory/current-state]] if the repo-wide state changed.
- Write a formal report with [[workflows/ahamkara-reporting-profile]] when a
  meaningful reporting boundary is reached.
