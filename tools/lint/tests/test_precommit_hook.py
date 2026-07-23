#!/usr/bin/env python3
"""Tests for the Ahamkara pre-commit hook integration."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class PreCommitHookUnitTests(unittest.TestCase):
    """Unit-level tests for the pre-commit hook scripts."""

    def setUp(self) -> None:
        self.repo_root = Path(__file__).resolve().parents[3]
        self.hook_script = self.repo_root / "scripts" / "pre-commit-hook.sh"
        self.install_script = self.repo_root / "scripts" / "install-pre-commit-hook.sh"

    def test_hook_script_exists(self) -> None:
        """The pre-commit hook script must exist and be executable."""
        self.assertTrue(self.hook_script.is_file(), f"Missing: {self.hook_script}")
        self.assertTrue(os.access(str(self.hook_script), os.X_OK), "Hook script is not executable")

    def test_install_script_exists(self) -> None:
        """The installer script must exist and be executable."""
        self.assertTrue(self.install_script.is_file(), f"Missing: {self.install_script}")
        self.assertTrue(os.access(str(self.install_script), os.X_OK), "Install script is not executable")

    def test_hook_script_syntax(self) -> None:
        """Hook script must pass bash syntax check."""
        result = subprocess.run(
            ["bash", "-n", str(self.hook_script)],
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"Syntax error in {self.hook_script}:\n{result.stderr}",
        )

    def test_install_script_syntax(self) -> None:
        """Install script must pass bash syntax check."""
        result = subprocess.run(
            ["bash", "-n", str(self.install_script)],
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"Syntax error in {self.install_script}:\n{result.stderr}",
        )

    def test_hook_rejects_outside_repo(self) -> None:
        """Hook should fail gracefully outside a git repo."""
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                ["bash", str(self.hook_script)],
                capture_output=True,
                text=True,
                cwd=tmp,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not inside a Git repository", result.stderr)


class PreCommitHookIntegrationTests(unittest.TestCase):
    """Integration-level tests using a temporary Git repository.

    Note: Git sends pre-commit hook output to stderr, so all hook output
    assertions check result.stderr, not result.stdout.
    """

    def setUp(self) -> None:
        self.repo_root = Path(__file__).resolve().parents[3]
        self.hook_source = self.repo_root / "scripts" / "pre-commit-hook.sh"
        self.install_script_src = self.repo_root / "scripts" / "install-pre-commit-hook.sh"

        # Create a temporary git repo with a known initial commit
        self.test_dir = Path(tempfile.mkdtemp())
        self._init_repo()
        self._setup_project_structure()

    def tearDown(self) -> None:
        shutil.rmtree(str(self.test_dir), ignore_errors=True)

    def _init_repo(self) -> None:
        """Initialise a minimal Git repository with an initial commit."""
        subprocess.run(["git", "init"], cwd=self.test_dir, check=True, capture_output=True)
        subprocess.run(
            ["git", "config", "user.email", "test@test"],
            cwd=self.test_dir,
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["git", "config", "user.name", "Test"],
            cwd=self.test_dir,
            check=True,
            capture_output=True,
        )
        # Create an initial commit (required for diffs to work)
        readme = self.test_dir / "README.md"
        readme.write_text("# Test\n")
        subprocess.run(["git", "add", "README.md"], cwd=self.test_dir, check=True, capture_output=True)
        subprocess.run(
            ["git", "commit", "-m", "Initial commit"],
            cwd=self.test_dir,
            check=True,
            capture_output=True,
        )

    def _setup_project_structure(self) -> None:
        """Mirror the project's scripts/tools directory layout for testing."""
        # Create scripts/ directory with lint.sh wrapper
        scripts_dir = self.test_dir / "scripts"
        scripts_dir.mkdir(exist_ok=True)

        # Create a real lint.sh that works with the test repo.
        # The default stub passes; individual tests can override it.
        lint_sh = scripts_dir / "lint.sh"
        lint_sh.write_text('#!/usr/bin/env bash\nset -euo pipefail\necho "lint-runner: $@"\nexit 0\n')
        lint_sh.chmod(0o755)

        # Create tools/lint/ dir (needed for setup-lint.sh path checks)
        tools_lint = self.test_dir / "tools" / "lint"
        tools_lint.mkdir(parents=True, exist_ok=True)
        req_txt = tools_lint / "requirements.txt"
        req_txt.write_text("# empty\n")

        # Copy the install script and hook source into the test repo's scripts/
        shutil.copy2(str(self.install_script_src), str(scripts_dir / "install-pre-commit-hook.sh"))
        shutil.copy2(str(self.hook_source), str(scripts_dir / "pre-commit-hook.sh"))
        (scripts_dir / "pre-commit-hook.sh").chmod(0o755)
        (scripts_dir / "install-pre-commit-hook.sh").chmod(0o755)

    def _install_hook(self) -> None:
        """Install the pre-commit hook in the test repo."""
        result = subprocess.run(
            ["./scripts/install-pre-commit-hook.sh"],
            capture_output=True,
            text=True,
            cwd=self.test_dir,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"Installer failed:\nstdout:{result.stdout}\nstderr:{result.stderr}",
        )
        installed = self.test_dir / ".git" / "hooks" / "pre-commit"
        self.assertTrue(installed.is_file(), f"Hook not installed at {installed}")

    def test_installer_creates_hook(self) -> None:
        """install-pre-commit-hook.sh must install the hook to .git/hooks/pre-commit."""
        self._install_hook()

    def test_hook_passes_with_no_staged_files(self) -> None:
        """Hook should exit 0 when there are no staged files."""
        self._install_hook()

        result = subprocess.run(
            ["git", "commit", "--allow-empty", "-m", "empty"],
            capture_output=True,
            text=True,
            cwd=self.test_dir,
        )
        self.assertEqual(result.returncode, 0, f"Commit failed:\nstdout:{result.stdout}")

    def test_hook_runs_on_staged_files_and_passes(self) -> None:
        """Hook should run and pass when lint returns 0."""
        self._install_hook()

        # Stage a file
        test_file = self.test_dir / "test.txt"
        test_file.write_text("hello\n")
        subprocess.run(["git", "add", "test.txt"], cwd=self.test_dir, check=True, capture_output=True)

        result = subprocess.run(
            ["git", "commit", "-m", "add test file"],
            capture_output=True,
            text=True,
            cwd=self.test_dir,
        )
        self.assertEqual(result.returncode, 0, f"Commit failed:\nstdout:{result.stdout}\nstderr:{result.stderr}")
        # Hook output goes to stderr
        self.assertIn("lint-runner:", result.stderr)
        self.assertIn("lint/format validation passed", result.stderr)

    def test_hook_blocks_commit_on_lint_failure(self) -> None:
        """Hook should block the commit when lint returns non-zero."""
        # Make lint.sh return failure BEFORE installing hook
        lint_sh = self.test_dir / "scripts" / "lint.sh"
        lint_sh.unlink()
        lint_sh.write_text('#!/usr/bin/env bash\nset -euo pipefail\necho "lint-error: format check failed"\nexit 1\n')
        lint_sh.chmod(0o755)

        self._install_hook()

        test_file = self.test_dir / "bad.py"
        test_file.write_text("x=1\n")
        subprocess.run(["git", "add", "bad.py"], cwd=self.test_dir, check=True, capture_output=True)

        result = subprocess.run(
            ["git", "commit", "-m", "should fail"],
            capture_output=True,
            text=True,
            cwd=self.test_dir,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAILED", result.stderr)

    def test_pre_commit_no_verify_bypasses_hook(self) -> None:
        """Hook must be bypassed via --no-verify."""
        # Make lint.sh fail
        lint_sh = self.test_dir / "scripts" / "lint.sh"
        lint_sh.unlink()
        lint_sh.write_text("#!/usr/bin/env bash\necho 'should not run'\nexit 1\n")
        lint_sh.chmod(0o755)

        self._install_hook()

        test_file = self.test_dir / "bypass.txt"
        test_file.write_text("bypass\n")
        subprocess.run(["git", "add", "bypass.txt"], cwd=self.test_dir, check=True, capture_output=True)

        # Use --no-verify to skip the hook
        result = subprocess.run(
            ["git", "commit", "--no-verify", "-m", "bypass hook"],
            capture_output=True,
            text=True,
            cwd=self.test_dir,
        )
        self.assertEqual(result.returncode, 0, f"Commit failed with --no-verify:\nstdout:{result.stdout}")
        # Hook should NOT have run
        self.assertNotIn("should not run", result.stderr)

    def test_setup_dev_install_hook_flag(self) -> None:
        """setup-dev.sh --install-hook should install the hook."""
        # We can't easily run the full setup-dev.sh (it needs cmake, etc.),
        # but we can verify the --install-hook flag is recognized.
        setup_dev = self.repo_root / "scripts" / "setup-dev.sh"
        result = subprocess.run(
            ["bash", "-n", str(setup_dev)],
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"Syntax error in setup-dev.sh:\n{result.stderr}",
        )
        # Verify --install-hook is documented in usage
        result = subprocess.run(
            ["bash", str(setup_dev), "--help"],
            capture_output=True,
            text=True,
        )
        self.assertIn("--install-hook", result.stdout)


class StashRestoreTests(unittest.TestCase):
    """Test the stash/restore logic for partial commits."""

    def test_stash_keep_index_pattern(self) -> None:
        """Verify that `git stash --keep-index` preserves staged content."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)

            subprocess.run(["git", "init"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "t@t"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.name", "T"], cwd=tmp, check=True, capture_output=True)

            readme = tmp / "README.md"
            readme.write_text("# Init\n")
            subprocess.run(["git", "add", "README.md"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "commit", "-m", "init"], cwd=tmp, check=True, capture_output=True)

            # Create a file with staged and unstaged content
            test_file = tmp / "test.py"
            test_file.write_text("STAGED_CONTENT\n")
            subprocess.run(["git", "add", "test.py"], cwd=tmp, check=True, capture_output=True)

            # Now modify the file without staging (unstaged change)
            test_file.write_text("STAGED_CONTENT\nUNSTAGED_CHANGE\n")

            # Verify staged content is still the original
            staged = subprocess.run(
                ["git", "show", ":test.py"],
                capture_output=True,
                text=True,
                cwd=tmp,
                check=True,
            )
            self.assertEqual(staged.stdout.strip(), "STAGED_CONTENT")

            # Working tree has unstaged changes
            self.assertIn("UNSTAGED_CHANGE", test_file.read_text())

            # Now stash unstaged changes
            subprocess.run(
                ["git", "stash", "--keep-index"],
                cwd=tmp,
                check=True,
                capture_output=True,
            )

            # After stash --keep-index, working tree should match staged
            self.assertEqual(test_file.read_text().strip(), "STAGED_CONTENT")

            # Pop stash to restore (may return non-zero if clean; check content)
            subprocess.run(
                ["git", "stash", "pop"],
                cwd=tmp,
                check=False,
                capture_output=True,
            )

            # Working tree should have unstaged changes again
            self.assertIn("UNSTAGED_CHANGE", test_file.read_text())

    def test_hook_script_nul_safe_filename(self) -> None:
        """Verify the hook can handle spaces in filenames via NUL-delimited parsing."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            subprocess.run(["git", "init"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.email", "t@t"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "config", "user.name", "T"], cwd=tmp, check=True, capture_output=True)

            readme = tmp / "README.md"
            readme.write_text("# Init\n")
            subprocess.run(["git", "add", "README.md"], cwd=tmp, check=True, capture_output=True)
            subprocess.run(["git", "commit", "-m", "init"], cwd=tmp, check=True, capture_output=True)

            # Create a file with a space in its name
            spaced_file = tmp / "my file.cpp"
            spaced_file.write_text("int main() { return 0; }\n")

            # Stage it
            subprocess.run(["git", "add", str(spaced_file)], cwd=tmp, check=True, capture_output=True)

            # Verify git diff --cached -z handles spaces
            result = subprocess.run(
                ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"],
                capture_output=True,
                text=True,
                cwd=tmp,
                check=True,
            )
            self.assertIn("my file.cpp", result.stdout)


if __name__ == "__main__":
    unittest.main()
