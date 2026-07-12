#!/usr/bin/env python3
"""
Ahamkara Agent Runner — replaces Kronn for autonomous issue processing.

Two-pass workflow:
  1. DeepSeek (coder): reads issue, generates implementation commands
  2. Gemini (reviewer): reviews errors/failures and guides fixes

Polls GitHub for open issues with label 'task' in tfq26/Project-Ahamkara,
and notifies via Jupiter on start/completion/failure.

Config: /home/taufe/agent-runner/config.env (sourced by run.sh)
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

# ── Config ───────────────────────────────────────────────────────────────────
REPO = "tfq26/Project-Ahamkara"
WORKTREE_BASE = Path("/home/taufe/Projects")
AHAMKARA_CLONE = Path("/home/taufe/Projects/ahamkara")
LOG_DIR = Path("/home/taufe/agent-runner/logs")
PROCESSED_LOG = Path("/home/taufe/agent-runner/processed_issues.json")
JUPITER_URL = "http://localhost:9001/notify"
JUPITER_CHANNEL = "ahamkara"
MAX_RETRIES = 5

# Build commands
TEST_CMD = "cd build/debug && ctest --output-on-failure -j$(nproc)"
BUILD_CMD = "cmake --build build/debug -j$(nproc)"
CMAKE_CONFIGURE_CMD = "cmake --preset debug"

# ── Init from config.env (sourced by run.sh) ────────────────────────────────
GITHUB_TOKEN = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
GEMINI_API_KEY = os.environ.get("GEMINI_API_KEY")
GEMINI_MODEL = os.environ.get("GEMINI_MODEL", "gemini-2.5-flash")
DEEPSEEK_API_KEY = os.environ.get("DEEPSEEK_API_KEY")
DEEPSEEK_MODEL = os.environ.get("DEEPSEEK_MODEL", "deepseek-chat")
DEEPSEEK_URL = os.environ.get("DEEPSEEK_URL", "https://api.deepseek.com/v1/chat/completions")

if not GITHUB_TOKEN:
    print("FATAL: GITHUB_TOKEN not set", flush=True)
    sys.exit(1)
if not GEMINI_API_KEY:
    print("FATAL: GEMINI_API_KEY not set", flush=True)
    sys.exit(1)
if not DEEPSEEK_API_KEY:
    print("FATAL: DEEPSEEK_API_KEY not set", flush=True)
    sys.exit(1)

GEMINI_API_URL = f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={GEMINI_API_KEY}"
LOG_DIR.mkdir(parents=True, exist_ok=True)


# ── API calls ────────────────────────────────────────────────────────────────


def deepseek_call(messages, temperature=0):
    """Call DeepSeek via OpenAI-compatible API. Returns response text or None."""
    payload = json.dumps({
        "model": DEEPSEEK_MODEL,
        "messages": messages,
        "temperature": temperature,
    }).encode()
    req = Request(
        DEEPSEEK_URL,
        data=payload,
        headers={
            "Authorization": f"Bearer {DEEPSEEK_API_KEY}",
            "Content-Type": "application/json",
        },
    )
    try:
        with urlopen(req, timeout=180) as resp:
            data = json.loads(resp.read())
        return data["choices"][0]["message"]["content"]
    except HTTPError as e:
        err = e.read().decode()
        print(f"  [ERROR] DeepSeek API error {e.code}: {err[:500]}", flush=True)
        return None
    except Exception as e:
        print(f"  [ERROR] DeepSeek call failed: {e}", flush=True)
        return None


def gemini_call(prompt, system_instruction=None):
    """Call Gemini via REST API. Returns response text or None."""
    payload = {"contents": [{"parts": [{"text": prompt}]}]}
    if system_instruction:
        payload["systemInstruction"] = {"parts": [{"text": system_instruction}]}
    body = json.dumps(payload).encode()
    req = Request(
        GEMINI_API_URL,
        data=body,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urlopen(req, timeout=120) as resp:
            data = json.loads(resp.read())
        candidates = data.get("candidates", [])
        if not candidates:
            return None
        return candidates[0]["content"]["parts"][0].get("text", "")
    except HTTPError as e:
        err = e.read().decode()
        print(f"  [ERROR] Gemini API error {e.code}: {err[:300]}", flush=True)
        return None
    except Exception as e:
        print(f"  [ERROR] Gemini call failed: {e}", flush=True)
        return None


# ── GitHub helpers ───────────────────────────────────────────────────────────


def _gh_headers():
    return {
        "Authorization": f"Bearer {GITHUB_TOKEN}",
        "Accept": "application/vnd.github.v3+json",
        "User-Agent": "ahamkara-agent-runner",
    }


def api_get(path):
    req = Request(f"https://api.github.com/repos/{REPO}/{path}", headers=_gh_headers())
    with urlopen(req) as resp:
        return json.loads(resp.read())


def api_post(path, data):
    body = json.dumps(data).encode()
    req = Request(
        f"https://api.github.com/repos/{REPO}/{path}",
        data=body, headers={**_gh_headers(), "Content-Type": "application/json"},
        method="POST",
    )
    with urlopen(req) as resp:
        return json.loads(resp.read())


def api_patch(path, data):
    body = json.dumps(data).encode()
    req = Request(
        f"https://api.github.com/repos/{REPO}/{path}",
        data=body, headers={**_gh_headers(), "Content-Type": "application/json"},
        method="PATCH",
    )
    with urlopen(req) as resp:
        return json.loads(resp.read())


def jupiter_notify(title, message, priority="info"):
    try:
        body = json.dumps({
            "title": title, "message": message[:1800],
            "priority": priority, "channel_id": JUPITER_CHANNEL,
        }).encode()
        req = Request(JUPITER_URL, data=body, headers={"Content-Type": "application/json"})
        with urlopen(req, timeout=10) as resp:
            return json.loads(resp.read())
    except Exception as e:
        print(f"  [WARN] Jupiter notify failed: {e}", flush=True)
        return None


def gh(*args, capture_output=True, check=True, **kwargs):
    cmd = ["gh", "--repo", REPO] + list(args)
    return subprocess.run(cmd, capture_output=capture_output, text=True, check=check, **kwargs)


# ── Helpers ──────────────────────────────────────────────────────────────────


def comment_on_issue(issue_num, body):
    api_post(f"issues/{issue_num}/comments", {"body": body})


def load_processed():
    if PROCESSED_LOG.exists():
        return set(json.loads(PROCESSED_LOG.read_text()).get("processed", []))
    return set()


def save_processed(issue_num):
    processed = load_processed()
    processed.add(issue_num)
    PROCESSED_LOG.write_text(json.dumps({"processed": sorted(processed)}, indent=2))
    print(f"  [INFO] Marked issue #{issue_num} as processed", flush=True)


def run_cmd(cmd, cwd=None, timeout=120, env=None):
    cwd = str(cwd) if cwd else str(AHAMKARA_CLONE)
    try:
        result = subprocess.run(
            cmd, shell=True, cwd=cwd, capture_output=True, text=True,
            timeout=timeout, env={**os.environ, **(env or {})},
        )
        return result.returncode == 0, result.stdout.strip(), result.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "", f"Command timed out after {timeout}s"
    except Exception as e:
        return False, "", str(e)


def extract_bash_commands(text):
    """Extract ```bash blocks and return each as a single shell script.
    This preserves heredocs and multi-line commands."""
    scripts = []
    blocks = re.findall(r'```bash\s*\n(.*?)```', text, re.DOTALL)
    for block in blocks:
        block = block.strip()
        if not block:
            continue
        # Filter out blocks that are only comment/explanation lines
        lines = [l for l in block.split("\n") if l.strip()]
        has_real_command = False
        for line in lines:
            stripped = line.strip()
            if stripped.startswith("sudo"):
                continue  # Skip sudo entirely
            if not stripped.startswith("//") and not stripped.startswith("/*") and not stripped.startswith("*"):
                if stripped and stripped != "#":
                    has_real_command = True
        if has_real_command:
            # Filter out leading comment lines but keep heredoc content
            scripts.append(block)
    return scripts


# ── Prompt builders ──────────────────────────────────────────────────────────


def get_project_context():
    parts = []
    readme = AHAMKARA_CLONE / "README.md"
    if readme.exists():
        parts.append(f"--- README.md ---\n{readme.read_text()[:2000]}")
    agents = AHAMKARA_CLONE / "AGENTS.md"
    if agents.exists():
        text = agents.read_text()
        for k, v in [
            ("{{STACK_SUMMARY}}", "C++20 game engine with CMake + Ninja"),
            ("{{TEST_CMD}}", "cd build && ctest --output-on-failure -j$(nproc)"),
            ("{{DO_NOT_1}}", "Do not make changes outside the scope of the issue"),
            ("{{DO_NOT_2}}", "Do not modify dependencies or third-party code"),
            ("{{PROJECT_NAME}}", "Ahamkara"),
            ("{{PROJECT_LANGUAGE}}", "English"),
        ]:
            text = text.replace(k, v)
        parts.append(f"--- AGENTS.md ---\n{text[:2000]}")
    engine = AHAMKARA_CLONE / "engine"
    if engine.exists():
        subs = [d.name for d in engine.iterdir() if d.is_dir()]
        parts.append(f"--- engine/ subdirs ---\n{', '.join(subs)}")
    tests = AHAMKARA_CLONE / "tests"
    if tests.exists():
        parts.append(f"--- tests/ ---\n{', '.join(f.name for f in tests.iterdir())}")
    return "\n\n".join(parts)


def build_coder_prompt(issue, project_context):
    return f"""You are an expert C++20 game engine developer implementing a task on the Ahamkara project.

## Issue #{issue['number']}: {issue['title']}

{issue['body']}

## Project Context

{project_context[:4000]}

## Repository Layout

- `engine/` — Engine libraries (core, network, platform, render, runtime)
- `client/` — Playable client
- `server/` — Dedicated server
- `game/` — Game types & logic
- `tests/` — Test targets

## Rules

1. DO NOT edit auto-generated files.
2. DO NOT skip tests — every code change must include tests.
3. Read relevant source files first before making changes.
4. Fix only what is described in the issue. No unrelated changes.
5. DO NOT use sudo or try to install packages.
6. Working directory is /home/taufe/Projects/ahamkara

## Output Format

Your entire response must consist of ONLY concrete bash commands that I will execute.
Wrap all commands in a single ```bash block at the end.

Write files using heredocs:
```bash
cat > path/to/file << 'EOF'
...file content...
EOF
```

Read files with: cat path/to/file

Build: cmake --build build -j$(nproc)
Test: cd build && ctest --output-on-failure -j$(nproc)

CRITICAL RULES FOR THE BASH BLOCK:
- Every line inside ```bash must be a valid shell command that can be executed by /bin/sh
- Do NOT include C++ source code comments (// or /*) inside the bash block
- Do NOT include any text that is not a valid shell command
- Do NOT output sudo commands
- All file modifications must use heredocs (cat > file << 'EOF')
- Outside the bash block you may explain your approach

Start by reading the relevant source files.
"""


def build_fix_prompt(error_context):
    return f"""The previous implementation had errors. Analyze the errors below and provide FIXED commands.

Errors:
{error_context[:4000]}

Output ONLY corrected bash commands in a ```bash block.
Every line inside ```bash must be a valid shell command. No // comments, no pseudo-code.
"""


def build_reviewer_prompt(issue, coder_output, build_log, test_log):
    return f"""You are a code reviewer for the Ahamkara C++20 game engine project.

## Issue #{issue['number']}: {issue['title']}

{issue['body']}

## Changes Proposed by the Coder

{coder_output[:3000]}

## Build Output

{build_log[-2000:]}

## Test Output

{test_log[-2000:]}

## Your Task

Analyze the build and test output above. Determine:
1. Are there any errors or test failures? If so, summarize them.
2. What is the root cause of each error?
3. What needs to change to fix it?

If there are NO errors and NO test failures, respond with: REVIEW: PASS

If there ARE errors, describe each issue briefly (1-2 sentences each) and explain what needs to change.
Be specific about which files and what to change.

IMPORTANT: Do NOT suggest installing system packages (apt, yum, brew, etc.). The build environment is complete.
All fixes should be code changes, not system configuration changes.
"""


# ── Worktree management ──────────────────────────────────────────────────────


def ensure_worktree(issue_num, branch_name):
    worktree_path = WORKTREE_BASE / f"ahamkara-issue-{issue_num}"
    if worktree_path.exists():
        print(f"  [INFO] Worktree exists, resetting...", flush=True)
        run_cmd("git fetch origin", cwd=worktree_path, timeout=30)
        run_cmd("git reset --hard origin/main", cwd=worktree_path, timeout=30)
        run_cmd(f"git checkout -b {branch_name} 2>/dev/null || git checkout {branch_name}", cwd=worktree_path, timeout=15)
    else:
        ok, _, err = run_cmd(f"git worktree add {worktree_path} origin/main", cwd=AHAMKARA_CLONE, timeout=30)
        if not ok:
            print(f"  [ERROR] Worktree creation failed: {err[:300]}", flush=True)
            return None
        run_cmd(f"git checkout -b {branch_name}", cwd=worktree_path, timeout=15)
    return worktree_path


def cleanup_worktree(issue_num):
    wp = WORKTREE_BASE / f"ahamkara-issue-{issue_num}"
    if wp.exists():
        run_cmd(f"git worktree remove {wp}", cwd=AHAMKARA_CLONE, timeout=15)


DOCKER_BUILD_IMAGE = "ahamkara-build"


def docker_run(worktree_path, cmd, timeout=300):
    """Run a command inside the ahamkara-build Docker container with worktree mounted."""
    # Match the host user's UID/GID so build artifacts are owned by the right user
    docker_cmd = (
        f"docker run --rm --user $(id -u):$(id -g) "
        f"-e HOME=/tmp "
        f"-v {worktree_path}:/workspace "
        f"-w /workspace {DOCKER_BUILD_IMAGE} "
        f"bash -c {shlex.quote(cmd)}"
    )
    return run_cmd(docker_cmd, timeout=timeout)


def build_project(worktree_path):
    print("  [BUILD] Configuring CMake (inside Docker)...", flush=True)
    ok, _, err = docker_run(worktree_path, CMAKE_CONFIGURE_CMD, timeout=120)
    if not ok:
        return False, f"CMake configure failed:\n{err[:1500]}"
    print("  [BUILD] Building (inside Docker)...", flush=True)
    ok, out, err = docker_run(worktree_path, BUILD_CMD, timeout=300)
    if not ok:
        return False, f"Build failed:\n{err[-2500:]}"
    return True, f"Build succeeded:\n{out[-500:]}"


def test_project(worktree_path):
    print("  [TEST] Running tests (inside Docker)...", flush=True)
    ok, out, err = docker_run(worktree_path, TEST_CMD, timeout=300)
    if not ok:
        return False, f"Tests failed:\n{out[-2000:]}\n{err[:1000]}"
    return True, f"Tests passed:\n{out[:1000]}"


def generate_branch_name(issue):
    slug = re.sub(r'[^a-z0-9]+', '-', issue["title"].lower()).strip('-')[:60]
    return f"task/issue-{issue['number']}-{slug}"


# ── Main processing ──────────────────────────────────────────────────────────


def process_issue(issue):
    num = issue["number"]
    title = issue["title"]
    branch = generate_branch_name(issue)
    print(f"\n{'='*60}", flush=True)
    print(f"Processing issue #{num}: {title}", flush=True)
    print(f"Branch: {branch}", flush=True)
    print(f"Coder: DeepSeek ({DEEPSEEK_MODEL})", flush=True)
    print(f"Reviewer: Gemini ({GEMINI_MODEL})", flush=True)

    # Step 1: Comment + Notify
    comment_on_issue(num, f"🤖 Agent Runner starting work on this issue.\nBranch: `{branch}`\nCoder: DeepSeek\nReviewer: Gemini")
    jupiter_notify(
        f"🎯 Starting Issue #{num}",
        f"Agent Runner starting:\n**{title}**\nBranch: `{branch}`\nCoder: DeepSeek\nReviewer: Gemini",
        priority="info",
    )

    # Step 2: Create worktree
    worktree = ensure_worktree(num, branch)
    if not worktree:
        comment_on_issue(num, "❌ Agent Runner: Failed to create worktree.")
        jupiter_notify(f"❌ Issue #{num} Failed", f"Worktree failed: {title}", priority="critical")
        return
    print(f"  [INFO] Worktree at {worktree}", flush=True)

    # Step 3: Gather context
    project_context = get_project_context()
    coder_prompt = build_coder_prompt({"number": num, "title": title, "body": issue.get("body", "")}, project_context)

    # Step 4: Implementation loop
    build_log = ""
    test_log = ""
    implementation_log = []
    success = False

    for attempt in range(1, MAX_RETRIES + 1):
        print(f"\n  [AI] Attempt {attempt}/{MAX_RETRIES} — DeepSeek coding...", flush=True)

        # ── CODER: DeepSeek generates implementation ──
        if attempt == 1:
            ds_messages = [
                {"role": "system", "content": "You are an expert C++20 game engine developer. Generate ONLY valid bash shell commands to implement the task. Every line in your ```bash block must be executable by /bin/sh."},
                {"role": "user", "content": coder_prompt},
            ]
        else:
            fix_prompt = build_fix_prompt(implementation_log[-1])
            ds_messages = [
                {"role": "system", "content": "You are an expert C++20 game engine developer. Fix the errors by generating corrected bash commands. Only output valid shell commands."},
                {"role": "user", "content": fix_prompt},
            ]

        response = deepseek_call(ds_messages)
        if not response:
            print("  [ERROR] DeepSeek returned no response", flush=True)
            implementation_log.append(f"Attempt {attempt}: DeepSeek no response")
            continue

        # Log response
        resp_log = LOG_DIR / f"ds-issue-{num}-attempt-{attempt}.md"
        resp_log.write_text(response)
        print(f"  [INFO] DeepSeek response saved ({len(response)} chars)", flush=True)

        # Extract scripts (each ```bash block is a single multi-line script)
        scripts = extract_bash_commands(response)
        if not scripts:
            print("  [WARN] No valid bash scripts found", flush=True)
            implementation_log.append(f"Attempt {attempt}: No valid bash scripts")
            continue

        # Execute each script block as a whole (preserves heredocs)
        all_ok = True
        for i, script in enumerate(scripts):
            preview = script[:100].replace("\n", " ").strip()
            print(f"  [EXEC] Script {i+1}: {preview}...", flush=True)
            # Write script to temp file and execute it
            script_path = LOG_DIR / f"script-{num}-attempt-{attempt}-{i}.sh"
            script_path.write_text(script)
            ok, out, err = run_cmd(f"bash {script_path}", cwd=worktree, timeout=180)
            if ok:
                summary = out[-200:] if out else "OK"
                print(f"    [OK] {summary[:100]}", flush=True)
            else:
                err_snip = err[-400:] if err else out[-400:]
                print(f"    [FAIL] {err_snip[:150]}", flush=True)
                all_ok = False
                implementation_log.append(f"Script {i+1} failed:\n{err_snip}")
                break

        if not all_ok:
            continue

        # Step 5: Build
        build_ok, build_msg = build_project(worktree)
        build_log = build_msg
        print(f"  [BUILD] {'OK' if build_ok else 'FAIL'}", flush=True)

        if not build_ok:
            implementation_log.append(build_msg)

            # ── REVIEWER: Gemini reviews the build failure ──
            print("  [REVIEW] Gemini analyzing build failure...", flush=True)
            review_prompt = build_reviewer_prompt(
                {"number": num, "title": title, "body": issue.get("body", "")},
                response, build_msg, "",
            )
            review = gemini_call(review_prompt)
            if review:
                review_log = LOG_DIR / f"review-issue-{num}-attempt-{attempt}.md"
                review_log.write_text(review)
                print(f"  [REVIEW] Saved review ({len(review)} chars)", flush=True)
                if "REVIEW: PASS" in review:
                    success = True
                    break
                implementation_log.append(f"Gemini review:\n{review[:2000]}")
            continue

        # Step 6: Test
        test_ok, test_msg = test_project(worktree)
        test_log = test_msg
        print(f"  [TEST] {'OK' if test_ok else 'FAIL'}", flush=True)

        if not test_ok:
            implementation_log.append(test_msg)

            # ── REVIEWER: Gemini reviews test failure ──
            print("  [REVIEW] Gemini analyzing test failure...", flush=True)
            review_prompt = build_reviewer_prompt(
                {"number": num, "title": title, "body": issue.get("body", "")},
                response, build_log, test_msg,
            )
            review = gemini_call(review_prompt)
            if review:
                review_log = LOG_DIR / f"review-issue-{num}-attempt-{attempt}.md"
                review_log.write_text(review)
                print(f"  [REVIEW] Saved review ({len(review)} chars)", flush=True)
                if "REVIEW: PASS" in review:
                    success = True
                    break
                implementation_log.append(f"Gemini review:\n{review[:2000]}")
            continue

        # Both build and test passed
        success = True
        break

    # ── Handle result ──
    if not success:
        print(f"  [FAIL] All {MAX_RETRIES} attempts exhausted", flush=True)
        last_errors = "\n---\n".join(implementation_log[-3:])
        comment_on_issue(
            num,
            f"🤖 Agent Runner: ❌ Failed after {MAX_RETRIES} attempts.\n"
            f"Last errors:\n```\n{last_errors[:2000]}\n```\n"
            f"See `{LOG_DIR}` for logs.",
        )
        jupiter_notify(
            f"❌ Issue #{num} Failed",
            f"**{title}**\n{MAX_RETRIES} attempts exhausted.",
            priority="critical",
        )
        cleanup_worktree(num)
        save_processed(num)
        return  # ❌ Do NOT close issue on failure

    # ── Success: Commit, push, PR, close ──
    print("  [GIT] Committing...", flush=True)
    run_cmd("git add -A", cwd=worktree, timeout=30)
    ok, _, err = run_cmd(
        f'git diff --cached --quiet || git commit -m "Implement {title}\n\nIssue: #{num}\n\nCo-Authored-By: Oz <oz-agent@warp.dev>"',
        cwd=worktree, timeout=30,
    )
    has_changes = True
    if not ok and "nothing to commit" in err.lower():
        has_changes = False
        print("  [INFO] No changes to commit", flush=True)
    elif not ok:
        print(f"  [WARN] Commit: {err[:200]}", flush=True)

    pr_url = ""
    if has_changes:
        print("  [GIT] Pushing...", flush=True)
        ok, _, err = run_cmd(f"git push -u origin {branch}", cwd=worktree, timeout=60)
        if not ok:
            print(f"  [ERROR] Push failed: {err[:300]}", flush=True)
            comment_on_issue(num, f"🤖 Agent Runner: ⚠️ Changes implemented but push failed.\nBranch: `{branch}`")
            jupiter_notify(f"⚠️ Issue #{num} Partial", f"Push failed for: {title}", priority="warning")
            cleanup_worktree(num)
            save_processed(num)
            return

        # Create PR
        existing = gh("pr", "list", "--head", branch, "--json", "url", capture_output=True, check=False)
        if existing.returncode == 0 and existing.stdout.strip():
            existing_data = json.loads(existing.stdout)
            if existing_data:
                pr_url = existing_data[0].get("url", "")
        if not pr_url:
            result = gh("pr", "create", "--title", f"[Task #{num}] {title}",
                        "--body", f"## Summary\n\nImplements issue #{num}: **{title}**\n\n---\n_🤖 Ahamkara Agent Runner_",
                        capture_output=True, check=False)
            pr_url = result.stdout.strip() if result.returncode == 0 else ""

        # Close issue
        api_patch(f"issues/{num}", {"state": "closed"})
        print(f"  [INFO] Issue #{num} closed", flush=True)

    # Notify completion
    link = pr_url if pr_url else f"branch `{branch}`"
    comment_text = f"✅ Issue implemented!\n- Branch: `{branch}`\n"
    if pr_url:
        comment_text += f"- PR: {pr_url}\n"
    comment_on_issue(num, comment_text)
    jupiter_notify(f"✅ Issue #{num} Completed", f"**{title}**\n{link}", priority="success")
    cleanup_worktree(num)
    save_processed(num)


def main():
    parser = argparse.ArgumentParser(description="Ahamkara Agent Runner")
    parser.add_argument("--process", type=int, help="Process a specific issue by number")
    parser.add_argument("--dry-run", action="store_true", help="List issues without processing")
    args = parser.parse_args()

    if not AHAMKARA_CLONE.exists():
        print(f"FATAL: Ahamkara clone not found at {AHAMKARA_CLONE}", flush=True)
        sys.exit(1)

    print(f"[{datetime.now(timezone.utc).isoformat()}] Agent Runner starting", flush=True)
    print(f"  Repo: {REPO}", flush=True)
    print(f"  Clone: {AHAMKARA_CLONE}", flush=True)
    print(f"  Coder: DeepSeek ({DEEPSEEK_MODEL})", flush=True)
    print(f"  Reviewer: Gemini ({GEMINI_MODEL})", flush=True)

    run_cmd("git fetch origin", cwd=AHAMKARA_CLONE, timeout=60)

    if args.process:
        try:
            issue = api_get(f"issues/{args.process}")
            process_issue(issue)
        except Exception as e:
            print(f"ERROR: {e}", flush=True)
            traceback.print_exc()
        return

    try:
        issues = api_get("issues?labels=task&state=open&per_page=10")
    except Exception as e:
        print(f"ERROR fetching issues: {e}", flush=True)
        sys.exit(1)

    if args.dry_run:
        print(f"\nOpen task issues ({len(issues)}):")
        for i in issues:
            print(f"  #{i['number']}: {i['title']}")
        return

    processed = load_processed()
    new_issues = [i for i in issues if i["number"] not in processed]

    if not new_issues:
        print(f"  No new issues (processed: {len(processed)})", flush=True)
        return

    print(f"  Found {len(new_issues)} new issue(s)", flush=True)
    for issue in new_issues:
        try:
            process_issue(issue)
        except Exception as e:
            print(f"ERROR issue #{issue['number']}: {e}", flush=True)
            traceback.print_exc()
            jupiter_notify(f"❌ Issue #{issue['number']} Error", f"Unexpected error", priority="critical")
            continue

    print(f"\n[{datetime.now(timezone.utc).isoformat()}] Agent Runner finished", flush=True)


if __name__ == "__main__":
    main()
