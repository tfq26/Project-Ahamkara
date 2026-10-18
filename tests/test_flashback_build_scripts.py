#!/usr/bin/env python3
"""Exercise playable-build packaging and promotion without a real build."""

import os
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGE_SCRIPT = ROOT / "scripts/package_flashback.sh"
PROMOTE_SCRIPT = ROOT / "scripts/promote_flashback_build.sh"


class FlashbackBuildScriptsTests(unittest.TestCase):
    def test_package_contains_flashback_assets_and_checksum(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            project = temporary / "project"
            build = project / "build/release"
            (build / "samples/flashback").mkdir(parents=True)
            (build / "tools").mkdir(parents=True)
            (project / "scripts").mkdir()
            (project / "assets").mkdir()
            (project / "client/config").mkdir(parents=True)
            (project / "CMakeLists.txt").write_text("project(Ahamkara VERSION 1.2.3 LANGUAGES CXX)\n")
            (project / "assets/manifest.assets").write_text("# fixture\n")
            (project / "client/config/ahamkara.cfg").write_text("fixture=true\n")
            (build / "samples/flashback/flashback").write_text("#!/usr/bin/env sh\nprintf 'Flashback fixture\\n'\n")
            (build / "samples/flashback/flashback").chmod(0o755)
            importer = build / "tools/ahamkara_asset_importer"
            importer.write_text(
                "#!/usr/bin/env sh\n"
                "mkdir -p assets/compiled\n"
                "printf 'fixture\\n' > assets/compiled/asset_registry.tsv\n"
            )
            importer.chmod(0o755)
            subprocess.run(["git", "init", "-q", str(project)], check=True)
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(project),
                    "-c",
                    "user.name=Test",
                    "-c",
                    "user.email=test@example.invalid",
                    "commit",
                    "--allow-empty",
                    "-qm",
                    "fixture",
                ],
                check=True,
            )
            package_script = project / "scripts/package_flashback.sh"
            package_script.write_bytes(PACKAGE_SCRIPT.read_bytes())
            package_script.chmod(0o755)
            output = temporary / "artifacts"
            environment = os.environ | {"AHAMKARA_PROJECT_ROOT": str(project)}

            subprocess.run(
                [str(package_script), str(build), str(output), "linux-x86_64"],
                check=True,
                env=environment,
                capture_output=True,
                text=True,
            )

            archives = list(output.glob("Flashback-linux-x86_64-*.tar.gz"))
            self.assertEqual(len(archives), 1)
            self.assertTrue(Path(f"{archives[0]}.sha256").is_file())
            with tarfile.open(archives[0], "r:gz") as archive:
                members = set(archive.getnames())
            self.assertIn("Flashback/bin/flashback", members)
            self.assertIn("Flashback/assets/compiled/asset_registry.tsv", members)
            self.assertIn("Flashback/client/config/ahamkara.cfg", members)

    def test_promotion_keeps_previous_build_as_backup(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            install_root = temporary / "install"
            first = self._candidate(temporary, "1" * 40)
            second = self._candidate(temporary, "2" * 40)

            for archive in (first, second):
                subprocess.run(
                    [str(PROMOTE_SCRIPT), str(archive), str(install_root)],
                    check=True,
                    capture_output=True,
                    text=True,
                )

            self.assertEqual(os.readlink(install_root / "current"), f"versions/{'2' * 40}")
            self.assertEqual(os.readlink(install_root / "backup"), f"versions/{'1' * 40}")
            self.assertTrue((install_root / "current/bin/flashback").is_file())
            self.assertTrue((install_root / "current/assets/compiled/asset_registry.tsv").is_file())

    def test_promotion_rejects_parent_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary = Path(temporary_directory)
            archive_path = temporary / "unsafe.tar.gz"
            with tarfile.open(archive_path, "w:gz") as archive:
                data = b"unsafe"
                member = tarfile.TarInfo("../outside")
                member.size = len(data)
                archive.addfile(member, __import__("io").BytesIO(data))

            result = subprocess.run(
                [str(PROMOTE_SCRIPT), str(archive_path), str(temporary / "install")],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((temporary / "outside").exists())

    @staticmethod
    def _candidate(directory: Path, commit: str) -> Path:
        archive_path = directory / f"Flashback-linux-x86_64-{commit}.tar.gz"
        app = directory / f"app-{commit}"
        (app / "bin").mkdir(parents=True)
        (app / "assets/compiled").mkdir(parents=True)
        executable = app / "bin/flashback"
        executable.write_text("#!/usr/bin/env sh\nexit 0\n")
        executable.chmod(0o755)
        (app / "assets/compiled/asset_registry.tsv").write_text("fixture\n")
        (app / "build-info.txt").write_text(f"commit={commit}\n")
        with tarfile.open(archive_path, "w:gz") as archive:
            archive.add(app, arcname="Flashback")
        return archive_path


if __name__ == "__main__":
    unittest.main()
