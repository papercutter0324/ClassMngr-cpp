import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


if len(sys.argv) != 3:
    raise SystemExit(
        "usage: test_next_resource_install_prefix_guard.py <cmake> <guard-script>"
    )

CMAKE = sys.argv[1]
GUARD_SCRIPT = Path(sys.argv[2]).resolve()
sys.argv = [sys.argv[0]]


class ResourceInstallPrefixGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory(
            prefix="classmngr-resource-install-prefix-"
        )
        cls.root = Path(cls._temporary_directory.name).resolve()

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def configure_case(self, name, bindir, datadir):
        case_root = self.root / name
        source = case_root / "source"
        build = case_root / "build"
        prefix = case_root / "configured-prefix"
        source.mkdir(parents=True)
        (source / "payload.txt").write_text("resource payload\n", encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            "\n".join(
                (
                    "cmake_minimum_required(VERSION 3.25)",
                    "project(ClassMngrResourceInstallPrefixFixture NONE)",
                    f'include("{GUARD_SCRIPT.as_posix()}")',
                    "classmngr_next_add_install_prefix_guard(",
                    "    ClassMngrNextResources",
                    '    "${CMAKE_INSTALL_BINDIR}"',
                    '    "${CMAKE_INSTALL_DATADIR}"',
                    ")",
                    "install(FILES \"${CMAKE_CURRENT_SOURCE_DIR}/payload.txt\"",
                    '    DESTINATION "${CMAKE_INSTALL_DATADIR}/ClassMngr"',
                    "    COMPONENT ClassMngrNextResources",
                    "    EXCLUDE_FROM_ALL",
                    ")",
                    "",
                )
            ),
            encoding="utf-8",
        )

        completed = subprocess.run(
            (
                CMAKE,
                "-S",
                str(source),
                "-B",
                str(build),
                f"-DCMAKE_INSTALL_PREFIX={prefix}",
                f"-DCMAKE_INSTALL_BINDIR={bindir}",
                f"-DCMAKE_INSTALL_DATADIR={datadir}",
            ),
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(
            completed.returncode,
            0,
            completed.stdout + completed.stderr,
        )
        return source, build, prefix

    def install_component(self, build, prefix=None, environment=None):
        command = [
            CMAKE,
            "--install",
            str(build),
            "--component",
            "ClassMngrNextResources",
        ]
        if prefix is not None:
            command.extend(("--prefix", str(prefix)))
        return subprocess.run(
            command,
            capture_output=True,
            text=True,
            env=environment,
            check=False,
        )

    def assert_guard_rejected_prefix_override(
        self,
        build,
        override_prefix,
        blocked_payload,
    ):
        completed = self.install_component(build, override_prefix)
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn(
            "mixed absolute and relative",
            completed.stdout + completed.stderr,
        )
        self.assertIn(
            "configured CMAKE_INSTALL_PREFIX",
            completed.stdout + completed.stderr,
        )
        self.assertIn(
            "use DESTDIR for staging",
            completed.stdout + completed.stderr,
        )
        self.assertFalse(blocked_payload.exists())

    def test_relative_directories_allow_install_prefix_override(self):
        _, build, _ = self.configure_case("relative", "bin", "share")
        override_prefix = self.root / "relative" / "override-prefix"

        completed = self.install_component(build, override_prefix)

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        installed_payload = override_prefix / "share" / "ClassMngr" / "payload.txt"
        self.assertTrue(installed_payload.is_file())

    def test_both_absolute_directories_are_not_guarded(self):
        case_root = self.root / "both-absolute"
        configured_prefix = case_root / "configured-prefix"
        bindir = configured_prefix / "bin"
        datadir = configured_prefix / "share"
        _, build, _ = self.configure_case(
            "both-absolute",
            str(bindir),
            str(datadir),
        )
        override_prefix = case_root / "override-prefix"

        completed = self.install_component(build, override_prefix)

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertTrue((datadir / "ClassMngr" / "payload.txt").is_file())

    def test_absolute_bindir_and_relative_datadir_reject_changed_prefix(self):
        case_root = self.root / "absolute-bindir"
        configured_prefix = case_root / "configured-prefix"
        bindir = configured_prefix / "bin"
        _, build, default_prefix = self.configure_case(
            "absolute-bindir",
            str(bindir),
            "share",
        )

        configured_install = self.install_component(build)
        self.assertEqual(
            configured_install.returncode,
            0,
            configured_install.stdout + configured_install.stderr,
        )
        self.assertTrue(
            (default_prefix / "share" / "ClassMngr" / "payload.txt").is_file()
        )
        self.assert_guard_rejected_prefix_override(
            build,
            case_root / "override-prefix",
            case_root / "override-prefix" / "share" / "ClassMngr" / "payload.txt",
        )

    def test_relative_bindir_and_absolute_datadir_reject_changed_prefix(self):
        case_root = self.root / "absolute-datadir"
        configured_prefix = case_root / "configured-prefix"
        datadir = configured_prefix / "share"
        _, build, default_prefix = self.configure_case(
            "absolute-datadir",
            "bin",
            str(datadir),
        )

        configured_install = self.install_component(build)
        self.assertEqual(
            configured_install.returncode,
            0,
            configured_install.stdout + configured_install.stderr,
        )
        self.assertTrue((datadir / "ClassMngr" / "payload.txt").is_file())
        (datadir / "ClassMngr" / "payload.txt").unlink()
        self.assert_guard_rejected_prefix_override(
            build,
            case_root / "override-prefix",
            datadir / "ClassMngr" / "payload.txt",
        )

    @unittest.skipIf(os.name == "nt", "DESTDIR staging semantics are Unix-specific")
    def test_destdir_staging_keeps_configured_prefix_for_mixed_directories(self):
        case_root = self.root / "destdir"
        configured_prefix = case_root / "configured-prefix"
        bindir = configured_prefix / "bin"
        _, build, default_prefix = self.configure_case(
            "destdir",
            str(bindir),
            "share",
        )
        destdir = case_root / "stage-root"
        environment = os.environ.copy()
        environment["DESTDIR"] = str(destdir)

        completed = self.install_component(build, environment=environment)

        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        installed_payload = (
            destdir
            / default_prefix.relative_to(default_prefix.anchor)
            / "share"
            / "ClassMngr"
            / "payload.txt"
        )
        self.assertTrue(installed_payload.is_file())


if __name__ == "__main__":
    unittest.main(verbosity=2)
