#!/usr/bin/env python3
"""
Ahamkara Reviewer Runner — runs on the build server.

Watches Forgejo for task-issue branches pushed by the local coding agent,
pulls them, builds in Docker, runs tests, and uses Gemini to review failures.
Posts feedback as Forgejo issue comments. On success, creates a pull request
and closes the issue.

Requires FORGEJO_URL, FORGEJO_REPOSITORY, FORGEJO_TOKEN, and GEMINI_API_KEY.
"""

import json
import logging
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
FORGEJO_URL = os.getenv("FORGEJO_URL", "").rstrip("/")
FORGEJO_REPOSITORY = os.getenv("FORGEJO_REPOSITORY", "")
FORGEJO_TOKEN = os.getenv("FORGEJO_TOKEN", "")
AHAMKARA_CLONE = Path("/home/taufe/Projects/ahamkara")
LOG_DIR = Path(os.getenv("REVIEWER_LOG_DIR", "/home/taufe/agent-runner/logs"))
PROCESSED_LOG = Path("/home/taufe/agent-runner/processed_issues.json")
JUPITER_URL = os.getenv("JUPITER_URL", "http://localhost:9001/notify")
JUPITER_CHANNEL = os.getenv("JUPITER_CHANNEL", "ahamkara")
POLL_INTERVAL = int(os.getenv("POLL_INTERVAL", "60"))  # seconds between polls
MAX_ATTEMPTS = int(os.getenv("MAX_ATTEMPTS", "5"))  # max rounds per issue

DOCKER_IMAGE = "ahamkara-build"
DOCKER_WORKSPACE = "/workspace"

# Gemini reviewer config
GEMINI_MODEL = os.getenv("GEMINI_MODEL", "gemini-2.5-flash")
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")
GEMINI_API_URL = (
    f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={GEMINI_API_KEY}"
)


# Build commands (run inside Docker)
CMAKE_CONFIGURE_CMD = "cmake --preset debug"
BUILD_CMD = "cmake --build build/debug -j$(nproc)"
TEST_CMD = "cd build/debug && ctest --output-on-failure -j$(nproc)"

# Markers for machine-to-machine communication on Forgejo issue comments
MARKER_CODING_DONE = "<!-- CODING_DONE -->"
MARKER_GEMINI_REVIEW = "<!-- GEMINI_REVIEW -->"

LOG_DIR.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(LOG_DIR / "reviewer-runner.log"),
    ],
)
log = logging.getLogger("reviewer-runner")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def fatal_missing(name: str):
    log.error("FATAL: %s not set", name)
    sys.exit(1)


def die(msg: str):
    log.error("FATAL: %s", msg)
    sys.exit(1)


def build_api_url(path: str, params: dict | None = None) -> str:
    """Build a Forgejo repository API URL, preserving an instance URL prefix."""
    from urllib.parse import quote, urlencode, urlsplit

    parts = urlsplit(FORGEJO_URL)
    if parts.scheme not in {"http", "https"} or not parts.netloc or parts.username or parts.password:
        raise ValueError("FORGEJO_URL must be an http(s) instance URL without embedded credentials")
    repo_parts = FORGEJO_REPOSITORY.split("/")
    if len(repo_parts) != 2 or not all(re.fullmatch(r"[A-Za-z0-9_.-]+", part) for part in repo_parts):
        raise ValueError("FORGEJO_REPOSITORY must be in owner/repository form")

    owner, repo = (quote(part, safe="") for part in repo_parts)
    endpoint = str(path).strip("/")
    url = f"{FORGEJO_URL}/api/v1/repos/{owner}/{repo}"
    if endpoint:
        url += "/" + quote(endpoint, safe="/")
    if params:
        url += "?" + urlencode(params)
    return url


def _forgejo_headers() -> dict[str, str]:
    if not FORGEJO_TOKEN:
        raise RuntimeError("FORGEJO_TOKEN is not set")
    return {
        "Authorization": f"token {FORGEJO_TOKEN}",
        "Accept": "application/json",
        "User-Agent": "ahamkara-reviewer-runner",
    }


def api_request(method: str, path: str, data: dict | None = None, params: dict | None = None):
    """Make one authenticated Forgejo repository API request."""
    body = json.dumps(data).encode() if data is not None else None
    headers = _forgejo_headers()
    if body is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(build_api_url(path, params), data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read()
        return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        log.warning("Forgejo %s %s failed: %s %s", method, path, e.code, e.read().decode()[:200])
        return {} if method == "GET" else None


def api_get(path: str, params: dict | None = None):
    return api_request("GET", path, params=params)


def api_post(path: str, data: dict):
    return api_request("POST", path, data=data)


def api_patch(path: str, data: dict):
    return api_request("PATCH", path, data=data)


def api_get_all(path: str, params: dict | None = None, page_size: int = 50) -> list[dict]:
    """Fetch all pages from a Forgejo list endpoint."""
    if page_size < 1 or page_size > 50:
        raise ValueError("page_size must be between 1 and 50")
    items = []
    page = 1
    while page <= 1000:
        batch = api_get(path, {**(params or {}), "page": page, "limit": page_size})
        if not isinstance(batch, list):
            raise ValueError(f"Expected a list response from Forgejo API path {path!r}")
        items.extend(batch)
        if len(batch) < page_size:
            return items
        page += 1
    raise RuntimeError(f"Forgejo pagination exceeded 1000 pages for API path {path!r}")


# ---------------------------------------------------------------------------
# Discord notifications
# ---------------------------------------------------------------------------
def jupiter_notify(title: str, message: str, priority: str = "info") -> None:
    """Send a notification through Jupiter to Discord."""
    if not JUPITER_URL:
        return
    payload = {
        "channel": JUPITER_CHANNEL,
        "title": title,
        "message": message,
        "priority": priority,
    }
    try:
        req = urllib.request.Request(
            JUPITER_URL,
            data=json.dumps(payload).encode(),
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            resp.read()
    except Exception as e:
        log.warning("jupiter_notify failed: %s", e)


# ---------------------------------------------------------------------------
# Gemini reviewer
# ---------------------------------------------------------------------------
def gemini_call(prompt: str, system_instruction: str | None = None) -> str | None:
    """Call Gemini API and return the response text."""
    payload: dict = {
        "contents": [{"parts": [{"text": prompt}]}],
    }
    if system_instruction:
        payload["systemInstruction"] = {"parts": [{"text": system_instruction}]}

    req = urllib.request.Request(
        GEMINI_API_URL,
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = json.loads(resp.read().decode())
        candidates = data.get("candidates", [])
        if not candidates:
            return None
        parts = candidates[0].get("content", {}).get("parts", [])
        return parts[0].get("text") if parts else None
    except urllib.error.HTTPError as e:
        err_body = e.read().decode()[:300]
        log.warning("Gemini API error %s: %s", e.code, err_body)
        return None
    except Exception as e:
        log.warning("Gemini call failed: %s", e)
        return None


# ---------------------------------------------------------------------------
# Forgejo operations
# ---------------------------------------------------------------------------
def get_issue_comments(issue_num: int) -> list[dict]:
    """Get all comments on an issue."""
    data = api_get_all(f"issues/{issue_num}/comments")
    return data if isinstance(data, list) else []


def comment_on_issue(issue_num: int, body: str) -> None:
    """Post a comment on an issue."""
    api_post(f"issues/{issue_num}/comments", {"body": body})


def fetch_issue(issue_num: int) -> dict | None:
    """Fetch a single issue by number."""
    data = api_get(f"issues/{issue_num}")
    return data if isinstance(data, dict) and "number" in data else None


def load_processed() -> dict:
    """Load processed issues tracker. Returns dict of issue_num -> status."""
    if PROCESSED_LOG.exists():
        try:
            return json.loads(PROCESSED_LOG.read_text())
        except (json.JSONDecodeError, OSError):
            return {}
    return {"processed": [], "active": {}}


def save_processed(tracker: dict) -> None:
    """Write processed issues tracker."""
    PROCESSED_LOG.write_text(json.dumps(tracker, indent=2))


def _label_names(issue: dict) -> set[str]:
    labels = issue.get("labels", [])
    names = set()
    for label in labels if isinstance(labels, list) else []:
        if isinstance(label, str):
            names.add(label.casefold())
        elif isinstance(label, dict) and isinstance(label.get("name"), str):
            names.add(label["name"].casefold())
    return names


def open_task_issues() -> list[dict]:
    """Fetch open task issues, excluding pull requests returned by Forgejo."""
    issues = api_get_all("issues", {"state": "open"})
    return [
        issue
        for issue in issues
        if isinstance(issue, dict)
        and str(issue.get("state", "open")).casefold() == "open"
        and not issue.get("pull_request")
        and "task" in _label_names(issue)
    ]


def find_open_pull_request(branch: str) -> dict | None:
    """Find an open pull request from this repository branch."""
    expected_repo = FORGEJO_REPOSITORY.casefold()
    for pull in api_get_all("pulls", {"state": "open"}):
        head = pull.get("head") or {}
        head_repo = head.get("repo") or {}
        if head.get("ref") != branch:
            continue
        if head_repo.get("full_name") and head_repo["full_name"].casefold() != expected_repo:
            continue
        return pull
    return None


def create_pull_request(title: str, body: str, head: str, base: str) -> dict | None:
    """Create a pull request through Forgejo's repository API."""
    if not base:
        raise ValueError("Pull request base branch is required")
    result = api_post("pulls", {"title": title, "body": body, "head": head, "base": base})
    return result if isinstance(result, dict) else None


def pull_request_url(pull: dict | None) -> str:
    """Return the web URL supplied by Forgejo, with a same-instance fallback."""
    if not pull:
        return ""
    html_url = pull.get("html_url")
    if isinstance(html_url, str) and html_url:
        return html_url
    number = pull.get("number")
    if number is None:
        return ""
    return f"{FORGEJO_URL}/{FORGEJO_REPOSITORY}/pulls/{number}"


def issue_url(issue_num: int) -> str:
    return f"{FORGEJO_URL}/{FORGEJO_REPOSITORY}/issues/{issue_num}"


# ---------------------------------------------------------------------------
# Docker build + test
# ---------------------------------------------------------------------------
def docker_run(worktree_path: str, cmd: str, timeout: int = 300) -> tuple[bool, str, str]:
    """Run a command inside the ahamkara-build Docker container."""
    docker_cmd = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{worktree_path}:{DOCKER_WORKSPACE}",
        "--user",
        f"{os.getuid()}:{os.getgid()}",
        "-w",
        DOCKER_WORKSPACE,
        DOCKER_IMAGE,
        "bash",
        "-c",
        cmd,
    ]
    try:
        result = subprocess.run(
            docker_cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        ok = result.returncode == 0
        return ok, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", f"Command timed out after {timeout}s"
    except Exception as e:
        return False, "", str(e)


def build_project(worktree_path: str) -> tuple[bool, str, str]:
    """Configure and build the project in Docker."""
    # Remove stale CMake cache so Docker paths don't conflict with host paths
    run_cmd(f"rm -rf {worktree_path}/build/debug/CMakeCache.txt {worktree_path}/build/debug/CMakeFiles/", timeout=10)

    log.info("Configuring CMake...")
    ok, out, err = docker_run(worktree_path, CMAKE_CONFIGURE_CMD, timeout=120)
    if not ok:
        return False, out, err

    log.info("Building...")
    return docker_run(worktree_path, BUILD_CMD, timeout=300)


def test_project(worktree_path: str) -> tuple[bool, str, str]:
    """Run tests in Docker."""
    log.info("Running tests...")
    return docker_run(worktree_path, TEST_CMD, timeout=300)


# ---------------------------------------------------------------------------
# Review logic
# ---------------------------------------------------------------------------
def build_review_prompt(issue: dict, build_log: str, test_log: str) -> str:
    """Build a prompt for Gemini to review build/test failures."""
    return f"""You are a C++ game engine code reviewer. Review the build and test output below.

Issue: #{issue["number"]} - {issue["title"]}
{issue.get("body", "")[:1500]}

BUILD LOG:
{build_log[:3000] if build_log else "(none)"}

TEST LOG:
{test_log[:3000] if test_log else "(none)"}

Analyze the errors. Identify:
1. Root cause of each error
2. Which files need changes and what the fix should be
3. Whether this is a simple fix or a fundamental design problem

If you believe the build/test results are acceptable (e.g. failures are pre-existing, not caused by the changes), respond with "REVIEW: PASS" followed by a brief explanation.

Otherwise, provide specific, actionable fixes. Use this format for each issue:
- **File**: path/to/file.cpp:LINE
- **Problem**: what's wrong
- **Fix**: specific code change needed"""


def review_with_gemini(issue: dict, build_log: str, test_log: str) -> str | None:
    """Send build/test logs to Gemini and get review feedback."""
    prompt = build_review_prompt(issue, build_log, test_log)
    system = "You are a senior C++ game engine developer. Be concise and specific. Focus on actionable fixes."
    return gemini_call(prompt, system)


# ---------------------------------------------------------------------------
# Git operations
# ---------------------------------------------------------------------------
def checkout_branch(branch: str) -> bool:
    """Fetch and checkout a remote branch in the local clone."""
    ok, _out, err = run_cmd(f"git fetch origin {branch}:{branch}", str(AHAMKARA_CLONE))
    if not ok:
        log.warning("Failed to fetch branch %s: %s", branch, err[:200])
        return False

    ok, _out, err = run_cmd(f"git checkout {branch}", str(AHAMKARA_CLONE))
    if not ok:
        log.warning("Failed to checkout branch %s: %s", branch, err[:200])
        return False
    return True


def run_cmd(cmd: str, cwd: str | None = None, timeout: int = 120) -> tuple[bool, str, str]:
    """Run a shell command."""
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
        )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", f"Command timed out after {timeout}s"
    except Exception as e:
        return False, "", str(e)


# ---------------------------------------------------------------------------
# Issue processing
# ---------------------------------------------------------------------------
def process_issue(issue: dict) -> None:
    """Process a single issue: build, test, review, loop."""
    num = issue["number"]
    title = issue["title"]
    branch = f"task/{num}"

    log.info("=== Processing issue #%s: %s ===", num, title)
    log.info("Branch: %s", branch)

    comment_on_issue(num, f"Reviewer-runner starting build for branch `{branch}`...")

    attempt = 0
    while attempt < MAX_ATTEMPTS:
        attempt += 1
        log.info("Attempt %d/%d", attempt, MAX_ATTEMPTS)

        # 1. Fetch the branch from origin
        ok, _out, err = run_cmd(f"git fetch origin {branch}:{branch}", str(AHAMKARA_CLONE))
        if not ok:
            msg = f"Failed to fetch branch `{branch}`. Is the local agent done pushing?"
            log.warning(msg)
            comment_on_issue(num, msg)
            return

        # 2. Checkout the branch
        ok, _out, err = run_cmd(f"git checkout {branch}", str(AHAMKARA_CLONE))
        if not ok:
            log.warning("Failed to checkout %s: %s", branch, err[:200])
            return

        worktree_path = str(AHAMKARA_CLONE)

        # 3. Build
        build_ok, build_out, build_err = build_project(worktree_path)
        if not build_ok:
            build_log = build_out + "\n" + build_err
            log.info("Build failed (attempt %d)", attempt)

            # Gemini review
            review = review_with_gemini(issue, build_log, "")
            if review:
                review_text = (
                    f"{MARKER_GEMINI_REVIEW}\n\n**Build failure — attempt {attempt}/{MAX_ATTEMPTS}**\n\n{review}"
                )
                comment_on_issue(num, review_text)

                # Check if Gemini says it's a PASS
                if "REVIEW: PASS" in review.upper():
                    log.info("Gemini passed the build failure (pre-existing issue)")
                    continue

            if attempt >= MAX_ATTEMPTS:
                msg = f"Build failed after {MAX_ATTEMPTS} attempts. Needs manual attention."
                comment_on_issue(num, msg)
                jupiter_notify(
                    f"⚠️ Issue #{num} needs attention",
                    f"Build failing for [{title}]({issue_url(num)}) after {MAX_ATTEMPTS} attempts.",
                    priority="high",
                )
                return

            # Post CODING_DONE marker to signal local agent it's the local agent's turn
            comment_on_issue(num, f"{MARKER_CODING_DONE} Build failed, review posted above.")
            return  # Exit — local agent will see review and push a fix

        log.info("Build succeeded!")

        # 4. Test
        test_ok, test_out, test_err = test_project(worktree_path)
        if not test_ok:
            test_log = test_out + "\n" + test_err
            log.info("Tests failed (attempt %d)", attempt)

            review = review_with_gemini(issue, "", test_log)
            if review:
                review_text = (
                    f"{MARKER_GEMINI_REVIEW}\n\n**Test failure — attempt {attempt}/{MAX_ATTEMPTS}**\n\n{review}"
                )
                comment_on_issue(num, review_text)

                if "REVIEW: PASS" in review.upper():
                    log.info("Gemini passed the test failure (pre-existing issue)")
                    continue

            if attempt >= MAX_ATTEMPTS:
                msg = f"Tests failed after {MAX_ATTEMPTS} attempts. Needs manual attention."
                comment_on_issue(num, msg)
                jupiter_notify(
                    f"⚠️ Issue #{num} needs attention",
                    f"Tests failing for [{title}]({issue_url(num)}) after {MAX_ATTEMPTS} attempts.",
                    priority="high",
                )
                return

            comment_on_issue(num, f"{MARKER_CODING_DONE} Tests failed, review posted above.")
            return

        log.info("Tests passed!")

        # 5. Build + Tests passed! Create PR and close issue.
        jupiter_notify(
            f"✅ Issue #{num} build passed",
            f"[{title}]({issue_url(num)}) — all builds and tests pass.",
            priority="info",
        )

        comment_on_issue(num, "✅ **Build and tests passed!** Creating PR...")

        # Check if PR already exists
        existing_pr = find_open_pull_request(branch)
        if existing_pr:
            pr_url = pull_request_url(existing_pr)
            log.info("PR already exists: %s", pr_url)
        else:
            created_pr = create_pull_request(
                f"[Task #{num}] {title}",
                f"Closes #{num}\n\nAutomated implementation by the Ahamkara pipeline.\n\nCo-Authored-By: Oz <oz-agent@warp.dev>",
                branch,
                "main",
            )
            pr_url = pull_request_url(created_pr)
            if not pr_url:
                log.warning("Forgejo did not return a pull request URL for issue #%s", num)
            else:
                log.info("PR created: %s", pr_url)

        # Close the issue
        api_patch(f"issues/{num}", {"state": "closed"})
        comment_on_issue(num, f"Issue closed. PR: {pr_url}")
        log.info("Issue #%s closed", num)
        return

    log.warning("Issue #%s exceeded max attempts without resolution", num)


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main() -> None:
    # Validate required env
    if not FORGEJO_URL:
        fatal_missing("FORGEJO_URL")
    if not FORGEJO_REPOSITORY:
        fatal_missing("FORGEJO_REPOSITORY")
    if not FORGEJO_TOKEN:
        fatal_missing("FORGEJO_TOKEN")
    if not GEMINI_API_KEY:
        fatal_missing("GEMINI_API_KEY")

    log.info("Reviewer Runner starting")
    log.info("Forgejo repository: %s/%s", FORGEJO_URL, FORGEJO_REPOSITORY)
    log.info("Model: Gemini (%s)", GEMINI_MODEL)
    log.info("Poll interval: %ds", POLL_INTERVAL)

    tracker = load_processed()

    # Handle --process flag for single-issue mode
    if len(sys.argv) > 1 and sys.argv[1] == "--process":
        issue_num = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        if not issue_num:
            die("Usage: reviewer-runner.py --process <issue_num>")
        issue = fetch_issue(issue_num)
        if not issue:
            die(f"Issue #{issue_num} not found")
        process_issue(issue)
        return

    # --once: single poll cycle (for cron mode)
    run_once = "--once" in sys.argv

    log.info("Starting poll loop%s...", " (single run)" if run_once else "")

    def poll_cycle():
        """One full poll cycle."""
        nonlocal tracker
        try:
            issues = open_task_issues()
            log.info("Found %d open task issues", len(issues))

            for issue in issues:
                num = issue["number"]

                # Check if we already processed this issue
                active = tracker.get("active", {})
                if str(num) in active:
                    continue

                # Check if issue has a CODING_DONE comment (local agent pushed code)
                comments = get_issue_comments(num)
                has_coding_done = any(MARKER_CODING_DONE in (c.get("body", "") or "") for c in comments)

                if not has_coding_done:
                    continue  # Wait for local agent to code this issue

                # Check if we've already reviewed this round (avoid duplicate reviews)
                our_comments = [c for c in comments if MARKER_GEMINI_REVIEW in (c.get("body", "") or "")]
                last_review_round = 0
                for c in our_comments:
                    body = c.get("body", "") or ""
                    m = re.search(r"attempt (\d+)/", body)
                    if m:
                        last_review_round = max(last_review_round, int(m.group(1)))

                coding_done_count = sum(1 for c in comments if MARKER_CODING_DONE in (c.get("body", "") or ""))

                if last_review_round >= coding_done_count:
                    log.info("Issue #%s: already reviewed round %d, skipping", num, last_review_round)
                    continue

                if "active" not in tracker:
                    tracker["active"] = {}
                tracker["active"][str(num)] = {"started": datetime.now(UTC).isoformat()}
                save_processed(tracker)

                process_issue(issue)

                tracker = load_processed()
                if "active" in tracker and str(num) in tracker["active"]:
                    del tracker["active"][str(num)]
                    processed_list = tracker.get("processed", [])
                    if num not in processed_list:
                        processed_list.append(num)
                    tracker["processed"] = processed_list
                    save_processed(tracker)

        except Exception as e:
            log.error("Poll loop error: %s", e)

    if run_once:
        poll_cycle()
    else:
        while True:
            poll_cycle()
            time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
