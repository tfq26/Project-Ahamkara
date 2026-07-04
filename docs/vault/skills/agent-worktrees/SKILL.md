---
name: agent-worktrees
description: Use git worktrees to isolate agent branches and let multiple writers work safely in parallel. Use when an agent needs a separate checkout for a subtask, when a parent agent is coordinating subagents, or when a dirty workspace should not be shared across concurrent work.
---

# Agent Worktrees

Use this skill when an agent should work in an isolated git worktree instead of
sharing the main checkout.

## When To Use

Use this skill when:

- multiple agents need to work at the same time
- a subagent needs a clean isolated checkout
- the main workspace is dirty but work still needs to continue
- a task is large enough to benefit from separate branches or checkouts

Do not use this skill to justify parallel edits in the same files without a
clear ownership split.

## Read First

- `../../guides/remote-agent-workflow.md`
- `../../guides/agent-handoff.md`
- `../subagent-collaboration-protocol/SKILL.md`
- `../subagent-reporting/SKILL.md`

## Core Rules

1. One active writer per worktree.
2. One branch per worktree.
3. One task slice per worktree.
4. Do not share a branch across simultaneous writers.
5. Keep the parent checkout for integration and review.

## Recommended Setup

Start from the parent repository root:

```sh
git fetch origin
git worktree add ../ahamkara-subagent-ui -b agent/codex/ui-pass origin/main
```

Then run the task inside the new directory.

Suggested branch naming:

- `agent/<agent-name>/<task-name>`
- `feature/<feature-name>`
- `fix/<bug-name>`

## Worker Workflow

1. Confirm the branch name and task scope.
2. Create a worktree from a clean base branch.
3. Make only the task's scoped changes.
4. Run the requested validation in that worktree.
5. Write the report in `docs/reports/subagents/`.
6. Return the branch or diff to the parent agent for review.

## Subagent Mode

If the model can spawn subagents, use them to split a large task into narrow
slices and give each subagent its own worktree.

Rules:

- One subagent per worktree.
- One task slice per subagent.
- Subagents should commit only their own slice.
- Subagents must write reports into `docs/reports/subagents/`.
- The parent agent remains responsible for integration, review, and final
  queue updates.

Use this mode when the roadmap or task list is broad enough that parallel,
isolated writers will reduce coordination risk rather than increase it.

## Parent Workflow

1. Assign a narrow slice to each worker.
2. Give each worker its own worktree.
3. Review worker reports and diffs from the parent checkout.
4. Integrate accepted changes.
5. Remove the worktree when the branch is done.

## Safe Cleanup

```sh
git worktree list
git worktree remove ../ahamkara-subagent-ui
git branch -d agent/codex/ui-pass
```

## Notes

- Do not commit from the wrong checkout.
- Do not let two workers edit the same branch concurrently.
- If a task depends on a shared file, keep the worktree slice smaller and make
  the ownership explicit in the report.
