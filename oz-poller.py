#!/usr/bin/env python3
"""
Ahamkara Oz Poller — lightweight cron script for the local Mac.

Polls Forgejo for `task`-labeled issues and writes pending ones to a queue file
for the Oz agent to pick up and process via child agents.

Does NOT call any AI APIs — that's the Oz agent's job.

Requires env: FORGEJO_URL, FORGEJO_REPOSITORY, FORGEJO_TOKEN

Output: ~/Projects/Ahamkara/.ahamkara-queue/pending.json
"""

import json
import logging
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

QUEUE_DIR = Path.home() / "Projects" / "Ahamkara" / ".ahamkara-queue"
QUEUE_FILE = QUEUE_DIR / "pending.json"
PROCESSED_FILE = QUEUE_DIR / "processed.json"
LOG_DIR = QUEUE_DIR / "logs"

log = logging.getLogger("oz-poller")


def configure_logging() -> None:
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler(LOG_DIR / "oz-poller.log"),
        ],
    )


API_PAGE_LIMIT = 50
MAX_API_PAGES = 1000


def forgejo_api_url(path: str, *, page: int | None = None, limit: int = API_PAGE_LIMIT) -> str:
    """Build a repository-scoped Forgejo API URL from environment config."""
    base_url = os.environ.get("FORGEJO_URL", "").strip().rstrip("/")
    repository = os.environ.get("FORGEJO_REPOSITORY", "").strip("/")
    if not base_url or not repository:
        raise RuntimeError("FORGEJO_URL and FORGEJO_REPOSITORY must be set")

    api_root = base_url if base_url.endswith("/api/v1") else f"{base_url}/api/v1"
    endpoint, separator, raw_query = path.lstrip("/").partition("?")
    query = urllib.parse.parse_qsl(raw_query, keep_blank_values=True) if separator else []
    if page is not None:
        query = [(key, value) for key, value in query if key not in {"page", "limit"}]
        query.extend((("limit", str(limit)), ("page", str(page))))
    query_string = urllib.parse.urlencode(query, doseq=True)
    url = f"{api_root}/repos/{repository}/{endpoint}"
    return f"{url}?{query_string}" if query_string else url


def forgejo_api_headers(token: str | None = None) -> dict[str, str]:
    """Return the authenticated headers required by Forgejo's API."""
    api_token = token if token is not None else os.environ.get("FORGEJO_TOKEN", "")
    if not api_token:
        raise RuntimeError("FORGEJO_TOKEN must be set")
    return {
        "Authorization": f"token {api_token}",
        "Accept": "application/json",
    }


def _has_next_page(link_header: str) -> bool:
    for link in link_header.split(","):
        relation = re.search(r";\s*rel\s*=\s*\"?([^;\"]+)", link, re.IGNORECASE)
        if relation and relation.group(1).strip().lower() == "next":
            return True
    return False


def _get_api_page(path: str, *, page: int | None = None) -> tuple[dict | list, bool]:
    request = urllib.request.Request(
        forgejo_api_url(path, page=page),
        headers=forgejo_api_headers(),
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        data = json.loads(response.read().decode())
        return data, _has_next_page(response.headers.get("Link", ""))


def api_get(path: str) -> dict | list:
    """GET one Forgejo API resource."""
    try:
        data, _has_next = _get_api_page(path)
        return data
    except urllib.error.HTTPError as error:
        log.warning("api_get %s failed: %s", path, error.code)
        return {}
    except Exception as error:
        log.warning("api_get %s failed: %s", path, error)
        return {}


def api_get_paginated(path: str) -> list[dict]:
    """Collect every list page using Forgejo's Link rel=next response header."""
    items: list[dict] = []
    for page in range(1, MAX_API_PAGES + 1):
        try:
            data, has_next = _get_api_page(path, page=page)
        except urllib.error.HTTPError as error:
            log.warning("api_get %s page %d failed: %s", path, page, error.code)
            return []
        except Exception as error:
            log.warning("api_get %s page %d failed: %s", path, page, error)
            return []

        if not isinstance(data, list):
            return []
        items.extend(data)
        if not data or not has_next:
            return items

    log.warning("api_get %s exceeded the %d-page safety limit", path, MAX_API_PAGES)
    return items


def load_queue() -> dict:
    if QUEUE_FILE.exists():
        try:
            return json.loads(QUEUE_FILE.read_text())
        except (json.JSONDecodeError, OSError):
            return {}
    return {"pending": {}, "in_progress": {}}


def save_queue(q: dict):
    QUEUE_FILE.write_text(json.dumps(q, indent=2))


def load_processed() -> set:
    if PROCESSED_FILE.exists():
        try:
            return set(json.loads(PROCESSED_FILE.read_text()))
        except (json.JSONDecodeError, OSError):
            return set()
    return set()


def save_processed(processed: set):
    PROCESSED_FILE.write_text(json.dumps(sorted(processed), indent=2))


def has_coding_done(issue_num: int) -> bool:
    data = api_get_paginated(f"issues/{issue_num}/comments")
    if not isinstance(data, list):
        return False
    return any("<!-- CODING_DONE -->" in (c.get("body", "") or "") for c in data)


def main():
    configure_logging()
    processed = load_processed()
    queue = load_queue()

    issues = api_get_paginated("issues?labels=task&state=open")
    if not isinstance(issues, list):
        log.warning("Failed to fetch issues")
        return

    log.info("Found %d open task issues", len(issues))

    for issue in issues:
        num = issue["number"]
        title = issue["title"]

        # Skip if already done
        if num in processed:
            continue

        # Skip if already in queue or in progress
        if str(num) in queue.get("pending", {}) or str(num) in queue.get("in_progress", {}):
            continue

        # Skip if already has CODING_DONE + PASSED (already completed)
        comments_data = api_get_paginated(f"issues/{num}/comments")
        if isinstance(comments_data, list):
            all_text = " ".join(c.get("body", "") or "" for c in comments_data)
            if "Build and tests passed" in all_text or (
                "BUILD OK" in all_text.upper() and "<!-- GEMINI_REVIEW -->" in all_text
            ):
                processed.add(num)
                save_processed(processed)
                continue

        # New issue — add to pending queue
        issue_data = {
            "number": num,
            "title": title,
            "body": (issue.get("body") or "")[:2000],
            "detected_at": datetime.now(UTC).isoformat(),
            "labels": [label.get("name", "") for label in (issue.get("labels") or [])],
            "html_url": issue.get("html_url", ""),
        }

        if "pending" not in queue:
            queue["pending"] = {}
        queue["pending"][str(num)] = issue_data
        log.info("Queued issue #%s: %s", num, title)

    save_queue(queue)
    n_pending = len(queue.get("pending", {}))
    n_in_prog = len(queue.get("in_progress", {}))
    log.info("Pending: %d | In progress: %d | Processed: %d", n_pending, n_in_prog, len(processed))

    if n_pending > 0 and n_pending <= 3:
        log.info("--- New issues waiting! Talk to Oz to implement them. ---")


if __name__ == "__main__":
    main()
