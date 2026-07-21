#!/usr/bin/env python3
"""
Plane Intake Triage Worker

Automates the triage lifecycle:
  Intake (PENDING) -> Accept -> Assign labels/priority -> Place in Cycle -> Assign to Module

Runs via cron or manually:
  python3 plane_triage_worker.py         # single run
  python3 plane_triage_worker.py --watch  # poll every 60s
"""

import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime, timedelta

# -- Configuration -----------------------------------------------------------
PLANE_BASE   = os.environ.get("PLANE_BASE", "http://172.18.0.29:8000/api/v1")
PLANE_WS     = "projects"
PLANE_PROJECT = "d491cc85-ce1e-4bfd-a8c0-67d7dbaebd5e"
PLANE_KEY    = os.environ.get("PLANE_API_KEY", "ec945f9d2896435591989c968eb4c341")

HDRS = {"X-API-Key": PLANE_KEY, "Content-Type": "application/json"}


# -- API Helpers -------------------------------------------------------------

def _req(path, data=None, method="GET"):
    url = f"{PLANE_BASE}{path}"
    kwargs = {"headers": HDRS, "method": method}
    if data is not None:
        kwargs["data"] = json.dumps(data).encode()
    req = urllib.request.Request(url, **kwargs)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")[:300]
        print(f"  [HTTP {e.code}] {method} {path[:50]}: {body}")
        return None


def _get(path):
    return _req(path)


def _post(path, data):
    return _req(path, data, "POST")


def _patch(path, data):
    return _req(path, data, "PATCH")


# -- Intake Operations -------------------------------------------------------

def list_pending_intake():
    data = _get(f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/intake-issues/")
    if not data:
        return []
    return [r for r in data.get("results", []) if r.get("status") == -2]


def accept_intake(issue_id):
    return _patch(
        f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/intake-issues/{issue_id}/",
        {"status": 1},
    )


def update_issue(issue_id, data):
    return _patch(
        f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/issues/{issue_id}/",
        data,
    )


# -- Infrastructure Management -----------------------------------------------

def get_or_create_current_cycle():
    """Find active cycle or create a weekly one."""
    data = _get(f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/cycles/")
    now = datetime.now()
    if data:
        for c in data.get("results", []):
            start = c.get("start_date", "")
            end = c.get("end_date", "")
            if start and end and start <= now.strftime("%Y-%m-%d") <= end:
                return c["id"]

    week_start = now - timedelta(days=now.weekday())
    week_end = week_start + timedelta(days=6)
    name = f"Sprint {week_start.strftime('%b %d')} - {week_end.strftime('%b %d, %Y')}"
    result = _post(
        f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/cycles/",
        {
            "name": name,
            "project_id": PLANE_PROJECT,
            "start_date": week_start.strftime("%Y-%m-%d"),
            "end_date": week_end.strftime("%Y-%m-%d"),
        },
    )
    if result:
        print(f"  [CYCLE] Created: {name}")
        return result.get("id")
    return None


def get_or_create_modules():
    """Fetch/create modules, return {name: id}."""
    data = _get(f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/modules/")
    modules = {}
    for m in (data or {}).get("results", []):
        modules[m["name"]] = m["id"]

    desired = [
        ("Engine Core", "engine/core, engine/runtime"),
        ("Rendering", "engine/render"),
        ("Physics & Collision", "engine/physics, engine/collision"),
        ("Networking", "engine/network"),
        ("Tools & SDK", "tools"),
        ("Animation & Audio", "engine/animation, engine/audio"),
        ("Governance", "docs, tracking"),
    ]

    for name, _ in desired:
        if name not in modules:
            result = _post(
                f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/modules/",
                {"name": name, "description": f"Work items for {name}"},
            )
            if result:
                modules[name] = result["id"]
                print(f"  [MODULE] Created: {name}")

    return modules


def get_label_map():
    data = _get(f"/workspaces/{PLANE_WS}/projects/{PLANE_PROJECT}/labels/")
    return {r["name"]: r["id"] for r in (data or {}).get("results", [])} if data else {}


# -- Rule-based Classifier ---------------------------------------------------

def classify(issue_detail):
    """Return (priority, set_of_labels, module_name)."""
    name = (issue_detail.get("name") or "").lower()
    desc = (issue_detail.get("description_html") or "").lower()
    text = f"{name} {desc}"

    # Priority keywords
    if re.search(r"crash|critical|security|data.loss|blocker|p0|sev0", text):
        priority = "urgent"
    elif re.search(r"bug|broken|fail|error|regression|p1|sev1", text):
        priority = "high"
    elif re.search(r"improve|refactor|optimize|p2|sev2", text):
        priority = "medium"
    else:
        priority = "none"

    # Label detection
    rules = {
        "engine/core":       r"\b(engine.core|core\s+(engine|module|system))\b",
        "engine/render":     r"\b(render|graphics|gpu|shader|vulkan|dx12|metal|lighting|shadow)\b",
        "engine/network":    r"\b(network|multiplayer|netcode|replication|snapshot|lag)\b",
        "engine/physics":    r"\b(physics|collision|rigidbody|character.controller)\b",
        "engine/animation":  r"\b(animation|skeleton|rig|blend|ik|skinning)\b",
        "engine/audio":      r"\b(audio|sound|music|mixer|spatial)\b",
        "engine/runtime":    r"\b(runtime|ecs|entity|system|schedule|job|workflow)\b",
        "tools":             r"\b(tool|editor|pipeline|build|ci|packaging|cmake)\b",
        "client":            r"\b(client|player|gameplay|input|controller|hud)\b",
        "server":            r"\b(server|backend|auth|session|lobby)\b",
        "docs":              r"\b(doc|readme|changelog|wiki|guide|spec)\b",
        "game":              r"\b(gameplay|level|map|weapon|enemy|ai|combat|boss)\b",
        "assets":            r"\b(asset|model|texture|material|mesh|fbx|gltf)\b",
        "blocked":           r"\b(blocked|blocking|depend|waiting)\b",
        "review-needed":     r"\b(review|feedback|approve|pr)\b",
        "observability":     r"\b(log|telemetry|monitor|tracing|metrics|profiling)\b",
        "tracking":          r"\b(epic|tracking|roadmap|phase|milestone)\b",
        "priority-critical": r"\b(priority.critical|p0|sev0|urgent)\b",
        "priority-high":     r"\b(priority.high|p1|sev1|important)\b",
    }

    labels = set()
    for label, pattern in rules.items():
        if re.search(pattern, text, re.IGNORECASE):
            labels.add(label)

    if not labels:
        labels.add("task")

    # Module assignment
    module = "Engine Core"
    if labels & {"engine/render"}:
        module = "Rendering"
    elif labels & {"engine/animation", "engine/audio"}:
        module = "Animation & Audio"
    elif labels & {"engine/core", "engine/runtime"}:
        module = "Engine Core"
    elif labels & {"engine/physics", "engine/collision"}:
        module = "Physics & Collision"
    elif labels & {"engine/network"}:
        module = "Networking"
    elif labels & {"tools"}:
        module = "Tools & SDK"
    elif labels & {"docs", "tracking"}:
        module = "Governance"

    return priority, labels, module


# -- Main Logic --------------------------------------------------------------

def run_triage():
    print("=" * 60)
    print(f"Plane Triage Worker - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 60)

    # Infrastructure
    modules = get_or_create_modules()
    current_cycle = get_or_create_current_cycle()
    label_map = get_label_map()
    print(f"  Modules: {len(modules)}  |  Cycle: {'yes' if current_cycle else 'no'}  |  Labels: {len(label_map)}")

    # Pending intake items
    pending = list_pending_intake()
    print(f"\nPending intake items: {len(pending)}")

    if not pending:
        print("No items to triage.")
        return

    stats = {"accepted": 0, "errors": 0}

    for idx, item in enumerate(pending):
        detail = item.get("issue_detail", {})
        issue_id = detail.get("id")
        name = detail.get("name", "?")

        print(f"\n  [{idx+1}/{len(pending)}] {name[:70]}")

        if not issue_id:
            print("    No issue_id, skipping")
            continue

        # Classify
        priority, labels, module_name = classify(detail)
        label_ids = [label_map.get(l) for l in labels if label_map.get(l)]
        print(f"    Priority: {priority}  Labels: {sorted(labels)}  Module: {module_name}")

        # Update issue while still in Triage
        update = {"priority": priority, "labels": label_ids}
        if module_name in modules:
            update["module_id"] = modules[module_name]
        if current_cycle:
            update["cycle_id"] = current_cycle

        result = update_issue(issue_id, update)
        if not result:
            print("    Failed to update issue")
            stats["errors"] += 1
            continue

        # Accept (moves Triage -> Backlog)
        result = accept_intake(issue_id)
        if result:
            stats["accepted"] += 1
        else:
            stats["errors"] += 1

        time.sleep(0.5)

    print(f"\n{'=' * 60}")
    print(f"Triage: {stats['accepted']} accepted, {stats['errors']} errors")
    print(f"{'=' * 60}")


def watch(interval=60):
    print(f"Watching every {interval}s (Ctrl+C to stop)")
    while True:
        try:
            run_triage()
        except KeyboardInterrupt:
            print("\nStopped")
            break
        except Exception as e:
            print(f"\nError: {e}")
        time.sleep(interval)


if __name__ == "__main__":
    if "--watch" in sys.argv:
        watch()
    else:
        run_triage()
