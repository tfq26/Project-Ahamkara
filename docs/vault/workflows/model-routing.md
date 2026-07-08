# Model Routing

Status: Active

This note maps task types to worker, primary reviewer, secondary reviewer, and
Codex escalation.

## Roles

- `worker model` - executes queued tasks
- `primary reviewer` - lower-cost review pass
- `secondary reviewer` - stronger review pass for `high` escalation tasks
- `Codex` - planner, escalation, bug hunter, and final fallback

## Escalation Guidance

- `low` escalation:
  worker -> primary reviewer -> complete
- `high` escalation:
  worker -> primary reviewer -> secondary reviewer -> primary reviewer final
  decision

## Practical Suggestion

- worker:
  cheaper coding model
- primary reviewer:
  cost-aware but reliable reviewer
- secondary reviewer:
  stronger model reserved for `high` escalation
- Codex:
  use after repeated revision loops or when architecture is genuinely contested

## Ownership Rule

Secrets may be shared across projects on the same machine, but model permission
and role assignment remain project-specific.

Recommended split:

- shared secrets file in `Projects/`
- per-project routing and overrides in the project's own config

Project-side authorization should follow:

- [Project model allowlist policy](project-model-allowlist-policy.md)
