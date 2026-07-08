# Agent Runner

This directory will host the local queue dispatcher for Ahamkara.

Current state:

- `agent-runner.py` can inspect queue state, print the next open/review-needed
  tasks, and auto-promote claimed tasks to `review-needed/` when their report
  file appears and matches the subagent report template.
- `agent-runner.py` can group open work by slice and dispatch one subagent per
  slice with slice task IDs in the environment.
- `agent-runner.py` can also dispatch a roadmap phase by reading the phase
  block from `docs/roadmap/roadmap.md`, grouping its tasks by slice, and
  exporting phase context to the worker.
- `agent-runner.sh` is a thin wrapper for Warp workflows.
- `warp-worker.sh` is a small Warp-friendly worker launcher that prints the
  assigned slice/phase context before `exec`ing the worker command.
- worker spawn, review handoff, and file watching are implemented.
- `watch-background.sh` starts the watcher in the background with a pidfile and
  log under `/private/tmp/ahamkara-agent-runner`.

Examples:

```sh
./scripts/start-agents.sh
./scripts/start-everything.sh
./scripts/start-everything.sh --run-game
./tools/agent-runner/agent-runner.sh status
./tools/agent-runner/agent-runner.sh next
./tools/agent-runner/agent-runner.sh sync
./tools/agent-runner/agent-runner.sh watch --once
./tools/agent-runner/watch-background.sh
./tools/agent-runner/watch-stop.sh
./tools/agent-runner/warp-worker.sh "$SHELL"
./tools/agent-runner/agent-runner.sh slices
./tools/agent-runner/agent-runner.sh dispatch-slices --dry-run
./tools/agent-runner/agent-runner.sh phase 2 --dry-run
```
