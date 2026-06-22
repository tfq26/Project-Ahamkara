---
name: supervisor-loop
description: Supervise one or more implementation agents through bounded report-review-correction loops until work is complete, blocked, or ready for the user. Use when Codex or another frontier model should coordinate an agent organization; ask whether this model is acting as supervisor, worker, hybrid, or observer; maintain a shared team roster, Obsidian control plane, Kanban task board, and task notes; honor user stop or steering requests; read reports from OpenCode, Claude Code, Gemini CLI, Codex subagents, or another coding agent; issue concrete corrections or continuation instructions; verify evidence; and only report back to the user after the loop reaches a truthful terminal state.
---

# Supervisor Loop

Use this skill when one or more agents are doing implementation work and a stronger or more senior model is acting as reviewer, coordinator, and completion judge.

The goal is to keep workers moving with clear next actions, catch gaps early, make progress visible to the user, and prevent premature user-facing claims.

## Roles

- `User`: owns the goal, acceptance criteria, and steering authority.
- `Supervisor`: breaks down work, assigns tasks, reviews reports, and decides whether to continue, revise, verify, complete, or block.
- `Worker`: claims assigned work, implements, validates, and writes reports. This may be OpenCode or any code agent.
- `Hybrid`: supervises some work while also implementing a clearly scoped slice.
- `Observer`: reads state and gives advice without claiming work or issuing assignments.

## Joining A Team

At the start, if the model's role is not already explicit, ask the user to confirm:

```md
Should I join this team as `supervisor`, `worker`, `hybrid`, or `observer`?
```

After confirmation, announce the role and write or update the team state if the workspace exists.

Use these shared files by default when the user has not named another location:

```text
docs/vault/
  control/
    user-directives.md
    stop-requests.md
    steering-requests.md
  team/
    team-roster.md
    agent-task-board.md
    supervisor-decisions.md
    progress-dashboard.md
docs/reports/subagents/
  subagent-master-log.md
  YYYY-MM-DD-HHMM-scope-agent.md
```

For Obsidian-backed teams in this repo, use `docs/vault/` as the vault root and
treat task notes as the source of truth. The bundled
`assets/obsidian-agent-vault/` is only a reference template.

Use `$subagent-collaboration-protocol` when multiple agents or threads touch the same codebase.
Use `$subagent-reporting` when the worker needs to produce a structured handoff report.

## Organization Model

The supervisor maintains the organization through shared artifacts:

- `docs/vault/team/team-roster.md`: who is participating, role, scope, status,
  and last report.
- task notes: one Markdown file per task with frontmatter status, owner, progress, and stop fields.
- `docs/vault/team/agent-task-board.md`: generated or manually maintained
  Kanban view grouped by task status.
- `docs/vault/team/progress-dashboard.md`: human-readable snapshot of active
  work and agent health.
- `docs/vault/team/supervisor-decisions.md`: one decision block for each review
  turn.
- `docs/reports/subagents/subagent-master-log.md`: compact chronological report
  index.
- `docs/reports/subagents/`: full worker reports.

Use task statuses:

- `open`: ready for a worker to claim.
- `claimed`: assigned or self-claimed by a worker.
- `in_progress`: worker is actively editing or validating.
- `review_needed`: worker report is ready for supervisor review.
- `revise_needed`: supervisor requested corrections.
- `verify_needed`: implementation likely ready, but evidence is insufficient.
- `complete`: supervisor accepted the task.
- `blocked`: task cannot proceed without external input.
- `stopped`: user or supervisor stopped the task intentionally.

For multiple supervisors, assign each supervisor a domain such as `frontend`, `backend`, `testing`, `integration`, or `release`. One lead supervisor should synthesize the final user-facing answer.

## User Control Plane

The user must be able to see progress and steer the organization. Agents must treat these as authoritative control surfaces:

- `docs/vault/control/user-directives.md`: standing instructions and current priorities.
- `docs/vault/control/stop-requests.md`: individual, group, task, or global stop requests.
- `docs/vault/control/steering-requests.md`: requested changes in direction or priority.
- `docs/vault/team/team-roster.md`: agent status, heartbeat, group, current task, and stop flag.
- `docs/vault/team/progress-dashboard.md`: user-readable progress snapshot.
- `docs/vault/team/agent-task-board.md`: Kanban view.

Before each new work step, each agent must:

1. Read current directives.
2. Check its roster entry.
3. Check its current task note.
4. If a stop request applies, checkpoint work, write a report, set status to `stopped`, and stop.
5. If a steering request applies, acknowledge it in the next report and adapt.

The Obsidian vault requests cooperative stops. A process runner or server supervisor may be needed to forcibly stop non-cooperative long-running agents.

## Loop Contract

Run the loop as a sequence of review turns:

1. Capture the user goal and any explicit acceptance criteria.
2. Break the goal into task notes with clear owners or claimable open tasks.
3. Ask workers to claim scoped slices and write reports.
4. Read worker reports, changed files summaries, validation output, and known gaps.
5. Make exactly one supervisor decision per reviewed task.
6. Send the worker a concrete instruction if more work is needed.
7. Update task status, progress, dashboard, and Kanban.
8. Repeat until all required tasks are `complete`, `stopped`, or at least one blocking task is `blocked`.
9. Report to the user only after the terminal state, unless the user asks for an interim status.

Never treat "worker says done" as sufficient. Completion requires evidence.

## Supervisor Decisions

Use one of these decision labels on every review turn:

- `continue`: the direction is right, and the worker should keep implementing the next slice.
- `revise`: specific defects, inconsistencies, or missing requirements need correction.
- `verify`: implementation may be ready, but evidence is incomplete or stale.
- `complete`: acceptance criteria are satisfied, relevant validation passed, and no blocking gaps remain.
- `blocked`: the worker cannot make meaningful progress without user input, credentials, missing files, external systems, or a product decision.
- `stopped`: work was intentionally halted by user or supervisor request.

Prefer `verify` over `complete` when the only missing piece is validation.
Prefer `revise` over `continue` when there is a known defect.
Use `blocked` only when the next action depends on information or access the worker cannot obtain.

## Required Evidence

Before deciding, inspect as much of this as available:

- worker report
- files changed
- diff summary
- validation commands and output
- runtime logs when relevant
- screenshots or browser observations for frontend work
- unresolved TODOs or known gaps
- user acceptance criteria
- relevant user directives, stop requests, and steering requests

If evidence is missing, ask the worker for it with `verify` instead of guessing.

## Task Breakdown Rules

Supervisors should break work into slices that can be owned cleanly:

- one behavior or subsystem per task
- explicit in-bounds and out-of-bounds scope
- named files or areas when known
- acceptance bar that another model can verify
- dependencies on other tasks
- expected validation commands when known

Avoid splitting work so finely that coordination costs exceed implementation value.
Avoid assigning two workers to the same shared file unless there is a clear ownership split.

## Kanban Rules

Task notes are the source of truth. The Kanban board is a view.

When possible, update task note frontmatter and regenerate the board with:

```bash
python3 scripts/generate_kanban.py <obsidian-vault-path>
```

Regenerate after task creation, task claim, progress change, review decision, stop request, or completion.

## Review Checklist

For every worker report, check:

- Does the work match the original user goal?
- Are all explicit acceptance criteria addressed?
- Did the worker modify only appropriate files?
- Are validation commands named exactly?
- Do validation results actually support the claim?
- Are known gaps compatible with completion, or do they require more work?
- Is the next step specific enough for another agent to execute?
- Are there risks the user should hear about in the final response?
- Did the worker honor current stop and steering controls?

## Worker Instruction Format

When assigning or continuing work, send the worker a compact instruction:

```md
Team role: worker
Task ID: parser-001
Supervisor decision: revise

Reason:
- The report says tests were not run, but the change touches shared parsing logic.
- The diff also leaves an unused helper in `src/parser.ts`.

Next actions:
1. Add or update focused parser tests.
2. Remove the unused helper or wire it into the implementation.
3. Run `npm test -- parser`.
4. Write a new report using `$subagent-reporting`.
5. Update the task note and regenerate the Kanban board.

Completion bar:
- Parser tests pass.
- No unused helper remains.
- Report separates implemented, validated, and unverified claims.
```

Keep instructions concrete. Name files, commands, and evidence whenever possible.

## Exit Rules

Decide `complete` only when:

- the requested behavior is implemented
- relevant validation passed or the lack of validation is justified and acceptable
- known gaps are non-blocking and disclosed
- no further worker action is clearly needed

Decide `blocked` when:

- the same blocker has persisted after a reasonable retry
- the missing input or access is outside the worker's control
- continuing would produce speculative or risky changes

Decide `stopped` when:

- the user stops an agent, group, task, or the full organization
- the agent checkpointed work and wrote its report

When blocked or stopped, report the state to the user with the smallest useful set of options.

## Private Server Workspace

A private server can host this system by combining:

- SSH access for agents and user terminals
- project repositories under a shared workspace path
- an Obsidian vault stored as plain Markdown files
- a process runner for long-running agents
- Git or file sync for backup and history
- VPN or key-only SSH for access control

Read `references/private-server-workspace.md` before designing or modifying a hosted setup.

## Loop Boundaries

Set a loop limit before starting. Use 3 review turns by default, 5 for complex multi-file tasks, and more only if the user explicitly wants a long-running loop.

At the limit:

- decide `complete` if the work satisfies the exit rules
- decide `blocked` if a real blocker remains
- otherwise report a truthful partial state and the recommended next action

Do not create indefinite polish loops.

## Final User Report

When the loop ends, tell the user:

- what was completed
- what validation was run
- what remains uncertain, if anything
- whether the terminal state is complete, blocked, or stopped

Keep internal loop chatter out of the final response unless the user asks for details.

## References

Read these when needed:

- `references/loop-decision-schema.md` when creating prompts, parsing worker reports, or designing automation around this loop.
- `references/team-artifacts.md` when creating or updating the roster, task board, or supervisor decision log.
- `references/obsidian-control-plane.md` when creating an Obsidian vault, dashboard, Kanban board, or user steering workflow.
- `references/private-server-workspace.md` when setting up a self-hosted SSH workspace for projects and agents.
