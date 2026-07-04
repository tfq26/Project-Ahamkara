# Queue Agent Runner Architecture

Status: Draft

This doc sketches the minimal local runner that turns the repo queue into an
automated worker/supervisor loop.

## Goal

The runner should:

- read the repo queue
- spawn workers in isolated git worktrees
- wait for reports
- prompt the supervisor to review
- update queue state from explicit file transitions

It should not attempt to judge code quality itself.

## Design Principles

1. **Files are the mailbox.** Task notes and reports are the only durable
   handoff.
2. **Supervisor decides.** The runner may notify, but it does not accept work.
3. **One worktree per worker.** No shared-write worktree for parallel tasks.
4. **Explicit transitions only.** Completion is a folder/status move, not a guess.
5. **Small commands.** The CLI should stay narrow and predictable.

## Proposed Repository Layout

```text
tools/
  agent-runner/
    agent-runner.sh
    agent-runner.py
    lib/
      queue.py
      worktree.py
      reports.py
      watch.py
      process.py
      sync.py
```

Suggested responsibilities:

- `agent-runner.sh`: thin shell entrypoint for Warp workflows and manual use.
- `agent-runner.py`: main CLI dispatcher.
- `queue.py`: task discovery, state transitions, report linkage.
- `worktree.py`: create/list/remove worktrees and branch names.
- `reports.py`: write report stubs, validate report paths, append master log.
- `watch.py`: watch queue/report changes and emit events.
- `process.py`: launch workers, track PIDs, stream logs.
- `sync.py`: regenerate board/dashboard from queue state.

If the repo later wants a TUI or service wrapper, this same core can back it.

## Commands

```sh
agent-runner next
agent-runner spawn <task-id>
agent-runner phase <phase-number>
agent-runner watch
agent-runner review <task-id>
agent-runner sync
agent-runner status
```

Expected behavior:

- `next`: choose the next eligible open task, claim it, create a worktree, and
  launch a worker.
- `spawn <task-id>`: launch a worker for a specific task note.
- `phase <phase-number>`: read the roadmap phase block, group its open tasks by
  slice, and launch one worker per slice.
- `watch`: monitor queue folders and report files for state transitions.
- `review <task-id>`: surface the report and diff to the supervisor.
- `sync`: regenerate board/dashboard artifacts from queue files.
- `status`: show current watcher state, running workers, and pending reviews.

## Worker Flow

1. Read the task note from `docs/vault/queue-tasks/open/`.
2. Verify the task is not deferred or blocked.
3. Create a dedicated worktree and branch.
4. Launch the worker with:
   - task file path
   - report template path
   - allowed file scope
   - validation commands
5. Worker edits only its slice.
6. Worker writes a report and master-log entry.
7. Worker moves the task to `review-needed/`.
8. Runner emits a review event.

## Supervisor Flow

1. Watch for `review-needed/` transitions.
2. Read the task note, report, master log, and diff.
3. Decide `complete`, `verify`, `revise`, or `blocked`.
4. Update task status and folder state.
5. Optionally requeue `revise` work or close the slice.

## Watcher Events

The watcher should react to these file events:

- task moved into `open/`
- task moved into `claimed/`
- task moved into `review-needed/`
- report file created or updated
- task frontmatter `status:` changed
- master log appended
- task moved into `completed/` or `blocked/`

The watcher does not need to parse source code. It only needs to correlate
queue state and report files.

## Completion Stamp

Each full report should carry a small structured stamp that the watcher can
parse without reading prose:

```yaml
completion:
  task_id: TASK-YYYYMMDD-HHMM-name
  worker: worker-name
  branch: agent/worker/task-name
  worktree: ../project-worker-task-name
  validation:
    - cmake --build --preset debug
    - ./scripts/run-tests.sh --preset debug
  report: docs/reports/subagents/...
  confidence: medium
```

## Failure Handling

- If worktree creation fails, mark the task blocked and include the reason.
- If the worker crashes, preserve its logs and keep the task claimed or blocked.
- If the report is missing, do not promote the task to review-needed.
- If the queue state and folder disagree, prefer the folder state only after a
  repair pass or explicit supervisor correction.

## MVP Sequence

1. Implement `status` and `sync`.
2. Implement `next` and `spawn`.
3. Implement the file watcher.
4. Add the review notification path.
5. Add optional parallel worker fan-out.

## Warp Integration

Warp should only be the operator surface:

- run `agent-runner` commands
- show split panes for worker output
- show notifications when review is needed
- open worktrees and diffs quickly

Warp should not own queue state.
