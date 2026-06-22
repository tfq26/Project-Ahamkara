# Local Workbench UI Spec

Status: Active

This note defines the first desktop UI for the shared multi-agent engineering
workbench.

## Goal

Build a small local desktop operator console for the workbench.

The UI should make it easy to:

- choose a project
- submit a request
- inspect queue state
- inspect task, report, and review artifacts
- see which models are configured
- run common orchestration actions
- respond to human approval checkpoints

The UI does not need to be visually fancy. It should be reliable, local-first,
and easy to configure.

## Product Position

This UI is not a chatbot.

It is a local operator console for a repo-first engineering workflow built
around:

- shared workbench vault
- project-local vaults
- LangGraph orchestration
- LangSmith tracing
- queue task states
- report and review artifacts

## Recommended Stack

### Desktop shell

- `Tauri`

### Frontend

- `React`
- `TypeScript`

### Local integration strategy

Phase 1 should use a file-backed and CLI-backed approach:

- read vault files directly
- call orchestrator CLI commands
- parse outputs

Do not block MVP on a formal HTTP API.

## Core UX Principles

1. Show real task and review state, not a synthetic summary only.
2. Keep the queue visible at all times.
3. Make human approval requests obvious and interruptible.
4. Show model identity by nickname, not raw provider model id.
5. Never expose secret values in the UI.
6. Keep project-local and shared-vault boundaries understandable.

## MVP Screens

### 1. Project Selector

Purpose:

- pick a project from the shared registry
- show project path and vault path
- show whether the project is active, registered, or incomplete

Data sources:

- shared workbench registry
- project config

UI elements:

- project list
- project status
- repo path
- vault path
- quick open button

### 2. Request Composer

Purpose:

- submit a new workbench request
- resume an existing session

UI elements:

- multiline request input
- submit button
- session id input for resume
- resume button
- latest session list if available

Actions:

- `python -m workbench.server.orchestrator.run -p <project> -r "<request>"`
- `python -m workbench.server.orchestrator.run -p <project> -s <session_id> --resume`

### 3. Queue Board

Purpose:

- show queue state clearly

Columns:

- `open`
- `claimed`
- `review-needed`
- `completed`
- `blocked`

Each task card should show:

- task id
- short goal
- escalation tier
- revision count
- assigned worker if known
- linked report/review status

### 4. Task Detail Panel

Purpose:

- inspect the selected task and everything attached to it

Tabs or sections:

- task
- worker report
- primary review
- secondary review
- decision history

Show:

- frontmatter fields
- acceptance bar
- likely files
- validation requested
- current status
- revision count

### 5. Session Status Panel

Purpose:

- show the live orchestration state for the active session

Fields:

- session id
- current node
- task type
- risk level
- escalation tier
- final decision if present
- transition blocked flag
- latest human questions

### 6. Model & Policy Panel

Purpose:

- show role routing and whether credentials/config are ready

Show:

- role
- model nickname
- provider
- whether required env vars are present
- whether the model is allowed for this project

Do not show:

- API key values
- secret contents

Optional labels:

- `configured`
- `missing env`
- `denied by project`
- `escalation only`

### 7. Workbench Actions Panel

Purpose:

- give the operator common maintenance controls

Actions:

- run drift scan
- start watcher
- stop watcher
- refresh queue state
- open current project vault
- open shared workbench vault

CLI hooks:

- `python -m workbench.server.orchestrator.run -p <project> --scan-drift`
- `python -m workbench.server.orchestrator.run -p <project> --watch`

### 8. Approval / Checkpoint Panel

Purpose:

- surface human approval requests and allow response

Show:

- reason for checkpoint
- pending questions
- current session id
- resume command preview

Actions:

- answer question
- submit resume response

CLI hook:

- `python -m workbench.server.orchestrator.run -p <project> -s <session_id> --resume --human-response "<response>"`

### 9. Trace Panel

Purpose:

- show whether LangSmith tracing is active
- link out to traces when possible

Show:

- tracing enabled or disabled
- current project id
- session id
- latest known trace link if available

For MVP, a simple trace metadata panel is enough.

## Phase 1 Data Sources

Use direct file reads plus CLI calls.

### Read directly

- shared registry
- model routing
- project config
- queue task folders
- reports
- review notes
- feature briefs

### Execute via CLI

- new request
- resume session
- drift scan
- watcher

### Parse outputs

- session id
- current decision
- warnings
- checkpoint questions

## Phase 1 Architecture

### Frontend

The frontend should hold:

- selected project id
- selected session id
- selected task id
- queue board state
- detail panel state
- model/policy state

### Tauri backend bridge

Provide a small bridge for:

- reading files safely
- listing queue folders
- running orchestrator CLI commands
- streaming watcher output if needed

Do not build a large custom backend layer for MVP.

## Phase 2 Improvements

After MVP is proven:

- add a local structured API instead of shelling out for everything
- add richer session history
- add better trace linking
- add queue repair actions
- add per-project model allowlist inspection
- add better live updates from watcher events

## Phase 3 Improvements

Later:

- graph step timeline
- decision heatmap / metrics
- batch secondary review panel
- cross-project pattern search UI
- generated human views from machine records

## What Not To Build Yet

Do not build yet:

- secrets editor
- cloud auth
- multi-user collaboration
- remote deployment dashboard
- giant analytics dashboards
- embedded full code editor
- arbitrary plugin marketplace

## Data Format Direction

The system is currently still Markdown-heavy in active project coordination.

Directionally, the UI should assume a future split:

- structured machine records as canonical operational state
- rendered human-facing views as projections

For MVP, it may still read Markdown and queue files directly.

But the UI should be designed so it can later switch to:

- structured task records
- structured review records
- task truth snapshots
- event log + snapshot model

without major UX changes.

## Recommended First Implementation Order

1. Project selector
2. Queue board
3. Task detail panel
4. Request composer
5. Session status panel
6. Workbench actions panel
7. Approval panel
8. Model & policy panel
9. Trace panel

## Success Criteria

The MVP is successful if a human can:

1. open the app
2. choose Ahamkara
3. submit a request
4. see a queued task appear
5. inspect the task, report, and review notes
6. respond to a checkpoint
7. run drift scan and see the result
8. understand which models are configured without seeing secrets

## Related

- [Cross-project LangGraph workbench](cross-project-langgraph-workbench.md)
- [Model routing](model-routing.md)
- [Local secrets workflow](local-secrets-workflow.md)
- [Project model allowlist policy](project-model-allowlist-policy.md)
- [OpenCode task queue workflow](opencode-task-queue.md)
- [Codex review workflow](codex-review-workflow.md)
