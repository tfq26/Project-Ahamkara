#!/usr/bin/env python3
"""Validate the marketing website files exist and contain expected content."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEBSITE = ROOT / "website"
ASSETS = ROOT / "assets"


def test_files_exist() -> None:
    assert WEBSITE.is_dir(), f"website/ directory missing at {WEBSITE}"
    assert (WEBSITE / "index.html").is_file(), "website/index.html missing"
    assert (WEBSITE / "style.css").is_file(), "website/style.css missing"


def test_html_sections() -> None:
    html = (WEBSITE / "index.html").read_text(encoding="utf-8")
    sections = {
        "Hero": "hero",
        "What is Ahamkara": "engine",
        "What is Wish": "wish",
        "Inspiration": "inspiration",
    }
    for name, section_id in sections.items():
        assert f'id="{section_id}"' in html, f"Section '{name}' (id={section_id}) not found in index.html"


def test_html_images_exist() -> None:
    """Verify all referenced image files exist under assets/."""
    import re

    html = (WEBSITE / "index.html").read_text(encoding="utf-8")
    # Find assets/... paths in src or href attributes
    for match in re.finditer(r'(?:src|href)="(assets/[^"]+)"', html):
        ref = match.group(1)
        # Resolve relative to the project root (same as deployed structure)
        target = (ROOT / ref).resolve()
        assert target.is_file(), f"Referenced asset not found: {ref} (resolved: {target})"


def test_css_has_media_queries() -> None:
    css = (WEBSITE / "style.css").read_text(encoding="utf-8")
    assert "@media" in css, "style.css must include responsive media queries"
    assert "@media (max-width: 768px)" in css, "style.css missing tablet breakpoint"


def test_css_assets_exist() -> None:
    """Verify all referenced asset paths in CSS exist under assets/."""
    import re

    css = (WEBSITE / "style.css").read_text(encoding="utf-8")
    for match in re.finditer(r'url\(["\']?(assets/[^"\')\s]+)["\'\s)]?', css):
        ref = match.group(1)
        target = (ROOT / ref).resolve()
        assert target.is_file(), f"CSS referenced asset not found: {ref} (resolved: {target})"


def test_html_has_meta_viewport() -> None:
    html = (WEBSITE / "index.html").read_text(encoding="utf-8")
    assert 'name="viewport"' in html, "index.html missing viewport meta tag for responsive design"


def main() -> int:
    try:
        test_files_exist()
        test_html_sections()
        test_html_images_exist()
        test_css_has_media_queries()
        test_css_assets_exist()
        test_html_has_meta_viewport()
        print("Website marketing page tests passed")
        return 0
    except AssertionError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
