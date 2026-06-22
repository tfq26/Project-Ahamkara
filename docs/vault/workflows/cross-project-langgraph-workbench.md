# Cross-Project LangGraph Workbench

Status: Active

This note rewrites the multi-agent engineering workbench concept into a leaner,
repo-first design that can be reused across projects while still using
LangGraph, LangSmith, MCP-compatible tools, and multi-model routing.

## Project Goal

Build a production-style multi-agent software engineering workbench that helps a
human collaborate with multiple AI agents in a controlled project room.

The workbench should support:

- LangGraph for explicit stateful orchestration
- LangSmith for tracing, evaluation, observability, and run comparison
- MCP-compatible tools for repo search, file reads, shell commands, docs, and
  external integrations
- multi-model routing across worker, reviewer, and escalation roles
- human approval gates for meaningful risk, cost, or architecture decisions
- persistent project memory and task state
- cross-project reuse of proven feature patterns and review workflows

The system should not behave like open-ended agent chat. It should behave like a
disciplined engineering workflow.

## Core Design Philosophy

### 1. Repo-first state, platform-second orchestration

The repo vault is the primary human-readable working memory layer.

LangGraph should orchestrate the workflow, not replace repo-local memory.

The most important durable artifacts live in the repo:

- feature briefs
- portable feature notes
- queue tasks
- worker reports
- review notes
- decision records
- system maps
- known traps
- model routing rules

LangGraph state should reference these artifacts, not hide them.

### 2. Explicit workflow over free-form debate

Agents should not talk without an end condition.

The default loop is:

```text
human request
-> supervisor/planner
-> queued task
-> worker
-> primary review
-> optional secondary review
-> complete / revise / blocked
```

### 3. Human is product owner, not micro-manager

Ask the human for:

- product direction
- architecture tradeoffs
- dependency introduction
- destructive actions
- privacy/security decisions
- final approval where needed

Do not ask the human for:

- which file to read
- whether to write tests
- whether to inspect the diff
- what a compiler error means

### 4. Separate execution from judgment

One worker should own a task implementation slice.

Review and critique should be separate from editing. For higher-risk work, a
secondary review pass should happen before final acceptance.

### 5. Cross-project reuse should focus on patterns, not blind copy-paste

The workbench should make it easy for agents to search another project's vault
for a relevant feature pattern, decision note, or report and adapt it carefully.

Example:

- a pause/menu ownership pattern from Ahamkara
- a settings apply/cancel pattern from another repo
- a queue/review flow from a tools repo

Recommended reusable note type:

- [portable-feature-template.md](../templates/portable-feature-template.md)

## Recommended Architecture

### Project-local layer

Each repo keeps its own vault, for example:

```text
docs/vault/
  features/
  systems/
  memory/
  queue-tasks/
  templates/
  workflows/
```

This layer stores project truth, project context, and active task state.

Each project should also own its model permission boundary: which worker,
primary reviewer, secondary reviewer, and optional bookkeeping models are
allowed for that project.

### Orchestration layer

LangGraph owns:

- task classification
- risk and escalation classification
- revision-count escalation
- routing between worker/reviewer roles
- retry and revision loops
- human approval checkpoints
- final decision transitions

### Observability layer

LangSmith tracks:

- which model acted
- which role it played
- what tools it used
- which vault artifacts it read
- what decision it made
- latency, cost, and token usage
- test and review outcomes

### Tool layer

Use MCP-compatible or equivalent tool interfaces where possible.

Safe default tools:

- repo search
- read file
- list files
- git diff
- docs lookup
- propose patch

Approval-gated tools:

- apply patch
- delete file
- install dependency
- modify env/config secrets
- database migration
- git push
- deploy

### Secrets Model

Secrets should be shared at the machine `Projects/` level, not copied into
every project.

Recommended path:

- `/Users/taufeeqali/Projects/.workbench-secrets.env`

Project-local configuration remains responsible for saying which models are
allowed to operate in that specific project.

Recommended project-side authorization:

- [project-model-allowlist-policy.md](project-model-allowlist-policy.md)

## Agent Roles

Keep the default role set small.

### Supervisor / Planner

Responsibilities:

- classify request type
- classify risk and escalation tier
- decide next workflow step
- decide when the human should be asked
- queue implementation tasks
- summarize final decisions

### Worker

Responsibilities:

- implement one queued task
- stay within scope
- run validation
- write report
- return task to review

### Primary Reviewer

Responsibilities:

- check scope
- catch obvious flaws
- check validation evidence
- decide `complete`, `verify`, `revise`, or `blocked` for low-risk tasks
- pass high-risk tasks to secondary review when first-pass review succeeds

### Secondary Reviewer

Responsibilities:

- review high-risk tasks after primary review
- catch subtle lifecycle, ownership, threading, and architecture issues
- send back a recommendation to the primary reviewer

### Human

Responsibilities:

- act as product owner
- approve risky actions
- steer priority and direction

## Task And Review Tiers

Use two escalation tiers.

### Low escalation

Use for:

- docs
- queue bookkeeping
- narrow low-risk refactors
- report cleanup
- simple implementation tasks with small blast radius

Flow:

```text
worker -> primary reviewer -> complete
```

### High escalation

Use for:

- lifecycle and shutdown
- render/present ordering
- threading ownership
- pause/menu/input state
- shared engine/runtime abstractions
- changes likely to hide subtle regressions

Flow:

```text
worker -> primary reviewer -> secondary reviewer -> primary reviewer final decision
```

The primary reviewer still owns the final queue state transition.

## Vault Context Strategy

For any task, the system should load only the relevant vault context:

- current feature brief
- current queue task
- relevant system maps
- known traps
- prior reports for that subsystem
- decision notes if the task touches earlier architecture choices

For cross-project reuse, the system may also load:

- a matching feature note from another repo vault
- a matching report from another repo
- a reusable pattern note from another repo

Agents should adapt patterns, not blindly copy code.

## Cross-Project Reuse Model

This workbench is intended to be reused across projects.

Each project should keep repo-local memory, but follow a shared structure.

Reusable cross-project patterns should be written so another project can search:

- what problem the feature solved
- what design constraints mattered
- what implementation pattern worked
- what mistakes happened
- what validation was required
- what was project-specific vs portable

This should be written into portable feature notes, not inferred only from
source code.

Example reusable note types:

- `portable-feature`
- `reusable-pattern`
- `review-pattern`
- `known-trap`

## LangGraph State Shape

LangGraph state should stay compact and point outward to vault artifacts rather
than duplicating them.

Suggested state fields:

- session id
- project id
- user request
- task type
- risk level
- escalation tier
- current node
- current queued task path
- active worker report path
- active review path
- open human questions
- tool calls
- files referenced
- files changed
- final decision

## Recommended Initial Graph

Start with a small graph:

```text
receive_request
-> classify_task
-> classify_risk_and_escalation
-> queue_or_plan
-> worker_execution
-> primary_review
-> maybe_secondary_review
-> final_decision
```

Possible transitions:

- `revise` -> back to queue/open
- `verify` -> remain in review-needed
- `blocked` -> blocked
- `complete` -> completed

## Model Routing

Use roles, not one-model-fits-all.

Example:

- worker:
  lower-cost coding model
- primary reviewer:
  lower-cost but reliable reviewer
- secondary reviewer:
  stronger model for high-risk tasks
- Codex:
  planner, escalation, final fallback

The exact model names can vary by project budget, but the roles should stay
stable.

## Evaluation

LangSmith should evaluate:

- correctness
- review quality
- task routing quality
- unnecessary escalations
- cost per completed task
- latency per stage
- rate of reopened tasks
- rate of subtle bugs caught by secondary review

## Recommendation

Do not start by building the full “platform room” product surface.

Start by formalizing the repo-native loop and instrumenting it:

1. vault artifacts
2. queue tasks
3. worker reports
4. primary review
5. secondary review for high escalation
6. LangGraph orchestration
7. LangSmith traces

Then grow the dashboard around that proven workflow.
