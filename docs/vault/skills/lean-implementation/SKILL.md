---
name: lean-implementation
description: Apply YAGNI-first implementation discipline before adding code, abstractions, or dependencies. Use when an agent is planning, implementing, or reviewing a feature, refactor, or bug fix and should prefer the simplest solution that satisfies the task while still preserving correctness, validation, security, and maintainability.
---

# Lean Implementation

Use this skill to keep agents from overbuilding.

This skill is inspired by a practical YAGNI ladder:

1. Does this need to exist?
2. Can the existing codebase already do it?
3. Can the standard library or platform do it?
4. Can an already-installed dependency do it?
5. Can this be solved with a smaller change?
6. Only then write the minimum new code that works.

## When To Use

Use this skill when an agent is:

- proposing an implementation plan
- writing a patch
- reviewing a refactor
- deciding whether to add a helper, wrapper, abstraction, or dependency
- tempted to broaden a task beyond its queued scope

Do not use this skill to justify skipping:

- required tests
- validation
- security checks
- data safety
- error handling that the task clearly needs

## Worker Rules

Before adding code, ask in order:

1. Can I solve the task by changing less code?
2. Can I reuse an existing local function, class, workflow, or subsystem?
3. Can the language or stdlib do this directly?
4. Can the platform or framework do this directly?
5. Is a new abstraction actually reducing complexity right now?

If the answer is "no" to all of those, write the smallest new code that
satisfies the queued task.

## Reviewer Rules

When reviewing, check for:

- unnecessary abstraction
- wrapper code around platform or stdlib behavior
- avoidable dependency introduction
- helpers that exist only to make the diff feel "architected"
- a larger refactor than the task required

Flag these as `revise` when they increase complexity without solving a real
problem in the queued acceptance bar.

## Queue Interaction

This skill works with the existing queue system.

For queued tasks:

- keep the implementation inside the stated scope
- prefer the smallest change that satisfies the acceptance bar
- if a simpler approach would change the plan materially, note it in the report
- if a task should be split rather than solved with one broad refactor, say so

## Safe Output Style

Good implementation note:

- "Reused existing X rather than adding a new service layer."

Good review note:

- "Functionally correct, but introduces a new abstraction without reducing local
  complexity. Requesting revise."

Bad implementation note:

- "Built a flexible framework for future cases."

Bad review note:

- "This might be overengineered maybe."

## Related

Read these when relevant:

- `../workflows/feature-task-workflow.md`
- `../workflows/review-escalation-policy.md`
- `../templates/opencode-queued-task.md`
- `../templates/codex-review-template.md`
