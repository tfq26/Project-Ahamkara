# Team Artifacts

Use these artifacts to let multiple models coordinate through the filesystem, chat messages, or any shared workspace.

## Role Handshake

If role is unknown, ask:

```md
Should I join this team as `supervisor`, `worker`, `hybrid`, or `observer`?
```

After the user confirms, write a roster entry:

```md
## codex-frontend-supervisor
Role: supervisor
Domain: frontend
Status: active
Scope: Review UI tasks and browser validation evidence
Last Seen: 2026-06-11 18:45
Current Task: none
Reports To: lead-supervisor
Notes: Owns frontend review decisions only
```

## Task Board Template

Use one block per task:

```md
## TASK-001 - Add parser tests
Status: open
Owner: unclaimed
Supervisor: codex-lead
Domain: backend
Depends On: none
Scope: Add focused tests for parser edge cases touched by the latest worker diff
Out Of Scope: Parser redesign
Acceptance Bar:
- Tests cover empty input, invalid token, and nested expression cases.
- `npm test -- parser` passes.
Last Report: none
Next Action: worker claim needed
```

Task status values:

- `open`
- `claimed`
- `in_progress`
- `review_needed`
- `revise_needed`
- `verify_needed`
- `complete`
- `blocked`

## Supervisor Decision Log Template

Append one block for each review decision:

```md
## 2026-06-11 18:50 - TASK-001 - revise
Supervisor: codex-lead
Worker: opencode-parser-worker
Evidence Checked:
- subagent-reports/2026-06-11-1845-parser-worker.md
- `git diff -- src/parser.ts tests/parser.test.ts`
- `npm test -- parser`
Reason:
- Tests cover valid input only.
- Worker report claims edge cases are handled, but no validation shows that.
Next Actions:
1. Add invalid token and empty input tests.
2. Run `npm test -- parser`.
3. Write a new report using `$subagent-reporting`.
Completion Bar:
- Edge-case tests exist and pass.
- Report names exact validation output.
User Visible Notes:
- None yet.
```

## Worker Claim Template

Workers should claim work before editing:

```md
Claiming: TASK-001
Worker: opencode-parser-worker
Scope: tests/parser.test.ts and parser behavior needed for these tests
Expected Report: subagent-reports/YYYY-MM-DD-HHMM-parser-worker.md
```

## Multi-Supervisor Rules

Use a lead supervisor when possible.

The lead supervisor:

- decomposes the user's goal into tasks
- assigns supervisor domains
- resolves conflicting supervisor decisions
- synthesizes the final user report

Domain supervisors:

- review only their assigned task area
- avoid issuing instructions outside their domain
- escalate cross-domain conflicts to the lead supervisor

Workers:

- follow the task's named supervisor
- report conflicts or overlapping edits immediately
- do not accept contradictory instructions silently
