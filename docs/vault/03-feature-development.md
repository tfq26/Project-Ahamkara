# Feature development

Status: Active

## Before implementation

1. Read [[00-start-here]] and the relevant canonical architecture/system docs.
2. Inspect current source, tests, and target definitions.
3. Search [GitHub Issues](https://github.com/tfq26/Project-Ahamkara/issues) for
   an existing canonical issue.
4. Create or refine the GitHub issue when work needs priority, dependencies,
   acceptance criteria, or execution status.
5. Create a vault feature brief only when durable design context would remain
   useful after the issue closes.

## During implementation

- Keep one branch/worktree responsible for one scoped issue.
- Link the issue to architecture/design context instead of copying that context
  into the issue body repeatedly.
- Add tests with code changes and record commands actually run.
- Update a design doc when the mechanism changes; update a system doc when
  current behavior changes.

## After implementation

- Update the GitHub issue/PR with validation and outcome.
- Add a report only at a meaningful investigation or handoff boundary.
- Append durable decisions to [[memory/decision-log]].
- Update [[memory/current-state]] only for repo-wide direction or structure,
  not every completed issue.

See [[workflows/feature-task-workflow]] for the full handoff standard.
