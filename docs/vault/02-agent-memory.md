# Agent Memory

Status: Active

Agent memory is the durable project context that should survive beyond one chat
thread or one coding session.

## Memory Notes

- [[memory/current-state]] - Current durable understanding of the repo.
- [[memory/open-questions]] - Questions that may affect future architecture or
  implementation work.
- [[memory/decision-log]] - Lightweight decision records.

## When To Write Memory

Write or update memory when:

- A feature changes project direction.
- A confusing subsystem has been mapped clearly.
- A repeated failure mode has a known cause or workaround.
- A multi-agent handoff would otherwise lose important context.
- A decision should be visible to future humans and agents.

## When Not To Write Memory

Avoid memory updates for:

- Tiny local edits with no lasting context.
- Facts that are already obvious from code names or tests.
- Temporary guesses that have not been checked.
- Generated command output that belongs in a task summary instead.

## Suggested Handoff Format

Use [[templates/agent-handoff]] for substantial work. Keep handoffs short enough
that another agent can act on them quickly.

For formal subagent reports, use [[05-agent-skills]] and
[[templates/subagent-report-template]].
