---
type: review
status: active
created: 2026-06-15
reviewer: codex
task: TASK-20260615-1200-client-frame-pipeline
report: TASK-20260615-1200-client-frame-pipeline-report.md
decision: revise
subsystems:
  - client
  - engine/render
  - engine/runtime
---

# Codex Review

## Task

TASK-20260615-1200-client-frame-pipeline

## Report

[TASK-20260615-1200-client-frame-pipeline-report.md](TASK-20260615-1200-client-frame-pipeline-report.md)

## Decision

`revise`

## Scope Check

The pipeline extraction is moving in the right direction, but there is a real
control-flow bug in the new owner that should be fixed before acceptance.

## Evidence Checked

- `git diff`
- queued task acceptance bar
- OpenCode report
- frame pipeline implementation
- queue state

## Findings

1. `run_one_frame()` does not honor its own shutdown contract.
   The header says it returns false when shutdown is requested in
   [client_frame_pipeline.h](/Users/taufeeqali/Projects/Ahamkara/client/include/ahamkara/client/client_frame_pipeline.h:50),
   but the implementation always returns `true` in
   [client_frame_pipeline.cpp](/Users/taufeeqali/Projects/Ahamkara/client/src/client_frame_pipeline.cpp:45).
   `stage_poll_input()` can call `application_.shutdown()` in
   [client_frame_pipeline.cpp](/Users/taufeeqali/Projects/Ahamkara/client/src/client_frame_pipeline.cpp:103),
   yet later stages still run for that frame.

2. Queue state drifted badly during handoff.
   The same task was left in `open/`, `claimed/`, and `review-needed/` at once.
   That is a workflow issue rather than a code bug, but it needs to be cleaned
   up so the next pass is unambiguous.

## Validation Assessment

The debug build evidence is useful, but it does not outweigh the lifecycle bug
in the pipeline owner.

## Risks

- The client may continue executing render/UI/post-frame work after shutdown has
  already been requested.
- The queue becomes harder to trust if task state is duplicated across folders.

## Next Action

Move this task back to `open/` and revise it:

- make `run_one_frame()` return `false` or short-circuit later stages when
  shutdown is requested
- clean the queue so this task lives in one state only
