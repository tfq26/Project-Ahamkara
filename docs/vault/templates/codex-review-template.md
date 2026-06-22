---
type: review
status: draft
created:
reviewer: codex
reviewer_role: primary
reviewer_model:
task:
report:
decision:
escalation_tier:
secondary_review:
subsystems: []
---

# Codex Review

## Task

Link the queued task being reviewed.

## Report

Link the OpenCode report being reviewed.

## Decision

One of:

- `complete`
- `verify`
- `revise`
- `blocked`

## Escalation Tier

`low` or `high`

## Scope Check

Did the diff stay within the queued task scope?

## Evidence Checked

- `git status`
- `git diff`
- validation commands
- report contents
- relevant docs/tests

## Findings

Concrete issues, confirmations, or missing evidence.

## Validation Assessment

What was actually validated, and what still is not proven.

## Secondary Review

For `high` escalation tasks, link the secondary review note and summarize what
it changed about the final decision.

## Risks

Anything that may still break or needs follow-up.

## Next Action

Exactly what should happen next:

- move to `completed/`
- move back to `open/` with revision note
- move to `blocked/`
- request more verification
