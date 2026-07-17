#!/usr/bin/env python3
"""
Ahamkara Oz Poller — lightweight cron script for the local Mac.

Polls GitHub for `task`-labeled issues and writes pending ones to a queue file
for the Oz agent (this conversation) to pick up and process via child agents.

Does NOT call any AI APIs — that's the Oz agent's job.

Requires env: GITHUB_TOKEN

Output: ~/Projects/Ahamkara/.ahamkara-queue/pending.json
"""

import json
import logging
import subprocess
import urllib.error
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

REPO = "tfq26/Project-Ahamkara"
QUEUE_DIR = Path.home() / "Projects" / "Ahamkara" / ".ahamkara-queue"
QUEUE_FILE = QUEUE_DIR / "pending.json"
PROCESSED_FILE = QUEUE_DIR / "processed.json"
LOG_DIR = QUEUE_DIR / "logs"

LOG_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(LOG_DIR / "oz-poller.log"),
    ],
)
log = logging.getLogger("oz-poller")


def gh_token() -> str:
    """Get GitHub token from gh CLI keychain."""
    result = subprocess.run(
        ["gh", "auth", "token"],
        capture_output=True,
        text=True,
        timeout=10,
    )
    if result.returncode != 0:
        raise RuntimeError("gh auth token failed: " + result.stderr[:200])
    return result.stdout.strip()


def api_get(path: str) -> dict | list:
    token = gh_token()
    url = f"https://api.github.com/repos/{REPO}/{path.lstrip('/')}"
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github.v3+json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        log.warning("api_get %s failed: %s", path, e.code)
        return {}
    except Exception as e:
        log.warning("api_get %s failed: %s", path, e)
        return {}


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
    data = api_get(f"issues/{issue_num}/comments")
    if not isinstance(data, list):
        return False
    return any("<!-- CODING_DONE -->" in (c.get("body", "") or "") for c in data)


def main():
    processed = load_processed()
    queue = load_queue()

    issues = api_get("issues?labels=task&state=open&per_page=20")
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
        comments_data = api_get(f"issues/{num}/comments")
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
