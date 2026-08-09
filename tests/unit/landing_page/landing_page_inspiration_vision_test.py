#!/usr/bin/env python3
"""Validate the "Inspiration & Vision" section on the Ahamkara landing page.

Acceptance criteria for the "Write Content: Project Inspiration and Vision"
issue:
  * A section titled "Inspiration & Vision" exists.
  * The section contains at least 2 paragraphs.
  * The section references games, frameworks, or philosophies that inspired
    the project.

The landing page lives in public/index.html and public/styles.css.

Each check returns a list of error strings; an empty list means the check
passed. This mirrors the existing tests/unit/landing_page/*.py validation
scripts.
"""

from __future__ import annotations

import html
import re
import sys
from pathlib import Path

# This file lives at tests/unit/landing_page/, so the repo root is three
# parents up.
ROOT = Path(__file__).resolve().parents[3]

INDEX = ROOT / "public" / "index.html"

# Required section id that the navigation bar links to.
SECTION_ID = "inspiration"

# Expected section title as a visitor sees it (HTML entities unescaped).
EXPECTED_TITLE = "Inspiration & Vision"

# Minimum number of paragraphs the section must contain.
MIN_PARAGRAPHS = 2

# Named references the issue requires. We look for at least one of these so
# the section demonstrably cites games, frameworks, or philosophies.
REFERENCE_KEYWORDS = [
    "Destiny",
    "Call of Duty",
    "Clean architecture",
    "data-oriented design",
    "deterministic networking",
    "C++20",
]


def _read_index() -> str | None:
    return INDEX.read_text(encoding="utf-8") if INDEX.is_file() else None


def _strip_comments(html_text: str) -> str:
    """Remove <!-- ... --> comments so docs inside them never look like real
    markup (e.g. a comment that mentions "Inspiration & Vision")."""
    return re.sub(r"<!--.*?-->", "", html_text, flags=re.DOTALL)


def _unescape(text: str) -> str:
    """Turn HTML entities back into the characters a visitor would read."""
    return html.unescape(text)


def _section_block(html_text: str, section_id: str) -> str | None:
    """Return the raw inner block of the first <section id=...> element."""
    pattern = rf'<section\b[^>]*\bid=["\']{re.escape(section_id)}["\'][^>]*>(.*?)</section>'
    match = re.search(pattern, _strip_comments(html_text), re.IGNORECASE | re.DOTALL)
    return match.group(1) if match else None


def _paragraph_texts(block: str) -> list[str]:
    """Return the unescaped inner text of every <p>...</p> in the block."""
    texts = []
    for para in re.findall(r"<p\b[^>]*>(.*?)</p>", block, re.IGNORECASE | re.DOTALL):
        # Collapse whitespace so paragraph length checks are meaningful.
        text = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", para)).strip()
        texts.append(text)
    return texts


def _navbar_links(html_text: str) -> list[str]:
    """Return the hrefs of anchor links inside the first <nav> element."""
    nav_match = re.search(r"<nav\b[^>]*>(.*?)</nav>", _strip_comments(html_text), re.IGNORECASE | re.DOTALL)
    if nav_match is None:
        return []
    return [
        href.group(2).strip()
        for anchor in re.findall(r"<a\b[^>]*>.*?</a>", nav_match.group(1), re.IGNORECASE | re.DOTALL)
        if (href := re.search(r'\bhref=(["\'])(.*?)\1', anchor, re.IGNORECASE | re.DOTALL))
    ]


# ---------------------------------------------------------------------------
# Acceptance-criterion checks
# ---------------------------------------------------------------------------


def test_files_exist() -> list[str]:
    errors = []
    if not INDEX.is_file():
        errors.append(f"Missing {INDEX.relative_to(ROOT)}")
    return errors


def test_section_present() -> list[str]:
    """A section with the expected id must exist on the landing page."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    if _section_block(html_text, SECTION_ID) is None:
        errors.append(f"Missing <section id={SECTION_ID!r}>")
    return errors


def test_section_title() -> list[str]:
    """The section must be titled 'Inspiration & Vision' (case-insensitive)."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    block = _section_block(html_text, SECTION_ID)
    if block is None:
        errors.append(f"Missing <section id={SECTION_ID!r}>")
        return errors
    title = _unescape(block)
    if re.search(re.escape(EXPECTED_TITLE), title, re.IGNORECASE) is None:
        errors.append(f"Section is not titled {EXPECTED_TITLE!r}")
    return errors


def test_section_has_at_least_two_paragraphs() -> list[str]:
    """Acceptance criterion: the section contains at least 2 paragraphs."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    block = _section_block(html_text, SECTION_ID)
    if block is None:
        errors.append(f"Missing <section id={SECTION_ID!r}>")
        return errors
    paragraphs = _paragraph_texts(block)
    # A lead paragraph that is only the section__lead is still a paragraph,
    # so count every <p> in the section.
    if len(paragraphs) < MIN_PARAGRAPHS:
        errors.append(f"Section contains {len(paragraphs)} paragraph(s); expected at least {MIN_PARAGRAPHS}")
    return errors


def test_section_references_inspirations() -> list[str]:
    """Acceptance criterion: the section references games, frameworks, or
    philosophies that inspired the project."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    block = _section_block(html_text, SECTION_ID)
    if block is None:
        errors.append(f"Missing <section id={SECTION_ID!r}>")
        return errors
    text = _unescape(_strip_comments(block))
    found = [keyword for keyword in REFERENCE_KEYWORDS if keyword.lower() in text.lower()]
    if not found:
        errors.append(
            "Section does not reference any known game, framework, or philosophy "
            f"(expected one of {REFERENCE_KEYWORDS})"
        )
    return errors


def test_nav_links_to_inspiration_section() -> list[str]:
    """The navigation bar must keep a link to the Inspiration & Vision
    section so visitors can reach it from the top of the page."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    hrefs = _navbar_links(html_text)
    if f"#{SECTION_ID}" not in hrefs:
        errors.append(f"Navigation bar has no link to section #{SECTION_ID}")
    return errors


def test_section_has_non_empty_paragraphs() -> list[str]:
    """Edge case: paragraphs must contain actual content, not empty shells."""
    html_text = _read_index()
    if html_text is None:
        return []
    errors = []
    block = _section_block(html_text, SECTION_ID)
    if block is None:
        errors.append(f"Missing <section id={SECTION_ID!r}>")
        return errors
    for index, paragraph in enumerate(_paragraph_texts(block), start=1):
        if len(paragraph) < 40:
            errors.append(f"Paragraph {index} is too short to carry real content ({len(paragraph)} chars)")
    return errors


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

CHECKS = [
    ("files_exist", test_files_exist),
    ("section_present", test_section_present),
    ("section_title", test_section_title),
    ("section_has_at_least_two_paragraphs", test_section_has_at_least_two_paragraphs),
    ("section_references_inspirations", test_section_references_inspirations),
    ("nav_links_to_inspiration_section", test_nav_links_to_inspiration_section),
    ("section_has_non_empty_paragraphs", test_section_has_non_empty_paragraphs),
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
        print("\nLanding page Inspiration & Vision validation FAILED.")
        return 1

    print("\nLanding page Inspiration & Vision validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
