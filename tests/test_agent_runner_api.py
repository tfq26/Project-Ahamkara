"""Focused, offline tests for the Forgejo API boundary in agent-runner.py."""

import importlib.util
import os
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = ROOT / "agent-runner.py"
TEST_ENV = {
    "FORGEJO_URL": "https://forgejo.example/forgejo/",
    "FORGEJO_REPOSITORY": "taufeeq26/Project-Ahamkara",
    "FORGEJO_TOKEN": "unit-test-token",
    "GEMINI_API_KEY": "unit-test-gemini-key",
    "DEEPSEEK_API_KEY": "unit-test-deepseek-key",
}

with patch.dict(os.environ, TEST_ENV):
    spec = importlib.util.spec_from_file_location("ahamkara_agent_runner", RUNNER_PATH)
    runner = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = runner
    spec.loader.exec_module(runner)


class ForgejoApiTests(unittest.TestCase):
    def test_build_api_url_keeps_instance_prefix_and_encodes_query(self):
        url = runner.build_api_url(
            "issues",
            {"state": "open", "page": 2, "limit": 50},
        )
        self.assertEqual(
            url,
            "https://forgejo.example/forgejo/api/v1/repos/taufeeq26/Project-Ahamkara/issues?state=open&page=2&limit=50",
        )

    def test_auth_headers_use_configured_forgejo_token(self):
        with patch.object(runner, "FORGEJO_TOKEN", "test-auth-token"):
            headers = runner._forgejo_headers()
        self.assertEqual(headers["Authorization"], "token test-auth-token")
        self.assertEqual(headers["Accept"], "application/json")
        self.assertNotIn("test-auth-token", headers["User-Agent"])

    def test_api_get_all_fetches_each_page_until_short_page(self):
        get_page = Mock(side_effect=[[{"number": 1}, {"number": 2}], [{"number": 3}]])
        with patch.object(runner, "api_get", get_page):
            items = runner.api_get_all("issues", {"state": "open"}, page_size=2)

        self.assertEqual([item["number"] for item in items], [1, 2, 3])
        self.assertEqual(
            get_page.call_args_list,
            [
                unittest.mock.call("issues", {"state": "open", "page": 1, "limit": 2}),
                unittest.mock.call("issues", {"state": "open", "page": 2, "limit": 2}),
            ],
        )

    def test_task_issue_parser_excludes_pull_requests_and_non_task_issues(self):
        task_issue = {
            "number": 7,
            "state": "open",
            "labels": [{"name": "Task"}],
        }
        pull_request = {
            "number": 8,
            "state": "open",
            "labels": [{"name": "task"}],
            "pull_request": {"url": "https://forgejo.example/api/v1/repos/x/y/pulls/8"},
        }
        other_issue = {"number": 9, "state": "open", "labels": [{"name": "bug"}]}
        closed_task = {"number": 10, "state": "closed", "labels": [{"name": "task"}]}

        self.assertTrue(runner.is_open_task_issue(task_issue))
        self.assertFalse(runner.is_open_task_issue(pull_request))
        self.assertFalse(runner.is_open_task_issue(other_issue))
        self.assertFalse(runner.is_open_task_issue(closed_task))


if __name__ == "__main__":
    unittest.main()
