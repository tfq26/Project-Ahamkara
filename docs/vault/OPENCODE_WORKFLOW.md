# OpenCode workflow

GitHub Issues is the work source of truth. OpenCode or another worker should
not read, claim, or move local queue files.

## Start

1. Read [the docs entry point](../README.md).
2. Read the assigned GitHub issue and its dependencies.
3. Read relevant architecture/design/system docs.
4. Inspect current source, tests, CMake targets, and worktree state.
5. Confirm one bounded issue scope before editing.

Use the project-local
[GitHub issue skill](skills/github-issues/SKILL.md) for issue conventions.

## Work

- One issue scope per branch/worktree.
- Preserve unrelated user changes.
- Add tests with code changes.
- Separate implemented, build-validated, test-validated, and runtime-confirmed
  claims.
- Put durable design changes in docs; put mutable progress in the issue/PR.

## Finish

1. Run the narrow and full relevant validation.
2. Update the GitHub issue or PR with evidence and remaining risks.
3. Write a report only when the investigation or handoff has durable value.
4. Do not close the issue unless acceptance criteria are actually met.

If no issue is assigned, search existing GitHub Issues or ask for direction;
do not invent a local task.
