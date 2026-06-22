---
name: subagent-reporting
description: Write structured end-of-task reports for subagents and append concise status entries to a shared master log. Use when an agent finishes implementation, validation, debugging, review, or partial work and needs to leave a reliable handoff artifact for another agent to review later.
---

# Subagent Reporting

Use this skill when OpenCode finishes, pauses, or blocks on a queued task.

## Output Paths

- Reports: `docs/reports/subagents/`
- Master log: `docs/reports/subagents/subagent-master-log.md`
- Template: `docs/vault/templates/subagent-report-template.md`

## Required Sections

Every report should include:

1. Task
2. Status
3. Scope
4. Files Changed
5. What Changed
6. Validation Run
7. Validation Results
8. Known Gaps
9. Runtime Risks
10. Cross-Agent Dependencies
11. Recommended Next Step
12. Confidence

## Rule

Separate implemented, validated, and runtime-confirmed claims.

## Detailed Reference

For the fuller project-local version of this skill, see
[subagent-reporting/SKILL.md](subagent-reporting/SKILL.md).
