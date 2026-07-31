#!/usr/bin/env python3
"""Validate the deployed marketing website — structure, navigation, content, and responsiveness."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEBSITE = ROOT / "website"


def read_html() -> str:
    return (WEBSITE / "index.html").read_text(encoding="utf-8")


def read_css() -> str:
    return (WEBSITE / "style.css").read_text(encoding="utf-8")


# ── File structure ──

def test_required_files_exist() -> None:
    assert WEBSITE.is_dir(), "website/ directory missing"
    assert (WEBSITE / "index.html").is_file(), "website/index.html missing"
    assert (WEBSITE / "style.css").is_file(), "website/style.css missing"


# ── HTML structure ──

def test_doctype_and_lang() -> None:
    html = read_html()
    assert html.startswith("<!DOCTYPE html>"), "Missing DOCTYPE"
    assert 'lang="en"' in html, "Missing lang attribute"


def test_meta_charset() -> None:
    html = read_html()
    assert 'charset="UTF-8"' in html or 'charset="utf-8"' in html, "Missing charset meta"


def test_viewport_meta() -> None:
    html = read_html()
    assert 'name="viewport"' in html, "Missing viewport meta for responsive design"
    assert "width=device-width" in html, "Viewport missing width=device-width"
    assert "initial-scale=1" in html, "Viewport missing initial-scale=1"


def test_page_title() -> None:
    html = read_html()
    m = re.search(r"<title>(.*?)</title>", html)
    assert m, "Missing <title>"
    assert "Ahamkara" in m.group(1), "Title should mention Ahamkara"


def test_description_meta() -> None:
    html = read_html()
    assert 'name="description"' in html, "Missing meta description for SEO"


def test_all_sections_present() -> None:
    html = read_html()
    sections = {
        "hero": "Hero banner",
        "engine": "What is Ahamkara / Engine",
        "wish": "What is Wish",
        "inspiration": "Inspiration",
    }
    for sid, label in sections.items():
        assert f'id="{sid}"' in html, f"Section '{label}' (id={sid}) not found"


# ── Navigation ──

def test_navbar_exists() -> None:
    html = read_html()
    assert '<nav' in html or 'class="navbar"' in html, "Navigation bar missing"


def test_nav_links() -> None:
    html = read_html()
    expected_links = ["#engine", "#wish", "#inspiration"]
    for link in expected_links:
        assert f'href="{link}"' in html, f"Nav link to {link} missing"


def test_external_links() -> None:
    html = read_html()
    # GitHub link
    assert "github.com/tfq26/Project-Ahamkara" in html, "GitHub link missing"
    assert 'href="https://github.com/tfq26/Project-Ahamkara"' in html, "GitHub URL incorrect"


# ── Call-to-action buttons ──

def test_cta_buttons() -> None:
    html = read_html()
    assert 'btn-primary' in html or 'class="btn' in html, "No CTA buttons found"
    assert "#engine" in html, "CTA should link to engine section"


# ── Content correctness ──

def test_engine_description() -> None:
    html = read_html()
    keywords = ["C++20", "game engine", "multiplayer", "networking"]
    for kw in keywords:
        assert kw.lower() in html.lower(), f"Engine description missing keyword: {kw}"


def test_wish_description() -> None:
    html = read_html()
    keywords = ["backend", "session", "heartbeat", "HTTP admin"]
    for kw in keywords:
        assert kw.lower() in html.lower(), f"Wish description missing keyword: {kw}"


def test_game_name_consistency() -> None:
    html = read_html()
    assert "Ahamkara" in html, "Game name missing from page"
    assert "AHAMKARA" in html, "Game name (uppercase) missing"


# ── Responsive CSS ──

def test_media_queries() -> None:
    css = read_css()
    assert "@media" in css, "CSS must include media queries for responsiveness"
    assert "max-width: 768px" in css, "CSS missing tablet breakpoint (768px)"
    assert "max-width: 480px" in css, "CSS missing mobile breakpoint (480px)"


def test_responsive_nav() -> None:
    css = read_css()
    # At 768px breakpoint nav should adapt
    idx = css.index("@media (max-width: 768px)")
    block = css[idx:idx + 500]
    assert ".nav-links" in block or ".navbar" in block or ".nav-inner" in block, \
        "768px breakpoint should adjust navigation"


def test_responsive_grid() -> None:
    css = read_css()
    idx = css.index("@media (max-width: 768px)")
    block = css[idx:idx + 500]
    assert "grid-template-columns" in block, \
        "768px breakpoint should adjust grid layout"


# ── Image & asset references ──

def test_hero_background_image() -> None:
    css = read_css()
    assert "Javelin-4.jpg" in css or "assets/" in css, "Hero background image missing in CSS"


def test_page_images_exist() -> None:
    """All referenced images in the HTML must exist in the assets directory."""
    html = read_html()
    for match in re.finditer(r'src="(assets/[^"]+)"', html):
        ref = match.group(1)
        target = (ROOT / ref).resolve()
        assert target.is_file(), f"Referenced image not found: {ref}"


# ── Accessibility ──

def test_images_have_alt() -> None:
    html = read_html()
    for match in re.finditer(r'<img[^>]+src="[^"]+"', html):
        img_tag = match.group(0)
        if 'alt=' not in img_tag:
            print(f"  WARNING: Image without alt text: {img_tag[:80]}", file=sys.stderr)


def test_semantic_html() -> None:
    html = read_html()
    elements = ["nav", "section", "footer", "h1", "h2", "h3", "h4", "ul", "li"]
    for el in elements:
        if f"<{el}" not in html and f"<{el} " not in html:
            print(f"  NOTE: Semantic element <{el}> not found", file=sys.stderr)


# ── Performance ──

def test_stylesheet_external() -> None:
    html = read_html()
    assert '<link rel="stylesheet" href="style.css"' in html or \
           '<link rel="stylesheet" href="./style.css"' in html, \
        "CSS should be linked externally (not inline all)"


def test_resources_use_loading_lazy() -> None:
    html = read_html()
    for match in re.finditer(r'<img[^>]+src="[^"]+"', html):
        if "loading=" not in match.group(0):
            print(f"  NOTE: Image without loading attribute: {match.group(0)[:60]}", file=sys.stderr)


def main() -> int:
    tests = [
        ("Required files", test_required_files_exist),
        ("DOCTYPE and lang", test_doctype_and_lang),
        ("Meta charset", test_meta_charset),
        ("Viewport meta", test_viewport_meta),
        ("Page title", test_page_title),
        ("Description meta", test_description_meta),
        ("All sections present", test_all_sections_present),
        ("Navigation bar", test_navbar_exists),
        ("Nav links", test_nav_links),
        ("External links", test_external_links),
        ("CTA buttons", test_cta_buttons),
        ("Engine description", test_engine_description),
        ("Wish description", test_wish_description),
        ("Game name consistency", test_game_name_consistency),
        ("Media queries", test_media_queries),
        ("Responsive nav", test_responsive_nav),
        ("Responsive grid", test_responsive_grid),
        ("Hero background image", test_hero_background_image),
        ("Page images exist", test_page_images_exist),
        ("Images have alt text", test_images_have_alt),
        ("Semantic HTML", test_semantic_html),
        ("External stylesheet", test_stylesheet_external),
        ("Lazy loading", test_resources_use_loading_lazy),
    ]

    failures = 0
    for name, test_fn in tests:
        try:
            test_fn()
            print(f"  ✓ {name}")
        except AssertionError as e:
            print(f"  ✗ {name}: {e}", file=sys.stderr)
            failures += 1
        except Exception as e:
            print(f"  ✗ {name}: ERROR — {e}", file=sys.stderr)
            failures += 1

    if failures:
        print(f"\n❌ {failures} test(s) FAILED", file=sys.stderr)
        return 1
    print("\n✓ All site validation tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
