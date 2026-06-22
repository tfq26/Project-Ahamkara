# Ahamkara Reporting Profile

Status: Active

Use this profile with the project-local [subagent reporting skill](../skills/subagent-reporting/SKILL.md).

## Default Paths

- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`
- Report template: `docs/vault/templates/subagent-report-template.md`
- Historical report index: `docs/reports/subagents/README.md`

## Report Categories

Use one category when writing formal reports:

- `implementation`
- `validation`
- `bug-investigation`
- `architecture-review`
- `doc-cleanup`
- `handoff`
- `blocked`

## Ahamkara Subsystems

Use subsystem names that match the repo:

- `engine/core`
- `engine/network`
- `engine/runtime`
- `engine/render`
- `engine/collision`
- `engine/physics`
- `engine/animation`
- `engine/audio`
- `game`
- `client`
- `server`
- `wish`
- `tools`
- `assets`
- `docs`

## Evidence Expectations

For non-UI code:

```sh
cmake --build --preset debug-headless
./scripts/run-tests.sh --preset debug-headless
```

For full client, render, input, or audio behavior, add the relevant local run:

```sh
./scripts/start.sh
./scripts/start.sh network
```

When validation is omitted, the report must say why.

## Report Frontmatter

```yaml
---
type: subagent-report
category: implementation
status: implemented_not_validated
created: 2026-06-14
agent: codex
subsystems: [engine/render, client]
branch:
validation:
  - not_run
---
```

## Related

- [Subagent report template](../templates/subagent-report-template.md)
- [Known good commands](../memory/known-good-commands.md)
- [Known traps](../memory/known-traps.md)
