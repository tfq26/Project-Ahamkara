---
name: subagent-reporting
description: Write structured end-of-task reports for subagents and append concise status entries to a shared master log. Use when an agent finishes implementation, validation, debugging, review, or partial work and needs to leave a reliable handoff artifact that other agents or models can read later. Use for Codex, Gemini CLI, Claude Code, or any agent workflow that benefits from provider-neutral reporting discipline.
---

# Subagent Reporting

Write a full task report and append a compact master-log entry every time work ends in one of these states:

- implemented
- implemented_not_validated
- validated_with_known_gaps
- blocked

Treat the report as a machine-readable handoff for other agents, not as a marketing summary.

## Shared Paths

Use these shared workspace paths unless the caller explicitly overrides them:

- Master log:
  `docs/reports/subagents/subagent-master-log.md`
- Report folder:
  `docs/reports/subagents/`
- Report template:
  `docs/vault/templates/subagent-report-template.md`

For this project, preserve the same repo-relative structure:

```text
Ahamkara/
  docs/
    reports/
      subagents/
        subagent-master-log.md
        YYYY-MM-DD-HHMM-scope-agent.md
    vault/
      templates/
        subagent-report-template.md
```

## Required Workflow

1. Determine the run status truthfully.
2. Write one full report file in `docs/reports/subagents/`.
3. Append one short summary entry to
   `docs/reports/subagents/subagent-master-log.md`.
4. Keep claims separated from evidence.
5. Name exact commands used for validation.

Do not skip the master-log append just because the full report exists.

## Report Naming

Use this naming pattern:

`YYYY-MM-DD-HHMM-scope-agent.md`

Examples:

- `2026-06-01-2140-sync-confidence-agent.md`
- `2026-06-01-2215-session-recovery-agent.md`

If the exact time is unavailable, use a stable timestamp or sequence that avoids collisions.

## Required Report Sections

Every full report must contain these sections in this order:

1. `Task`
2. `Status`
3. `Scope`
4. `Files Changed`
5. `What Changed`
6. `Validation Run`
7. `Validation Results`
8. `Known Gaps`
9. `Runtime Risks`
10. `Cross-Agent Dependencies`
11. `Recommended Next Step`
12. `Confidence`

Use the template in `docs/vault/templates/subagent-report-template.md`.

## Content Rules

Follow these rules exactly:

- `Task`:
  one or two sentences describing the assignment
- `Status`:
  one of the four canonical statuses only
- `Scope`:
  what was intentionally in-bounds and out-of-bounds
- `Files Changed`:
  flat bullet list with absolute or repo-relative paths
- `What Changed`:
  concrete behavior, not intentions
- `Validation Run`:
  exact commands
- `Validation Results`:
  pass/fail plus notable warnings
- `Known Gaps`:
  real unresolved limitations
- `Runtime Risks`:
  things that may break in actual usage despite compile/smoke success
- `Cross-Agent Dependencies`:
  files or systems another agent must understand before continuing
- `Recommended Next Step`:
  one specific follow-up
- `Confidence`:
  `high`, `medium`, or `low` with one sentence

Never write “done” unless the requested slice is actually implemented and validated.
Never hide uncertainty behind optimistic wording.

## Master Log Entry Format

Append one compact block per run to the master log using this structure:

```md
## 2026-06-01 21:40 — sync-confidence-agent
Status: validated_with_known_gaps
Scope: Sync health, device identity, conflict handling
Files: src-tauri/src/storage/sync.rs; src-tauri/src/commands.rs; ui/js/auth.js
Validation: cargo check; bun run smoke:phase1
Top Risk: conflict bookkeeping is partial
Next: wire pending conflict counts into stored health
Report: ./2026-06-01-2140-sync-confidence-agent.md
```

Keep it short. The full report holds the detail.

## Claim Hygiene

Use this distinction consistently:

- `implemented`:
  code exists
- `validated`:
  code was exercised by named checks
- `confirmed in runtime`:
  behavior was actually observed end-to-end

Do not collapse these into one statement.

Bad:

- “Fully complete and working”

Better:

- “Implemented and compile-validated; runtime behavior still needs browser verification”

## Provider-Neutral Guidance

This skill is designed to work across Codex, Gemini CLI, Claude Code, and any other agent system.

If the host supports “skills”, load this skill directly.
If the host only supports prompt files, paste the relevant sections from this skill and the template into the prompt.

## References

Read these when needed:

- `references/reporting-spec.md`
- `references/master-log-spec.md`
