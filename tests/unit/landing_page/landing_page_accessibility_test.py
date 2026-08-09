#!/usr/bin/env python3
"""Validate accessibility features of the Ahamkara landing page.

Acceptance criteria for the "Add Accessibility Features to Landing Page"
issue:
  * All images have alt text.
  * Color contrast meets WCAG 2.1 AA standards.
  * Navigation is keyboard accessible.

The landing page lives in public/index.html and public/styles.css (the
source is also mirrored into the deployed site via the Pages workflow).

Each check returns a list of error strings; an empty list means the check
passed. This mirrors the existing tests/src/*.py web-validation scripts.
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

# Required foreground/background pairs expressed as CSS custom properties.
# Threshold: 4.5:1 for normal text, 3:1 for large text / non-text UI.
CONTRAST_PAIRS = [
    ("--color-text", "--color-bg", 4.5, "body text"),
    ("--color-text-muted", "--color-bg", 4.5, "secondary text"),
    ("--color-link", "--color-bg", 4.5, "link text"),
    ("--color-accent", "--color-bg", 3.0, "large heading gradient / visited link"),
    ("--color-focus", "--color-bg", 3.0, "keyboard focus indicator"),
]

# WCAG AA contrast threshold for normal-size text.
NORMAL_TEXT_RATIO = 4.5


def _read_index() -> str | None:
    return INDEX.read_text(encoding="utf-8") if INDEX.is_file() else None


def _read_styles() -> str | None:
    return STYLES.read_text(encoding="utf-8") if STYLES.is_file() else None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _srgb_to_linear(c: float) -> float:
    return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4


def _relative_luminance(hex_color: str) -> float:
    """Compute WCAG relative luminance for a #rrggbb color."""
    match = re.fullmatch(r"#([0-9a-fA-F]{6})", hex_color.strip())
    if not match:
        raise ValueError(f"Not a #rrggbb color: {hex_color!r}")
    h = match.group(1)
    r, g, b = (int(h[i : i + 2], 16) for i in (0, 2, 4))
    return (
        0.2126 * _srgb_to_linear(r / 255.0) + 0.7152 * _srgb_to_linear(g / 255.0) + 0.0722 * _srgb_to_linear(b / 255.0)
    )


def _contrast_ratio(fg: str, bg: str) -> float:
    l1, l2 = _relative_luminance(fg), _relative_luminance(bg)
    if l1 < l2:
        l1, l2 = l2, l1
    return (l1 + 0.05) / (l2 + 0.05)


def _css_custom_property(css: str, name: str) -> str | None:
    """Extract a custom property value, e.g. --color-bg -> #0d1117."""
    pattern = re.compile(rf"{re.escape(name)}\s*:\s*([^;}}]+)", re.IGNORECASE)
    match = pattern.search(css)
    return match.group(1).strip() if match else None


def _parse_stylesheet_link(html: str) -> list[str]:
    """Return hrefs of <link rel="stylesheet"> tags."""
    links = []
    for tag in re.findall(r"<link\b[^>]*>", html, re.IGNORECASE):
        rel = re.search(r'\brel=(["\'])(.*?)\1', tag, re.IGNORECASE | re.DOTALL)
        href = re.search(r'\bhref=(["\'])(.*?)\1', tag, re.IGNORECASE | re.DOTALL)
        if rel and href and "stylesheet" in rel.group(2).lower():
            links.append(href.group(2).strip())
    return links


# ---------------------------------------------------------------------------
# Acceptance-criterion checks
# ---------------------------------------------------------------------------


def test_index_and_stylesheet_exist() -> list[str]:
    errors = []
    if not INDEX.is_file():
        errors.append(f"Missing {INDEX.relative_to(ROOT)}")
    if not STYLES.is_file():
        errors.append(f"Missing {STYLES.relative_to(ROOT)}")
    return errors


def test_images_have_alt_text() -> list[str]:
    """Acceptance criterion: all images have alt text."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    for img in re.findall(r"<img\b[^>]*>", html, re.IGNORECASE):
        alt = re.search(r"\balt=([\"'])(.*?)\1", img, re.IGNORECASE | re.DOTALL)
        if alt is None:
            errors.append(f"Image missing alt attribute: {img[:80]}")
        elif alt.group(2).strip() == "" and 'role="presentation"' not in img:
            # Decorative images may use alt="" only when explicitly marked
            # with role="presentation"; otherwise the alt must be non-empty.
            errors.append(f"Image has empty alt text: {img[:80]}")
    return errors


def test_navigation_keyboard_accessible() -> list[str]:
    """Acceptance criterion: navigation is keyboard accessible.

    Navigation links must be native <a href> elements (focusable by
    default), wrapped in a <nav> landmark with an accessible label.
    """
    html = _read_index()
    if html is None:
        return []
    errors = []

    nav_tags = re.findall(r"<nav\b[^>]*>", html, re.IGNORECASE)
    if not nav_tags:
        errors.append("Missing <nav> landmark for navigation")
    else:
        for tag in nav_tags:
            aria = re.search(r"\b(?:aria-label|aria-labelledby)=([\"'])(.*?)\1", tag, re.IGNORECASE | re.DOTALL)
            if not aria or not aria.group(2).strip():
                errors.append(f"<nav> landmark has no accessible name: {tag[:80]}")

    links = re.findall(r"<a\b[^>]*>(.*?)</a>", html, re.IGNORECASE | re.DOTALL)
    for tag in re.findall(r"<a\b[^>]*>", html, re.IGNORECASE):
        href = re.search(r"\bhref=([\"'])(.*?)\1", tag, re.IGNORECASE | re.DOTALL)
        tabindex = re.search(r"\btabindex=([\"']?)(-1)\1", tag, re.IGNORECASE)
        if href is None:
            errors.append(f"Link is not keyboard-focusable (no href): {tag[:80]}")
        if tabindex:
            errors.append(f"Link is excluded from tab order (tabindex=-1): {tag[:80]}")

    if not links:
        errors.append("Navigation contains no links")
    return errors


def test_skip_link_present() -> list[str]:
    """A skip link must be the first focusable element and target the main
    content, so keyboard users can bypass repeated navigation."""
    html = _read_index()
    if html is None:
        return []
    errors = []

    skip = re.search(
        r'<a\b[^>]*class=["\'][^"\']*\bskip-link\b[^"\']*["\'][^>]*href=["\']([^"\']+)["\']',
        html,
        re.IGNORECASE,
    )
    if skip is None:
        errors.append('Missing skip link with class="skip-link" and href="#main-content"')
        return errors

    target = skip.group(1)
    target_id = target.lstrip("#")
    if not re.search(rf'<main\b[^>]*id=["\']{re.escape(target_id)}["\']', html, re.IGNORECASE):
        errors.append(f"Skip link href={target!r} does not match a main#id target")

    if html.find('class="skip-link"') > html.find("<main"):
        errors.append("Skip link is not positioned before the main content")

    return errors


def test_stylesheet_linked() -> list[str]:
    """The landing page styles must live in public/styles.css (linked from
    index.html), not hidden in a <style> block."""
    html = _read_index()
    if html is None:
        return []
    errors = []
    hrefs = _parse_stylesheet_link(html)
    if not hrefs:
        errors.append("index.html does not link an external stylesheet")
    elif not any("styles.css" in href for href in hrefs):
        errors.append(f"index.html does not link styles.css (found: {hrefs})")
    if "<style>" in html:
        errors.append("index.html contains an inline <style> block; use public/styles.css")
    return errors


def test_focus_visible_styles() -> list[str]:
    """Keyboard users must get a visible focus indicator (WCAG 2.4.7)."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if ":focus-visible" not in css:
        errors.append("styles.css does not define :focus-visible styles")
    if re.search(r"\boutline\s*:\s*3px", css) is None:
        errors.append(":focus-visible must define a visible outline (3px)")
    return errors


def test_skip_link_hidden_until_focus() -> list[str]:
    """The skip link must be visually hidden until it receives focus."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if not re.search(r"\.skip-link(?::not\(:focus[^)]*\))?\s*\{", css):
        errors.append("styles.css does not define .skip-link hidden-until-focus rules")
    if ".skip-link:focus" not in css and ".skip-link:focus-visible" not in css:
        errors.append("styles.css does not reveal .skip-link on focus")
    return errors


def test_prefers_reduced_motion() -> list[str]:
    """The page must honor the reduced-motion user preference (WCAG 2.3.3)."""
    css = _read_styles()
    if css is None:
        return []
    errors = []
    if "prefers-reduced-motion" not in css:
        errors.append("styles.css does not include @media (prefers-reduced-motion: reduce)")
    return errors


def test_wcag_aa_contrast() -> list[str]:
    """Acceptance criterion: color contrast meets WCAG AA standards.

    Contrast is computed from the CSS custom properties in styles.css and
    compared against WCAG 2.1 thresholds (4.5:1 normal text, 3:1 large
    text / non-text UI). Fails loudly if a token is missing or unparsable.
    """
    css = _read_styles()
    if css is None:
        return []
    errors = []

    resolved: dict[str, str] = {}
    for fg_name, bg_name, _threshold, _label in CONTRAST_PAIRS:
        for var, color in ((fg_name, resolved), (bg_name, resolved)):
            if var not in color:
                value = _css_custom_property(css, var)
                if value is None:
                    errors.append(f"Missing CSS custom property {var} in styles.css")
                    continue
                try:
                    _relative_luminance(value)
                except ValueError:
                    errors.append(f"CSS custom property {var} is not a #rrggbb color: {value}")
                    continue
                color[var] = value

    for fg_name, bg_name, threshold, label in CONTRAST_PAIRS:
        if fg_name not in resolved or bg_name not in resolved:
            continue  # already reported above
        try:
            ratio = _contrast_ratio(resolved[fg_name], resolved[bg_name])
        except ValueError as exc:
            errors.append(f"Could not compute contrast for {label}: {exc}")
            continue
        if ratio < threshold:
            errors.append(
                f"Contrast {label} {resolved[fg_name]}/{resolved[bg_name]} "
                f"= {ratio:.2f}:1 is below WCAG AA ({threshold}:1)"
            )
    return errors


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

CHECKS = [
    ("files_exist", test_index_and_stylesheet_exist),
    ("images_have_alt_text", test_images_have_alt_text),
    ("navigation_keyboard_accessible", test_navigation_keyboard_accessible),
    ("skip_link_present", test_skip_link_present),
    ("stylesheet_linked", test_stylesheet_linked),
    ("focus_visible_styles", test_focus_visible_styles),
    ("skip_link_hidden_until_focus", test_skip_link_hidden_until_focus),
    ("prefers_reduced_motion", test_prefers_reduced_motion),
    ("wcag_aa_contrast", test_wcag_aa_contrast),
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
        print("\nLanding page accessibility validation FAILED.")
        return 1

    print("\nLanding page accessibility validation passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
