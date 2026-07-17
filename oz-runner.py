#!/usr/bin/env python3
"""
Ahamkara Oz Runner — runs on the local Mac.

Polls GitHub for `task`-labeled issues, creates git worktrees, uses
DeepSeek/OpenAI to implement the issue, pushes to a branch, then waits for the
server-side Reviewer Runner to build, test, and Gemini-review the code.

The server posts feedback as GitHub issue comments.  This script reads those
comments and loops the coding agent until the build passes.

Requires env:
  GITHUB_TOKEN
  DEEPSEEK_API_KEY, DEEPSEEK_MODEL (default deepseek-chat)
  -- OR -- OPENAI_ATHENA_KEY (uses gpt-4o)
  AHAMKARA_CLONE (path to the local clone)
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
REPO = "tfq26/Project-Ahamkara"
DEFAULT_CLONE = Path.home() / "Projects" / "Ahamkara"
AHAMKARA_CLONE = Path(os.getenv("AHAMKARA_CLONE", str(DEFAULT_CLONE)))
WORKTREE_BASE = AHAMKARA_CLONE.parent
LOG_DIR = Path(os.getenv("OZ_LOG_DIR", str(AHAMKARA_CLONE / ".oz-runner")))
STATE_FILE = LOG_DIR / "active_issues.json"
POLL_INTERVAL = int(os.getenv("OZ_POLL_INTERVAL", "120"))  # seconds
MAX_RETRIES = int(os.getenv("OZ_MAX_RETRIES", "10"))

# Coder AI config — try DeepSeek first, fall back to OpenAI
DEEPSEEK_API_KEY = os.getenv("DEEPSEEK_API_KEY", "")
DEEPSEEK_MODEL = os.getenv("DEEPSEEK_MODEL", "deepseek-chat")
DEEPSEEK_URL = os.getenv("DEEPSEEK_URL", "https://api.deepseek.com/v1/chat/completions")

OPENAI_API_KEY = os.getenv("OPENAI_ATHENA_KEY", "")
OPENAI_MODEL = os.getenv("OZ_CODER_MODEL", "gpt-4o")
OPENAI_URL = "https://api.openai.com/v1/chat/completions"

# Markers
MARKER_CODING_DONE = "<!-- CODING_DONE -->"
MARKER_GEMINI_REVIEW = "<!-- GEMINI_REVIEW -->"
MARKER_WINDOWS_BUILD_FAIL = "<!-- WINDOWS_BUILD_FAIL -->"

# Windows build host (gaming PC)
WINDOWS_HOST = os.getenv("WINDOWS_BUILD_HOST", "100.124.18.104")
WINDOWS_USER = os.getenv("WINDOWS_BUILD_USER", "taufe")
WINDOWS_PATH = os.getenv("WINDOWS_BUILD_PATH", "C:\\Users\\taufe\\ahamkara")

LOG_DIR.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(LOG_DIR / "oz-runner.log"),
    ],
)
log = logging.getLogger("oz-runner")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def fatal(msg: str):
    log.error("FATAL: %s", msg)
    sys.exit(1)


def api_get(path: str) -> dict | list:
    """GET from the GitHub REST API."""
    url = f"https://api.github.com/repos/{REPO}/{path.lstrip('/')}"
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"Bearer {os.environ['GITHUB_TOKEN']}",
            "Accept": "application/vnd.github.v3+json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        log.warning("api_get %s failed: %s", path, e.code)
        return {}


def api_post(path: str, data: dict) -> dict | None:
    """POST to the GitHub REST API."""
    url = f"https://api.github.com/repos/{REPO}/{path.lstrip('/')}"
    body = json.dumps(data).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Authorization": f"Bearer {os.environ['GITHUB_TOKEN']}",
            "Accept": "application/vnd.github.v3+json",
            "Content-Type": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        log.warning("api_post %s failed: %s", path, e.code)
        return None


def comment_on_issue(issue_num: int, body: str) -> None:
    """Post a comment on an issue."""
    api_post(f"issues/{issue_num}/comments", {"body": body})


def get_issue_comments(issue_num: int) -> list[dict]:
    """Get all comments on an issue."""
    data = api_get(f"issues/{issue_num}/comments")
    if isinstance(data, list):
        return data
    return []


def fetch_issue(issue_num: int) -> dict | None:
    """Fetch a single issue by number."""
    data = api_get(f"issues/{issue_num}")
    if isinstance(data, dict) and "number" in data:
        return data
    return None


def open_task_issues() -> list[dict]:
    """Fetch open issues with the 'task' label."""
    data = api_get("issues?labels=task&state=open&per_page=10")
    if isinstance(data, list):
        return data
    return []


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
# AI Coder
# ---------------------------------------------------------------------------
def coder_call(messages: list[dict], temperature: float = 0) -> str | None:
    """Call the coder AI (DeepSeek or OpenAI fallback)."""
    if DEEPSEEK_API_KEY:
        url = DEEPSEEK_URL
        key = DEEPSEEK_API_KEY
        model = DEEPSEEK_MODEL
    elif OPENAI_API_KEY:
        url = OPENAI_URL
        key = OPENAI_API_KEY
        model = OPENAI_MODEL
    else:
        log.error("No API key configured for coder (DeepSeek or OpenAI)")
        return None

    payload = {
        "model": model,
        "messages": messages,
        "temperature": temperature,
    }

    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode(),
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {key}",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=180) as resp:
            data = json.loads(resp.read().decode())
        return data.get("choices", [{}])[0].get("message", {}).get("content", "")
    except urllib.error.HTTPError as e:
        err_body = e.read().decode()[:300]
        log.warning("Coder API error %s: %s", e.code, err_body)
        return None
    except Exception as e:
        log.warning("Coder call failed: %s", e)
        return None


def extract_bash_commands(text: str) -> list[str]:
    """Extract ```bash ... ``` blocks from AI response, preserving heredocs."""
    blocks = re.findall(r"```(?:bash)?\s*\n(.*?)```", text, re.DOTALL)
    result = []
    for block in blocks:
        lines = block.strip().splitlines()
        # Filter out comment-only blocks and sudo lines
        clean = [line for line in lines if not line.strip().startswith("#")]
        if not clean:
            continue
        if any(line.strip().startswith("sudo") for line in clean):
            log.warning("Skipping block with sudo commands")
            continue
        result.append("\n".join(clean))
    return result


def get_project_context() -> str:
    """Read project README and AGENTS.md for context."""
    parts = []
    readme = AHAMKARA_CLONE / "README.md"
    if readme.exists():
        parts.append(f"# README\n{readme.read_text()[:2000]}")

    agents_md = AHAMKARA_CLONE / "docs" / "AGENTS.md"
    if agents_md.exists():
        parts.append(f"# AGENTS.md\n{agents_md.read_text()[:3000]}")

    # List source directories
    src_dirs = []
    for d in (AHAMKARA_CLONE / "game" / "src").iterdir():
        if d.is_dir():
            src_dirs.append(d.name)
    parts.append(f"# Source directories\ngame/src/: {', '.join(sorted(src_dirs))}")

    return "\n\n".join(parts)


def build_coder_prompt(issue: dict, project_context: str, fix_context: str = "") -> str:
    """Build a prompt for the coder AI."""
    body = issue.get("body", "") or ""

    fix_section = ""
    if fix_context:
        fix_section = f"""
## Previous attempt feedback

The build/tests failed on the previous attempt. Here is the review:

{fix_context}

Fix the issues described above. Do NOT re-implement working code.
"""

    return f"""You are an autonomous AI agent implementing GitHub issues for a C++ game engine called Ahamkara.

## Task

Implement issue #{issue["number"]}: {issue["title"]}

{body[:2000]}

## Project context

{project_context[:3000]}
{fix_section}
## Working directory

{AHAMKARA_CLONE}

## Rules

1. First, explore the codebase to understand the relevant files. Use `find`, `grep` to locate files.
2. Make ONLY the minimum changes needed to implement the issue.
3. Use existing code patterns and conventions.
4. Output your implementation as one or more bash code blocks (```bash ... ```) containing the actual commands to run.
5. Each bash block will be executed in order in the working directory.
6. Use `sed`, `echo >`, or write files using heredocs.
7. Do NOT include sudo commands.
8. After implementing, you may optionally run `cmake --preset debug && cmake --build build/debug -j$(nproc)` locally to verify, but the real build will happen on the server.
9. Make sure git is configured and you commit and push your changes.
10. Commit message format: "[Task #{issue["number"]}] {issue["title"]}

Co-Authored-By: Oz <oz-agent@warp.dev>"
"""


# ---------------------------------------------------------------------------
# Git worktrees
# ---------------------------------------------------------------------------
def ensure_worktree(issue_num: int, branch: str) -> Path:
    """Create a git worktree for the issue if it doesn't exist."""
    worktree_path = WORKTREE_BASE / f"ahamkara-task-{issue_num}"

    if worktree_path.exists():
        log.info("Worktree %s already exists", worktree_path)
        return worktree_path

    # Fetch latest and create worktree
    ok, _out, err = run_cmd("git fetch origin", str(AHAMKARA_CLONE))
    if not ok:
        log.warning("git fetch failed: %s", err[:200])

    ok, _out, err = run_cmd(
        f"git worktree add {worktree_path} -b {branch} origin/main",
        str(AHAMKARA_CLONE),
        timeout=30,
    )
    if ok:
        log.info("Created worktree at %s (branch %s)", worktree_path, branch)
    else:
        # Maybe branch already exists remotely — try with that
        ok2, _out2, err2 = run_cmd(
            f"git worktree add {worktree_path} {branch}",
            str(AHAMKARA_CLONE),
            timeout=30,
        )
        if not ok2:
            log.warning("Failed to create worktree: %s", err2[:200])
            log.warning("Fallback error: %s", err2[:200])

    return worktree_path


def cleanup_worktree(issue_num: int):
    """Remove a git worktree."""
    worktree_path = WORKTREE_BASE / f"ahamkara-task-{issue_num}"
    if not worktree_path.exists():
        return
    run_cmd(f"git worktree remove {worktree_path}", str(AHAMKARA_CLONE), timeout=15)
    log.info("Removed worktree %s", worktree_path)


# ---------------------------------------------------------------------------
# Gemini review parser
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Windows build on gaming PC
# ---------------------------------------------------------------------------
def run_windows_build(branch: str) -> tuple[bool, str]:
    """
    SSH into the gaming PC, pull the branch, rebuild clean, and return
    (success, log_text).
    """
    log.info("=== Windows build: pulling branch %s on %s@%s ===", branch, WINDOWS_USER, WINDOWS_HOST)

    # Step 1: git pull branch on the PC
    ssh_git = [
        "ssh",
        f"{WINDOWS_USER}@{WINDOWS_HOST}",
        f"cd {WINDOWS_PATH} && git fetch origin && git checkout {branch} && git pull",
    ]
    try:
        r = subprocess.run(ssh_git, capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            return False, f"Git checkout failed:\n{r.stderr[:1000]}"
    except subprocess.TimeoutExpired:
        return False, "SSH timeout during git checkout (60s)"
    except Exception as e:
        return False, f"SSH failed during git checkout: {e}"

    # Step 2: clean rebuild (delete build dir then reconfigure + build)
    log.info("Windows build: clean rebuild...")

    # We chain commands via cmd.exe over SSH with vcvars64.bat.
    # Build script that runs via SSH:
    build_cmds = (
        f"rmdir /s /q {WINDOWS_PATH}\\build\\debug 2>nul && "
        f'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" && '
        f'"C:\\Program Files\\CMake\\bin\\cmake.exe" -S {WINDOWS_PATH} -B {WINDOWS_PATH}\\build\\debug '
        f"-G Ninja -DCMAKE_BUILD_TYPE=Debug "
        f"-DCMAKE_TOOLCHAIN_FILE={WINDOWS_PATH}\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake "
        f"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON >nul 2>&1 && "
        f'"C:\\Program Files\\CMake\\bin\\cmake.exe" --build {WINDOWS_PATH}\\build\\debug -- -j8'
    )
    ssh_build = ["ssh", f"{WINDOWS_USER}@{WINDOWS_HOST}", f'cmd.exe /q /c ""{build_cmds}""']

    try:
        r = subprocess.run(ssh_build, capture_output=True, text=True, timeout=600)
        if r.returncode != 0:
            # Build failed — capture tail of output for error context
            err_lines = (r.stdout + r.stderr).splitlines()
            # Grab last 100 lines
            tail = "\n".join(err_lines[-100:])
            return "fail", f"Build exited with code {r.returncode}.\nLast 100 lines:\n{tail}"
        # Build succeeded
        lines = r.stdout.splitlines()
        tail = "\n".join(lines[-20:])
        return "pass", f"Build OK.\n{tail}"
    except subprocess.TimeoutExpired:
        return "fail", "Windows build timed out after 600s"
    except Exception as e:
        return "skip", f"Windows build SSH failed: {e}"


def extract_latest_gemini_review(comments: list[dict]) -> str | None:
    """Extract the most recent GEMINI_REVIEW comment body."""
    for comment in reversed(comments):
        body = comment.get("body", "") or ""
        if MARKER_GEMINI_REVIEW in body:
            # Strip the marker
            return body.replace(MARKER_GEMINI_REVIEW, "").strip()
    return None


def has_coding_done_comment(comments: list[dict]) -> bool:
    """Check if there's a CODING_DONE comment (posted by us)."""
    return any(MARKER_CODING_DONE in (c.get("body", "") or "") for c in comments)


# ---------------------------------------------------------------------------
# Issue processing
# ---------------------------------------------------------------------------
def process_issue(issue: dict) -> None:
    """Process a single issue: code, push, wait for review, loop."""
    num = issue["number"]
    title = issue["title"]
    branch = f"task/{num}"

    log.info("=== Processing issue #%s: %s ===", num, title)

    # 1. Ensure worktree
    worktree = ensure_worktree(num, branch)
    if not worktree.exists():
        log.error("Worktree not available for issue #%s", num)
        return

    project_context = get_project_context()
    fix_context = ""
    attempt = 0
    max_attempts = MAX_RETRIES

    while attempt < max_attempts:
        attempt += 1
        log.info("Coding attempt %d/%d", attempt, max_attempts)

        # 2. Build coder prompt
        prompt = build_coder_prompt(issue, project_context, fix_context)
        messages = [
            {
                "role": "system",
                "content": "You are an expert C++ game engine developer. Generate bash commands to implement the given issue. Output code changes inside ```bash blocks.",
            },
            {"role": "user", "content": prompt},
        ]

        # 3. Call AI coder
        log.info("Calling coder AI...")
        response = coder_call(messages, temperature=0.2)
        if not response:
            log.error("Coder AI returned no response")
            comment_on_issue(num, "❌ Coder AI failed to generate a response.")
            return

        # Save response for debugging
        log_path = LOG_DIR / f"coder-issue-{num}-attempt-{attempt}.md"
        log_path.write_text(response)

        # 4. Extract and execute bash commands
        commands = extract_bash_commands(response)
        if not commands:
            log.warning("No bash commands extracted from coder response")
            comment_on_issue(num, "⚠️ No bash commands generated. Trying again...")
            continue

        log.info("Executing %d command blocks in %s", len(commands), worktree)

        all_ok = True
        for i, script in enumerate(commands):
            log.info("  Running block %d/%d (%d lines)...", i + 1, len(commands), len(script.splitlines()))
            ok, _out, err = run_cmd(script, str(worktree), timeout=120)
            if not ok:
                log.warning("  Block %d failed: %s", i + 1, err[:200])
                all_ok = False

        if not all_ok:
            log.warning("Some commands failed, but continuing to push anyway")

        # 5. Commit and push
        ok, _out, err = run_cmd("git add -A", str(worktree))
        # Check for changes
        ok2, _out2, _err2 = run_cmd("git diff --cached --quiet", str(worktree))
        if ok2:
            log.info("No changes to commit")
            comment_on_issue(num, f"⚠️ Attempt {attempt}: No code changes were generated.")
            fix_context = "The coder produced no changes. Please implement the issue."
            continue

        commit_msg = f"[Task #{num}] {title}\n\nCo-Authored-By: Oz <oz-agent@warp.dev>"
        ok3, _out3, err3 = run_cmd(f"git commit -m '{commit_msg}'", str(worktree))
        if not ok3:
            log.warning("Commit failed: %s", err3[:200])

        ok4, _out4, err4 = run_cmd(f"git push -u origin {branch}", str(worktree), timeout=60)
        if not ok4:
            log.warning("Push failed: %s", err4[:300])
            comment_on_issue(num, f"❌ Push failed: {err4[:200]}")
            return

        log.info("Pushed branch %s", branch)

        # 6. Post CODING_DONE marker
        comment_on_issue(num, f"{MARKER_CODING_DONE} Attempt {attempt} pushed to `{branch}`.")

        # 6.5 Build on Windows (gaming PC) — gate before server review
        log.info("=== Windows build gate for branch %s ===", branch)
        win_ok, win_log = run_windows_build(branch)

        if not win_ok:
            log.warning("Windows build FAILED for attempt %d", attempt)
            failure_body = (
                f"{MARKER_WINDOWS_BUILD_FAIL}\n"
                f"### Windows Build Failed (attempt {attempt})\n\n"
                f"```\n{win_log[-3000:]}\n```"
            )
            comment_on_issue(num, failure_body)
            fix_context = (
                f"The Windows build failed for this attempt. Error output:\n\n"
                f"```\n{win_log[-1500:]}\n```\n\n"
                f"Fix the issues above and try again."
            )
            continue  # Loop back to coder

        log.info("Windows build PASSED for attempt %d", attempt)

        # 7. Wait for server to build and post Gemini review
        log.info("Windows build OK. Waiting for server to build and Gemini review...")

        # Poll for new GEMINI_REVIEW comments
        wait_rounds = 30  # 30 * 30s = 15 min max wait
        for w in range(wait_rounds):
            time.sleep(30)
            comments = get_issue_comments(num)
            review = extract_latest_gemini_review(comments)

            if review:
                log.info("Received Gemini review (attempt %d)", attempt)

                # Check if the review says it's a PASS
                if "REVIEW: PASS" in review.upper():
                    log.info("Gemini passed! Build is good.")
                    return  # Done!

                # Check if the review says max attempts reached / needs attention
                if "needs manual attention" in review.lower() or "needs attention" in review.lower():
                    log.warning("Gemini flagged issue #%s as needing manual attention", num)
                    return

                # Store fix context and loop
                fix_context = review
                break  # Exit wait loop, go to next coding attempt

            # Also check if there's a "Build pass" comment without GEMINI_REVIEW (edge case)
            all_comments_text = " ".join(c.get("body", "") or "" for c in comments)
            if "Build and tests passed" in all_comments_text or "Build OK" in all_comments_text:
                log.info("Build passed (detected from comments)")
                return

            if w % 5 == 0:
                log.info("  Still waiting for server review... (waited %d min)", (w * 30) // 60)

        if not extract_latest_gemini_review(get_issue_comments(num)):
            log.warning("Timed out waiting for server review on attempt %d", attempt)
            comment_on_issue(num, f"⚠️ Timeout waiting for build review on attempt {attempt}.")

    # Max attempts reached
    log.warning("Issue #%s exceeded max attempts", num)
    comment_on_issue(num, f"❌ Exceeded {MAX_RETRIES} coding attempts. Needs manual intervention.")


# ---------------------------------------------------------------------------
# State management
# ---------------------------------------------------------------------------
def load_state() -> dict:
    """Load active issues state."""
    if STATE_FILE.exists():
        try:
            return json.loads(STATE_FILE.read_text())
        except (json.JSONDecodeError, OSError):
            return {}
    return {}


def save_state(state: dict) -> None:
    """Write active issues state."""
    STATE_FILE.write_text(json.dumps(state, indent=2))


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main() -> None:
    # Validate
    token = os.getenv("GITHUB_TOKEN") or os.getenv("GH_TOKEN")
    if not token:
        fatal("GITHUB_TOKEN not set")
    os.environ["GITHUB_TOKEN"] = token

    if not DEEPSEEK_API_KEY and not OPENAI_API_KEY:
        fatal("Neither DEEPSEEK_API_KEY nor OPENAI_ATHENA_KEY is set")

    log.info("Oz Runner starting")
    log.info("Repo: %s", REPO)
    log.info("Clone: %s", AHAMKARA_CLONE)
    log.info(
        "Coder: %s (%s)",
        "DeepSeek" if DEEPSEEK_API_KEY else "OpenAI",
        DEEPSEEK_MODEL if DEEPSEEK_API_KEY else OPENAI_MODEL,
    )
    log.info("Poll interval: %ds", POLL_INTERVAL)

    # Handle --process flag
    if len(sys.argv) > 1 and sys.argv[1] == "--process":
        issue_num = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        if not issue_num:
            fatal("Usage: oz-runner.py --process <issue_num>")
        issue = fetch_issue(issue_num)
        if not issue:
            fatal(f"Issue #{issue_num} not found")
        state = load_state()
        state[str(issue_num)] = {"started": datetime.now(UTC).isoformat()}
        save_state(state)
        process_issue(issue)
        state = load_state()
        state.pop(str(issue_num), None)
        save_state(state)
        log.info("Issue #%s done", issue_num)
        return

    run_once = "--once" in sys.argv
    log.info("Starting poll loop%s...", " (single run)" if run_once else "")

    def poll_cycle():
        """One round of checking for new issues."""
        nonlocal state
        try:
            issues = open_task_issues()
            log.info("Found %d open task issues", len(issues))
            state = load_state()

            for issue in issues:
                num = issue["number"]
                title = issue["title"]

                # Skip if already active
                if str(num) in state:
                    started = state.get(str(num), {}).get("started", "")
                    if started:
                        started_dt = datetime.fromisoformat(started)
                        elapsed = (datetime.now(UTC) - started_dt).total_seconds()
                        if elapsed > 7200:
                            log.warning("Issue #%s has been active for %.0f min, may be stale", num, elapsed / 60)
                    continue

                # Check comments to see if we already started this issue
                comments = get_issue_comments(num)
                if has_coding_done_comment(comments):
                    has_review = any(MARKER_GEMINI_REVIEW in (c.get("body", "") or "") for c in comments)
                    if has_review:
                        latest_review = extract_latest_gemini_review(comments)
                        if latest_review and "REVIEW: PASS" in latest_review.upper():
                            log.info("Issue #%s already passed review, skipping", num)
                            continue
                        log.info("Issue #%s has pending review, re-processing", num)

                log.info("New issue detected: #%s - %s", num, title)

                state[str(num)] = {"started": datetime.now(UTC).isoformat()}
                save_state(state)

                process_issue(issue)

                state = load_state()
                state.pop(str(num), None)
                save_state(state)

            active = load_state()
            if active:
                log.info("Active issues: %s", ", ".join(active.keys()))

        except Exception as e:
            log.error("Poll loop error: %s", e)
            import traceback

            log.error(traceback.format_exc())

    if run_once:
        poll_cycle()
    else:
        while True:
            poll_cycle()
            time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()
