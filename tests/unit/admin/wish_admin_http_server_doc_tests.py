#!/usr/bin/env python3
"""Validation tests for docs/wish_admin_http_server.md.

The admin HTTP server architecture doc is a documentation-only deliverable, so
the test suite for it validates that the document exists, covers every topic the
issue's acceptance criteria requires (request flow, templating, landing page,
security), and that every `[src: file: ...]` citation resolves to a real file at
the cited line range. This keeps the doc from drifting away from the code it
describes.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DOC = ROOT / "docs" / "wish_admin_http_server.md"

failures: list[str] = []


def check(cond: bool, label: str) -> None:
    if not cond:
        failures.append(label)


def read_doc() -> str:
    if not DOC.is_file():
        failures.append(f"document missing: {DOC.relative_to(ROOT)}")
        return ""
    return DOC.read_text(encoding="utf-8")


def check_section(text: str, heading: str) -> None:
    check(
        f"## {heading}" in text,
        f"document must contain a '## {heading}' section",
    )


def check_mentions(text: str, needle: str, label: str) -> None:
    check(needle in text, f"document must mention {label!r}")


def check_citations_resolve(text: str) -> None:
    """Every [src: file: path:start(-end)] citation must resolve to a real file
    with the cited line range inside it."""
    pattern = re.compile(r"\[src: file: ([^:\]]+?):(\d+)(?:-(\d+))?\]")
    for match in pattern.finditer(text):
        path, start, end = match.group(1), int(match.group(2)), match.group(3)
        target = ROOT / path
        if not target.is_file():
            failures.append(f"citation file does not exist: {path}")
            continue
        total_lines = len(target.read_text(encoding="utf-8").splitlines())
        if start < 1 or start > total_lines:
            failures.append(f"citation start line {start} out of range for {path} ({total_lines} lines)")
        if end is not None and int(end) > total_lines:
            failures.append(f"citation end line {end} out of range for {path} ({total_lines} lines)")


def main() -> int:
    text = read_doc()

    # Acceptance criteria: request flow, templating, and security considerations.
    check_section(text, "Request flow")
    check_section(text, "Rendering and templating")
    check_section(text, "Security considerations")
    check_section(text, "Landing page")

    # Core technical facts that must stay accurate.
    check_mentions(text, "HttpAdminServer", "the server class")
    check_mentions(text, "/health", "the health endpoint")
    check_mentions(text, "/metrics", "the metrics endpoint")
    check_mentions(text, "/match/status", "the match status endpoint")
    check_mentions(text, "/players", "the players endpoint")
    check_mentions(text, "/api/v1/heartbeat", "the heartbeat endpoint")
    check_mentions(text, "7778", "default admin port")
    check_mentions(text, "4096", "request buffer cap")
    check_mentions(text, "escape_json", "JSON escaping")
    check_mentions(text, "escape_html", "HTML escaping")
    check_mentions(text, "Cache-Control: no-store", "no-store cache header")
    check_mentions(text, "render_info_page", "landing page renderer")
    check_mentions(text, "Content-Type: text/html", "landing page content type")
    check(
        "no tls" in text.lower(),
        "document must mention the TLS limitation",
    )
    check_mentions(text, "INADDR_ANY", "bind address")

    # Edge cases: a fully empty/missing document, or a document with placeholder
    # text, must fail rather than silently pass.
    check(bool(text), "document body must not be empty")
    check(
        "{{" not in text,
        "document must not contain an unresolved template placeholder ({{)",
    )

    # Every file citation must resolve against the repository.
    check_citations_resolve(text)

    if failures:
        print("wish_admin_http_server_doc_tests FAILED:")
        for item in failures:
            print(f"  - {item}")
        return 1

    print("wish_admin_http_server_doc_tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
