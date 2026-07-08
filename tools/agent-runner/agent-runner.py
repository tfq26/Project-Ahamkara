#!/usr/bin/env python3
"""Local queue runner for Ahamkara.

The runner is intentionally file-driven:
- queue task notes are the source of truth
- reports are the handoff artifacts
- worktrees isolate worker edits
- queue folder transitions trigger review
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


TASK_STATES = ("open", "claimed", "review-needed", "completed", "blocked")
STATE_HEADING = {
    "open": "Open",
    "claimed": "Claimed",
    "review-needed": "Review Needed",
    "completed": "Complete",
    "blocked": "Blocked",
}


@dataclass(frozen=True)
class TaskInfo:
    task_id: str
    status: str
    path: Path
    title: str
    report: str = ""
    slice_key: str = ""
    slice_label: str = ""


ROADMAP_PATH = Path("docs/roadmap/roadmap.md")
PHASE_HEADING_RE = re.compile(r"^#### Phase (\d+)\b")
TASK_ID_RE = re.compile(r"TASK-\d{8}-\d{4}-[A-Za-z0-9-]+")


def repo_root(start: Path | None = None) -> Path:
    cursor = (start or Path.cwd()).resolve()
    while cursor != cursor.parent:
        if (cursor / "docs" / "vault" / "queue-tasks").is_dir():
            return cursor
        cursor = cursor.parent
    raise SystemExit("Could not locate repo root from current directory.")


def queue_root(root: Path) -> Path:
    return root / "docs" / "vault" / "queue-tasks"


def task_state_dir(root: Path, state: str) -> Path:
    return queue_root(root) / state


def parse_frontmatter(text: str) -> dict[str, str]:
    if not text.startswith("---\n"):
        return {}
    end = text.find("\n---", 4)
    if end == -1:
        return {}
    meta: dict[str, str] = {}
    for raw in text[4:end].splitlines():
        if ":" not in raw:
            continue
        key, value = raw.split(":", 1)
        meta[key.strip()] = value.strip().strip('"').strip("'")
    return meta


def split_document(text: str) -> tuple[str | None, str]:
    lines = text.splitlines(keepends=True)
    if not lines or lines[0].strip() != "---":
        return None, text
    for idx in range(1, len(lines)):
        if lines[idx].strip() == "---":
            return "".join(lines[1:idx]), "".join(lines[idx + 1 :])
    return None, text


def rewrite_frontmatter(text: str, updates: dict[str, str]) -> str:
    front, body = split_document(text)
    if front is None:
        lines = ["---\n"]
        for key, value in updates.items():
            lines.append(f"{key}: {value}\n")
        lines.append("---\n")
        lines.append(body if body.endswith("\n") else body + "\n")
        return "".join(lines)

    out: list[str] = []
    seen: set[str] = set()
    for raw in front.splitlines(keepends=True):
        match = re.match(r"^([A-Za-z0-9_-]+):(.*)$", raw)
        if match and match.group(1) in updates:
            key = match.group(1)
            suffix = "\n" if raw.endswith("\n") else ""
            value = updates[key]
            out.append(f"{key}: {value}{suffix}")
            seen.add(key)
        else:
            out.append(raw)

    for key, value in updates.items():
        if key not in seen:
            out.append(f"{key}: {value}\n")

    return "---\n" + "".join(out) + "---\n" + body


def first_heading(text: str, fallback: str) -> str:
    for line in text.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    return fallback


def task_short_name(task_id: str) -> str:
    if task_id.startswith("TASK-"):
        parts = task_id.split("-", 3)
        if len(parts) == 4 and parts[3].strip():
            return parts[3].strip()
    return task_id


def display_title(task_id: str, title: str) -> str:
    cleaned = title.strip()
    if not cleaned:
        return task_id
    if cleaned == task_id:
        return task_id
    prefix = f"{task_id} - "
    if cleaned.startswith(prefix):
        stripped = cleaned[len(prefix) :].strip()
        return stripped or task_id
    return cleaned


def normalize_status(status: str | None, fallback: str) -> str:
    value = (status or "").strip().lower().replace("_", "-")
    if value in TASK_STATES:
        return value
    if value in {"complete", "done", "finished"}:
        return "completed"
    if value in {"review-needed"}:
        return "review-needed"
    if value in {"claimed", "in-progress", "inprogress", "working"}:
        return "claimed"
    if value in {"blocked", "deferred", "stopped"}:
        return "blocked"
    if fallback in TASK_STATES:
        return fallback
    return "open"


def normalize_slice_key(value: str) -> str:
    cleaned = value.strip()
    if not cleaned:
        return ""
    path_value = Path(cleaned)
    if path_value.suffix:
        cleaned = path_value.stem
    elif "/" in cleaned or "\\" in cleaned:
        cleaned = path_value.name
    return slugify(cleaned, max_len=80)


def task_slice_key(meta: dict[str, str], task_id: str) -> str:
    explicit = meta.get("slice") or meta.get("slice_group")
    if explicit:
        return normalize_slice_key(explicit)
    related = meta.get("related_feature")
    if related:
        return normalize_slice_key(related)
    return normalize_slice_key(task_short_name(task_id))


def task_slice_label(meta: dict[str, str], task_id: str) -> str:
    explicit = meta.get("slice") or meta.get("slice_group")
    if explicit:
        return explicit.strip()
    related = meta.get("related_feature")
    if related:
        return Path(related).stem
    return task_short_name(task_id)


def slugify(value: str, max_len: int = 64) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")
    return slug[:max_len].strip("-") or "task"


def load_tasks(root: Path) -> list[TaskInfo]:
    tasks: list[TaskInfo] = []
    for state in TASK_STATES:
        state_dir = task_state_dir(root, state)
        if not state_dir.is_dir():
            continue
        for path in sorted(state_dir.glob("TASK-*.md")):
            text = path.read_text(encoding="utf-8")
            meta = parse_frontmatter(text)
            task_id = meta.get("task_id") or path.stem
            tasks.append(
                TaskInfo(
                    task_id=task_id,
                    status=normalize_status(meta.get("status"), state),
                    path=path,
                    title=first_heading(text, task_id),
                    report=meta.get("report", ""),
                    slice_key=task_slice_key(meta, task_id),
                    slice_label=task_slice_label(meta, task_id),
                )
            )
    return tasks


def roadmap_phase_task_ids(root: Path, phase: int) -> list[str]:
    roadmap_path = root / ROADMAP_PATH
    if not roadmap_path.exists():
        return []

    lines = roadmap_path.read_text(encoding="utf-8").splitlines()
    start = None
    for index, line in enumerate(lines):
        match = PHASE_HEADING_RE.match(line.strip())
        if match and int(match.group(1)) == phase:
            start = index + 1
            break
    if start is None:
        return []

    end = len(lines)
    for index in range(start, len(lines)):
        stripped = lines[index].strip()
        if PHASE_HEADING_RE.match(stripped) or stripped.startswith("#### ⛳"):
            end = index
            break

    seen: set[str] = set()
    ordered: list[str] = []
    for line in lines[start:end]:
        for task_id in TASK_ID_RE.findall(line):
            if task_id not in seen:
                seen.add(task_id)
                ordered.append(task_id)
    return ordered


def find_task(tasks: Iterable[TaskInfo], task_id: str) -> TaskInfo | None:
    for task in tasks:
        if task.task_id == task_id:
            return task
    return None


def render_board(tasks: list[TaskInfo]) -> str:
    by_status = {status: [] for status in TASK_STATES}
    unknown: list[TaskInfo] = []
    for task in tasks:
        if task.status in by_status:
            by_status[task.status].append(task)
        else:
            unknown.append(task)

    lines = [
        "# Agent Task Board",
        "",
        "> Generated from task note frontmatter. Edit task notes, then run `agent-runner sync`.",
        "",
    ]
    for state in TASK_STATES:
        heading = STATE_HEADING[state]
        lines.extend([f"## {heading}", ""])
        items = by_status[state]
        if items:
            for task in items:
                rel = f"../queue-tasks/{state}/{task.path.name}"
                title = display_title(task.task_id, task.title)
                label = task.task_id if title == task.task_id else f"{task.task_id} - {title}"
                lines.append(f"- [{label}]({rel})")
        else:
            lines.append("- _No tasks_")
        lines.append("")

    if unknown:
        lines.extend(["## Unknown Status", ""])
        for task in unknown:
            rel = f"../queue-tasks/{task.path.parent.name}/{task.path.name}"
            title = display_title(task.task_id, task.title)
            label = task.task_id if title == task.task_id else f"{task.task_id} - {title}"
            lines.append(f"- [{label}]({rel}) status: `{task.status}`")
        lines.append("")

    return "\n".join(lines)


def replace_section(text: str, heading: str, replacement: str) -> str:
    pattern = re.compile(
        rf"(?ms)^## {re.escape(heading)}\n.*?(?=^## |\Z)"
    )
    if pattern.search(text):
        return pattern.sub(replacement.rstrip("\n") + "\n\n", text, count=1)
    if text and not text.endswith("\n"):
        text += "\n"
    return text + "\n" + replacement.rstrip("\n") + "\n"


def render_dashboard(tasks: list[TaskInfo], existing: str | None = None) -> str:
    counts = {state: 0 for state in TASK_STATES}
    grouped = {state: [] for state in TASK_STATES}
    for task in tasks:
        if task.status in counts:
            counts[task.status] += 1
            grouped[task.status].append(task)

    total_active = counts["open"] + counts["claimed"] + counts["review-needed"]
    overall = "active" if total_active else "idle"

    snapshot = "\n".join(
        [
            "## Snapshot",
            "",
            f"Overall Status: {overall}",
            "Lead Supervisor: codex-lead-supervisor",
            f"Active Workers: {counts['claimed']}",
            f"Tasks Open: {counts['open']}",
            f"Tasks Claimed: {counts['claimed']}",
            f"Tasks Needing Review: {counts['review-needed']}",
            f"Tasks Blocked: {counts['blocked']}",
            "Tasks Stopped: 0",
            f"Tasks Complete: {counts['completed']}",
        ]
    )

    active_lines = ["## Active Work", ""]
    for state in ("open", "claimed", "review-needed", "blocked", "completed"):
        items = grouped[state]
        if items:
            for task in items:
                active_lines.append(f"- {STATE_HEADING[state]}: {task.task_id}")
        elif state != "completed":
            active_lines.append(f"- {STATE_HEADING[state]}: _none_")
    active = "\n".join(active_lines)

    if existing is None:
        existing = _dashboard_shell()
    existing = replace_section(existing, "Snapshot", snapshot)
    existing = replace_section(existing, "Active Work", active)
    return existing


def _dashboard_shell() -> str:
    return (
        "# Progress Dashboard\n\n"
        "## Snapshot\n\n"
        "Overall Status: active\n"
        "Lead Supervisor: codex-lead-supervisor\n"
        "Active Workers: 0\n"
        "Tasks Open: 0\n"
        "Tasks Claimed: 0\n"
        "Tasks Needing Review: 0\n"
        "Tasks Blocked: 0\n"
        "Tasks Stopped: 0\n"
        "Tasks Complete: 0\n\n"
        "## Active Work\n\n"
        "- Open: _none_\n"
        "- Claimed: _none_\n"
        "- Review Needed: _none_\n"
        "- Blocked: _none_\n\n"
        "## Needs User Attention\n\n"
        "- _No items_\n\n"
        "## Recent Updates\n\n"
        "- _No updates yet_\n\n"
        "## Controls\n\n"
        "- Stop requests: [[../control/stop-requests]]\n"
        "- Steering requests: [[../control/steering-requests]]\n"
        "- Task board: [[agent-task-board]]\n"
    )


def status(root: Path) -> int:
    tasks = load_tasks(root)
    counts = {state: 0 for state in TASK_STATES}
    for task in tasks:
        if task.status in counts:
            counts[task.status] += 1

    print("Queue Status")
    print(f"- repo: {root}")
    for state in TASK_STATES:
        print(f"- {state}: {counts[state]}")

    next_open = next((task for task in tasks if task.status == "open"), None)
    if next_open:
        title = display_title(next_open.task_id, next_open.title)
        if title == next_open.task_id:
            print(f"- next_open: {next_open.task_id}")
        else:
            print(f"- next_open: {next_open.task_id} ({title})")
    else:
        print("- next_open: none")

    waiting_review = next((task for task in tasks if task.status == "review-needed"), None)
    if waiting_review:
        title = display_title(waiting_review.task_id, waiting_review.title)
        if title == waiting_review.task_id:
            print(f"- review_needed: {waiting_review.task_id}")
        else:
            print(f"- review_needed: {waiting_review.task_id} ({title})")
    else:
        print("- review_needed: none")

    return 0


def sync(root: Path) -> int:
    tasks = load_tasks(root)
    board_path = root / "docs" / "vault" / "team" / "agent-task-board.md"
    dashboard_path = root / "docs" / "vault" / "team" / "progress-dashboard.md"

    board_path.write_text(render_board(tasks) + "\n", encoding="utf-8")
    existing_dashboard = dashboard_path.read_text(encoding="utf-8") if dashboard_path.exists() else None
    dashboard_path.write_text(render_dashboard(tasks, existing_dashboard), encoding="utf-8")

    print(f"Wrote {board_path}")
    print(f"Wrote {dashboard_path}")
    return 0


def queue_task_by_id(root: Path, task_id: str) -> TaskInfo:
    tasks = load_tasks(root)
    task = find_task(tasks, task_id)
    if task is None:
        raise SystemExit(f"Task not found: {task_id}")
    return task


def group_tasks_by_slice(tasks: list[TaskInfo], *, include_statuses: Iterable[str] | None = None) -> list[tuple[str, str, list[TaskInfo]]]:
    allowed = set(include_statuses) if include_statuses is not None else None
    grouped: dict[str, list[TaskInfo]] = {}
    labels: dict[str, str] = {}
    for task in tasks:
        if allowed is not None and task.status not in allowed:
            continue
        key = task.slice_key or normalize_slice_key(task.task_id)
        grouped.setdefault(key, []).append(task)
        labels.setdefault(key, task.slice_label or task.title or key)
    return [
        (key, labels.get(key, key), sorted(items, key=lambda item: item.task_id))
        for key, items in sorted(grouped.items(), key=lambda item: item[0])
    ]


def resolve_task_path(root: Path, task: TaskInfo, path_text: str) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path

    candidates = [
        (task.path.parent / path).resolve(),
        (root / path).resolve(),
        (root / "docs" / path).resolve(),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def task_report_path(root: Path, task: TaskInfo) -> Path | None:
    if not task.report:
        return None
    return resolve_task_path(root, task, task.report)


def report_status(report_path: Path) -> str:
    try:
        text = report_path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""
    meta = parse_frontmatter(text)
    return normalize_status(meta.get("status"), "open")


def report_has_valid_type(report_path: Path) -> bool:
    try:
        text = report_path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return False
    meta = parse_frontmatter(text)
    return meta.get("type") == "subagent-report"


def promote_task_after_report(root: Path, task: TaskInfo) -> Path | None:
    if task.status != "claimed":
        return None

    report_path = task_report_path(root, task)
    if report_path is None or not report_path.exists() or not report_has_valid_type(report_path):
        return None

    report_state = report_status(report_path)
    if report_state == "blocked":
        return move_task(root, task, "blocked", updates={"status": "blocked"})
    if report_state in {"open", "claimed", "review-needed", "completed"}:
        return move_task(root, task, "review-needed", updates={"status": "review-needed"})
    return None


def update_task_file(task_path: Path, updates: dict[str, str]) -> str:
    text = task_path.read_text(encoding="utf-8")
    updated = rewrite_frontmatter(text, updates)
    task_path.write_text(updated, encoding="utf-8")
    return updated


def move_task(root: Path, task: TaskInfo, new_state: str, updates: dict[str, str] | None = None) -> Path:
    updates = dict(updates or {})
    updates.setdefault("status", new_state)
    updated_text = rewrite_frontmatter(task.path.read_text(encoding="utf-8"), updates)
    destination = task_state_dir(root, new_state) / task.path.name
    destination.parent.mkdir(parents=True, exist_ok=True)
    task.path.write_text(updated_text, encoding="utf-8")
    shutil.move(str(task.path), str(destination))
    return destination


def run_command(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> int:
    proc = subprocess.run(cmd, cwd=cwd, env=env, check=False)
    return proc.returncode


def worker_env(
    root: Path,
    task: TaskInfo,
    worktree_path: Path,
    branch: str,
    task_file: Path | None = None,
    report_path: Path | None = None,
    slice_key: str = "",
    slice_label: str = "",
    slice_task_ids: list[str] | None = None,
    slice_task_files: list[str] | None = None,
    phase: str = "",
    phase_task_ids: list[str] | None = None,
) -> dict[str, str]:
    env = os.environ.copy()
    slice_task_ids = slice_task_ids or [task.task_id]
    slice_task_files = slice_task_files or [str(task_file or task.path)]
    phase_task_ids = phase_task_ids or []
    env.update(
        {
            "AGENT_RUNNER_REPO_ROOT": str(root),
            "AGENT_RUNNER_TASK_ID": task.task_id,
            "AGENT_RUNNER_TASK_FILE": str(task_file or task.path),
            "AGENT_RUNNER_WORKTREE_PATH": str(worktree_path),
            "AGENT_RUNNER_BRANCH": branch,
            "AGENT_RUNNER_REPORT_PATH": str(report_path or ""),
            "AGENT_RUNNER_SLICE_KEY": slice_key,
            "AGENT_RUNNER_SLICE_LABEL": slice_label,
            "AGENT_RUNNER_SLICE_TASK_IDS": ",".join(slice_task_ids),
            "AGENT_RUNNER_SLICE_TASK_FILES": ",".join(slice_task_files),
            "AGENT_RUNNER_SLICE_SIZE": str(len(slice_task_ids)),
            "AGENT_RUNNER_PHASE": phase,
            "AGENT_RUNNER_PHASE_TASK_IDS": ",".join(phase_task_ids),
            "AGENT_RUNNER_PHASE_SIZE": str(len(phase_task_ids)),
            "AGENT_RUNNER_REPORT_TEMPLATE": str(root / "docs" / "vault" / "templates" / "subagent-report-template.md"),
            "AGENT_RUNNER_QUEUE_ROOT": str(queue_root(root)),
        }
    )
    return env


def launch_worker(
    worker_cmd: str,
    *,
    root: Path,
    task: TaskInfo,
    worktree_path: Path,
    branch: str,
    task_file: Path,
    report_path: Path | None = None,
    slice_key: str = "",
    slice_label: str = "",
    slice_task_ids: list[str] | None = None,
    slice_task_files: list[str] | None = None,
    phase: str = "",
    phase_task_ids: list[str] | None = None,
) -> int:
    print(f"Launching worker: {worker_cmd}")
    return subprocess.run(
        worker_cmd,
        cwd=worktree_path,
        env=worker_env(
            root,
            task,
            worktree_path,
            branch,
            task_file=task_file,
            report_path=report_path,
            slice_key=slice_key,
            slice_label=slice_label,
            slice_task_ids=slice_task_ids,
            slice_task_files=slice_task_files,
            phase=phase,
            phase_task_ids=phase_task_ids,
        ),
        shell=True,
        check=False,
    ).returncode


def spawn(root: Path, task_id: str) -> int:
    task = queue_task_by_id(root, task_id)
    if task.status != "open":
        raise SystemExit(f"Task is not open: {task.task_id} ({task.status})")

    slug = slugify(task.task_id)
    branch = f"agent/runner/{slug}"
    worktree_path = root.parent / f"{root.name}-{slug}"
    if worktree_path.exists():
        raise SystemExit(f"Worktree path already exists: {worktree_path}")

    report_rel = task.report or f"docs/reports/subagents/{task.task_id}-report.md"
    report_abs = resolve_task_path(root, task, report_rel)

    git_cmd = ["git", "-C", str(root), "worktree", "add", "-b", branch, str(worktree_path), "HEAD"]
    print(f"Creating worktree: {worktree_path}")
    subprocess.run(git_cmd, check=True)

    claimed_path = move_task(
        root,
        task,
        "claimed",
        updates={
            "status": "claimed",
            "claimed_by": "agent-runner",
            "branch": branch,
            "worktree": str(worktree_path),
            "report": report_rel,
        },
    )

    print(f"Claimed task: {claimed_path}")
    print(f"Branch: {branch}")
    print(f"Worktree: {worktree_path}")

    worker_cmd = os.environ.get("AGENT_RUNNER_WORKER_CMD", "").strip()
    if worker_cmd:
        rc = launch_worker(
            worker_cmd,
            root=root,
            task=task,
            worktree_path=worktree_path,
            branch=branch,
            task_file=claimed_path,
            report_path=report_abs,
            slice_key=task.slice_key,
            slice_label=task.slice_label,
            slice_task_ids=[task.task_id],
            slice_task_files=[str(claimed_path)],
        )
        print(f"Worker exited with code {rc}")
        promoted = queue_task_by_id(root, task.task_id)
        if promoted.status == "claimed":
            auto_path = promote_task_after_report(root, promoted)
            if auto_path is not None:
                print(f"Auto-promoted task after worker exit: {auto_path}")
                sync(root)
                final_task = queue_task_by_id(root, task.task_id)
                if final_task.status == "review-needed":
                    print(f"Review ready: {final_task.task_id}")
        return rc

    print("No AGENT_RUNNER_WORKER_CMD configured.")
    print(f"Run your worker in {worktree_path} with the task file {claimed_path}.")
    return 0


def dispatch_task_slices(
    root: Path,
    all_tasks: list[TaskInfo],
    tasks: list[TaskInfo],
    *,
    dry_run: bool = False,
    limit: int | None = None,
    phase: str = "",
    phase_task_ids: list[str] | None = None,
    phase_label: str = "",
) -> int:
    slice_groups = group_tasks_by_slice(tasks, include_statuses=("open",))
    dispatched = 0

    active_by_slice = {
        key
        for key, _, items in group_tasks_by_slice(all_tasks)
        if any(task.status in {"claimed", "review-needed"} for task in items)
    }

    for slice_key, slice_label, slice_tasks in slice_groups:
        if limit is not None and dispatched >= limit:
            break
        if slice_key in active_by_slice:
            print(f"Skipping active slice: {slice_key}")
            continue

        open_tasks = sorted(slice_tasks, key=lambda task: task.task_id)
        if not open_tasks:
            continue

        anchor = open_tasks[0]
        branch_root = "phase" if phase else "slice"
        branch_name_source = f"{phase}-{slice_key}" if phase else slice_key
        branch_suffix = slugify(branch_name_source, max_len=48)
        branch = f"agent/runner/{branch_root}/{branch_suffix}"
        worktree_path = root.parent / f"{root.name}-{branch_root}-{branch_suffix}"
        report_rel = anchor.report or f"docs/reports/subagents/{anchor.task_id}-report.md"
        report_abs = resolve_task_path(root, anchor, report_rel)
        slice_ids = [task.task_id for task in open_tasks]
        slice_files = [str(task.path) for task in open_tasks]

        print(f"Slice: {slice_key} ({slice_label})")
        print(f"  tasks: {', '.join(slice_ids)}")
        if phase:
            print(f"  phase: {phase}")
            if phase_label:
                print(f"  phase label: {phase_label}")
            print(f"  phase tasks: {', '.join(phase_task_ids or [])}")
        print(f"  branch: {branch}")
        print(f"  worktree: {worktree_path}")

        if dry_run:
            dispatched += 1
            continue

        if worktree_path.exists():
            raise SystemExit(f"Worktree path already exists: {worktree_path}")

        subprocess.run(
            ["git", "-C", str(root), "worktree", "add", "-b", branch, str(worktree_path), "HEAD"],
            check=True,
        )

        for task in open_tasks:
            claimed_path = move_task(
                root,
                task,
                "claimed",
                updates={
                    "status": "claimed",
                    "claimed_by": "agent-runner",
                    "branch": branch,
                    "worktree": str(worktree_path),
                    "report": task.report or f"docs/reports/subagents/{task.task_id}-report.md",
                    "slice": slice_key,
                },
            )
            print(f"Claimed slice task: {claimed_path}")

        worker_cmd = os.environ.get("AGENT_RUNNER_WORKER_CMD", "").strip()
        if worker_cmd:
            rc = launch_worker(
                worker_cmd,
                root=root,
                task=anchor,
                worktree_path=worktree_path,
                branch=branch,
                task_file=open_tasks[0].path,
                report_path=report_abs,
                slice_key=slice_key,
                slice_label=slice_label,
                slice_task_ids=slice_ids,
                slice_task_files=slice_files,
                phase=phase,
                phase_task_ids=phase_task_ids,
            )
            print(f"Worker exited with code {rc}")
        else:
            print("No AGENT_RUNNER_WORKER_CMD configured for slice dispatch.")

        dispatched += 1

    if dispatched == 0:
        print("No eligible open slices found.")
    return 0


def dispatch_slices(root: Path, dry_run: bool = False, limit: int | None = None) -> int:
    tasks = load_tasks(root)
    return dispatch_task_slices(root, tasks, tasks, dry_run=dry_run, limit=limit)


def phase(root: Path, phase_number: int, dry_run: bool = False, limit: int | None = None) -> int:
    phase_task_ids = roadmap_phase_task_ids(root, phase_number)
    if not phase_task_ids:
        print(f"No roadmap tasks found for phase {phase_number}.")
        return 0

    tasks = load_tasks(root)
    phase_tasks = [task for task in tasks if task.task_id in phase_task_ids]
    phase_label = f"Phase {phase_number}"
    open_phase_tasks = [task for task in phase_tasks if task.status == "open"]

    print(f"{phase_label}: roadmap tasks = {', '.join(phase_task_ids)}")
    if not open_phase_tasks:
        print(f"No open tasks found for {phase_label}.")
        return 0

    return dispatch_task_slices(
        root,
        tasks,
        open_phase_tasks,
        dry_run=dry_run,
        limit=limit,
        phase=str(phase_number),
        phase_task_ids=phase_task_ids,
        phase_label=phase_label,
    )


def slices(root: Path) -> int:
    tasks = load_tasks(root)
    groups = group_tasks_by_slice(tasks)
    if not groups:
        print("No slices found.")
        return 0

    for slice_key, slice_label, items in groups:
        states = ", ".join(f"{task.task_id}:{task.status}" for task in items)
        print(f"- {slice_key} ({slice_label})")
        print(f"  {states}")
    return 0


def review(root: Path, task_id: str) -> int:
    task = queue_task_by_id(root, task_id)
    print(f"Task: {task.task_id}")
    print(f"Status: {task.status}")
    print(f"Path: {task.path}")
    print(f"Title: {task.title}")
    if task.report:
        report_path = resolve_task_path(root, task, task.report)
        print(f"Report: {report_path}")
        if report_path.exists():
            print("Report file exists.")
    else:
        print("Report: none")
    print(f"Suggested diff command: git -C {root} diff -- {task.path}")
    return 0


def watch(root: Path, interval: float = 5.0, once: bool = False) -> int:
    last_state: dict[str, tuple[str, str]] = {}
    reviewed: set[str] = set()
    notify_cmd = os.environ.get("AGENT_RUNNER_REVIEW_CMD", "").strip()

    while True:
        tasks = load_tasks(root)
        current = {task.task_id: (task.status, task.report) for task in tasks}
        changed = False

        for task in tasks:
            promoted = promote_task_after_report(root, task)
            if promoted is not None:
                changed = True
                updated = queue_task_by_id(root, task.task_id)
                current[updated.task_id] = (updated.status, updated.report)
                print(f"AUTO TRANSITION: {updated.task_id} -> {updated.status}")
                print("\a", end="", flush=True)
                if notify_cmd and updated.status == "review-needed":
                    env = worker_env(root, updated, updated.path.parent, "unknown")
                    env["AGENT_RUNNER_EVENT"] = "review-needed"
                    env["AGENT_RUNNER_EVENT_REASON"] = "report-ready"
                    subprocess.run(
                        notify_cmd,
                        cwd=root,
                        env=env,
                        shell=True,
                        check=False,
                    )

        for task in tasks:
            key = (task.status, task.report)
            prev = last_state.get(task.task_id)
            if task.status == "review-needed" and (prev != key or task.task_id not in reviewed):
                reviewed.add(task.task_id)
                print(f"REVIEW NEEDED: {task.task_id} -> {task.path}")
                if task.report:
                    print(f"  report: {task.report}")
                print("\a", end="", flush=True)
                if notify_cmd:
                    env = worker_env(root, task, task.path.parent, "unknown")
                    env["AGENT_RUNNER_EVENT"] = "review-needed"
                    subprocess.run(
                        notify_cmd,
                        cwd=root,
                        env=env,
                        shell=True,
                        check=False,
                    )

        if changed:
            sync(root)
            tasks = load_tasks(root)
            current = {task.task_id: (task.status, task.report) for task in tasks}

        last_state = current

        if once:
            return 0
        time.sleep(interval)


def next_task(root: Path) -> int:
    tasks = load_tasks(root)
    open_task = next((task for task in tasks if task.status == "open"), None)
    if open_task is None:
        print("No open tasks found.")
        return 0
    print(f"Next task: {open_task.task_id}")
    print(f"Path: {open_task.path}")
    title = display_title(open_task.task_id, open_task.title)
    print(f"Title: {title}")
    print(f"Suggested command: agent-runner spawn {open_task.task_id}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="agent-runner")
    parser.add_argument(
        "--repo",
        default=".",
        help="Path inside the repo; the runner walks upward to find the root.",
    )
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("status")
    sub.add_parser("sync")
    sub.add_parser("next")
    sub.add_parser("slices")

    p_spawn = sub.add_parser("spawn")
    p_spawn.add_argument("task_id")

    p_dispatch = sub.add_parser("dispatch-slices")
    p_dispatch.add_argument("--dry-run", action="store_true")
    p_dispatch.add_argument("--limit", type=int, default=None)

    p_review = sub.add_parser("review")
    p_review.add_argument("task_id")

    p_phase = sub.add_parser("phase")
    p_phase.add_argument("phase_number", type=int)
    p_phase.add_argument("--dry-run", action="store_true")
    p_phase.add_argument("--limit", type=int, default=None)

    p_watch = sub.add_parser("watch")
    p_watch.add_argument("--interval", type=float, default=5.0)
    p_watch.add_argument("--once", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    root = repo_root(Path(args.repo))

    if args.command == "status":
        return status(root)
    if args.command == "sync":
        return sync(root)
    if args.command == "next":
        return next_task(root)
    if args.command == "slices":
        return slices(root)
    if args.command == "spawn":
        return spawn(root, args.task_id)
    if args.command == "dispatch-slices":
        return dispatch_slices(root, dry_run=args.dry_run, limit=args.limit)
    if args.command == "review":
        return review(root, args.task_id)
    if args.command == "phase":
        return phase(root, args.phase_number, dry_run=args.dry_run, limit=args.limit)
    if args.command == "watch":
        return watch(root, interval=args.interval, once=args.once)

    parser.error(f"Unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
