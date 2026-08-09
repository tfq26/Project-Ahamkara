#!/usr/bin/env python3
"""Validation tests for the README Landing Page section.

The Landing Page change is documentation-only, so per docs/testing-quality.md
("Documentation-only change | Link/citation/path validation") the test verifies
that README.md documents the landing page purpose, features, file locations,
local development flow, and Vercel deployment instructions.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
README_PATH = REPO_ROOT / "README.md"


class ReadmeLandingPageTests(unittest.TestCase):
    """Checks that README.md documents the landing page and its deployment."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.readme = README_PATH.read_text(encoding="utf-8")

    def test_readme_exists(self) -> None:
        self.assertTrue(README_PATH.exists(), "README.md should exist at repo root")

    def test_has_landing_page_section(self) -> None:
        self.assertIn("## Landing Page", self.readme)

    def test_documents_landing_page_purpose(self) -> None:
        self.assertRegex(self.readme, r"## Purpose")
        self.assertIn("public-facing", self.readme)

    def test_documents_landing_page_features(self) -> None:
        self.assertIn("## Features", self.readme)
        self.assertIn("src/views/Home.vue", self.readme)
        self.assertIn("src/views/Docs.vue", self.readme)
        self.assertIn("Vue Router", self.readme)

    def test_documents_file_locations(self) -> None:
        # Acceptance criterion: README documents landing page file locations.
        expected_paths = [
            "index.html",
            "src/main.js",
            "src/router/routes.js",
            "src/views/Home.vue",
            "src/views/Docs.vue",
            "src/components/NavBar.vue",
            "vite.config.js",
            "package.json",
        ]
        for path in expected_paths:
            with self.subTest(path=path):
                self.assertIn(path, self.readme)

    def test_documents_local_development(self) -> None:
        self.assertIn("## Local Development", self.readme)
        self.assertIn("npm install", self.readme)
        self.assertIn("npm run dev", self.readme)
        self.assertIn("npm run build", self.readme)

    def test_documents_vercel_deployment(self) -> None:
        # Acceptance criterion: README documents how to deploy/update via Vercel.
        self.assertIn("## Deploying to Vercel", self.readme)
        self.assertIn("Vercel", self.readme)
        self.assertIn("npm run build", self.readme)
        self.assertIn("dist", self.readme)
        self.assertRegex(self.readme, r"vercel --prod")

    def test_links_to_info_site_repository(self) -> None:
        self.assertIn("ahamkara-info-site", self.readme)
        self.assertIn("git.2helix.org/taufeeq26/ahamkara-info-site", self.readme)

    def test_local_dev_server_port_matches_source(self) -> None:
        # The dev server port is documented by the info-site repo.
        self.assertIn("localhost:5173", self.readme)


class ReadmeMarkdownHealthTests(unittest.TestCase):
    """Lightweight markdown sanity checks for the edited README section."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.readme = README_PATH.read_text(encoding="utf-8")

    def test_section_headings_use_atx_level_two(self) -> None:
        for heading in (
            "## Landing Page",
            "## Repository Layout",
        ):
            with self.subTest(heading=heading):
                self.assertIn(heading, self.readme)

    def test_inline_code_blocks_are_balanced(self) -> None:
        # No stray single backticks within the landing page section. Fenced
        # code block markers (```) are excluded; only inline code pairs count.
        landing = self.readme.split("## Landing Page", 1)[1].split("## Repository Layout", 1)[0]
        backtick_lines = [ln for ln in landing.splitlines() if "`" in ln]
        for ln in backtick_lines:
            if re.match(r"^\s*(```|~~~)", ln):
                continue
            self.assertEqual(ln.count("`") % 2, 0, f"unbalanced backticks: {ln!r}")

    def test_fenced_code_blocks_are_closed(self) -> None:
        landing = self.readme.split("## Landing Page", 1)[1].split("## Repository Layout", 1)[0]
        fences = [ln for ln in landing.splitlines() if re.match(r"^\s*(```|~~~)", ln)]
        self.assertEqual(len(fences) % 2, 0, "unbalanced fenced code blocks")


if __name__ == "__main__":
    unittest.main()
