---
name: manage-github-issues
description: Manage Ahamkara, Flashback, Wish, and cross-product work through GitHub Issues from backlog audit and task selection through issue creation, agent handoff, PR review, dependency updates, and evidence-based closure. Use when an agent needs to inspect open or closed issues, choose work, create or refine an issue, coordinate an implementation pass, reconcile merged PRs with issue state, or maintain the project backlog.
---

# Manage GitHub Issues

Treat GitHub Issues as the only source of truth for work state, priority,
dependencies, and acceptance criteria. Keep durable architecture in `docs/`;
never recreate local task queues, status mirrors, or issue-body copies.

## Establish context

1. Resolve the repository from the local `origin`; do not guess it.
2. Read `docs/AGENTS.md`, `docs/architecture/overview.md`,
   `docs/architecture/repository-split.md`, and `docs/repo-map.md` when scope or
   ownership is relevant.
3. Prefer the connected GitHub app for issues, comments, labels, milestones,
   and PR metadata. Use authenticated `gh` only when the connector lacks the
   necessary read or write.
4. Inspect current code and tests before asserting implementation status.
   Historical reports and PR descriptions are evidence, not current truth.
5. Keep reads and writes separate. Complete the audit before changing GitHub.

## Product ownership

- **Ahamkara** owns reusable engine modules, SDK contracts, generic runtime
  hosting, engine tools, packaging, and engine tests. It must not import or name
  Flashback or Wish product types.
- **Flashback** owns gameplay, presentation, content, product configuration,
  and adapters that compose public Ahamkara and Wish contracts.
- **Wish** owns game-neutral identity, sessions, activities, replication, and
  backend/service contracts. It must build independently of Ahamkara and
  Flashback.
- **Cross-product** owns governance, release compatibility, and CI matrices;
  concrete translation between products normally belongs to a Flashback
  adapter.

If ownership is mixed, split the work at a public contract boundary before
assigning it.

## Audit the backlog

Inventory open issues, closed issues, open PRs, merged PRs, labels, milestones,
assignees, and comments. Reconcile them against one another and the code.

Classify each open issue as:

- `ready`: every blocker is closed, scope remains valid, and no active work
  duplicates it;
- `in-progress`: an active branch or PR is linked;
- `blocked`: at least one concrete prerequisite remains open;
- `review-needed`: implementation exists but acceptance evidence needs review;
- `tracking`: an epic/index that agents must not implement directly;
- `stale`: state, scope, ownership, or dependencies no longer match reality.

Flag these conditions explicitly:

- a merged PR references an issue that remains open;
- a closed issue lacks acceptance evidence;
- a blocked issue has no remaining open blockers;
- a PR implements work without linking its issue;
- duplicate or superseded issues exist;
- an issue claims code that is absent, partial, or owned by another product;
- an open PR is based on an obsolete branch or has been superseded.

Do not close, reopen, relabel, or edit during a read-only audit. Present the
proposed reconciliation first unless the user explicitly authorized updates.

## Select work for an agent

Select only a `ready` issue. Prefer critical-path correctness, packaging,
tests, and contract work over isolated feature polish. Confirm:

1. no open PR or branch already owns the scope;
2. every dependency is actually closed;
3. the issue is small enough for one agent pass;
4. acceptance criteria are objectively testable;
5. required validation is possible in the available environment;
6. the likely files and product owner agree with current architecture.

When work starts, mark it `in-progress` and link the branch or PR. Never let two
agents write the same branch or issue scope.

## Claim and release work

Use a GitHub-visible claim so every agent observes the same ownership state.
Treat only issues labeled `open` as available. Treat `in-progress` as claimed
even when the implementation branch is not visible locally.

To claim an issue:

1. Re-fetch the issue and comments immediately before claiming. Stop if it is
   closed, blocked, tracking, already `in-progress`, assigned to another
   worker, or contains a current claim marker.
2. Confirm every blocker is closed and no open PR duplicates the scope.
3. Remove the `open` label and add `in-progress`. Assign the implementing
   GitHub user when possible.
4. Add one structured comment:

```markdown
<!-- ISSUE_CLAIM owner=<agent-handle> claimed_at=<UTC-ISO8601> expires_at=<UTC-ISO8601> -->
Claimed by `<agent-handle>`.

- Branch: `agent/<agent-handle>/<short-scope>`
- Scope: <one-sentence implementation boundary>
- Expected validation: <commands or evidence>
```

5. Re-fetch the issue and comments. If another valid claim was created first,
   the earliest claim wins; undo this claim and select another issue.
6. Create or link the branch/PR promptly. Do not claim speculative future work.

Use a 72-hour claim lease unless the issue states otherwise. Before expiry,
post a heartbeat comment that replaces the effective expiry:

```markdown
<!-- ISSUE_CLAIM_HEARTBEAT owner=<agent-handle> expires_at=<UTC-ISO8601> -->
Progress: <completed work, next action, and any blocker>.
```

A claim is stale only when its latest lease expired and there is no active PR or
recent implementation evidence. Before taking over, comment with the evidence,
remove the stale assignee when appropriate, return the issue to `open`, and
then perform the normal claim sequence. Never silently steal a claim.

Release a claim when work is abandoned or materially blocked:

1. post the branch/commit state and reason;
2. remove `in-progress` and the agent assignment;
3. add `open` if the issue remains runnable, otherwise add `blocked` and name
   the concrete blocker;
4. close or mark the abandoned PR accordingly.

When implementation is ready for review, keep it unavailable to new agents:
replace `in-progress` with `review-needed` when that label exists, link the
PR, and post exact validation evidence. Do not return review work to `open`.

## Create or repair an issue

Search for duplicates before creating anything. Use title format:

`TASK-YYYYMMDD-HHMM-<product>-<short-description>`

Include:

```markdown
## Agent Execution Contract

| Field | Value |
|---|---|
| Product owner | Ahamkara / Flashback / Wish / Cross-product |
| Sequence | milestone name |
| Priority | Critical / High / Normal |
| Readiness | Ready or blocked by #numbers |

## Canonical Context

## Problem

## Scope

### In scope

### Out of scope

## Acceptance Criteria

- [ ] Observable, falsifiable result
- [ ] Regression or contract test at the owning boundary

## Dependencies and Sequence

## Validation

Exact configure, build, test, package, and runtime commands required.

## Completion Workflow

- Link the implementation PR and post exact validation results.
- File newly discovered work as linked, narrowly scoped issues.
- Close only after every acceptance criterion is proven.
```

Use exactly one product label, one priority label, one workflow-state label,
the `task` label, and the appropriate sequence milestone. Tracking issues use
`tracking` instead of runnable task/workflow labels.

Do not use vague criteria such as “works,” “clean up,” or “tests pass.” Name the
behavior, boundary, failure cases, and proof required. Do not prescribe a large
implementation when a smaller contract-focused solution can meet the outcome.

## Review an implementation pass

Review the issue body, comments, PR patch, current code, and validation results.
Check every acceptance criterion independently. A merged PR, green build, agent
summary, or checked box is not proof by itself.

Require, as applicable:

- focused regression tests for claimed behavior and failure paths;
- full documented validation for the affected configuration;
- installed-package/out-of-tree proof for public Ahamkara contracts;
- standalone build proof for Wish boundaries;
- deterministic/headless proof for simulation and networking;
- runtime or visual confirmation when compilation cannot prove the result;
- no unrelated generated files, caches, logs, or product-boundary leakage.

If implementation is partial, keep the issue open and record the exact unmet
criteria. Create a follow-up only when the remaining work is independently
valuable and the original issue can honestly close.

## Close and unblock

Before closing:

1. verify the implementation is on the intended base branch;
2. verify every acceptance criterion against code or recorded evidence;
3. record the PR/commit and exact commands/results;
4. record limitations and deferred runtime checks;
5. ensure newly discovered work is linked and scoped.

After closing, find every open issue that names it as a blocker. Mark a dependent
issue ready only when all blockers are closed and its scope is still correct.
Update stale readiness text as well as labels so humans and agents see the same
state.

## Manager cadence

Periodically produce a concise reconciliation containing:

- recently closed work and whether closure evidence is sufficient;
- active PRs and issues needing review;
- newly ready issues after dependency changes;
- stale, duplicate, or merged-but-open issues;
- architecture or maintenance gaps discovered from code review;
- the recommended next 3–5 agent-sized tasks and why they are sequenced.

Make GitHub changes only within explicit authorization. Restate exact targets
before bulk edits, issue creation, closure, or relabeling.
