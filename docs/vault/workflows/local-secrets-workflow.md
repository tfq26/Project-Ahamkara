# Local Secrets Workflow

Status: Active

Use a local untracked secrets file for API keys instead of putting secrets into
the repo vault or shared workbench vault.

## Recommended Real Secrets File

`/Users/taufeeqali/Projects/.workbench-secrets.env`

This file should stay outside project repos and outside the shared vault, but
inside the broader `Projects/` area so all project workbenches can use the same
credential source.

## Example Template

Use:

- [workbench-secrets.example.env](../templates/workbench-secrets.example.env)

## Required Variables

From the current shared workbench routing config:

- `GOOGLE_API_KEY`
- `PRIMARY_REVIEWER_BASE_URL`
- `PRIMARY_REVIEWER_API_KEY`
- `OPENAI_API_KEY`

## Usage

Load the secrets into your shell before running the orchestrator:

```sh
source /Users/taufeeqali/Projects/.workbench-secrets.env
```

Then run the workbench as usual.

## Disable A Role

You can effectively disable a model-backed role by removing or blanking the
relevant API key or endpoint variable in the local secrets file.

Examples:

- blank `PRIMARY_REVIEWER_API_KEY` to disable the primary reviewer endpoint
- blank `OPENAI_API_KEY` to disable the secondary reviewer / escalation fallback

## Shared Secrets, Project Ownership

Use one shared secrets file for all projects, but keep model authorization
project-specific inside each project's own config.

Recommended pattern:

- secrets live in each project's root, e.g. `/Users/taufeeqali/projects/workbench-vault/.workbench-secrets.env`
- project config decides which roles and models are allowed in that project

See:

- [Project model allowlist policy](project-model-allowlist-policy.md)
- [project-model-policy-example.yaml](../templates/project-model-policy-example.yaml)

That means:

- the same machine can hold credentials for multiple providers
- each project still owns which reviewer/worker models it permits
- removing a key disables that backend globally
- removing or changing a project's model routing disables it only for that
  project

## Rule

Keep real secrets out of:

- `docs/vault/`
- the shared workbench vault
- git-tracked files
