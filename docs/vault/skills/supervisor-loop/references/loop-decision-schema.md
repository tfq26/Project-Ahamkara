# Loop Decision Schema

Use this reference when a supervisor loop needs machine-readable structure.

## Worker Report Inputs

The supervisor should request these fields from the worker:

```yaml
task: ""
task_id: ""
agent: ""
status: "implemented | implemented_not_validated | validated_with_known_gaps | blocked | stopped"
scope:
  in_bounds: []
  out_of_bounds: []
files_changed: []
what_changed: []
validation_run: []
validation_results: []
known_gaps: []
runtime_risks: []
dependencies: []
control_checks:
  user_directives_checked: false
  stop_requests_checked: false
  steering_requests_checked: false
recommended_next_step: ""
confidence: "high | medium | low"
```

If `$subagent-reporting` is available, prefer its full report format and add the task ID, agent name, and control checks when using an Obsidian control plane.

## Supervisor Decision Output

The supervisor should emit one decision block per review turn:

```yaml
task_id: ""
worker: ""
supervisor: ""
decision: "continue | revise | verify | complete | blocked | stopped"
reason:
  - ""
next_actions:
  - ""
completion_bar:
  - ""
evidence_checked:
  - ""
missing_evidence:
  - ""
control_state:
  stop_requested: false
  steering_applied: false
user_visible_notes:
  - ""
```

Field rules:

- `task_id`: stable ID from the task board or task note.
- `worker`: agent or model whose report is being reviewed.
- `supervisor`: agent or model making the decision.
- `decision`: exactly one of the six labels.
- `reason`: factual observations from report, diff, tests, logs, or control-plane files.
- `next_actions`: empty only when decision is `complete` or `stopped`.
- `completion_bar`: what must be true before `complete`.
- `evidence_checked`: reports, files, commands, logs, screenshots, diffs, task notes, and control files actually inspected.
- `missing_evidence`: validation or context still needed.
- `control_state`: whether stop or steering controls affected this decision.
- `user_visible_notes`: concise risks or outcomes for the final user report.

## Decision Heuristics

Use `continue` when:

- the worker made real progress
- no known defect blocks the current direction
- the next implementation slice is obvious
- no applicable stop request exists

Use `revise` when:

- behavior is wrong or incomplete
- code quality creates a practical maintenance or runtime risk
- the worker's claim conflicts with the evidence
- tests fail for reasons related to the change
- the worker ignored applicable steering

Use `verify` when:

- implementation appears plausible
- validation is missing, stale, too broad to prove the claim, or failed for unclear reasons

Use `complete` when:

- acceptance criteria are met
- validation is adequate for the risk
- known gaps are non-blocking
- no applicable stop or steering request remains unresolved
- the final user response can be honest and short

Use `blocked` when:

- credentials, missing files, external services, or product decisions are required
- repeated attempts would be speculative
- the next safe action is user input

Use `stopped` when:

- the user stopped an agent, group, task, or the whole organization
- the worker checkpointed current state and wrote a report
- no further worker action should happen until user or supervisor resumes it

## Anti-Patterns

Avoid:

- asking for broad "improvements" without naming the defect or target
- accepting "done" without validation evidence
- letting two workers edit the same shared files without explicit ownership boundaries
- having multiple supervisors issue conflicting instructions to the same worker
- ignoring stop or steering requests because the implementation is nearly finished
- requiring unrelated cleanup before completion
- letting the supervisor rewrite the whole task instead of sending a focused correction
- hiding partial or unvalidated work from the user
