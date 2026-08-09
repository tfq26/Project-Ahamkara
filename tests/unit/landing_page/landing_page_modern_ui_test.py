#!/usr/bin/env python3
"""Validate the modern landing page redesign of the Ahamkara site.

Acceptance criteria for the "Redesign Landing Page with Modern UI" issue:
  * Consistent color scheme (CSS custom-property tokens used throughout).
  * Readable fonts (readable font stack, comfortable line-height, fluid
    font sizes).
  * Clear section separation (visually distinct section cards with spacing).
  * Responsive design for desktop and mobile (viewport meta + media queries).
  * No inline styles; semantic HTML5 elements.

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


def _read_index() -> str | None:
    return INDEX.read_text(encoding="utf-8") if INDEX.is_file() else None


def _read_styles() -> str | None:
    return STYLES.read_text(encoding="utf-8") if STYLES.is_file() else None


def _strip_comments(text: str) -> str:
    """Remove CSS/HTML comments so docs never look like real markup."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"<!--.*?-->", "", text, flags=re.DOTALL)


def _custom_property_names(css: str) -> list[str]:
    """Return the names of CSS custom properties declared in :root."""
    root_match = re.search(r":root\s*\{([^}]*)\}", _strip_comments(css), re.DOTALL)
    if not root_match:
        return []
    return re.findall(r"(--[\w-]+)\s*:", root_match.group(1))


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


def test_no_inline_styles() -> list[str]:
    """Acceptance criterion: avoid inline styles."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    for match in re.finditer(r"\bstyle=([\"'])", html, re.IGNORECASE):
        snippet = html[max(0, match.start() - 40) : match.end() + 40]
        errors.append(f"Inline style attribute found: ...{snippet!r}...")
    if "<style" in html:
        errors.append("index.html contains an inline <style> block; keep styles in public/styles.css")
    return errors


def test_semantic_html5() -> list[str]:
    """Acceptance criterion: use semantic HTML5 elements."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    required = {
        "header": "site header",
        "nav": "navigation landmark",
        "main": "main content",
        "section": "content sections",
        "footer": "site footer",
    }
    for tag, label in required.items():
        if not re.search(rf"<{tag}\b", html, re.IGNORECASE):
            errors.append(f"Missing semantic element <{tag}> ({label})")
    # Navigation should be a list of links (keyboard and screen-reader friendly).
    if not re.search(r"<nav\b[^>]*>.*?<ul\b", html, re.IGNORECASE | re.DOTALL):
        errors.append("<nav> should contain a <ul> list of links")
    return errors


def test_heading_hierarchy() -> list[str]:
    """The page must have exactly one <h1> and at least two <h2> section
    headings so the document outline is scannable."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    h1_count = len(re.findall(r"<h1\b", html, re.IGNORECASE))
    h2_count = len(re.findall(r"<h2\b", html, re.IGNORECASE))
    if h1_count != 1:
        errors.append(f"Expected exactly one <h1>, found {h1_count}")
    if h2_count < 2:
        errors.append(f"Expected at least two <h2> section headings, found {h2_count}")
    return errors


def test_consistent_color_scheme() -> list[str]:
    """Acceptance criterion: consistent color scheme.

    Colors must be declared as CSS custom-property tokens in :root and every
    color value in the stylesheet must reference those tokens (via var()).
    Raw hex colors outside the :root token block are a sign of an
    inconsistent, ad-hoc palette.
    """
    css = _read_styles()
    if css is None:
        return []
    errors = []
    body = _strip_comments(css)
    root_block = re.search(r":root\s*\{([^}]*)\}", body, re.DOTALL)
    tokens = _custom_property_names(css)
    color_tokens = [t for t in tokens if t.startswith("--color-")]

    if not root_block:
        errors.append("styles.css does not define a :root token block")
        return errors
    if not color_tokens:
        errors.append("styles.css defines no --color-* custom property tokens")
        return errors

    # Require the core palette tokens.
    required_tokens = [
        "--color-bg",
        "--color-text",
        "--color-text-muted",
        "--color-link",
        "--color-accent",
        "--color-border",
    ]
    for token in required_tokens:
        if token not in tokens:
            errors.append(f"Missing color token {token} in :root")

    # Every color outside :root must come from a token.
    outside_root = body[root_block.end() :]
    hex_colors = re.findall(r"#[0-9a-fA-F]{3,8}\b", outside_root)
    for color in hex_colors:
        errors.append(f"Hard-coded color {color} used outside the :root token block")

    # The stylesheet should actually consume the tokens.
    var_usage = len(re.findall(r"var\(--color-", outside_root))
    if var_usage == 0:
        errors.append("styles.css defines color tokens but never uses var(--color-...)")

    return errors


def test_readable_typography() -> list[str]:
    """Acceptance criterion: readable fonts.

    The page must define a readable font stack, comfortable line-height, and
    fluid/relative font sizes (rem or clamp, not fixed px that break on
    small screens).
    """
    css = _read_styles()
    if css is None:
        return []
    errors = []
    body = _strip_comments(css)

    if not re.search(r"font-family\s*:", body):
        errors.append("styles.css does not define a font-family")
    if not re.search(r"line-height\s*:\s*1\.[5-9]", body):
        errors.append("styles.css does not set a readable line-height (>= 1.5)")
    if not re.search(r"font-size\s*:\s*clamp\(", body):
        errors.append("styles.css does not use clamp() for fluid heading sizes")
    for match in re.finditer(r"font-size\s*:\s*(\d+)px", body):
        errors.append(f"Fixed px font-size ({match.group(1)}px) hurts readability on small screens")
    return errors


def test_section_separation() -> list[str]:
    """Acceptance criterion: clear section separation.

    Each content section must be a visually distinct block (elevated card
    with border, radius, padding) and sections must be spaced apart in a
    grid.
    """
    css = _read_styles()
    if css is None:
        return []
    errors = []
    body = _strip_comments(css)

    for prop in ("background", "border", "border-radius", "padding"):
        if not re.search(r"\.section__card\b[^{}]*\{[^}]*" + prop + r"\s*:", body, re.DOTALL):
            errors.append(f".section__card does not set '{prop}' (no visual separation)")
    if not re.search(r"\.sections\b[^{}]*\{[^}]*display\s*:\s*grid", body, re.DOTALL):
        errors.append(".sections is not a grid layout")
    if not re.search(r"\.sections\b[^{}]*\{[^}]*gap\s*:", body, re.DOTALL):
        errors.append(".sections grid has no gap between cards")
    return errors


def test_responsive_design() -> list[str]:
    """Acceptance criterion: responsive design for desktop and mobile.

    The page must include a viewport meta tag and the stylesheet must switch
    between desktop and mobile layouts with media queries (a multi-column
    grid on wide screens collapsing to a single column on narrow screens).
    """
    html = _read_index()
    css = _read_styles()
    errors = []
    if html is None or css is None:
        return errors

    if not re.search(r'<meta\b[^>]*name=["\']viewport["\']', html, re.IGNORECASE):
        errors.append("index.html is missing the responsive viewport meta tag")

    body = _strip_comments(css)
    media_queries = re.findall(r"@media\s*[^{]*\{", body)
    if not media_queries:
        errors.append("styles.css contains no media queries")
    else:
        if not any("@media (max-width" in q or "@media (max-width:" in q for q in media_queries):
            errors.append("styles.css has no mobile breakpoint (@media (max-width: ...))")
        if not any("@media (min-width" in q or "@media (min-width:" in q for q in media_queries):
            errors.append("styles.css has no desktop breakpoint (@media (min-width: ...))")

    # Mobile layout must collapse the section grid to one column.
    if not re.search(r"@media\s*\(max-width[^{]*\{[^@]*grid-template-columns\s*:\s*1fr", body, re.DOTALL):
        errors.append("Mobile media query does not collapse .sections to a single column")

    return errors


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

CHECKS = [
    ("files_exist", test_files_exist),
    ("no_inline_styles", test_no_inline_styles),
    ("semantic_html5", test_semantic_html5),
    ("heading_hierarchy", test_heading_hierarchy),
    ("consistent_color_scheme", test_consistent_color_scheme),
    ("readable_typography", test_readable_typography),
    ("section_separation", test_section_separation),
    ("responsive_design", test_responsive_design),
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
        print("\nLanding page modern UI validation FAILED.")
        return 1

    print("\nLanding page modern UI validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
