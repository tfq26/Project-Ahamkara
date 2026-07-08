---
name: lean-implementation
description: Apply YAGNI-first implementation discipline before adding code, abstractions, or dependencies. Use when planning, implementing, or reviewing work and the simplest correct solution should be preferred over broad refactors or extra architecture.
---

# Lean Implementation

Use this skill when working from the Ahamkara queue and you want the worker or
reviewer to stay disciplined about keeping solutions small.

Canonical shared policy:

- [Workbench lean implementation policy](/Users/taufeeqali/Projects/workbench-vault/policies/lean-implementation-policy.md)

## Core Ladder

1. Does this need to exist?
2. Can the current code already do it?
3. Can stdlib or the platform do it?
4. Can an existing dependency do it?
5. Can this be solved with less code?
6. Only then write the minimum new code that works.

## Worker Rule

Stay inside the queued scope and prefer the smallest change that satisfies the
acceptance bar.

## Reviewer Rule

Flag unnecessary abstractions, avoidable helpers, and broader-than-needed
refactors as `revise`.

## Shared-First Reminder

Do not restate the full policy here. Link back to the shared workbench policy
when you need the canonical version, and keep this note as the local pointer.

## Detailed Reference

For the fuller project-local version of this skill, see
[lean-implementation/SKILL.md](lean-implementation/SKILL.md).
