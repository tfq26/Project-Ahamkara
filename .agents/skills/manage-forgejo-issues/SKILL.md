---
name: manage-forgejo-issues
description: Manage Ahamkara, Flashback, Wish, and cross-product work through the self-hosted Forgejo Issues and Pull Requests system.
---

# Manage Forgejo Issues

Forgejo is the only current source of truth for project work. The repository is
taufeeq26/Project-Ahamkara on https://git.2helix.org. GitHub issues and PRs
are retained as read-only archive records with Forgejo backlinks.

## Safe access

Use the Forgejo web UI or authenticated Forgejo API. Read credentials from the
authorized environment or secret store; never print, paste, or commit a token.
Do not use the GitHub CLI or GitHub APIs for new work.

For API access, use FORGEJO_TOKEN and FORGEJO_URL from the environment. Avoid
shell tracing and verbose HTTP logging. Use paginated endpoints and verify
every write by reading the resulting issue or PR.

## Audit and select work

Before selecting work, inventory open and closed issues, PRs, labels, milestones,
comments, and linked branches. Reconcile them against code and tests.

Classify open issues as ready, in progress, blocked, review needed, tracking, or
stale. Confirm dependencies and existing branches/PRs before claiming work.
Tracking epics are navigation records and must not be implemented directly.

## Claim and release work

Use Forgejo-visible labels, assignees, and comments so every agent observes the
same claim state. Before claiming, refresh the issue and comments; stop if it
is closed, blocked, tracking, already in progress, assigned to another worker,
or has an unexpired claim marker.

Record the branch, scope, and validation plan in a Forgejo comment. Use the
project's existing claim labels and lease conventions. Recheck the claim after
posting; if another worker won the race, undo your claim and select another
issue.

When work is abandoned, report the branch/commit and reason, remove the active
claim, restore the open or blocked state, and update the Forgejo PR. Do not
create local task queues, copied issue bodies, or status dashboards.

## Create or repair an issue

Search for duplicates first. Use the project task title format:

TASK-YYYYMMDD-HHMM-<product>-<short-description>

Include the product owner, sequence, priority, readiness, problem, scope,
acceptance criteria, dependencies, validation commands, and completion workflow.
Use existing project labels and milestones. Be explicit about blockers and keep
acceptance criteria falsifiable.

## Review and close

Check each acceptance criterion against code or recorded evidence. A merged PR,
green build, agent summary, or checked box is not proof by itself. Require the
relevant tests, package or runtime evidence, and independent verification when
the task calls for it.

Before closing, verify the intended base branch, link the Forgejo PR/commit,
record exact validation commands and results, and preserve known limitations.
