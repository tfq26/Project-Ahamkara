#!/usr/bin/env python3
"""Run Ahamkara's change-aware lint suite and emit durable reports."""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import time
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = ROOT / "build" / "lint"
CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
CPP_SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
TEXT_SUFFIXES = CPP_SUFFIXES | {
    ".cmake",
    ".css",
    ".html",
    ".ini",
    ".js",
    ".json",
    ".md",
    ".py",
    ".sh",
    ".toml",
    ".txt",
    ".yaml",
    ".yml",
}
EXCLUDED_PREFIXES = (
    "build/",
    ".git/",
    ".venv-lint/",
    "engine/render/glad/",
)
EXCLUDED_CPP_PREFIXES = (
    "engine/ui/imgui",
    "engine/ui/imstb",
)


@dataclasses.dataclass
class Selection:
    paths: list[str]
    diff_text: str = ""
    full_paths: set[str] = dataclasses.field(default_factory=set)


@dataclasses.dataclass
class CheckResult:
    check_id: str
    status: str
    files: int
    report: str
    summary: str
    command: str = ""
    exit_code: int | None = None
    duration_seconds: float = 0.0
    excerpt: str = ""


def run_git(args: Sequence[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=ROOT,
        check=check,
        capture_output=True,
        text=True,
    )


def split_nul(value: str) -> list[str]:
    return [item for item in value.split("\0") if item]


def normalize_path(path: str) -> str:
    normalized = path.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def is_excluded(path: str) -> bool:
    normalized = normalize_path(path)
    return normalized.startswith(EXCLUDED_PREFIXES) or "__pycache__" in Path(normalized).parts


def is_first_party_cpp(path: str) -> bool:
    normalized = normalize_path(path)
    return (
        Path(normalized).suffix.lower() in CPP_SUFFIXES
        and not is_excluded(normalized)
        and not normalized.startswith(EXCLUDED_CPP_PREFIXES)
    )


def is_cmake(path: str) -> bool:
    candidate = Path(path)
    return candidate.name == "CMakeLists.txt" or candidate.suffix.lower() == ".cmake"


def is_workflow(path: str) -> bool:
    normalized = normalize_path(path)
    return normalized.startswith(".github/workflows/") and Path(normalized).suffix.lower() in {".yaml", ".yml"}


def is_python(path: str) -> bool:
    normalized = normalize_path(path)
    return Path(normalized).suffix.lower() == ".py" and normalized != ".cmake-format.py"


def existing_repo_paths(paths: Iterable[str]) -> list[str]:
    selected: set[str] = set()
    for raw_path in paths:
        normalized = normalize_path(raw_path)
        candidate = ROOT / normalized
        if normalized and candidate.is_file() and not is_excluded(normalized):
            selected.add(normalized)
    return sorted(selected)


def tracked_paths() -> list[str]:
    result = run_git(["ls-files", "-z"])
    return existing_repo_paths(split_nul(result.stdout))


def diff_for_range(range_spec: str) -> tuple[list[str], str]:
    names = run_git(["diff", "--name-only", "--diff-filter=ACMR", "-z", range_spec]).stdout
    patch = run_git(["diff", "--no-color", "--relative", "-U0", range_spec]).stdout
    return split_nul(names), patch


def select_paths(args: argparse.Namespace) -> Selection:
    if args.paths:
        paths = existing_repo_paths(args.paths)
        missing = sorted({normalize_path(path) for path in args.paths} - set(paths))
        if missing:
            raise ValueError(f"Paths do not exist or are excluded: {', '.join(missing)}")
        return Selection(paths=paths, full_paths=set(paths))

    if args.all:
        paths = tracked_paths()
        return Selection(paths=paths, full_paths=set(paths))

    collected: list[str] = []
    patches: list[str] = []
    full_paths: set[str] = set()

    if args.base_ref:
        committed_paths, committed_patch = diff_for_range(f"{args.base_ref}...HEAD")
        collected.extend(committed_paths)
        patches.append(committed_patch)

    if not args.committed_only:
        working_paths, working_patch = diff_for_range("HEAD")
        collected.extend(working_paths)
        patches.append(working_patch)
        untracked = split_nul(run_git(["ls-files", "--others", "--exclude-standard", "-z"]).stdout)
        collected.extend(untracked)
        full_paths.update(existing_repo_paths(untracked))

    if not args.base_ref and args.committed_only:
        raise ValueError("--committed-only requires --base-ref")

    return Selection(
        paths=existing_repo_paths(collected),
        diff_text="\n".join(patch for patch in patches if patch),
        full_paths=full_paths,
    )


def write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def execute(
    check_id: str,
    command: Sequence[str],
    paths: Sequence[str],
    report_dir: Path,
    report_name: str,
    *,
    stdin: str | None = None,
    env: dict[str, str] | None = None,
) -> CheckResult:
    report_path = report_dir / report_name
    stderr_path = report_path.with_suffix(report_path.suffix + ".stderr.txt")
    started = time.monotonic()
    display_command = shlex.join(str(part) for part in command)

    try:
        completed = subprocess.run(
            list(command),
            cwd=ROOT,
            input=stdin,
            capture_output=True,
            text=True,
            env=env,
            check=False,
        )
        stdout = completed.stdout
        stderr = completed.stderr
        exit_code = completed.returncode
    except FileNotFoundError as error:
        stdout = ""
        stderr = f"Required tool is not installed: {error.filename}\nRun ./scripts/setup-lint.sh and retry.\n"
        exit_code = 127

    write_text(report_path, stdout)
    write_text(stderr_path, stderr)
    combined = "\n".join(part.strip() for part in (stdout, stderr) if part.strip())
    return CheckResult(
        check_id=check_id,
        status="pass" if exit_code == 0 else "fail",
        files=len(paths),
        report=str(report_path.relative_to(report_dir)),
        summary="clean" if exit_code == 0 else f"exit {exit_code}",
        command=display_command,
        exit_code=exit_code,
        duration_seconds=round(time.monotonic() - started, 3),
        excerpt=combined[:4000],
    )


def skipped(check_id: str, report_name: str, summary: str) -> CheckResult:
    return CheckResult(check_id, "skipped", 0, report_name, summary)


def run_hygiene(paths: Sequence[str], report_dir: Path) -> CheckResult:
    candidates = [path for path in paths if Path(path).suffix.lower() in TEXT_SUFFIXES or is_cmake(path)]
    issues: list[dict[str, Any]] = []
    started = time.monotonic()

    for path in candidates:
        data = (ROOT / path).read_bytes()
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as error:
            issues.append({"path": path, "code": "invalid-utf8", "detail": str(error)})
            continue

        if "\r\n" in text or "\r" in text:
            issues.append({"path": path, "code": "non-lf-line-ending"})
        if data and not data.endswith(b"\n"):
            issues.append({"path": path, "code": "missing-final-newline"})
        for line_number, line in enumerate(text.splitlines(), start=1):
            if line.endswith((" ", "\t")):
                issues.append({"path": path, "line": line_number, "code": "trailing-whitespace"})
        if Path(path).suffix.lower() == ".json":
            try:
                json.loads(text)
            except json.JSONDecodeError as error:
                issues.append({"path": path, "line": error.lineno, "code": "invalid-json", "detail": error.msg})

    report_name = "hygiene.json"
    write_text(report_dir / report_name, json.dumps(issues, indent=2, sort_keys=True) + "\n")
    excerpt = "\n".join(f"{item['path']}:{item.get('line', 0)}: {item['code']}" for item in issues[:40])
    return CheckResult(
        check_id="hygiene",
        status="fail" if issues else "pass",
        files=len(candidates),
        report=report_name,
        summary=f"{len(issues)} issue(s)" if issues else "clean",
        exit_code=1 if issues else 0,
        duration_seconds=round(time.monotonic() - started, 3),
        excerpt=excerpt,
    )


def run_clang_format(selection: Selection, report_dir: Path, fix: bool) -> CheckResult:
    paths = [path for path in selection.paths if is_first_party_cpp(path)]
    if not paths:
        return skipped("clang-format", "clang-format.diff", "no selected C/C++ files")

    outputs: list[str] = []
    errors: list[str] = []
    exit_code = 0
    started = time.monotonic()
    env = os.environ.copy()
    command_fragments: list[str] = []
    diff_paths = sorted(set(paths) - selection.full_paths)

    if selection.diff_text and diff_paths:
        diff_tool = shutil.which("clang-format-diff.py") or "clang-format-diff.py"
        binary = shutil.which("clang-format") or "clang-format"
        regex = "^(?:" + "|".join(re.escape(path) for path in diff_paths) + ")$"
        command = [diff_tool, "-p1", "-style=file", "-binary", binary, "-regex", regex]
        if fix:
            command.append("-i")
        command_fragments.append(shlex.join(command))
        completed = subprocess.run(
            command,
            cwd=ROOT,
            input=selection.diff_text,
            capture_output=True,
            text=True,
            env=env,
            check=False,
        )
        outputs.append(completed.stdout)
        errors.append(completed.stderr)
        if completed.returncode != 0 or (completed.stdout.strip() and not fix):
            exit_code = completed.returncode or 1

    full_paths = sorted(set(paths) & selection.full_paths)
    if full_paths:
        formatter = shutil.which("clang-format") or "clang-format"
        if fix:
            fix_command = [formatter, "-i", "--style=file", *full_paths]
            command_fragments.append(shlex.join(fix_command))
            completed = subprocess.run(fix_command, cwd=ROOT, capture_output=True, text=True, check=False)
            outputs.append(completed.stdout)
            errors.append(completed.stderr)
            if completed.returncode != 0:
                exit_code = completed.returncode
        verify_command = [formatter, "--dry-run", "--Werror", "--style=file", *full_paths]
        command_fragments.append(shlex.join(verify_command))
        completed = subprocess.run(verify_command, cwd=ROOT, capture_output=True, text=True, check=False)
        outputs.append(completed.stdout)
        errors.append(completed.stderr)
        if completed.returncode != 0:
            exit_code = completed.returncode

    stdout = "\n".join(part for part in outputs if part)
    stderr = "\n".join(part for part in errors if part)
    write_text(report_dir / "clang-format.diff", stdout)
    write_text(report_dir / "clang-format.stderr.txt", stderr)
    combined = "\n".join(part.strip() for part in (stdout, stderr) if part.strip())
    return CheckResult(
        check_id="clang-format",
        status="pass" if exit_code == 0 else "fail",
        files=len(paths),
        report="clang-format.diff",
        summary="clean" if exit_code == 0 else "formatting changes required",
        command=" && ".join(command_fragments),
        exit_code=exit_code,
        duration_seconds=round(time.monotonic() - started, 3),
        excerpt=combined[:4000],
    )


def compilation_database_files(compile_db: Path) -> set[Path]:
    entries = json.loads((compile_db / "compile_commands.json").read_text(encoding="utf-8"))
    return {Path(entry["file"]).resolve() for entry in entries}


def run_clang_tidy(paths: Sequence[str], report_dir: Path, compile_db: Path) -> CheckResult:
    sources = [path for path in paths if is_first_party_cpp(path) and Path(path).suffix.lower() in CPP_SOURCE_SUFFIXES]
    if not sources:
        return skipped("clang-tidy", "clang-tidy.txt", "no selected C/C++ source files")

    database_file = compile_db / "compile_commands.json"
    if not database_file.is_file():
        message = f"Compilation database not found at {database_file}. Run cmake --preset debug first.\n"
        write_text(report_dir / "clang-tidy.txt", message)
        return CheckResult(
            "clang-tidy",
            "fail",
            len(sources),
            "clang-tidy.txt",
            "missing compilation database",
            exit_code=2,
            excerpt=message,
        )

    compiled = compilation_database_files(compile_db)
    analyzable = [path for path in sources if (ROOT / path).resolve() in compiled]
    missing = sorted(set(sources) - set(analyzable))
    if missing:
        message = "No compile command for:\n" + "\n".join(missing) + "\n"
        write_text(report_dir / "clang-tidy.txt", message)
        return CheckResult(
            "clang-tidy",
            "fail",
            len(sources),
            "clang-tidy.txt",
            "source files missing from compilation database",
            exit_code=2,
            excerpt=message,
        )

    command = ["clang-tidy", "-p", str(compile_db), "--quiet", *analyzable]
    if platform.system() == "Darwin" and shutil.which("xcrun"):
        sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False).stdout.strip()
        if sdk:
            command.append(f"--extra-arg=-isysroot{sdk}")
    return execute("clang-tidy", command, analyzable, report_dir, "clang-tidy.txt")


def run_ruff(paths: Sequence[str], report_dir: Path, fix: bool) -> list[CheckResult]:
    python_paths = [path for path in paths if is_python(path)]
    if not python_paths:
        return [
            skipped("ruff-check", "ruff.json", "no selected Python files"),
            skipped("ruff-format", "ruff-format.diff", "no selected Python files"),
        ]

    if fix:
        subprocess.run(["ruff", "check", "--fix", *python_paths], cwd=ROOT, check=False)
        subprocess.run(["ruff", "format", *python_paths], cwd=ROOT, check=False)

    return [
        execute(
            "ruff-check",
            ["ruff", "check", "--output-format=json", *python_paths],
            python_paths,
            report_dir,
            "ruff.json",
        ),
        execute(
            "ruff-format",
            ["ruff", "format", "--check", "--diff", *python_paths],
            python_paths,
            report_dir,
            "ruff-format.diff",
        ),
    ]


def run_shellcheck(paths: Sequence[str], report_dir: Path) -> CheckResult:
    shell_paths = [path for path in paths if Path(path).suffix.lower() == ".sh"]
    if not shell_paths:
        return skipped("shellcheck", "shellcheck.json", "no selected shell files")
    return execute(
        "shellcheck",
        ["shellcheck", "--format=json", *shell_paths],
        shell_paths,
        report_dir,
        "shellcheck.json",
    )


def run_actionlint(paths: Sequence[str], report_dir: Path) -> CheckResult:
    if not any(is_workflow(path) for path in paths):
        return skipped("actionlint", "actionlint.json", "no selected workflow files")
    workflows = sorted(path for path in tracked_paths() if is_workflow(path))
    return execute(
        "actionlint",
        ["actionlint", "-format", "{{json .}}", *workflows],
        workflows,
        report_dir,
        "actionlint.json",
    )


def run_cmake_lint(paths: Sequence[str], report_dir: Path) -> CheckResult:
    cmake_paths = [path for path in paths if is_cmake(path)]
    if not cmake_paths:
        return skipped("cmake-lint", "cmake-lint.txt", "no selected CMake files")
    return execute(
        "cmake-lint",
        ["cmake-lint", "--suppress-decorations", *cmake_paths],
        cmake_paths,
        report_dir,
        "cmake-lint.txt",
    )


def markdown_summary(metadata: dict[str, Any], results: Sequence[CheckResult]) -> str:
    passed = all(result.status != "fail" for result in results)
    lines = [
        "# Ahamkara lint report",
        "",
        f"**Overall:** {'PASS' if passed else 'FAIL'}",
        "",
        f"Scope: `{metadata['scope']}` · Selected files: {metadata['selected_files']}",
        "",
        "| Check | Status | Files | Result | Report |",
        "| --- | --- | ---: | --- | --- |",
    ]
    for result in results:
        lines.append(
            f"| `{result.check_id}` | {result.status.upper()} | {result.files} | {result.summary} | `{result.report}` |"
        )

    failures = [result for result in results if result.status == "fail" and result.excerpt]
    if failures:
        lines.extend(["", "## Failure excerpts", ""])
        for result in failures:
            safe_excerpt = result.excerpt.replace("```", "` ` `")
            lines.extend(
                [
                    f"<details><summary>{result.check_id}</summary>",
                    "",
                    "```text",
                    safe_excerpt,
                    "```",
                    "",
                    "</details>",
                    "",
                ]
            )
    return "\n".join(lines).rstrip() + "\n"


def write_summary(
    report_dir: Path,
    metadata: dict[str, Any],
    results: Sequence[CheckResult],
) -> bool:
    passed = all(result.status != "fail" for result in results)
    payload = {
        "schema_version": 1,
        **metadata,
        "passed": passed,
        "checks": [dataclasses.asdict(result) for result in results],
    }
    write_text(report_dir / "summary.json", json.dumps(payload, indent=2, sort_keys=True) + "\n")
    write_text(report_dir / "summary.md", markdown_summary(metadata, results))
    return passed


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--all", action="store_true", help="lint every tracked maintained file")
    scope.add_argument("--paths", nargs="+", help="lint explicit repository-relative paths")
    parser.add_argument("--base-ref", help="lint changes from this Git ref through HEAD")
    parser.add_argument(
        "--committed-only",
        action="store_true",
        help="exclude working-tree and untracked files (CI use)",
    )
    parser.add_argument("--compile-db", default="build/debug", help="directory containing compile_commands.json")
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR), help="output directory for lint reports")
    parser.add_argument("--fix", action="store_true", help="apply safe C/C++ and Python formatting fixes")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    report_dir = Path(args.report_dir)
    if not report_dir.is_absolute():
        report_dir = ROOT / report_dir
    report_dir.mkdir(parents=True, exist_ok=True)

    try:
        selection = select_paths(args)
    except (subprocess.CalledProcessError, ValueError) as error:
        message = f"Unable to select lint scope: {error}\n"
        write_text(report_dir / "selection.txt", message)
        metadata = {
            "generated_at": dt.datetime.now(dt.UTC).isoformat(),
            "scope": "invalid",
            "base_ref": args.base_ref,
            "selected_files": 0,
        }
        result = CheckResult("selection", "fail", 0, "selection.txt", "invalid scope", exit_code=2, excerpt=message)
        write_summary(report_dir, metadata, [result])
        print(message, file=sys.stderr, end="")
        return 2

    if args.paths:
        scope_name = "paths"
    elif args.all:
        scope_name = "all"
    elif args.base_ref:
        scope_name = "base-ref"
    else:
        scope_name = "working-tree"

    metadata = {
        "generated_at": dt.datetime.now(dt.UTC).isoformat(),
        "scope": scope_name,
        "base_ref": args.base_ref,
        "selected_files": len(selection.paths),
        "compile_db": args.compile_db,
    }
    compile_db = Path(args.compile_db)
    if not compile_db.is_absolute():
        compile_db = ROOT / compile_db

    results: list[CheckResult] = [run_hygiene(selection.paths, report_dir)]
    results.append(run_clang_format(selection, report_dir, args.fix))
    results.append(run_clang_tidy(selection.paths, report_dir, compile_db))
    results.extend(run_ruff(selection.paths, report_dir, args.fix))
    results.append(run_shellcheck(selection.paths, report_dir))
    results.append(run_actionlint(selection.paths, report_dir))
    results.append(run_cmake_lint(selection.paths, report_dir))

    passed = write_summary(report_dir, metadata, results)
    print((report_dir / "summary.md").read_text(encoding="utf-8"), end="")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
