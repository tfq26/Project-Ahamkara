# Codex Review Workflow

Status: Active

Use this workflow when OpenCode places a task in `docs/vault/queue-tasks/review-needed/`.

## Inputs

Read these before deciding:

- queued task in `docs/vault/queue-tasks/review-needed/`
- linked report in `docs/reports/subagents/`
- `docs/reports/subagents/subagent-master-log.md`
- `git status`
- `git diff`
- relevant system docs or tests
- [Review escalation policy](review-escalation-policy.md)

## Review Questions

1. Did OpenCode stay inside the queued scope?
2. Did it touch any surprising files?
3. Were the requested validation commands actually run?
4. Do the results support the report claims?
5. Are the known gaps acceptable for completion?
6. Is runtime verification still missing?
7. Should the task be `complete`, `verify`, `revise`, or `blocked`?

## Decision Meanings

- `complete` - scope satisfied, evidence acceptable, no further worker action needed
- `verify` - implementation may be right, but proof is incomplete
- `revise` - change exists, but it misses scope, quality, or evidence requirements
- `blocked` - progress depends on user input, access, or unresolved external state

## Escalation Handling

For `low` escalation tasks:

- primary reviewer can decide `complete`, `verify`, `revise`, or `blocked`

For `high` escalation tasks:

1. Primary reviewer checks for obvious flaws.
2. If flawed, send back `revise` directly.
3. If it passes the first pass, request a secondary review using
   [secondary-review-template.md](../templates/secondary-review-template.md).
4. Primary reviewer records the final decision after reading secondary feedback.

## Output

Write the review using [codex-review-template.md](../templates/codex-review-template.md).

Then update queue state:

- `complete` -> `completed/`
- `verify` -> keep in `review-needed/` or add a clarification note
- `revise` -> move back to `open/` with clear next actions
- `blocked` -> move to `blocked/`
