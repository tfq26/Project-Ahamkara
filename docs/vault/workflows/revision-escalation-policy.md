# Revision Escalation Policy

Status: Active

Use this policy to prevent cheap reviewers or workers from looping indefinitely.

## Revision Count Rules

- revision `0` -> normal worker retry allowed
- revision `1` -> normal worker retry allowed with a concrete review note
- revision `2` -> escalate review or planning strength
- revision `3+` -> Codex or equivalent stronger planner should take over,
  re-scope, or rewrite the task

## Escalation Suggestions

- `low` escalation tasks:
  after 2 failed reviews, promote to `high`
- `high` escalation tasks:
  after 2 failed reviews, require secondary review if not already used
- after 3 failed reviews:
  route to stronger planner/supervisor and rewrite the task

## Rule

Every review that sends a task back to `open/` should increment `revision`.
