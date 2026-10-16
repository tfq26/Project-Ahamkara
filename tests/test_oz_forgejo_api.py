#!/usr/bin/env python3
"""Focused unit tests for Oz Forgejo API URL, auth, and pagination behavior."""

from __future__ import annotations

import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def load_script(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / filename)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {filename}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class FakeResponse:
    def __init__(self, body: dict | list, link: str = "") -> None:
        self.body = json.dumps(body).encode("utf-8")
        self.headers = {"Link": link}

    def __enter__(self) -> FakeResponse:
        return self

    def __exit__(self, *_args) -> None:
        return None

    def read(self) -> bytes:
        return self.body


class ForgejoApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp_dir = tempfile.TemporaryDirectory(prefix="oz-forgejo-tests-")
        cls._environment = patch.dict(
            os.environ,
            {
                "FORGEJO_URL": "https://forgejo.example.test",
                "FORGEJO_REPOSITORY": "owner/ahamkara",
                "FORGEJO_TOKEN": "unit-test-token",
                "AHAMKARA_CLONE": cls.temp_dir.name,
                "OZ_LOG_DIR": cls.temp_dir.name,
            },
        )
        cls._environment.start()
        cls.scripts = {
            "oz_poller": load_script("oz_poller_test_module", "oz-poller.py"),
            "oz_runner": load_script("oz_runner_test_module", "oz-runner.py"),
        }

    @classmethod
    def tearDownClass(cls) -> None:
        cls._environment.stop()
        cls.temp_dir.cleanup()

    def test_repository_api_url_supports_base_and_api_root_urls(self) -> None:
        for module in self.scripts.values():
            with self.subTest(module=module.__name__):
                self.assertEqual(
                    module.forgejo_api_url("issues/17/comments"),
                    "https://forgejo.example.test/api/v1/repos/owner/ahamkara/issues/17/comments",
                )
                with patch.dict(os.environ, {"FORGEJO_URL": "https://forgejo.example.test/api/v1/"}):
                    self.assertEqual(
                        module.forgejo_api_url("/issues/17"),
                        "https://forgejo.example.test/api/v1/repos/owner/ahamkara/issues/17",
                    )

    def test_forgejo_token_header_uses_token_scheme(self) -> None:
        for module in self.scripts.values():
            with self.subTest(module=module.__name__):
                self.assertEqual(
                    module.forgejo_api_headers(),
                    {"Authorization": "token unit-test-token", "Accept": "application/json"},
                )
                with patch.dict(os.environ, {}, clear=True), self.assertRaisesRegex(RuntimeError, "FORGEJO_TOKEN"):
                    module.forgejo_api_headers()

    def test_runner_post_uses_forgejo_url_and_token_header(self) -> None:
        requests = []

        def fake_urlopen(request, timeout):
            requests.append(request)
            self.assertEqual(timeout, 30)
            return FakeResponse({"id": 9})

        with patch.object(self.scripts["oz_runner"].urllib.request, "urlopen", side_effect=fake_urlopen):
            result = self.scripts["oz_runner"].api_post("issues/17/comments", {"body": "test comment"})

        self.assertEqual(result, {"id": 9})
        self.assertEqual(len(requests), 1)
        request = requests[0]
        self.assertEqual(
            request.full_url,
            "https://forgejo.example.test/api/v1/repos/owner/ahamkara/issues/17/comments",
        )
        self.assertEqual(request.get_method(), "POST")
        self.assertEqual(request.get_header("Authorization"), "token unit-test-token")
        self.assertEqual(request.get_header("Content-type"), "application/json")
        self.assertEqual(json.loads(request.data), {"body": "test comment"})

    def test_list_methods_follow_link_pagination_and_keep_query(self) -> None:
        for module in self.scripts.values():
            with self.subTest(module=module.__name__):
                requests = []

                def fake_urlopen(request, timeout, requests=requests):
                    requests.append(request)
                    self.assertEqual(timeout, 30)
                    if len(requests) == 1:
                        return FakeResponse(
                            [{"number": 1}],
                            '<https://ignored.example.test/page/2>; rel="next", '
                            '<https://ignored.example.test/page/3>; rel="last"',
                        )
                    return FakeResponse([{"number": 2}])

                with patch.object(module.urllib.request, "urlopen", side_effect=fake_urlopen):
                    items = module.api_get_paginated("issues?labels=task&state=open")

                self.assertEqual(items, [{"number": 1}, {"number": 2}])
                self.assertEqual(len(requests), 2)
                self.assertEqual(
                    requests[0].full_url,
                    "https://forgejo.example.test/api/v1/repos/owner/ahamkara/issues?labels=task&state=open&limit=50&page=1",
                )
                self.assertEqual(
                    requests[1].full_url,
                    "https://forgejo.example.test/api/v1/repos/owner/ahamkara/issues?labels=task&state=open&limit=50&page=2",
                )
                for request in requests:
                    self.assertEqual(request.get_header("Authorization"), "token unit-test-token")


if __name__ == "__main__":
    unittest.main()
