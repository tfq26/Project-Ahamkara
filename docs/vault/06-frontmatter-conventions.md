# Frontmatter Conventions

Status: Active

Use lightweight frontmatter on new vault notes, task notes, and formal reports
when it helps agents or Obsidian filter information.

## Common Fields

```yaml
---
type: feature-brief
status: draft
created: 2026-06-14
last_verified: 2026-06-14
subsystems: [client, game]
source_of_truth:
  - ../../game/src/world.cpp
---
```

## Types

- `repo-map`
- `system-map`
- `feature-brief`
- `task-note`
- `decision`
- `handoff`
- `subagent-report`
- `workflow`
- `memory`

## Status Values

- `draft` - useful but still forming
- `active` - current enough to guide work
- `needs-verification` - likely useful, but source/code should be checked first
- `historical` - retained for context, not current truth
- `superseded` - replaced by a newer note

## Rule

Frontmatter helps discovery, but links keep the graph alive. New notes should
link to at least one hub note such as [Start Here](00-start-here.md),
[Repo Map](01-repo-map.md), [Agent Skills](05-agent-skills.md), or a system map.
