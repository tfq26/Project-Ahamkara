#!/usr/bin/env python3
"""Validate the navigation bar and section anchors on the Ahamkara landing page.

Acceptance criteria for the "Add Navigation and Section Anchors" issue:
  * A navigation bar is visible at the top of the landing page on all screen
    sizes.
  * Clicking a nav link scrolls smoothly to the relevant section.

The landing page lives in public/index.html and public/styles.css.

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
STYLES = ROOT / "public" / "styles.css"

# Major sections the landing page is expected to expose and that the nav bar
# must link to. These mirror the section ids in public/index.html.
MAJOR_SECTIONS = ["philosophy", "wish", "inspiration"]


def _read_index() -> str | None:
    return INDEX.read_text(encoding="utf-8") if INDEX.is_file() else None


def _read_styles() -> str | None:
    return STYLES.read_text(encoding="utf-8") if STYLES.is_file() else None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _strip_comments(html: str) -> str:
    """Remove <!-- ... --> comments so docs inside them never look like real
    markup (e.g. a comment that mentions <a href="#anchor">)."""
    return re.sub(r"<!--.*?-->", "", html, flags=re.DOTALL)


def _all_element_ids(html: str) -> list[str]:
    """Return the value of every id="..." attribute in the document."""
    return [
        match.group(2)
        for match in re.finditer(r'\bid=(["\'])(.*?)\1', _strip_comments(html), re.IGNORECASE | re.DOTALL)
    ]


def _all_internal_hrefs(html: str) -> list[str]:
    """Return the fragment of every href="#..." attribute in the document."""
    return [
        match.group(2)
        for match in re.finditer(r'\bhref=(["\'])\.?#(.*?)\1', _strip_comments(html), re.IGNORECASE | re.DOTALL)
    ]


def _navbar_block(html: str) -> str | None:
    """Return the inner text of the first <nav> element, or None."""
    match = re.search(r"<nav\b[^>]*>(.*?)</nav>", _strip_comments(html), re.IGNORECASE | re.DOTALL)
    return match.group(1) if match else None


def _navbar_links(html: str) -> list[tuple[str, str]]:
    """Return (label, href) pairs for anchor links inside the first <nav>."""
    block = _navbar_block(html)
    if block is None:
        return []
    links = []
    for full in re.findall(r"<a\b[^>]*>.*?</a>", block, re.IGNORECASE | re.DOTALL):
        href = re.search(r'\bhref=(["\'])(.*?)\1', full, re.IGNORECASE | re.DOTALL)
        label = re.search(r">([^<]*)<", full, re.DOTALL)
        if href is not None:
            links.append((label.group(1).strip() if label else "", href.group(2).strip()))
    return links


# ---------------------------------------------------------------------------
# Acceptance-criterion checks
# ---------------------------------------------------------------------------


def test_files_exist() -> list[str]:
    errors = []
    if not INDEX.is_file():
        errors.append(f"Missing {INDEX.relative_to(ROOT)}")
    if not STYLES.is_file():
        errors.append(f"Missing {STYLES.relative_to(ROOT)}")
    return errors


def test_navbar_present() -> list[str]:
    """A navigation bar (<nav>) must exist on the landing page."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    if re.search(r"<nav\b[^>]*>", html, re.IGNORECASE) is None:
        errors.append("Missing <nav> navigation bar")
    return errors


def test_navbar_positioned_at_top() -> list[str]:
    """The navigation bar must appear at the top of the page, before the main
    content, so it is the first thing visitors see."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    nav_at = html.find("<nav")
    main_at = html.find("<main")
    if nav_at == -1:
        errors.append("Missing <nav> navigation bar")
    elif main_at == -1:
        errors.append("Missing <main> content element")
    elif nav_at > main_at:
        errors.append("Navigation bar is positioned after the main content; it must be at the top")
    return errors


def test_nav_links_to_major_sections() -> list[str]:
    """The navigation bar must link to every major section."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    links = _navbar_links(html)
    if not links:
        errors.append("Navigation bar contains no links")
        return errors

    hrefs = [href for _label, href in links]
    for section in MAJOR_SECTIONS:
        if f"#{section}" not in hrefs:
            errors.append(f"Navigation bar has no link to section #{section}")

    internal = [href for href in hrefs if href.startswith("#")]
    if not internal:
        errors.append("Navigation bar has no internal anchor links; nothing to scroll to")
    return errors


def test_nav_anchor_targets_exist() -> list[str]:
    """Every nav link that points to a section anchor must resolve to an
    element id present in the document."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    ids = set(_all_element_ids(html))
    links = _navbar_links(html)
    for label, href in links:
        if not href.startswith("#"):
            continue
        target = href[1:]
        if target not in ids:
            errors.append(f"Nav link {label!r} points to #{target}, but no element has that id")
    return errors


def test_all_internal_anchors_resolve() -> list[str]:
    """No internal anchor may be broken: every href="#x" must match an
    id="x" on the page (edge case: dead links)."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    ids = set(_all_element_ids(html))
    for target in _all_internal_hrefs(html):
        if target not in ids:
            errors.append(f"Anchor href=#{target} has no matching element id")
    return errors


def test_no_duplicate_ids() -> list[str]:
    """Anchor targets must be unique (edge case: duplicate ids make a nav
    link jump to the wrong section)."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    ids = _all_element_ids(html)
    seen: set[str] = set()
    for element_id in ids:
        if element_id in seen:
            errors.append(f"Duplicate element id={element_id!r}")
        seen.add(element_id)
    return errors


def test_smooth_scroll_enabled() -> list[str]:
    """Clicking a nav link must scroll smoothly to the section (CSS
    scroll-behavior: smooth on the html element)."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if not re.search(r"scroll-behavior\s*:\s*smooth", css):
        errors.append("styles.css does not enable scroll-behavior: smooth")
    return errors


def test_scroll_padding_for_navbar() -> list[str]:
    """Sections must not hide behind the sticky navigation bar when scrolled
    to, so scroll-padding-top must be set on the scrolling container."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if not re.search(r"scroll-padding-top\s*:", css):
        errors.append("styles.css does not set scroll-padding-top for the sticky navbar")
    return errors


def test_navbar_sticks_to_top() -> list[str]:
    """The navigation bar stays visible while scrolling: it must be sticky or
    fixed at the top of the viewport."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if not re.search(r"\.navbar\b[^{}]*\{[^}]*position\s*:\s*(sticky|fixed)", css, re.DOTALL):
        errors.append("styles.css does not pin .navbar to the top (position: sticky/fixed)")
    if not re.search(r"\.navbar\b[^{}]*\{[^}]*top\s*:\s*0", css, re.DOTALL):
        errors.append("styles.css does not set top: 0 on .navbar")
    return errors


def test_navbar_visible_on_all_screen_sizes() -> list[str]:
    """Acceptance criterion: the navigation bar is visible on all screen sizes.

    The navbar must never be display:none, and the stylesheet must include a
    responsive media query (so small-screen behaviour is explicitly handled).
    """
    css = _read_styles()
    if css is None:
        return []
    errors = []
    for selector in (r"\.navbar\b", r"\.navbar__links\b", r"\.navbar__links\s+a"):
        for block in re.findall(r"@media\s+[^{]*\{[^@]*?\}", css, re.DOTALL):
            if re.search(selector + r"[^{}]*\{[^}]*display\s*:\s*none", block, re.DOTALL):
                errors.append(f"{selector} is hidden (display: none) inside a media query")
                break
        if re.search(selector + r"[^{}]*\{[^}]*display\s*:\s*none", css, re.DOTALL):
            errors.append(f"{selector} is hidden (display: none)")

    if "@media" not in css:
        errors.append("styles.css has no responsive media query for small screens")
    return errors


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

CHECKS = [
    ("files_exist", test_files_exist),
    ("navbar_present", test_navbar_present),
    ("navbar_positioned_at_top", test_navbar_positioned_at_top),
    ("nav_links_to_major_sections", test_nav_links_to_major_sections),
    ("nav_anchor_targets_exist", test_nav_anchor_targets_exist),
    ("all_internal_anchors_resolve", test_all_internal_anchors_resolve),
    ("no_duplicate_ids", test_no_duplicate_ids),
    ("smooth_scroll_enabled", test_smooth_scroll_enabled),
    ("scroll_padding_for_navbar", test_scroll_padding_for_navbar),
    ("navbar_sticks_to_top", test_navbar_sticks_to_top),
    ("navbar_visible_on_all_screen_sizes", test_navbar_visible_on_all_screen_sizes),
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
        print("\nLanding page navigation validation FAILED.")
        return 1

    print("\nLanding page navigation validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
