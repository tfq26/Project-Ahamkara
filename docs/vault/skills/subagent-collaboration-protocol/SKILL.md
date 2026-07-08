---
name: subagent-collaboration-protocol
description: Coordinate multiple agents as disciplined teammates with shared ownership, honest status reporting, dependency awareness, and respectful handoffs. Use when more than one agent, provider, or thread is contributing to the same codebase or task and you want them to behave like colleagues instead of isolated completion engines.
---

# Subagent Collaboration Protocol

Use this protocol whenever multiple agents are working in parallel or sequentially on the same project.

Its purpose is to make agents act like teammates by standardizing:

- scope ownership
- claims of completion
- dependency handling
- handoff quality
- validation expectations

## Core Behavior

Each agent must act as if another teammate will inherit the work in 10 minutes.

That means:

- state scope clearly
- leave honest artifacts
- separate facts from assumptions
- call out blockers early
- avoid rewriting shared files casually
- make the next person faster, not slower

## Shared Rules

1. Claim only what you actually implemented.
2. Name exact validation commands.
3. Distinguish compile success from runtime success.
4. Treat other agents’ changes as legitimate unless proven conflicting.
5. Prefer additive, scoped edits over broad rewrites.
6. Leave dependency notes when touching shared files.

## Ownership Rules

At the start of work, define:

- primary scope
- allowed adjacent files
- out-of-scope areas

When touching a shared hub file such as `ui/main.js` or `src-tauri/src/commands.rs`:

- keep changes tightly grouped
- avoid opportunistic cleanup outside your slice
- mention the shared-file touch explicitly in the report

## Handoff Rules

At the end of work, always answer:

- what changed
- what was validated
- what remains uncertain
- what another agent should do next

Do this using `$subagent-reporting` when available.

## Dependency Rules

If your work depends on another unfinished slice:

- do not pretend the dependency is complete
- mark the dependency in your report
- describe whether you used a temporary assumption

Examples:

- “Assumed sync device list server support is not yet available”
- “UI ready; native renderer-crash emission still missing”

## Conflict Rules

If you encounter conflicting changes from another agent:

1. Stop broad edits in that area.
2. Minimize scope.
3. Report the conflict explicitly.
4. Avoid reverting their work unless directly instructed.

## Validation Rules

Use validation proportional to risk.

For most code slices:

- syntax or lint if relevant
- `cargo check` for Rust changes
- smoke test if it exists
- targeted tests when changing shared logic

If runtime verification is missing, say so clearly.

## Confidence Rules

Every agent should leave a confidence level:

- `high`:
  implemented and validated in the relevant execution path
- `medium`:
  implemented and compile/smoke validated, but not fully runtime-verified
- `low`:
  partial implementation, blocker, or fragile assumption remains

## Colleague Language

Use teammate-style language in reports and handoffs:

- “This slice is ready for the next agent”
- “The main unresolved dependency is…”
- “I changed X, but Y still needs runtime validation”

Avoid:

- victory language without evidence
- vague claims like “fully complete”
- hiding uncertainty

## Recommended Pairing With Other Skills

Use alongside:

- `$subagent-reporting` for artifacts

For non-Codex systems, copy the protocol sections directly into the task prompt.

## References

Read these when needed:

- `references/team-protocol.md`
- `references/handoff-checklist.md`

