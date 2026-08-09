#!/usr/bin/env python3
"""Validate the landing page HTML/CSS files carry inline comments.

Acceptance criteria for the "Document Landing Page Code with Inline
Comments" issue: all major sections and logic must be commented.

This script verifies that each major structural section in the landing
page HTML and CSS files is documented with an inline comment that
mentions the section. It does not change behaviour; it guards against the
documentation being removed.
"""

from __future__ import annotations

import sys
from pathlib import Path

# This file lives at tests/unit/landing_page/, so the repo root is three
# parents up.
ROOT = Path(__file__).resolve().parents[3]

# Files covered by the issue. The primary landing page HTML is
# public/index.html; the full marketing landing page HTML + CSS pair is
# site/index.html + site/styles.css (deployed via vercel.json).
#
# Each entry maps a required section to the keyword that a documenting
# comment for that section must mention (case-insensitive).
FILES = {
    "public/index.html": {
        "file_intent": "landing page",
        "head_metadata": "metadata",
        "inline_styles": "stylesheet",
        "main_content": "main",
        "spa_redirect": "spa redirect",
    },
    "site/index.html": {
        "file_intent": "landing page",
        "navigation": "navigation",
        "hero": "hero",
        "ahamkara": "ahamkara",
        "wish": "wish",
        "inspiration": "inspiration",
        "footer": "footer",
        "scripts": "script",
    },
    "site/styles.css": {
        "reset": "reset",
        "nav": "nav",
        "hero": "hero",
        "buttons": "buttons",
        "sections": "sections",
        "grid": "grid",
        "inspiration": "inspiration",
        "footer": "footer",
        "keyframes": "keyframes",
        "responsive": "responsive",
    },
}


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _comment_blocks(content: str, kind: str) -> list[str]:
    """Extract comment blocks. HTML uses <!-- ... -->, CSS uses /* ... */."""
    if kind == "html":
        start, end = "<!--", "-->"
    else:
        start, end = "/*", "*/"
    blocks: list[str] = []
    i = 0
    while True:
        i = content.find(start, i)
        if i == -1:
            break
        j = content.find(end, i + len(start))
        if j == -1:
            break
        blocks.append(content[i : j + len(end)])
        i = j + len(end)
    return blocks


def test_file_has_section_comments(path: str) -> list[str]:
    full_path = ROOT / path
    if not full_path.is_file():
        return [f"Missing file: {path}"]

    content = _read(full_path)
    kind = "html" if path.endswith(".html") else "css"
    blocks = _comment_blocks(content, kind)
    if not blocks:
        return [f"[{path}] contains no comments at all"]

    errors: list[str] = []
    for name, keyword in FILES[path].items():
        # Each required section must be described by an inline comment that
        # mentions the section keyword (case-insensitive).
        if not any(keyword.lower() in block.lower() for block in blocks):
            errors.append(f"[{name}] missing comment mentioning {keyword!r}")
    return errors


def test_comment_ratio(path: str) -> list[str]:
    """Sanity check: a documented file should have a meaningful share of
    comment lines. Catches edits that strip comments wholesale."""
    full_path = ROOT / path
    if not full_path.is_file():
        return []
    content = _read(full_path)
    lines = [ln.strip() for ln in content.splitlines() if ln.strip()]
    if not lines:
        return [f"[{path}] file is empty"]

    # Count lines that belong to a comment block (<!-- ... --> or /* ... */),
    # including the interior text of multi-line comments.
    kind = "html" if path.endswith(".html") else "css"
    start_marker = "<!--" if kind == "html" else "/*"
    end_marker = "-->" if kind == "html" else "*/"

    in_comment = False
    comment_lines = 0
    for ln in lines:
        if in_comment:
            comment_lines += 1
            if end_marker in ln:
                in_comment = False
            continue
        if start_marker in ln:
            comment_lines += 1
            if end_marker not in ln:
                in_comment = True

    ratio = comment_lines / len(lines)
    if ratio < 0.10:
        return [f"[{path}] comment ratio too low ({ratio:.0%}); expected >= 10%"]
    return []


def main() -> int:
    has_errors = False
    for path in FILES:
        errors = test_file_has_section_comments(path)
        errors += test_comment_ratio(path)
        if errors:
            has_errors = True
            print(f"FAIL [{path}]:")
            for err in errors:
                print(f"  - {err}")
        else:
            print(f"OK [{path}]")

    if has_errors:
        print("\nLanding page comment validation FAILED.")
        return 1

    print("\nLanding page comment validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
