# Project Model Allowlist Policy

Status: Active

Use one shared secrets file for provider credentials, but keep model permission
project-specific.

## Core Rule

Credentials are machine-level.
Model authorization is project-level.

That means:

- `/Users/taufeeqali/Projects/.workbench-secrets.env` may hold credentials for
  many providers
- each project decides which model refs may actually be used in that project

## Recommended Project Config Shape

Each project config should include a model allowlist section:

```yaml
model_policy:
  allowed_roles:
    worker:
      - worker_model
    primary_reviewer:
      - primary_reviewer_model
    secondary_reviewer:
      - secondary_reviewer_model
    classifier:
      - classifier_model
    bookkeeping_reviewer:
      - bookkeeping_reviewer_model

  denied_model_refs: []
```

## Optional Stronger Form

If you want stricter separation, support explicit approval tiers:

```yaml
model_policy:
  allowed_roles:
    worker:
      - worker_model
    primary_reviewer:
      - primary_reviewer_model
    secondary_reviewer:
      - secondary_reviewer_model

  role_requirements:
    secondary_reviewer:
      escalation_tier: high
    bookkeeping_reviewer:
      task_type:
        - docs
        - bookkeeping
```

## Enforcement Rule

Before invoking any model, the orchestrator should verify:

1. role is allowed for the project
2. referenced `model_ref` is allowed for that role in the project
3. the required env vars exist in the shared secrets file / environment

If any check fails:

- do not invoke the model
- raise a project-policy error
- optionally route to a human approval checkpoint or blocked state

## Why This Exists

This lets you:

- share credentials across projects
- keep risky models disabled for specific repos
- override worker/reviewer choices per project
- control which models are allowed without copying secrets around

## Related

- [Local secrets workflow](local-secrets-workflow.md)
- [Model routing](model-routing.md)
