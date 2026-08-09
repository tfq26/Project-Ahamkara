#!/usr/bin/env python3
"""Validate the Ahamkara Philosophy section on the landing page.

Acceptance criteria for the "Write Content: Ahamkara Philosophy Section"
issue:
  * The landing page has a section titled "Ahamkara Philosophy".
  * The section contains at least 2 paragraphs of descriptive text.
  * The content is non-technical and accessible to general audiences.

The landing page lives in public/index.html.

Each check returns a list of error strings; an empty list means the check
passed. This mirrors the existing tests/unit/landing_page/*.py validation
scripts.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# This file lives at tests/unit/landing_page/, so the repo root is three
# parents up.
ROOT = Path(__file__).resolve().parents[3]

INDEX = ROOT / "public" / "index.html"

# Technical jargon that would make the section opaque to general audiences.
# The section is meant to explain the philosophy and core design concepts in
# plain language, so none of these should appear in the section's body copy.
TECHNICAL_TERMS = [
    "C++20",
    "UDP",
    "GLFW3",
    "CMake",
    "Ninja",
    "netcode",
    "API",
    "SDK",
    "PBR",
    "entity-component",
]


def _read_index() -> str | None:
    return INDEX.read_text(encoding="utf-8") if INDEX.is_file() else None


def _strip_comments(html: str) -> str:
    """Remove <!-- ... --> comments so docs inside them never look like real
    markup."""
    return re.sub(r"<!--.*?-->", "", html, flags=re.DOTALL)


def _philosophy_section(html: str) -> str | None:
    """Return the inner HTML of the #philosophy section, or None."""
    match = re.search(
        r'<section\b[^>]*id=["\']philosophy["\'][^>]*>(.*?)</section>',
        _strip_comments(html),
        re.IGNORECASE | re.DOTALL,
    )
    return match.group(1) if match else None


# ---------------------------------------------------------------------------
# Acceptance-criterion checks
# ---------------------------------------------------------------------------


def test_index_exists() -> list[str]:
    errors = []
    if not INDEX.is_file():
        errors.append(f"Missing {INDEX.relative_to(ROOT)}")
    return errors


def test_philosophy_section_present() -> list[str]:
    """The landing page must contain a section with id="philosophy"."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    if _philosophy_section(html) is None:
        errors.append('Missing section with id="philosophy"')
    return errors


def test_section_titled_ahamkara_philosophy() -> list[str]:
    """Acceptance criterion: section is titled "Ahamkara Philosophy".

    The heading inside the philosophy section must read exactly
    "Ahamkara Philosophy" (case-insensitive).
    """
    html = _read_index()
    if html is None:
        return []
    section = _philosophy_section(html)
    if section is None:
        return []
    errors = []
    heading = re.search(r"<h2\b[^>]*>(.*?)</h2>", section, re.IGNORECASE | re.DOTALL)
    if heading is None:
        errors.append("Philosophy section has no <h2> heading")
    elif heading.group(1).strip().lower() != "ahamkara philosophy":
        errors.append(f'Philosophy section heading is {heading.group(1).strip()!r}; expected "Ahamkara Philosophy"')
    return errors


def test_at_least_two_paragraphs() -> list[str]:
    """Acceptance criterion: at least 2 paragraphs of descriptive text.

    Paragraphs are counted as <p> elements (excluding nothing special; the
    section lead is itself a <p>). Each paragraph must contain real words,
    not just a single label.
    """
    html = _read_index()
    if html is None:
        return []
    section = _philosophy_section(html)
    if section is None:
        return []
    errors = []
    paragraphs = re.findall(r"<p\b[^>]*>(.*?)</p>", section, re.IGNORECASE | re.DOTALL)
    meaningful = [
        text
        for text in paragraphs
        if len([w for w in re.split(r"\s+", re.sub(r"<[^>]+>", "", text).strip()) if w]) >= 15
    ]
    if len(paragraphs) < 2:
        errors.append(f"Philosophy section has only {len(paragraphs)} paragraph(s); expected at least 2")
    if len(meaningful) < 2:
        errors.append(f"Only {len(meaningful)} paragraph(s) contain substantive descriptive text; expected at least 2")
    return errors


def test_content_non_technical() -> list[str]:
    """Implementation note: content should be non-technical, accessible to
    general audiences. None of the project's heavy technical jargon should
    appear in the philosophy section's body copy."""
    html = _read_index()
    if html is None:
        return []
    section = _philosophy_section(html)
    if section is None:
        return []
    errors = []
    text = re.sub(r"<[^>]+>", " ", section)
    for term in TECHNICAL_TERMS:
        if re.search(rf"\b{re.escape(term)}\b", text, re.IGNORECASE):
            errors.append(f"Philosophy section contains technical term {term!r}; content should be non-technical")
    return errors


def test_no_placeholder_text() -> list[str]:
    """Proofread sanity check: the section must not contain placeholder or
    lorem-ipsum filler text."""
    html = _read_index()
    if html is None:
        return []
    section = _philosophy_section(html)
    if section is None:
        return []
    errors = []
    text = re.sub(r"<[^>]+>", " ", section).lower()
    for marker in ("lorem ipsum", "todo", "tbd", "placeholder", "xxx"):
        if marker in text:
            errors.append(f"Philosophy section contains placeholder text {marker!r}")
    return errors


def test_philosophy_heading_is_accessible_name() -> list[str]:
    """The section must expose its heading as the accessible name via
    aria-labelledby, matching the pattern used by the other sections."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    section = re.search(
        r'<section\b[^>]*id=["\']philosophy["\'][^>]*>',
        _strip_comments(html),
        re.IGNORECASE,
    )
    if section is None:
        return []
    tag = section.group(0)
    if 'aria-labelledby="philosophy-heading"' not in tag:
        errors.append('Philosophy section does not use aria-labelledby="philosophy-heading"')
    if not re.search(r'<h2\b[^>]*id=["\']philosophy-heading["\']', _philosophy_section(html) or ""):
        errors.append('Philosophy section heading does not use id="philosophy-heading"')
    return errors


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

CHECKS = [
    ("index_exists", test_index_exists),
    ("philosophy_section_present", test_philosophy_section_present),
    ("section_titled_ahamkara_philosophy", test_section_titled_ahamkara_philosophy),
    ("at_least_two_paragraphs", test_at_least_two_paragraphs),
    ("content_non_technical", test_content_non_technical),
    ("no_placeholder_text", test_no_placeholder_text),
    ("philosophy_heading_is_accessible_name", test_philosophy_heading_is_accessible_name),
]


def main() -> int:
    has_errors = False
    for name, fn in CHECKS:
        errors = fn()
        if errors:
            has_errors = True
            print(f"FAIL [{name}]:")
            for err in errors:
                print(f"  - {err}")
        else:
            print(f"OK   [{name}]")

    if has_errors:
        print("\nLanding page philosophy validation FAILED.")
        return 1

    print("\nLanding page philosophy validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
