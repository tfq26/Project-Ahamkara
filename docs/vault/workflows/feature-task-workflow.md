# Feature and issue workflow

Status: Active

## First read

1. [Docs index](../../README.md)
2. [Architecture overview](../../architecture/overview.md)
3. [Repository map](../../repo-map.md)
4. Relevant design/system docs
5. Current source, tests, and CMake targets

## Work record

Use one canonical GitHub issue for scope, state, dependencies, acceptance
criteria, and implementation discussion. Follow the
[GitHub issue skill](../skills/github-issues/SKILL.md).

Create a feature brief under `docs/vault/features/` only for durable context
that should remain after the issue closes. Do not copy the issue body.

## Report triggers

Write a historical report only for a meaningful boundary:

- non-obvious investigation result;
- major implementation handoff;
- validation failure another contributor must reproduce;
- architecture review with evidence.

Use [the reporting profile](ahamkara-reporting-profile.md).

## Completion bar

The issue/PR handoff says what changed, what was validated, what was not
validated, what risks remain, and whether the acceptance criteria are fully
met. Durable behavior/ownership changes update canonical docs.
