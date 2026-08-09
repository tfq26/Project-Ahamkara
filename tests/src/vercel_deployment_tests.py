#!/usr/bin/env python3
"""Validate the Vercel deployment configuration for the static landing page."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

VERCEL_CONFIG = ROOT / "vercel.json"
LANDING_PAGE_DIR = ROOT / "site"
README = ROOT / "README.md"

REQUIRED_README_MARKERS = [
    "vercel",
    "vercel.json",
    "site/",
    "vercel --prod",
]


def test_config_exists_and_is_valid_json() -> list[str]:
    if not VERCEL_CONFIG.is_file():
        return [f"Missing {VERCEL_CONFIG.relative_to(ROOT)}"]
    try:
        json.loads(VERCEL_CONFIG.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"vercel.json is not valid JSON: {exc}"]
    return []


def test_config_deploys_static_landing_page() -> list[str]:
    try:
        config = json.loads(VERCEL_CONFIG.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return ["Could not read vercel.json"]

    errors: list[str] = []

    output_dir = config.get("outputDirectory")
    if not output_dir:
        errors.append("vercel.json must define an 'outputDirectory'")
    elif output_dir != LANDING_PAGE_DIR.name:
        errors.append(f"outputDirectory must be '{LANDING_PAGE_DIR.name}', got '{output_dir}'")
    else:
        out_path = ROOT / output_dir
        if not out_path.is_dir():
            errors.append(f"output directory '{output_dir}' does not exist")
        if not (out_path / "index.html").is_file():
            errors.append(f"output directory '{output_dir}' is missing index.html")

    if config.get("buildCommand"):
        errors.append("static landing page should not require a build command")

    if not isinstance(config.get("rewrites"), list):
        errors.append("vercel.json should define a 'rewrites' list for routing")
    if not isinstance(config.get("headers"), list):
        errors.append("vercel.json should define a 'headers' list")

    return errors


def test_readme_documents_vercel_deploy() -> list[str]:
    if not README.is_file():
        return [f"Missing {README.relative_to(ROOT)}"]
    content = README.read_text(encoding="utf-8").lower()
    return [f"README.md must mention {marker!r}" for marker in REQUIRED_README_MARKERS if marker not in content]


def main() -> int:
    checks = [
        ("config_valid_json", test_config_exists_and_is_valid_json),
        ("config_deploys_landing_page", test_config_deploys_static_landing_page),
        ("readme_documents_vercel", test_readme_documents_vercel_deploy),
    ]
    has_errors = False
    for name, fn in checks:
        errors = fn()
        if errors:
            has_errors = True
            print(f"FAIL [{name}]:")
            for err in errors:
                print(f"  - {err}")
        else:
            print(f"OK [{name}]")
    return 1 if has_errors else 0


if __name__ == "__main__":
    sys.exit(main())
