#!/usr/bin/env python3
"""Validate the "What is Wish?" section on the Ahamkara landing page.

Acceptance criteria for the "Write Content: Wish Framework Description" issue:
  * A section titled "What is Wish?" exists.
  * The section contains at least 2 paragraphs.
  * The section explains the relationship between Wish and Ahamkara.

The landing page template lives in frontend/src/App.vue (a Vue single-file
component; the rendered markup is inside the <template> block).

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

# Vue single-file component that defines the landing page template.
APP_VUE = ROOT / "frontend" / "src" / "App.vue"

# The landing page entry HTML that mounts the Vue component.
INDEX_HTML = ROOT / "frontend" / "index.html"

# Expected section heading as a visitor sees it.
EXPECTED_TITLE = "What is Wish?"

# Minimum number of paragraphs the section must contain.
MIN_PARAGRAPHS = 2

# Keywords that demonstrate the section explains the relationship between
# Wish and Ahamkara. The section must mention both products.
RELATIONSHIP_KEYWORDS = ["Ahamkara", "Wish"]

# Additional relationship language that should appear when describing how the
# two connect (at least one must be present).
CONNECTION_KEYWORDS = [
    "connect",
    "connection",
    "live-service",
    "live service",
    "backend",
    "platform",
    "protocol",
]


def _read_source() -> str | None:
    return APP_VUE.read_text(encoding="utf-8") if APP_VUE.is_file() else None


def _strip_comments(text: str) -> str:
    """Remove <!-- ... --> comments so docs inside them never look like real
    markup (e.g. a comment that mentions "What is Wish?")."""
    return re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)


def _template_block(text: str) -> str:
    """Return the contents of the <template>...</template> block."""
    match = re.search(r"<template[^>]*>(.*?)</template>", text, re.IGNORECASE | re.DOTALL)
    return match.group(1) if match else text


def _section_block(template: str) -> str | None:
    """Return the inner content of the Wish section element."""
    # The section contains the heading; capture everything from the opening
    # <section ...> tag through its matching </section>.
    pattern = re.compile(
        r"<section\b[^>]*>(?P<body>.*?)</section>",
        re.IGNORECASE | re.DOTALL,
    )
    for match in pattern.finditer(template):
        body = match.group("body")
        if EXPECTED_TITLE in body:
            return body
    return None


def _paragraph_texts(block: str) -> list[str]:
    """Return the inner text of every <p>...</p> in the block."""
    texts = []
    for para in re.findall(r"<p\b[^>]*>(.*?)</p>", block, re.IGNORECASE | re.DOTALL):
        # Collapse whitespace so paragraph length checks are meaningful.
        text = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", para)).strip()
        texts.append(text)
    return texts


def test_index_exists() -> list[str]:
    """The landing page entry HTML must exist."""
    if not INDEX_HTML.is_file():
        return [f"Landing page entry HTML not found at {INDEX_HTML}"]
    return []


def test_template_exists() -> list[str]:
    """The Vue landing page template must exist."""
    if not APP_VUE.is_file():
        return [f"Landing page template not found at {APP_VUE}"]
    return []


def test_section_title() -> list[str]:
    """The section must be titled "What is Wish?"."""
    source = _read_source()
    if source is None:
        return ["Cannot read landing page template"]
    template = _template_block(_strip_comments(source))
    if EXPECTED_TITLE not in template:
        return [f"Landing page section titled {EXPECTED_TITLE!r} not found"]
    return []


def test_paragraph_count() -> list[str]:
    """The Wish section must contain at least MIN_PARAGRAPHS paragraphs."""
    source = _read_source()
    if source is None:
        return ["Cannot read landing page template"]
    template = _template_block(_strip_comments(source))
    block = _section_block(template)
    if block is None:
        return [f"Wish section ({EXPECTED_TITLE!r}) not found in template"]
    paragraphs = _paragraph_texts(block)
    if len(paragraphs) < MIN_PARAGRAPHS:
        return [
            f"Wish section has {len(paragraphs)} paragraph(s); "
            f"expected at least {MIN_PARAGRAPHS}"
        ]
    return []


def test_relationship_explained() -> list[str]:
    """The section must explain how Wish connects to Ahamkara."""
    source = _read_source()
    if source is None:
        return ["Cannot read landing page template"]
    template = _template_block(_strip_comments(source))
    block = _section_block(template)
    if block is None:
        return [f"Wish section ({EXPECTED_TITLE!r}) not found in template"]
    combined = " ".join(_paragraph_texts(block)).lower()
    missing_products = [
        kw for kw in RELATIONSHIP_KEYWORDS if kw.lower() not in combined
    ]
    if missing_products:
        return [
            f"Wish section does not mention: {', '.join(missing_products)}"
        ]
    if not any(kw in combined for kw in CONNECTION_KEYWORDS):
        return [
            "Wish section does not describe the Ahamkara/Wish connection "
            "(expected a relationship keyword such as 'backend', 'platform', "
            "'protocol', 'live-service', or 'connect')"
        ]
    return []


def test_diagram_present() -> list[str]:
    """Implementation notes encourage a diagram; keep it as a soft check."""
    source = _read_source()
    if source is None:
        return []
    template = _strip_comments(source)
    if "<svg" not in template.lower():
        return ["No diagram (<svg>) found in the landing page template"]
    return []


def main() -> int:
    has_errors = False
    for name, func in [
        ("index_exists", test_index_exists),
        ("template_exists", test_template_exists),
        ("section_title", test_section_title),
        ("paragraph_count", test_paragraph_count),
        ("relationship_explained", test_relationship_explained),
        ("diagram_present", test_diagram_present),
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
