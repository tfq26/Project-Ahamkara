---
name: agent-worktrees
description: Use git worktrees to isolate agent branches and let multiple writers work safely in parallel. Use when an agent needs a separate checkout for a subtask, when a parent agent is coordinating subagents, or when a dirty workspace should not be shared across concurrent work.
---

# Agent Worktrees

Use this skill when an agent should work in an isolated git worktree instead of
sharing the main checkout.

## Read First

- [Remote agent workflow](../../guides/remote-agent-workflow.md)
- [Agent handoff](../../guides/agent-handoff.md)
- [Subagent collaboration protocol](subagent-collaboration-protocol/SKILL.md)
- [Subagent reporting](subagent-reporting/SKILL.md)

## Core Rules

1. One active writer per worktree.
2. Never let two agents edit the same branch from different checkouts.
3. Keep each worktree on its own branch.
4. Use a worktree only for the scope it was created for.
5. Clean up the worktree after merge, abandon, or completion.

## Recommended Workflow

1. Start from a clean main checkout.
2. Fetch the remote branch state.
3. Create a new branch and worktree for the task.
4. Run the task inside the isolated worktree.
5. Validate and report from that worktree.
6. Review and integrate from the parent checkout.
7. Remove the worktree when finished.

Example:

```sh
git fetch origin
git worktree add ../ahamkara-subagent-ui -b agent/codex/ui-pass origin/main
```

## Worktree Safety

- Use a branch name that makes the task owner obvious.
- Do not create two worktrees from the same branch unless you explicitly know
  the branch will stay read-only in the duplicates.
- Do not commit from the parent checkout if the task is being developed in a
  separate worktree.
- Do not merge or rebase other agents' branches inside the wrong worktree.
- If the worktree becomes dirty in an unexpected way, stop and report it.

## Parent Agent Rules

When coordinating multiple agents:

- assign each agent a separate worktree
- keep the parent checkout for review and integration
- require each agent to report files changed, validation run, and known gaps
- rerun the relevant build or tests before accepting the work

## Cleanup

After the branch is merged, abandoned, or superseded:

```sh
git worktree list
git worktree remove ../ahamkara-subagent-ui
git branch -d agent/codex/ui-pass
```

## Related

- [OpenCode task queue](opencode-task-queue/SKILL.md)
- [Supervisor loop](supervisor-loop/SKILL.md)
