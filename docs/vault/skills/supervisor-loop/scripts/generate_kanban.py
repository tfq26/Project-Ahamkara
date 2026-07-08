#!/usr/bin/env python3
"""Generate an Obsidian-friendly Kanban board from task note frontmatter."""

from __future__ import annotations

import re
import sys
from pathlib import Path


STATUSES = [
    ("open", "Open"),
    ("claimed", "Claimed"),
    ("in_progress", "In Progress"),
    ("review_needed", "Review Needed"),
    ("revise_needed", "Revise Needed"),
    ("verify_needed", "Verify Needed"),
    ("blocked", "Blocked"),
    ("stopped", "Stopped"),
    ("complete", "Complete"),
]


def parse_frontmatter(text: str) -> dict[str, str]:
    if not text.startswith("---\n"):
        return {}
    end = text.find("\n---", 4)
    if end == -1:
        return {}
    data: dict[str, str] = {}
    for line in text[4:end].splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        value = value.strip().strip('"').strip("'")
        data[key.strip()] = value
    return data


def first_heading(text: str, fallback: str) -> str:
    match = re.search(r"^#\s+(.+)$", text, re.MULTILINE)
    return match.group(1).strip() if match else fallback


def display_title(task_id: str, title: str) -> str:
    prefix = f"{task_id} - "
    return title[len(prefix) :] if title.startswith(prefix) else title


def task_notes(vault: Path) -> list[dict[str, str]]:
    tasks_dir = vault / "03-Tasks"
    if not tasks_dir.exists():
        raise SystemExit(f"Missing tasks directory: {tasks_dir}")

    notes = []
    for path in sorted(tasks_dir.glob("TASK-*.md")):
        if path.name.endswith("-template.md"):
            continue
        text = path.read_text(encoding="utf-8")
        meta = parse_frontmatter(text)
        task_id = meta.get("id") or path.stem
        status = meta.get("status", "open")
        title = first_heading(text, task_id)
        notes.append(
            {
                "id": task_id,
                "status": status,
                "title": display_title(task_id, title),
                "owner": meta.get("owner", "unclaimed"),
                "supervisor": meta.get("supervisor", "unassigned"),
                "progress": meta.get("progress", "0"),
                "priority": meta.get("priority", "normal"),
                "path": f"../03-Tasks/{path.name}",
            }
        )
    return notes


def render_board(notes: list[dict[str, str]]) -> str:
    by_status = {status: [] for status, _ in STATUSES}
    unknown = []
    for note in notes:
        if note["status"] in by_status:
            by_status[note["status"]].append(note)
        else:
            unknown.append(note)

    lines = [
        "# Agent Task Board",
        "",
        "> Generated from task note frontmatter. Edit task notes, then regenerate this board.",
        "",
    ]
    for status, heading in STATUSES:
        lines.extend([f"## {heading}", ""])
        if by_status[status]:
            for note in by_status[status]:
                card = (
                    f"- [{note['id']} - {note['title']}]({note['path']}) "
                    f"`{note['progress']}%` owner: `{note['owner']}` "
                    f"supervisor: `{note['supervisor']}` priority: `{note['priority']}`"
                )
                lines.append(card)
        else:
            lines.append("- _No tasks_")
        lines.append("")

    if unknown:
        lines.extend(["## Unknown Status", ""])
        for note in unknown:
            lines.append(f"- [{note['id']} - {note['title']}]({note['path']}) status: `{note['status']}`")
        lines.append("")

    return "\n".join(lines)


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: generate_kanban.py <obsidian-vault-path>", file=sys.stderr)
        return 2

    vault = Path(sys.argv[1]).expanduser().resolve()
    dashboard_dir = vault / "02-Dashboard"
    dashboard_dir.mkdir(parents=True, exist_ok=True)
    notes = task_notes(vault)
    board = render_board(notes)
    output = dashboard_dir / "agent-task-board.md"
    output.write_text(board + "\n", encoding="utf-8")
    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
