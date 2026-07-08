#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


ROOT = Path(__file__).resolve().parents[3]
RUNNER = ROOT / "tools" / "agent-runner" / "agent-runner.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("agent_runner_test", RUNNER)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load runner from {RUNNER}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_display_title(mod) -> None:
    assert mod.display_title("TASK-1", "TASK-1") == "TASK-1"
    assert mod.display_title("TASK-1", "TASK-1 - Example") == "Example"
    assert mod.display_title("TASK-1", "Example") == "Example"


def test_status_normalization(mod) -> None:
    assert mod.normalize_status("complete", "claimed") == "completed"
    assert mod.normalize_status("review_needed", "claimed") == "review-needed"
    assert mod.normalize_status("self-validated", "claimed") == "claimed"
    assert mod.normalize_status("deferred", "claimed") == "blocked"


def test_auto_promote_after_report(mod) -> None:
    with TemporaryDirectory() as temp_dir:
        repo = Path(temp_dir)
        (repo / "docs/vault/queue-tasks/claimed").mkdir(parents=True)
        (repo / "docs/vault/queue-tasks/review-needed").mkdir(parents=True)
        (repo / "docs/reports/subagents").mkdir(parents=True)

        task_path = repo / "docs/vault/queue-tasks/claimed/TASK-20260704-1200-demo.md"
        task_path.write_text(
            """---\n"""
            "type: opencode-task\n"
            "status: claimed\n"
            "report: ../../../reports/subagents/TASK-20260704-1200-demo-report.md\n"
            "---\n"
            "# TASK-20260704-1200-demo\n",
            encoding="utf-8",
        )

        report_path = repo / "docs/reports/subagents/TASK-20260704-1200-demo-report.md"
        report_path.write_text(
            """---\n"""
            "type: subagent-report\n"
            "status: implemented\n"
            "---\n"
            "# Subagent Report\n",
            encoding="utf-8",
        )

        task = mod.queue_task_by_id(repo, "TASK-20260704-1200-demo")
        promoted = mod.promote_task_after_report(repo, task)

        assert promoted is not None
        assert promoted.parent.name == "review-needed"
        assert not task_path.exists()
        assert promoted.exists()
        assert mod.queue_task_by_id(repo, "TASK-20260704-1200-demo").status == "review-needed"


def test_slice_grouping(mod) -> None:
    with TemporaryDirectory() as temp_dir:
        repo = Path(temp_dir)
        (repo / "docs/vault/queue-tasks/open").mkdir(parents=True)

        feature_a = "features/2026-07-04-combat-system.md"
        feature_b = "features/2026-07-04-movement-system.md"

        (repo / "docs/vault/queue-tasks/open/TASK-20260704-1000-alpha.md").write_text(
            """---\n"""
            f"type: opencode-task\nstatus: open\nrelated_feature: {feature_a}\nreport: ../../../reports/subagents/TASK-20260704-1000-alpha-report.md\n---\n# TASK-20260704-1000-alpha\n",
            encoding="utf-8",
        )
        (repo / "docs/vault/queue-tasks/open/TASK-20260704-1010-bravo.md").write_text(
            """---\n"""
            f"type: opencode-task\nstatus: open\nrelated_feature: {feature_a}\nreport: ../../../reports/subagents/TASK-20260704-1010-bravo-report.md\n---\n# TASK-20260704-1010-bravo\n",
            encoding="utf-8",
        )
        (repo / "docs/vault/queue-tasks/open/TASK-20260704-1020-charlie.md").write_text(
            """---\n"""
            f"type: opencode-task\nstatus: open\nslice: movement-pass\nrelated_feature: {feature_b}\nreport: ../../../reports/subagents/TASK-20260704-1020-charlie-report.md\n---\n# TASK-20260704-1020-charlie\n",
            encoding="utf-8",
        )

        tasks = mod.load_tasks(repo)
        groups = mod.group_tasks_by_slice(tasks, include_statuses=("open",))
        grouped = {key: [task.task_id for task in items] for key, _, items in groups}

        combat_key = mod.normalize_slice_key(feature_a)
        movement_key = mod.normalize_slice_key("movement-pass")

        assert grouped[combat_key] == [
            "TASK-20260704-1000-alpha",
            "TASK-20260704-1010-bravo",
        ]
        assert grouped[movement_key] == ["TASK-20260704-1020-charlie"]


def main() -> int:
    mod = load_runner()
    test_display_title(mod)
    test_status_normalization(mod)
    test_auto_promote_after_report(mod)
    test_slice_grouping(mod)
    print("agent-runner python regression tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
