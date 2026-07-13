---
name: github-issues
description: Create and view GitHub issues using the `gh` CLI. Use when an agent needs to file a new issue (bug, task, feature request) or list/search existing issues to find work or context. Provider-neutral; works with any agent that can run shell commands.
---

# GitHub Issues

Use the `gh` CLI to create and view GitHub issues in this repository.

## Prerequisites

- `gh` must be installed and authenticated (`gh auth status`).
- Working directory should be inside the repo, or pass the `--repo owner/repo` flag to every command (e.g. `--repo taufeeqali/Ahamkara`).

## Listing Issues

### Default list (open, newest first)

```bash
gh issue list
```

### With filters

Use `--label`, `--state`, `--assignee`, `--search`, and `--limit`:

```bash
# Open issues with the "task" label
gh issue list --label task

# Open issues with "priority-critical" label
gh issue list --label priority-critical --label task

# All issues (open + closed)
gh issue list --state all

# Search by keyword
gh issue list --search "weapon"

# Limit the number of results
gh issue list --limit 20
```

### JSON output (machine-readable)

Use `--json` with the fields you need for programmatic consumption:

```bash
gh issue list --limit 10 --label task --json number,title,state,labels,createdAt,body
```

Useful fields: `number`, `title`, `state`, `labels`, `body`, `author`, `assignees`, `comments`, `createdAt`, `updatedAt`, `milestone`.

### View a single issue in detail

```bash
gh issue view <NUMBER>
gh issue view <NUMBER> --comments
gh issue view <NUMBER> --json number,title,body,comments,labels,state
```

## Creating Issues

### Minimal issue

```bash
gh issue create --title "TITLE" --body "BODY"
```

### With labels

```bash
gh issue create \
  --title "TASK-20260712-2230-ahamkara-fix-shader-error" \
  --body "## Agent Execution Contract\n\n| Field | Value |\n|---|---|\n| Product owner | Ahamkara |\n| Sequence | 1 — Ahamkara engine |\n| Priority | High |\n| Readiness | Open |\n\n## Problem\n\nDescription of the issue.\n\n## Scope\n\nWhat is in and out.\n\n## Acceptance Criteria\n\n- [ ] Criterion one\n- [ ] Criterion two\n\n## Dependencies and Sequence\n\n- No blocking issue." \
  --label task \
  --label open \
  --label priority-high \
  --label 'product:ahamkara' \
  --milestone '1 — Ahamkara engine'
```

### With assignee

```bash
gh issue create --title "..." --body "..." --assignee @me
gh issue create --title "..." --body "..." --assignee username
```

### With milestone

```bash
gh issue create --title "..." --body "..." --milestone "v0.2"
```

### Body from a file

```bash
gh issue create --title "..." --body-file body.md
```

## Issue Conventions — Ahamkara

This project follows these conventions for agent-driven issues:

### Title format for task issues

```
TASK-YYYYMMDD-HHMM-short-description
```

Examples:
- `TASK-20260713-1000-ahamkara-error-identity-foundation`
- `TASK-20260704-1010-flashback-weapon-fire-control`
- `TASK-20260704-1610-wish-activity-session-framework`

Include the product name in the slug so ownership is visible without opening
the issue.

### Label taxonomy

| Label | Color | Meaning |
|-------|-------|---------|
| `task` | blue | This is a work item for the agent pipeline |
| `open` | green | Ready for an agent to pick up |
| `blocked` | red | Cannot proceed; depends on something else |
| `in-progress` | yellow | Implementation is active; do not duplicate it |
| `priority-critical` | dark red | Must be done immediately |
| `priority-high` | pink | Should be done soon |
| `priority-normal` | light green | Standard priority |
| `product:ahamkara` | purple | Owned by the reusable engine/SDK |
| `product:flashback` | blue | Owned by the Flashback game |
| `product:wish` | orange | Owned by the independent Wish platform |
| `product:cross-product` | gray | Governance or released-contract integration |
| `tracking` | gray | Non-runnable index/epic; agents select child issues |

Every task issue must have exactly:

- The `task` label
- One product label
- A priority label (`priority-normal`, `priority-high`, or `priority-critical`)
- One sequence milestone
- One workflow-state label: `open`, `blocked`, or `in-progress`

Only `open` is runnable. `blocked` waits for dependencies; `in-progress` has an
active branch or pull request and must identify it in the body. Tracking issues
use `tracking` and must not carry `task`, `open`, `blocked`, or `in-progress`.

### Sequence milestones

| Milestone | Exit intent |
|---|---|
| `0 — Baseline and governance` | Repository/build/CI contracts are trustworthy |
| `1 — Ahamkara engine` | A standalone non-Flashback consumer uses installed Ahamkara packages |
| `2 — Flashback game` | Flashback builds from released Ahamkara contracts |
| `3 — Wish platform` | A dummy independent activity uses released Wish contracts |
| `4 — Cross-product integration` | Flashback adapters compose released Ahamkara and Wish contracts |

Do not use milestone placement as a substitute for direct dependency links.
If an issue requires another issue to close, label it `blocked` and list every
blocker by issue number.

### Priority meaning

- `priority-critical` — on the current critical path or required to make the
  repository trustworthy; agents choose these first.
- `priority-high` — important dependency or near-term product capability, but
  not ahead of unresolved critical work.
- `priority-normal` — valid sequenced work that should not displace higher
  priority blockers.

Priority does not override blocking. A critical blocked ticket is not
runnable.

### Issue body structure

Use markdown with these sections:

```
## Agent Execution Contract

| Field | Value |
|---|---|
| Product owner | Ahamkara / Flashback / Wish / Cross-product |
| Sequence | milestone name |
| Priority | Critical / High / Normal |
| Readiness | Ready or blocked by issue numbers |

## Canonical Context

- architecture and product-boundary links

## Problem

## Scope

In scope and out of scope.

## Proposed Solution

## Acceptance Criteria

- [ ] Criterion one
- [ ] Criterion two

## Dependencies and Sequence

- Blocked by #<NUMBER>.

## Validation

Exact required build/test/runtime evidence.

## Completion Workflow

- Update and close through GitHub; do not create local queue-task files.
```

Acceptance criteria must test the claimed product boundary, not merely say
"build passes." Ahamkara issues may not import Flashback/Wish types, Wish
issues must build independently, and cross-product translation belongs to a
Flashback-owned adapter.

### Closing / commenting

To close an issue:
```bash
gh issue close <NUMBER> --comment "Reason for closing"
```

To add a comment:
```bash
gh issue comment <NUMBER> --body "Your comment here"
```

To reopen:
```bash
gh issue reopen <NUMBER>
```

After closing an issue, inspect every issue that lists it as a blocker. Change
`blocked` to `open` only when all blockers are closed and the scope/acceptance
criteria are still correct.

When work begins, replace `open` with `in-progress` and link the branch or pull
request under `## Active Work`. Return it to `open` if work is abandoned, or
move it to `review-needed` according to the repository's review process once
implementation and validation are complete.

## Tips

- Use `--json` output and pipe to `jq` when you need to transform or filter results programmatically.
- For agent orchestrators polling for work, prefer `gh issue list --label task --label open --json number,title,labels,milestone --limit 20` to find ready-to-execute issues. Sort critical before high before normal; never select `blocked` or `tracking`.
- When creating issues from agent code, always include the `Co-Authored-By: Oz <oz-agent@warp.dev>` line in the issue body if the issue references agent-authored work.
