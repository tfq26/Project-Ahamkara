#!/usr/bin/env python3
"""check-docs-links.py

Validates that relative markdown links within docs/ resolve to existing
files or directories.  External (http://, https://) and anchor-only (#)
links are skipped.

Usage:
  ./scripts/check-docs-links.sh [--ci]

Options:
  --ci   Machine-readable output (GitHub Actions annotations)
"""

import os
import re
import sys
import pathlib
import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ci", action="store_true", help="GitHub Actions annotations")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent.parent
    os.chdir(repo_root)

    checked = 0
    failed = 0

    # Link pattern: [text](target)
    link_re = re.compile(r"\]\(([^)]+)\)")

    for md_file in sorted(repo_root.glob("docs/**/*.md")):
        # Skip vault files which use wiki links
        if "docs/vault" in md_file.parts:
            continue

        text = md_file.read_text()
        md_dir = md_file.parent

        for match in link_re.finditer(text):
            target = match.group(1)

            # Skip external URLs, anchors, mailto, and absolute paths
            # (absolute paths are machine-specific or external references)
            if target.startswith(("http://", "https://", "mailto:", "#", "/")):
                continue

            checked += 1

            # Strip fragment (e.g., docs/foo.md#section -> docs/foo.md)
            no_frag = target.split("#")[0]

            if not no_frag:
                # Pure anchor link in same file
                continue

            # Resolve relative path
            resolved = (md_dir / no_frag).resolve()

            if not resolved.exists():
                failed += 1
                rel_path = md_file.relative_to(repo_root)
                if args.ci:
                    print(f"::error file={rel_path},title=Broken docs link::"
                          f"Link '{target}' -> {resolved} not found")
                else:
                    print(f"[FAIL] {rel_path}: link '{target}' -> {resolved} not found")

    if args.ci:
        if failed == 0:
            print(f"::notice::All {checked} doc links resolved successfully")
        else:
            print(f"::notice::{checked} links checked, {failed} broken links found")
    else:
        print(f"\n{'All' if failed == 0 else ''} "
              f"Checked {checked} links"
              f"{', ' + str(failed) + ' broken' if failed else ', all OK'}.")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
