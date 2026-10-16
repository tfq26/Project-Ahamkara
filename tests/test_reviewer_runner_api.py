"""Focused offline tests for reviewer-runner's Forgejo API boundary."""

import importlib.util
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = ROOT / "reviewer-runner.py"
_LOG_TEMP = tempfile.TemporaryDirectory()
TEST_ENV = {
    "FORGEJO_URL": "https://forgejo.example/forgejo/",
    "FORGEJO_REPOSITORY": "taufeeq26/Project-Ahamkara",
    "FORGEJO_TOKEN": "offline-test-token",
    "GEMINI_API_KEY": "offline-gemini-key",
    "REVIEWER_LOG_DIR": _LOG_TEMP.name,
}

with patch.dict(os.environ, TEST_ENV):
    spec = importlib.util.spec_from_file_location("ahamkara_reviewer_runner", RUNNER_PATH)
    runner = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = runner
    spec.loader.exec_module(runner)


class ForgejoApiTests(unittest.TestCase):
    def test_build_api_url_keeps_prefix_and_encodes_query(self):
        url = runner.build_api_url(
            "issues/17/comments",
            {"state": "open", "page": 2, "limit": 50},
        )
        self.assertEqual(
            url,
            "https://forgejo.example/forgejo/api/v1/repos/"
            "taufeeq26/Project-Ahamkara/issues/17/comments"
            "?state=open&page=2&limit=50",
        )

    def test_auth_headers_use_forgejo_token(self):
        with patch.object(runner, "FORGEJO_TOKEN", "test-auth-token"):
            headers = runner._forgejo_headers()
        self.assertEqual(headers["Authorization"], "token test-auth-token")
        self.assertEqual(headers["Accept"], "application/json")
        self.assertNotIn("test-auth-token", headers["User-Agent"])

    def test_api_request_posts_json_to_forgejo_with_auth(self):
        response = Mock()
        response.__enter__ = Mock(return_value=response)
        response.__exit__ = Mock(return_value=False)
        response.read.return_value = b'{"number": 9, "html_url": "https://forgejo.example/x/y/pulls/9"}'

        with patch.object(runner.urllib.request, "urlopen", return_value=response) as urlopen:
            result = runner.api_request(
                "POST",
                "pulls",
                {"title": "Task", "body": "Closes #7", "head": "task/7", "base": "main"},
            )

        request = urlopen.call_args.args[0]
        self.assertEqual(
            request.full_url,
            "https://forgejo.example/forgejo/api/v1/repos/taufeeq26/Project-Ahamkara/pulls",
        )
        self.assertEqual(request.get_method(), "POST")
        self.assertEqual(request.get_header("Authorization"), "token offline-test-token")
        self.assertEqual(request.get_header("Content-type"), "application/json")
        self.assertEqual(
            result,
            {"number": 9, "html_url": "https://forgejo.example/x/y/pulls/9"},
        )

    def test_issue_review_comment_uses_repository_issue_comments_endpoint(self):
        review_body = f"{runner.MARKER_GEMINI_REVIEW}\n\nReview details"
        with patch.object(runner, "api_post") as api_post:
            runner.comment_on_issue(17, review_body)
        api_post.assert_called_once_with(
            "issues/17/comments",
            {"body": review_body},
        )

    def test_open_task_issues_filters_by_label_and_excludes_pull_requests(self):
        issues = [
            {"number": 7, "state": "open", "labels": [{"name": "Task"}]},
            {
                "number": 8,
                "state": "open",
                "labels": [{"name": "task"}],
                "pull_request": {"url": "https://forgejo.example/api/pulls/8"},
            },
            {"number": 9, "state": "open", "labels": [{"name": "bug"}]},
            {"number": 10, "state": "closed", "labels": [{"name": "task"}]},
        ]
        with patch.object(runner, "api_get_all", return_value=issues) as api_get_all:
            tasks = runner.open_task_issues()
        api_get_all.assert_called_once_with("issues", {"state": "open"})
        self.assertEqual([issue["number"] for issue in tasks], [7])

    def test_find_open_pull_request_matches_branch_and_same_repository(self):
        pulls = [
            {
                "number": 18,
                "head": {"ref": "task/7", "repo": {"full_name": "fork/Project-Ahamkara"}},
            },
            {
                "number": 19,
                "html_url": "https://forgejo.example/taufeeq26/Project-Ahamkara/pulls/19",
                "head": {"ref": "task/7", "repo": {"full_name": "taufeeq26/Project-Ahamkara"}},
            },
            {
                "number": 20,
                "head": {"ref": "task/8", "repo": {"full_name": "taufeeq26/Project-Ahamkara"}},
            },
        ]
        with patch.object(runner, "api_get_all", return_value=pulls) as api_get_all:
            pull = runner.find_open_pull_request("task/7")

        api_get_all.assert_called_once_with("pulls", {"state": "open"})
        self.assertEqual(pull["number"], 19)
        self.assertEqual(
            runner.pull_request_url(pull),
            "https://forgejo.example/taufeeq26/Project-Ahamkara/pulls/19",
        )

    def test_create_pull_request_posts_expected_payload(self):
        created = {"number": 22, "html_url": "https://forgejo.example/x/y/pulls/22"}
        payload = {
            "title": "[Task #7] Add feature",
            "body": "Closes #7",
            "head": "task/7",
            "base": "main",
        }
        with patch.object(runner, "api_post", return_value=created) as api_post:
            result = runner.create_pull_request(payload["title"], payload["body"], payload["head"], payload["base"])
        api_post.assert_called_once_with("pulls", payload)
        self.assertEqual(result, created)
        self.assertEqual(runner.pull_request_url(result), created["html_url"])

    def test_missing_forgejo_token_is_rejected(self):
        with (
            patch.object(runner, "FORGEJO_TOKEN", ""),
            self.assertRaisesRegex(RuntimeError, "FORGEJO_TOKEN is not set"),
        ):
            runner._forgejo_headers()


if __name__ == "__main__":
    unittest.main()
