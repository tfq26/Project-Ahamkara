#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 0 ]]; then
    echo "Usage: warp-worker.sh <worker command...>"
    echo "Run this from a Warp pane inside the claimed worktree."
    exit 1
fi

echo "Warp worker context:"
echo "- task: ${AGENT_RUNNER_TASK_ID:-unknown}"
echo "- slice: ${AGENT_RUNNER_SLICE_KEY:-unknown}"
echo "- label: ${AGENT_RUNNER_SLICE_LABEL:-unknown}"
echo "- phase: ${AGENT_RUNNER_PHASE:-none}"
echo "- worktree: ${AGENT_RUNNER_WORKTREE_PATH:-unknown}"
echo "- report: ${AGENT_RUNNER_REPORT_PATH:-unknown}"
echo
echo "Worker instructions:"
echo "- Read the claimed task note before editing."
echo "- If this is a phase batch, read the roadmap phase list first and treat each slice as a parent assignment."
echo "- If the task or slice is broad enough and the model can spawn subagents, split it into narrow subtasks."
echo "- Give each subagent its own git worktree."
echo "- Do not let multiple writers touch the same branch at the same time."
echo "- Each subagent must write a full report in docs/reports/subagents/ and append the master log."
echo "- Parent/supervisor subagents stay on integration, validation, and queue updates."
echo "- Nested subagents are allowed only for clearly isolated subtasks, not for shared-file churn."
exec "$@"
