# Obsidian Control Plane

Use Obsidian as the human-visible control plane for an agent organization.

The vault is shared Markdown state. Agents may edit it directly through the filesystem, while the user can inspect and steer it through Obsidian.

## Source Of Truth

Use task notes as the source of truth. The Kanban board and progress dashboard are views derived from task notes and roster state.

Recommended vault layout:

```text
00-Control/
  user-directives.md
  stop-requests.md
  steering-requests.md
01-Roster/
  team-roster.md
02-Dashboard/
  progress-dashboard.md
  agent-task-board.md
03-Tasks/
  TASK-000-template.md
04-Reports/
  subagent-master-log.md
  subagent-reports/
05-Decisions/
  supervisor-decisions.md
06-Notes/
07-Done/
```

## User Steering

The user can steer at four levels:

- global: applies to all agents and tasks
- group: applies to a group such as `frontend`, `backend`, `testing`, or `docs`
- agent: applies to one named agent
- task: applies to one task ID

Agents must check controls before starting a new step, before editing a shared file, after validation, and before claiming completion.

## Stop Semantics

Use cooperative stop semantics:

1. Stop changing project files.
2. Save or revert only work the agent explicitly owns.
3. Write a short report with current state, files touched, validation run, and safe next step.
4. Set agent or task status to `stopped`.
5. Leave the final decision to the supervisor or user.

Do not silently ignore a stop request because work is "almost done."

## Progress Semantics

Use frontmatter `progress` as a rough percentage:

- `0`: task created but not claimed
- `10`: claimed
- `25`: implementation started
- `50`: core implementation exists
- `70`: validation started
- `85`: report written and review needed
- `95`: verification/revision only
- `100`: supervisor accepted complete

Progress is not a substitute for status. A task can be `blocked` at `70`.

## Kanban

Prefer generated Kanban:

```bash
python3 scripts/generate_kanban.py <obsidian-vault-path>
```

Generated Kanban prevents drift between task frontmatter and the visual board.

If the Obsidian Kanban plugin is used, keep the same columns:

- Open
- Claimed
- In Progress
- Review Needed
- Revise Needed
- Verify Needed
- Blocked
- Stopped
- Complete

## Dashboard

The dashboard should answer:

- What is active right now?
- Which agents are healthy?
- Which tasks need review?
- What is blocked or stopped?
- What changed most recently?
- What can the user steer?

Keep the dashboard concise. Detailed evidence belongs in reports and decision logs.
