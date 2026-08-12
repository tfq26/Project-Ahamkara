#!/usr/bin/env python3
"""Validate the macOS Flashback development workflow guide.

The guide is the deliverable for the split-development workflow: Flashback
runs locally on a Mac with a display while headless engine/server/pipeline
work stays on servlenovo1, with Forgejo as the repository source of truth.

Checks:
  - the guide exists and contains the required sections;
  - it identifies the Forgejo URL as the source of truth;
  - it documents the macOS configure/build/launch/test commands;
  - it documents the headless server-side preset and CI validation;
  - every relative file link in the guide resolves to a real repo file.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DOC = REPO / "docs/guides/macos-flashback-workflow.md"

# Required content markers (each is a plain substring of the guide).
REQUIRED_SECTIONS = [
    "## 1. Clone the repository on the Mac",
    "## 2. Configure, build, and run Flashback locally (macOS)",
    "## 3. Keep headless engine/server work on servlenovo1",
    "## 4. Push local changes to Forgejo and validate on the server",
    "## Acceptance criteria mapping",
]

REQUIRED_MARKERS = [
    # Forgejo as source of truth, no GitHub drift.
    "https://git.2helix.org/taufeeq26/Project-Ahamkara",
    "git remote rename origin forgejo",
    "git push -u forgejo HEAD",
    "Avoiding GitHub drift",
    # macOS configure / build / launch / test commands.
    "brew install cmake ninja glfw",
    "xcode-select --install",
    "cmake --preset debug",
    "cmake --build --preset debug",
    "./scripts/start.sh flashback",
    "./scripts/run_flashback.sh",
    "ctest --test-dir build/debug --output-on-failure",
    # Headless server-side workflow.
    "./scripts/setup-dev.sh --preset debug-headless",
    "cmake --build --preset debug-headless",
    "./scripts/run-tests.sh --preset debug-headless",
    # Server validation via CI.
    ".forgejo/workflows/ci.yml",
]


def test_doc_exists() -> list[str]:
    errors: list[str] = []
    if not DOC.is_file():
        errors.append(f"Missing workflow guide: {DOC.relative_to(REPO)}")
    return errors


def test_required_sections() -> list[str]:
    errors: list[str] = []
    if not DOC.is_file():
        return errors
    content = DOC.read_text(encoding="utf-8")
    for section in REQUIRED_SECTIONS:
        if section not in content:
            errors.append(f"Missing required section in guide: '{section}'")
    return errors


def test_required_markers() -> list[str]:
    errors: list[str] = []
    if not DOC.is_file():
        return errors
    content = DOC.read_text(encoding="utf-8")
    for marker in REQUIRED_MARKERS:
        if marker not in content:
            errors.append(f"Missing required marker in guide: '{marker}'")
    return errors


def test_relative_links_resolve() -> list[str]:
    """Every relative markdown link in the guide must resolve to a real file."""
    errors: list[str] = []
    if not DOC.is_file():
        return errors
    content = DOC.read_text(encoding="utf-8")
    link_re = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
    for match in link_re.finditer(content):
        href = match.group(2)
        if href.startswith(("http://", "https://", "#", "mailto:")):
            continue
        target = href.split("#")[0]
        if not target:
            continue
        candidate = (DOC.parent / target).resolve()
        if not candidate.exists():
            errors.append(f"Broken link in guide: '{href}' -> {candidate.relative_to(REPO)}")
    return errors


def main() -> int:
    has_errors = False
    for name, func in [
        ("doc_exists", test_doc_exists),
        ("required_sections", test_required_sections),
        ("required_markers", test_required_markers),
        ("relative_links", test_relative_links_resolve),
    ]:
        errors = func()
        if errors:
            has_errors = True
            print(f"FAIL [{name}]:")
            for e in errors:
                print(f"  - {e}")
        else:
            print(f"OK [{name}]")
    return 1 if has_errors else 0


if __name__ == "__main__":
    sys.exit(main())
