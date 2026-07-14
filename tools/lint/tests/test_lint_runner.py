#!/usr/bin/env python3
"""Tests for the Ahamkara lint orchestrator."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

RUNNER_PATH = Path(__file__).resolve().parents[1] / "run.py"
SPEC = importlib.util.spec_from_file_location("ahamkara_lint_runner", RUNNER_PATH)
assert SPEC is not None and SPEC.loader is not None
lint_runner = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = lint_runner
SPEC.loader.exec_module(lint_runner)


class LintRunnerTests(unittest.TestCase):
    def test_normalize_path_preserves_hidden_directories(self) -> None:
        self.assertEqual(lint_runner.normalize_path("./.github/workflows/ci.yml"), ".github/workflows/ci.yml")

    def test_first_party_cpp_excludes_vendored_sources(self) -> None:
        self.assertTrue(lint_runner.is_first_party_cpp("engine/core/src/job_system.cpp"))
        self.assertFalse(lint_runner.is_first_party_cpp("engine/ui/imgui.cpp"))
        self.assertFalse(lint_runner.is_first_party_cpp("engine/ui/imstb_textedit.h"))
        self.assertFalse(lint_runner.is_first_party_cpp("engine/render/glad/src/gl.cpp"))

    def test_hygiene_report_is_machine_readable(self) -> None:
        original_root = lint_runner.ROOT
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            report_dir = root / "reports"
            (root / "bad.json").write_bytes(b'{"ok": true,} \r\n')
            lint_runner.ROOT = root
            try:
                result = lint_runner.run_hygiene(["bad.json"], report_dir)
            finally:
                lint_runner.ROOT = original_root

            self.assertEqual(result.status, "fail")
            issues = json.loads((report_dir / "hygiene.json").read_text(encoding="utf-8"))
            codes = {issue["code"] for issue in issues}
            self.assertIn("non-lf-line-ending", codes)
            self.assertIn("trailing-whitespace", codes)
            self.assertIn("invalid-json", codes)

    def test_summary_records_failure_and_report_location(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            report_dir = Path(temp_dir)
            metadata = {
                "generated_at": "2026-07-13T00:00:00+00:00",
                "scope": "paths",
                "base_ref": None,
                "selected_files": 1,
                "compile_db": "build/debug",
            }
            result = lint_runner.CheckResult(
                "example",
                "fail",
                1,
                "example.txt",
                "exit 1",
                exit_code=1,
                excerpt="example failure",
            )

            passed = lint_runner.write_summary(report_dir, metadata, [result])
            payload = json.loads((report_dir / "summary.json").read_text(encoding="utf-8"))

            self.assertFalse(passed)
            self.assertFalse(payload["passed"])
            self.assertEqual(payload["checks"][0]["report"], "example.txt")
            self.assertIn("Overall:** FAIL", (report_dir / "summary.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
