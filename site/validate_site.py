#!/usr/bin/env python3
"""Validate the marketing site files exist and contain expected content."""

import os
import re
import sys

SITE_DIR = os.path.dirname(os.path.abspath(__file__))

REQUIRED_FILES = [
    "index.html",
    "styles.css",
    "scripts.js",
    "src/assets/hero-bg.svg",
    "src/assets/ahamkara-emblem.svg",
    "src/assets/wish-emblem.svg",
    "src/assets/inspiration-bg.svg",
    "src/assets/Javelin-4.jpg",
]

REQUIRED_SECTIONS = [
    ("hero", "Hero"),
    ("ahamkara", "What is Ahamkara"),
    ("wish", "What is Wish"),
    ("inspiration", "Inspiration"),
]


def main():
    errors = []
    for f in REQUIRED_FILES:
        path = os.path.join(SITE_DIR, f)
        if not os.path.isfile(path):
            errors.append(f"MISSING: {f}")

    html_path = os.path.join(SITE_DIR, "index.html")
    if os.path.isfile(html_path):
        with open(html_path) as f:
            content = f.read()
        for section_id, name in REQUIRED_SECTIONS:
            if f'id="{section_id}"' not in content:
                errors.append(f"MISSING section '{name}' (id={section_id})")
        if '<!DOCTYPE html>' not in content:
            errors.append("MISSING DOCTYPE")
        if 'name="viewport"' not in content:
            errors.append("MISSING viewport meta tag")
        imgs = re.findall(r'<img[^>]+>', content)
        for img in imgs:
            if 'alt=""' in img or 'alt=' not in img:
                errors.append(f"Image missing alt text: {img[:80]}")
        if "@media" not in open(os.path.join(SITE_DIR, "styles.css")).read():
            errors.append("CSS missing media queries")

    if errors:
        print("VALIDATION FAILED:")
        for err in errors:
            print(f"  - {err}")
        sys.exit(1)
    print("All validations passed.")
    sys.exit(0)


if __name__ == "__main__":
    main()
