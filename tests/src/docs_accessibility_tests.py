#!/usr/bin/env python3
"""Validate documentation structure for accessibility (navigability, link integrity)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

DOCS = Path(__file__).resolve().parents[2] / "docs"

# Directories that should have a README or index file.
# The vault/ directory uses Obsidian wiki-links for navigation within its tree,
# so only top-level docs directories are validated.
CHECKED_DIRS = [
    DOCS / "architecture",
    DOCS / "design",
    DOCS / "guides",
    DOCS / "operations",
    DOCS / "reports",
    DOCS / "roadmap",
    DOCS / "systems",
    DOCS / "wish",
]


def test_all_dirs_have_readme() -> list[str]:
    errors: list[str] = []
    for dirpath in CHECKED_DIRS:
        if not dirpath.is_dir():
            continue
        has_index = any(
            (dirpath / name).is_file()
            for name in ("README.md", "index.md")
        )
        if not has_index:
            errors.append(f"Missing README.md or index.md in docs/{dirpath.relative_to(DOCS)}")
    return errors


def test_links_resolve() -> list[str]:
    errors: list[str] = []
    link_re = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")

    for md_file in sorted(DOCS.rglob("*.md")):
        if ".obsidian" in md_file.parts:
            continue
        rel = md_file.relative_to(DOCS)
        content = md_file.read_text(encoding="utf-8")

        for match in link_re.finditer(content):
            href = match.group(2)
            # Skip external URLs, anchors, mailto, and wiki-links
            if href.startswith(("http://", "https://", "#", "mailto:")):
                continue
            # Skip Obsidian wiki-links [[...]]
            if "[[" in content[max(0, match.start() - 1):match.start() + 1]:
                continue
            # Handle relative links
            target = (md_file.parent / href).resolve()
            # Strip anchor portion (e.g., file.md#section)
            if "#" in href:
                target = Path(href.split("#")[0])
                if not target.is_absolute():
                    target = (md_file.parent / target).resolve()
            # Allow links to files outside docs (source code references)
            try:
                target.relative_to(DOCS.parent)
            except ValueError:
                continue
            if not target.exists():
                errors.append(
                    f"Broken link in docs/{rel}: '{href}' (not found)"
                )
    return errors


def main() -> int:
    has_errors = False
    for name, func in [("dir_index", test_all_dirs_have_readme),
                       ("link_integrity", test_links_resolve)]:
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
