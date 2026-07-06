# Autonomous Scale Roadmap

Status: Living plan

This document expands the consolidated roadmap into a 10,000+ atomic task
program. The goal is to make the repo schedulable by frontier agents: each task
is small, owned, testable, and reviewable without requiring a human to play the
game by hand.

## Purpose

- Turn the long-horizon FPS plan into small queue items.
- Keep autonomous validation mandatory for gameplay-facing work.
- Make it possible for agents to test and reject changes without manual
  playthroughs.
- Provide a phase budget that can be decomposed into thousands of queue tasks.

## Tasking Rules

- One task should change one subsystem boundary or one narrowly scoped behavior
  family.
- Every task must have a machine-checkable validation bar when possible.
- If a task touches gameplay, the validation path should go through the
  autonomous harness first.
- Human-only validation is a last resort, not the default.
- Queue tasks should be small enough to review by diff and report evidence.

## Phase Budget

| Phase | Budget | Notes |
|---|---:|---|
| 11. Autonomous Validation Mesh | 850 | Agent play loops, replay, artifacts, evidence |
| 12. Traversal and Presentation Expansion | 950 | Movement, camera, viewmodel, accessibility |
| 13. Combat Sandbox Scale-Out | 1500 | Weapons, damage, recoil, attachments, ammo |
| 14. Encounter, AI, and Objective Systems | 1400 | Enemies, behaviors, scripted encounters |
| 15. Activities, Missions, and Progression | 1100 | Mission flow, loadouts, rewards, persistence |
| 16. World Scale and Destination Content | 1300 | Streaming, destination metadata, patrol content |
| 17. Social, Live Ops, and Services | 1000 | Parties, matchmaking, presence, live hooks |
| 18. Tools, Authoring, and Content Factory | 1500 | Importers, validators, batch generation, schema |
| 19. Performance, Stability, and Ship Hardening | 1100 | Budgets, crashes, threading, deterministic replay |
| 20. Platform, Accessibility, and Release Variants | 500 | Remap, accessibility, packaging, localization |

Total: 11,200 atomic tasks.

## Phase 11 - Autonomous Validation Mesh

Task families:

- Action vocabulary and input injection.
- Scenario runner and repeatable state setup.
- Movement, interaction, combat, respawn, and recovery scripts.
- Replay capture, evidence bundles, and failure artifacts.
- Queue/report integration for automated validation results.

Typical atomic tasks:

- Add one action verb to the harness.
- Add one scenario route to prove a gameplay loop.
- Add one state counter or snapshot hook for validation.
- Add one report field or artifact for failure analysis.

## Phase 12 - Traversal and Presentation Expansion

Task families:

- Locomotion variants and tuning.
- Camera states and first-person feel.
- Stance, slide, mantle, dodge, and traversal edge cases.
- Viewmodel states and presentation variants.
- Input remapping and accessibility fallbacks.

Typical atomic tasks:

- Add one movement verb.
- Add one camera transition.
- Add one accessibility toggle.
- Add one controller binding or input mapping path.

## Phase 13 - Combat Sandbox Scale-Out

Task families:

- Weapon archetypes and subfamilies.
- Fire modes, recoil, spread, reload, reserves, and ammo types.
- Damage models, crits, falloff, and shield behavior.
- Attachments, perks, mods, catalysts, and crafting hooks.
- Feedback systems for damage and combat clarity.

Typical atomic tasks:

- Add one weapon family.
- Add one damage rule.
- Add one ammo or reload rule.
- Add one feedback artifact.

## Phase 14 - Encounter, AI, and Objective Systems

Task families:

- AI senses, behavior, and squads.
- Enemy archetypes and boss variants.
- Objectives, triggers, and checkpoints.
- Encounter authoring and validation.
- Reward resolution on objective completion.

Typical atomic tasks:

- Add one enemy behavior state.
- Add one objective condition.
- Add one encounter trigger.
- Add one reward rule.

## Phase 15 - Activities, Missions, and Progression

Task families:

- Mission state machines and activity rules.
- Loadouts, inventory, gear, perks, and currencies.
- Persistence and save/load boundaries.
- Meta-progression and reward loops.
- Activity modifiers and mission variants.

Typical atomic tasks:

- Add one mission state.
- Add one inventory rule.
- Add one persistence field.
- Add one reward path.

## Phase 16 - World Scale and Destination Content

Task families:

- Sector streaming and residency.
- Destination metadata and region layout.
- Patrol spaces, social spaces, and transition zones.
- Spatial partitioning and distant-content rules.
- Streaming validation against budgets.

Typical atomic tasks:

- Add one streaming rule.
- Add one region type.
- Add one residency check.
- Add one culling/batching rule.

## Phase 17 - Social, Live Ops, and Services

Task families:

- Parties, invites, rosters, presence.
- Matchmaking and join/reconnect flow.
- Live modifiers and rotating content.
- Anti-cheat validation and telemetry hooks.
- Fallback paths for degraded service behavior.

Typical atomic tasks:

- Add one social state.
- Add one matchmaking transition.
- Add one live modifier.
- Add one telemetry event or guardrail.

## Phase 18 - Tools, Authoring, and Content Factory

Task families:

- Import/export validation.
- Batch generation and transformation.
- Higher-level content authoring helpers.
- Metadata and schema enforcement.
- Automated content tests and reports.

Typical atomic tasks:

- Add one validator.
- Add one batch tool.
- Add one schema rule.
- Add one content report.

## Phase 19 - Performance, Stability, and Ship Hardening

Task families:

- Frame-time budgets and perf gates.
- Memory budgets and leak detection.
- Threading and job system usage.
- Crash reporting and recovery evidence.
- Deterministic replay for regressions.

Typical atomic tasks:

- Add one perf metric.
- Add one budget assertion.
- Add one crash-safety guard.
- Add one replay/diagnostic hook.

## Phase 20 - Platform, Accessibility, and Release Variants

Task families:

- Accessibility options.
- Localization and text scaling.
- Packaging flavors and platform constraints.
- Save data and profile portability.
- Release-readiness checks.

Typical atomic tasks:

- Add one accessibility option.
- Add one localization rule.
- Add one packaging variant.
- Add one release check.
