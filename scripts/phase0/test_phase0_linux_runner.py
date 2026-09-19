#!/usr/bin/env python3
"""Contract, plan-safety, and Linux platform checks for the Phase 0 runner."""

from __future__ import annotations

import contextlib
import io
import json
import re
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_phase0_evidence_linux as runner


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
WINDOWS_RUNNER = REPOSITORY_ROOT / "scripts" / "phase0" / "run_phase0_evidence.ps1"


def write_x86_64_elf(path: Path) -> None:
    header = bytearray(64)
    header[:4] = b"\x7fELF"
    header[4] = 2
    header[5] = 1
    header[6] = 1
    header[16:18] = (2).to_bytes(2, "little")
    header[18:20] = runner.ELF_MACHINE_X86_64.to_bytes(2, "little")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header)
    path.chmod(0o755)


def make_package(package_root: Path) -> None:
    write_x86_64_elf(package_root / "bin" / "ClassMngr")
    write_x86_64_elf(package_root / "plugins" / "platforms" / "libqxcb.so")


def make_qt_prefix(qt_prefix: Path) -> None:
    for relative in (
        "lib/cmake/Qt6/Qt6Config.cmake",
        "lib/cmake/Qt6Test/Qt6TestConfig.cmake",
    ):
        path = qt_prefix / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("test fixture\n", encoding="utf-8")


class LinuxRunnerContractTests(unittest.TestCase):
    def test_linux_route_registry_matches_all_24_canonical_windows_routes(self) -> None:
        pattern = re.compile(
            r'\(New-RouteDefinition "([^"]+)" "([^"]+)" "([^"]+)" "([^"]+)" "([^"]+)"\)'
        )
        windows_routes = tuple(pattern.findall(WINDOWS_RUNNER.read_text(encoding="utf-8")))
        linux_routes = tuple(
            (
                route.route_id,
                route.category,
                route.artifact_path,
                route.scenario,
                route.fixture,
            )
            for route in runner.ROUTES
        )
        self.assertEqual(len(linux_routes), 24)
        self.assertEqual(linux_routes, windows_routes)
        self.assertEqual(runner.ALL_ROUTE_IDS, tuple(item[0] for item in windows_routes))

    def test_every_linux_route_has_a_confined_invocation_mapping(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-route-map-") as temporary:
            root = Path(temporary)
            application = root / "package" / "bin" / "ClassMngr"
            test_executable = root / "tools" / "ClassMngrStartupPerformanceTests"
            for route in runner.ROUTES:
                with self.subTest(route=route.route_id):
                    invocation = runner.route_invocation(
                        route.route_id,
                        root,
                        application,
                        test_executable,
                        REPOSITORY_ROOT,
                    )
                    self.assertIn(invocation["filePath"], {str(application), str(test_executable)})
                    self.assertTrue(invocation["arguments"])
                    self.assertEqual(invocation["workingDirectory"], str(REPOSITORY_ROOT))
                    self.assertEqual(invocation["environmentOverrides"]["QT_QPA_PLATFORM"], "xcb")
                    self.assertEqual(
                        invocation["environmentOverrides"]["QT_QPA_PLATFORM_PLUGIN_PATH"],
                        str(application.parent.parent / "plugins" / "platforms"),
                    )
                    self.assertTrue(
                        all(
                            name in {"QT_QPA_PLATFORM", "QT_QPA_PLATFORM_PLUGIN_PATH"}
                            or name.startswith("CLASSMNGR_")
                            for name in invocation["environmentOverrides"]
                        )
                    )
                    for value in invocation["arguments"]:
                        candidate = Path(value)
                        if candidate.is_absolute():
                            self.assertTrue(candidate.is_relative_to(root), value)
                    for name, value in invocation["environmentOverrides"].items():
                        if name == "QT_QPA_PLATFORM":
                            self.assertEqual(value, "xcb")
                        elif name == "QT_QPA_PLATFORM_PLUGIN_PATH":
                            self.assertEqual(
                                Path(value),
                                application.parent.parent / "plugins" / "platforms",
                            )
                        elif value != "1":
                            self.assertTrue(Path(value).is_relative_to(root), (name, value))

    def test_linux_platform_is_supplemental_and_official_gate_registry_is_unchanged(self) -> None:
        metadata = runner._platform_metadata()
        self.assertEqual(metadata["supportedPlatform"], "linux-x64")
        self.assertEqual(metadata["supportedPlatforms"], ["windows-x64", "macos-universal"])
        self.assertEqual(metadata["supplementalPlatforms"], ["linux-x64"])
        self.assertEqual(metadata["deferredPlatforms"], ["windows-arm64", "linux"])
        self.assertTrue(metadata["platform"]["supported"])
        self.assertTrue(metadata["platform"]["supplemental"])
        self.assertIn("does not add Linux", metadata["platform"]["acceptanceScope"])

    def test_plan_lists_smoke_and_all_routes_without_writing_or_spawning(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-runner-plan-") as temporary:
            parent = Path(temporary)
            package_root = parent / "package"
            qt_prefix = parent / "qt"
            evidence_root = parent / "fresh-evidence"
            make_package(package_root)
            make_qt_prefix(qt_prefix)
            stdout = io.StringIO()
            real_which = runner.shutil.which

            def fake_which(name: str) -> str | None:
                if name in {"cmake", "ninja", "git", "xvfb-run", "Xvfb", "xauth"}:
                    return f"/usr/bin/{name}"
                return real_which(name)

            with mock.patch.object(runner.platform, "system", return_value="Linux"), mock.patch.object(
                runner.platform,
                "machine",
                return_value="x86_64",
            ), mock.patch.object(runner.shutil, "which", side_effect=fake_which), mock.patch.object(
                runner.subprocess,
                "Popen",
                side_effect=AssertionError("plan mode must not start processes"),
            ):
                with contextlib.redirect_stdout(stdout):
                    result = runner.main(
                        [
                            "--plan",
                            "--evidence-root",
                            str(evidence_root),
                            "--package-root",
                            str(package_root),
                            "--qt-prefix",
                            str(qt_prefix),
                        ]
                    )

            plan = json.loads(stdout.getvalue())
            self.assertEqual(result, 0)
            self.assertEqual(plan["requestedRouteCount"], 24)
            self.assertEqual(plan["requestedRoutes"], list(runner.ALL_ROUTE_IDS))
            self.assertIn("--startup-performance-test", plan["commands"]["packageLaunchSmoke"])
            self.assertEqual(Path(plan["commands"]["packageLaunchSmoke"][0]).name, "xvfb-run")
            self.assertEqual(plan["packageLaunchEnvironment"]["QT_QPA_PLATFORM"], "xcb")
            self.assertEqual(
                plan["packageLaunchEnvironment"]["QT_QPA_PLATFORM_PLUGIN_PATH"],
                str(package_root / "plugins" / "platforms"),
            )
            self.assertEqual(len(plan["routeCommands"]), 24)
            self.assertTrue(
                all(Path(route["command"][0]).name == "xvfb-run" for route in plan["routeCommands"])
            )
            self.assertFalse(evidence_root.exists())
            self.assertFalse(Path(plan["testBuildDirectory"]).exists())

    def test_runtime_environment_selects_staged_xcb_plugin_and_excludes_qt_sdk_path(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-runtime-env-") as temporary:
            root = Path(temporary)
            package_root = root / "package"
            qt_prefix = root / "Qt" / "6.12.0" / "gcc_64"
            make_package(package_root)
            qt_bin = qt_prefix / "bin"
            with mock.patch.dict(
                runner.os.environ,
                {"PATH": f"/usr/bin{runner.os.pathsep}{qt_bin}"},
            ):
                env = runner._isolated_environment(
                    root / "evidence",
                    package_root / "bin" / "ClassMngr",
                    qt_prefix,
                )
            self.assertEqual(env["QT_QPA_PLATFORM"], "xcb")
            self.assertEqual(
                env["QT_QPA_PLATFORM_PLUGIN_PATH"],
                str(package_root / "plugins" / "platforms"),
            )
            self.assertNotIn(str(qt_bin), env["PATH"].split(runner.os.pathsep))
            self.assertIn("/usr/bin", env["PATH"].split(runner.os.pathsep))

    def test_missing_xvfb_writes_failed_24_route_artifact_without_counting_passes(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-missing-xvfb-") as temporary:
            parent = Path(temporary)
            package_root = parent / "package"
            qt_prefix = parent / "qt"
            evidence_root = parent / "evidence"
            make_package(package_root)
            make_qt_prefix(qt_prefix)
            stdout = io.StringIO()
            stderr = io.StringIO()
            real_which = runner.shutil.which

            def fake_which(name: str) -> str | None:
                if name == "xvfb-run":
                    return None
                if name in {"cmake", "ninja", "git", "Xvfb", "xauth"}:
                    return f"/usr/bin/{name}"
                return real_which(name)

            with mock.patch.object(runner.platform, "system", return_value="Linux"), mock.patch.object(
                runner.platform,
                "machine",
                return_value="x86_64",
            ), mock.patch.object(runner.shutil, "which", side_effect=fake_which):
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    result = runner.main(
                        [
                            "--evidence-root",
                            str(evidence_root),
                            "--package-root",
                            str(package_root),
                            "--qt-prefix",
                            str(qt_prefix),
                        ]
                    )

            run_manifest = json.loads((evidence_root / "run-manifest.json").read_text())
            summary = json.loads((evidence_root / "validation-summary.json").read_text())
            self.assertEqual(result, 2)
            self.assertIn("xvfb-run", stderr.getvalue())
            self.assertEqual(run_manifest["status"], "failed")
            self.assertEqual(run_manifest["routeExecution"]["requestedRouteCount"], 24)
            self.assertEqual(run_manifest["routeExecution"]["executedRouteIds"], [])
            self.assertEqual(run_manifest["routeExecution"]["failedRouteIds"], [])
            self.assertEqual(len(run_manifest["requestedRoutes"]), 24)
            self.assertEqual(summary["supplementalPlatformEvidence"]["linux-x64"]["presentRouteIds"], [])
            self.assertEqual(len(summary["supplementalPlatformEvidence"]["linux-x64"]["missingRouteIds"]), 24)
            self.assertEqual(summary["exitGate"]["status"], "incomplete")

    def test_evidence_path_safety_refuses_project_tree_without_mutation(self) -> None:
        unsafe_root = REPOSITORY_ROOT / "build" / "phase0-linux-unsafe-evidence"
        self.assertFalse(unsafe_root.exists())
        with self.assertRaises(runner.RunnerError):
            runner._validate_evidence_root(unsafe_root, REPOSITORY_ROOT, create=False)
        self.assertFalse(unsafe_root.exists())

    def test_package_preflight_requires_real_x86_64_elf_executable(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-package-audit-") as temporary:
            package_root = Path(temporary) / "package"
            make_package(package_root)
            application, audit = runner._validate_package_root(package_root)
            self.assertEqual(application, package_root / "bin" / "ClassMngr")
            self.assertEqual(audit["applicationArchitecture"], "x86_64")
            self.assertEqual(len(audit["applicationSha256"]), 64)
            self.assertEqual(audit["platformPluginBackend"], "xcb")
            self.assertEqual(audit["platformPluginArchitecture"], "x86_64")
            self.assertEqual(len(audit["platformPluginSha256"]), 64)

            invalid = package_root / "bin" / "ClassMngr"
            invalid.write_text("not an ELF executable\n", encoding="utf-8")
            with self.assertRaises(runner.RunnerError):
                runner._validate_package_root(package_root)

    def test_package_preflight_requires_staged_xcb_plugin(self) -> None:
        with tempfile.TemporaryDirectory(prefix="phase0-linux-package-plugin-") as temporary:
            package_root = Path(temporary) / "package"
            make_package(package_root)
            plugin = package_root / "plugins" / "platforms" / "libqxcb.so"
            plugin.unlink()
            with self.assertRaisesRegex(runner.RunnerError, "xcb platform plugin is missing"):
                runner._validate_package_root(package_root)


if __name__ == "__main__":
    unittest.main(verbosity=2)
