#!/usr/bin/env python3
"""Validate SEO and social sharing metadata in public/index.html.

Covers the acceptance criteria for the "Add Metadata and Open Graph Tags"
issue:
  * a <title> tag is present
  * a meta description is present
  * Open Graph tags (og:title, og:description, og:image, ...) are present
  * Twitter Card tags (twitter:card, twitter:title, ...) are present
  * a favicon link is present and the referenced file exists
  * og:url / og:image are absolute URLs that resolve to committed files
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INDEX = ROOT / "public" / "index.html"
FAVICON = ROOT / "public" / "favicon.svg"

# Open Graph and Twitter properties that must be present with a non-empty value.
REQUIRED_OG_PROPERTIES = [
    "og:title",
    "og:description",
    "og:type",
    "og:url",
    "og:image",
    "og:image:alt",
    "og:site_name",
]
REQUIRED_TWITTER_PROPERTIES = [
    "twitter:card",
    "twitter:title",
    "twitter:description",
    "twitter:image",
]


def _read_index() -> str | None:
    if not INDEX.is_file():
        return None
    return INDEX.read_text(encoding="utf-8")


def _meta_content(html: str, attr: str, value: str) -> str | None:
    """Return the ``content`` of the first <meta attr="value" ...> element."""
    tag_pattern = re.compile(
        rf'<meta\b[^>]*\b{re.escape(attr)}=(["\']){re.escape(value)}\1[^>]*>',
        re.IGNORECASE,
    )
    match = tag_pattern.search(html)
    if not match:
        return None
    content_match = re.search(r'\bcontent=(["\'])(.*?)\1', match.group(0), re.IGNORECASE | re.DOTALL)
    if not content_match:
        return None
    return content_match.group(2).strip()


def _link_href(html: str, rel: str) -> str | None:
    """Return the ``href`` of the first <link rel="..." ...> element."""
    tag_pattern = re.compile(
        rf'<link\b[^>]*\brel=(["\']){re.escape(rel)}\1[^>]*>',
        re.IGNORECASE,
    )
    match = tag_pattern.search(html)
    if not match:
        return None
    href_match = re.search(r'\bhref=(["\'])(.*?)\1', match.group(0), re.IGNORECASE | re.DOTALL)
    if not href_match:
        return None
    return href_match.group(2).strip()


def _title(html: str) -> str | None:
    match = re.search(r"<title[^>]*>(.*?)</title>", html, re.IGNORECASE | re.DOTALL)
    if not match:
        return None
    return match.group(1).strip()


def _repo_file_for_raw_url(url: str) -> Path | None:
    """Map a Forgejo raw URL back to a local repo path, or None."""
    match = re.match(r"^https?://[^/]+/.+/raw/branch/[^/]+/(.+)$", url)
    if not match:
        return None
    return ROOT / match.group(1)


def test_index_exists() -> list[str]:
    return [] if INDEX.is_file() else [f"Missing {INDEX.relative_to(ROOT)}"]


def test_title_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    title = _title(html)
    if not title:
        return ["Missing or empty <title> tag"]
    return []


def test_meta_description_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    description = _meta_content(html, "name", "description")
    if not description:
        return ['Missing or empty <meta name="description"> tag']
    if len(description) > 200:
        return ["meta description exceeds 200 characters (will be truncated by search engines)"]
    return []


def test_canonical_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    href = _link_href(html, "canonical")
    if not href:
        return ['Missing or empty <link rel="canonical">']
    if not href.startswith(("http://", "https://")):
        return [f"canonical URL must be absolute, got '{href}'"]
    return []


def test_favicon_link_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    href = _link_href(html, "icon")
    if not href:
        return ['Missing or empty <link rel="icon">']
    return []


def test_favicon_file_exists() -> list[str]:
    return [] if FAVICON.is_file() else [f"Favicon referenced by index.html is missing: {FAVICON.relative_to(ROOT)}"]


def test_og_properties_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    errors = []
    for prop in REQUIRED_OG_PROPERTIES:
        value = _meta_content(html, "property", prop)
        if not value:
            errors.append(f'Missing or empty <meta property="{prop}">')
    return errors


def test_twitter_properties_present() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    errors = []
    for prop in REQUIRED_TWITTER_PROPERTIES:
        value = _meta_content(html, "name", prop)
        if not value:
            errors.append(f'Missing or empty <meta name="{prop}">')
    return errors


def test_twitter_card_type_valid() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    card = _meta_content(html, "name", "twitter:card")
    if card and card not in {"summary", "summary_large_image"}:
        return [f"twitter:card must be 'summary' or 'summary_large_image', got '{card}'"]
    return []


def test_absolute_social_urls() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    errors = []
    og_url = _meta_content(html, "property", "og:url")
    og_image = _meta_content(html, "property", "og:image")
    if og_url and not og_url.startswith(("http://", "https://")):
        errors.append(f"og:url must be an absolute URL, got '{og_url}'")
    if og_image and not og_image.startswith(("http://", "https://")):
        errors.append(f"og:image must be an absolute URL, got '{og_image}'")
    return errors


def test_og_twitter_image_match() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    og_image = _meta_content(html, "property", "og:image")
    twitter_image = _meta_content(html, "name", "twitter:image")
    if og_image and twitter_image and og_image != twitter_image:
        return ["twitter:image should match og:image for a consistent preview"]
    return []


def test_social_image_resolves_to_file() -> list[str]:
    html = _read_index()
    if html is None:
        return []
    og_image = _meta_content(html, "property", "og:image")
    if not og_image:
        return []
    repo_file = _repo_file_for_raw_url(og_image)
    if repo_file is None:
        return [f"og:image URL is not a repo raw URL, cannot verify file: '{og_image}'"]
    if not repo_file.is_file():
        return [f"og:image references a file that does not exist in the repo: {repo_file.relative_to(ROOT)}"]
    return []


def main() -> int:
    checks = [
        ("index_exists", test_index_exists),
        ("title_present", test_title_present),
        ("meta_description_present", test_meta_description_present),
        ("canonical_present", test_canonical_present),
        ("favicon_link_present", test_favicon_link_present),
        ("favicon_file_exists", test_favicon_file_exists),
        ("og_properties_present", test_og_properties_present),
        ("twitter_properties_present", test_twitter_properties_present),
        ("twitter_card_type_valid", test_twitter_card_type_valid),
        ("absolute_social_urls", test_absolute_social_urls),
        ("og_twitter_image_match", test_og_twitter_image_match),
        ("social_image_resolves_to_file", test_social_image_resolves_to_file),
    ]
    has_errors = False
    for name, fn in checks:
        errors = fn()
        if errors:
            has_errors = True
            print(f"FAIL [{name}]:")
            for error in errors:
                print(f"  - {error}")
        else:
            print(f"OK [{name}]")
    return 1 if has_errors else 0


if __name__ == "__main__":
    sys.exit(main())
