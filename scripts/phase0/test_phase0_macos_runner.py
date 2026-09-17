#!/usr/bin/env python3
"""Focused no-write and route-contract checks for the macOS Phase 0 runner."""

from __future__ import annotations

import contextlib
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_phase0_evidence_macos as runner


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
WINDOWS_RUNNER = REPOSITORY_ROOT / "scripts" / "phase0" / "run_phase0_evidence.ps1"


class MacRunnerContractTests(unittest.TestCase):
    def test_route_definitions_match_canonical_windows_order_and_metadata(self) -> None:
        pattern = re.compile(
            r'\(New-RouteDefinition "([^"]+)" "([^"]+)" "([^"]+)" "([^"]+)" "([^"]+)"\)'
        )
        source = WINDOWS_RUNNER.read_text(encoding="utf-8")
        windows_routes = tuple(pattern.findall(source))
        mac_routes = tuple(
            (
                route.route_id,
                route.category,
                route.artifact_path,
                route.scenario,
                route.fixture,
            )
            for route in runner.ROUTES
        )
        self.assertEqual(len(mac_routes), 24)
        self.assertEqual(mac_routes, windows_routes)

    def test_run_manifest_platform_metadata_matches_contract_without_writing(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-platform-metadata-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary) / "new-evidence-root"
            qt_prefix = Path("/Users/example/Qt/6.12.0/macos")
            source_snapshot = {
                "recordedAtUtc": "2026-09-17T00:00:00.000Z",
                "platform": "macOS-test-host",
                "machine": "arm64",
                "macVersion": "26.6.2",
                "source": {"commit": "f8f53b2b7671887c702f22e3ab8b0b79b36cda68"},
                "qtPrefix": str(qt_prefix),
                "macosSdkPath": "/Xcode/SDKs/MacOSX26.5.sdk",
            }
            manifest = runner._new_manifest(
                root,
                REPOSITORY_ROOT,
                qt_prefix,
                source_snapshot,
                900,
                4,
            )

            self.assertEqual(manifest["supportedPlatform"], "macos-universal")
            self.assertEqual(
                manifest["supportedPlatforms"],
                ["windows-x64", "macos-universal"],
            )
            self.assertEqual(manifest["deferredPlatforms"], ["windows-arm64", "linux"])
            self.assertEqual(
                manifest["platformCoverage"],
                [
                    {
                        "name": "windows-x64",
                        "supported": True,
                        "status": "supported-but-not-run-on-this-host",
                    },
                    {
                        "name": "macos-universal",
                        "supported": True,
                        "status": "runnable-on-this-host",
                    },
                ],
            )
            self.assertEqual(
                manifest["platform"],
                {
                    "name": "macos-universal",
                    "supported": True,
                    "acceptanceScope": (
                        "Packaged Release macOS universal routes on Apple Silicon; "
                        "x86_64 is verified as a packaged universal slice."
                    ),
                },
            )
            self.assertFalse(root.exists())

    def test_all_route_invocations_are_mapped_and_confined_to_the_new_root(self) -> None:
        root = Path("/private/tmp/phase0-macos-contract-root")
        app = root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"
        test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
        qt_prefix = Path("/Users/example/Qt/6.12.0/macos")

        for route in runner.ROUTES:
            with self.subTest(route=route.route_id):
                invocation = runner.route_invocation(
                    route.route_id,
                    root,
                    app,
                    test_executable,
                    REPOSITORY_ROOT,
                )
                self.assertIn(invocation["filePath"], {str(app), str(test_executable)})
                self.assertTrue(invocation["arguments"])
                self.assertEqual(invocation["workingDirectory"], str(REPOSITORY_ROOT))
                self.assertEqual(
                    invocation["environmentOverrides"]["QT_QPA_PLATFORM"],
                    "offscreen",
                )
                self.assertTrue(
                    all(
                        name == "QT_QPA_PLATFORM" or name.startswith("CLASSMNGR_")
                        for name in invocation["environmentOverrides"]
                    )
                )
                for value in invocation["arguments"]:
                    candidate = Path(value)
                    if candidate.is_absolute():
                        self.assertTrue(candidate.is_relative_to(root), value)
                for name, value in invocation["environmentOverrides"].items():
                    if name == "QT_QPA_PLATFORM":
                        self.assertEqual(value, "offscreen")
                    elif value != "1":
                        self.assertTrue(Path(value).is_relative_to(root), value)

                environment = runner._isolated_environment(
                    root,
                    app,
                    test_executable,
                    qt_prefix,
                    invocation["environmentOverrides"],
                )
                self.assertEqual(environment["QT_QPA_PLATFORM"], "offscreen")
                self.assertEqual(environment["CLASSMNGR_TEST_APP_PATH"], str(app))
                self.assertEqual(environment["HOME"], str(root / "runtime" / "home"))
                self.assertEqual(environment["TMPDIR"], str(root / "tmp"))
                recorded_environment = runner._recorded_environment(environment)
                self.assertEqual(recorded_environment["QT_QPA_PLATFORM"], "offscreen")
                for name, value in environment.items():
                    if name.startswith("CLASSMNGR_"):
                        self.assertEqual(recorded_environment[name], value)

    def test_process_record_retains_route_environment_and_process_metadata(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-process-record-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary)
            application = root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"
            test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
            environment = runner._isolated_environment(
                root,
                application,
                test_executable,
                Path("/Users/example/Qt/6.12.0/macos"),
                {"CLASSMNGR_TEST_SCENARIO": "metadata-self-check"},
            )
            record = runner._process_record(
                "process-metadata-self-check",
                [sys.executable, "-c", "pass"],
                REPOSITORY_ROOT,
                environment,
                root / "runner",
                10,
            )

            self.assertEqual(record["environment"]["QT_QPA_PLATFORM"], "offscreen")
            self.assertEqual(
                record["environment"]["CLASSMNGR_TEST_APP_PATH"],
                str(application),
            )
            self.assertEqual(
                record["environment"]["CLASSMNGR_TEST_SCENARIO"],
                "metadata-self-check",
            )
            self.assertTrue(record["processFinished"])
            self.assertEqual(record["exitStatus"], "normal")
            self.assertEqual(record["exitCode"], 0)
            self.assertFalse(record["timedOut"])
            self.assertGreaterEqual(record["durationSeconds"], 0)
            self.assertTrue(Path(record["stdoutPath"]).is_file())
            self.assertTrue(Path(record["stderrPath"]).is_file())

    def test_route_manifest_serializes_validator_process_contract(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-route-manifest-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary)
            application = root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"
            test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
            route = runner.ROUTE_BY_ID["lifecycle-calendar-import"]
            invocation = {
                "filePath": sys.executable,
                "arguments": ["-c", "pass"],
                "environmentOverrides": {"QT_QPA_PLATFORM": "offscreen"},
                "workingDirectory": str(REPOSITORY_ROOT),
            }
            manifest: dict[str, object] = {"commands": [], "failures": []}

            with (
                mock.patch.object(runner, "ROUTES", (route,)),
                mock.patch.object(runner, "route_invocation", return_value=invocation),
            ):
                failed_routes = runner._execute_routes(
                    manifest,
                    root,
                    REPOSITORY_ROOT,
                    Path("/Users/example/Qt/6.12.0/macos"),
                    application,
                    test_executable,
                    10,
                )

            self.assertEqual(failed_routes, [])
            route_manifest_path = root / route.artifact_path / "route-manifest.json"
            route_manifest = json.loads(route_manifest_path.read_text(encoding="utf-8"))
            process = route_manifest["invocation"]
            self.assertEqual(process["command"], [sys.executable, "-c", "pass"])
            self.assertEqual(process["environment"]["QT_QPA_PLATFORM"], "offscreen")
            self.assertEqual(process["processFinished"], True)
            self.assertEqual(process["exitStatus"], "normal")
            self.assertEqual(process["exitCode"], 0)
            self.assertEqual(process["timedOut"], False)

    def test_plan_is_no_write_and_lists_all_routes_in_order(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-runner-plan-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary) / "fresh-evidence"
            stdout = io.StringIO()
            with mock.patch.object(
                runner.subprocess,
                "Popen",
                side_effect=AssertionError("plan mode must not start processes"),
            ), mock.patch.object(
                runner,
                "_discover_macos_sdk_path",
                return_value="/Xcode/SDKs/MacOSX26.5.sdk",
            ):
                with contextlib.redirect_stdout(stdout):
                    result = runner.main(
                        [
                            "--plan",
                            "--evidence-root",
                            str(root),
                            "--repository-root",
                            str(REPOSITORY_ROOT),
                            "--qt-prefix",
                            "/private/tmp/not-a-real-qt-prefix",
                        ]
                    )
            self.assertEqual(result, 0)
            self.assertFalse(root.exists())
            plan = json.loads(stdout.getvalue())
            planned_routes = [
                command["id"]
                for command in plan["commands"]
                if command["kind"] == "route"
            ]
            self.assertEqual(planned_routes, list(runner.ALL_ROUTE_IDS))
            self.assertEqual(len(planned_routes), 24)
            for command in plan["commands"]:
                if command["kind"] == "route":
                    self.assertEqual(command["environment"]["QT_QPA_PLATFORM"], "offscreen")
                    self.assertEqual(
                        command["environment"]["CLASSMNGR_TEST_APP_PATH"],
                        plan["applicationPath"],
                    )
            configure_commands = [
                command["command"]
                for command in plan["commands"]
                if command["kind"] == "configure"
            ]
            self.assertEqual(len(configure_commands), 2)
            for command in configure_commands:
                self.assertIn("-DCMAKE_OSX_DEPLOYMENT_TARGET=14.4", command)
                self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", command)
                self.assertIn(
                    "-DCMAKE_OSX_SYSROOT=/Xcode/SDKs/MacOSX26.5.sdk",
                    command,
                )

    def test_build_environment_pins_the_selected_macos_sdk(self) -> None:
        root = Path("/private/tmp/phase0-macos-build-env-root")
        sdk = "/Applications/Xcode.app/Contents/Developer/SDKs/MacOSX26.5.sdk"
        environment = runner._build_environment(
            root,
            Path("/Users/example/Qt/6.12.0/macos"),
            sdk,
        )
        self.assertEqual(environment["SDKROOT"], sdk)
        self.assertEqual(runner._record_environment(environment)["SDKROOT"], sdk)

    def test_plist_audit_passes_scoped_log_path_to_audited_command(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-plist-audit-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary)
            release_build = root / "build" / "release"
            debug_build = root / "build" / "debug"
            release_build.mkdir(parents=True)
            debug_build.mkdir(parents=True)
            package_contents = root / "package" / "ClassMngr.app" / "Contents"
            app = package_contents / "MacOS" / "ClassMngr"
            plugin = package_contents / "PlugIns" / "platforms" / "libqoffscreen.dylib"
            test_executable = debug_build / "ClassMngrStartupPerformanceTests"
            for path in (app, plugin, test_executable):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"test binary")
            (package_contents / "Info.plist").parent.mkdir(parents=True, exist_ok=True)
            (package_contents / "Info.plist").write_text("plist", encoding="utf-8")
            plist_output = root / "plist-output.txt"
            plist_output.write_text("14.4\n", encoding="utf-8")
            sdk = "/Xcode/SDKs/MacOSX26.5.sdk"
            cache_values = {
                "CMAKE_BUILD_TYPE": "Release",
                "BUILD_TESTING": "OFF",
                "CMAKE_OSX_ARCHITECTURES": "arm64;x86_64",
                "CMAKE_OSX_DEPLOYMENT_TARGET": "14.4",
                "CMAKE_OSX_SYSROOT": sdk,
                "CMAKE_PREFIX_PATH": "/Users/example/Qt/6.12.0/macos",
            }

            def audit_binary(_manifest, _root, _repo, _env, binary, relative, _timeout):
                kind = "non-Mach-O" if relative.endswith("Info.plist") else "Mach-O"
                return {
                    "path": str(binary),
                    "relativePath": relative,
                    "kind": kind,
                    "architectures": ["arm64", "x86_64"],
                    "minimumVersions": ["14.4", "14.4"],
                }

            manifest = {"packagingAudit": {"status": "pending"}}
            with (
                mock.patch.object(
                    runner,
                    "_read_cmake_cache",
                    side_effect=lambda path: {
                        **cache_values,
                        "CMAKE_BUILD_TYPE": "Release" if path.parent.name == "release" else "Debug",
                        "BUILD_TESTING": "OFF" if path.parent.name == "release" else "ON",
                    },
                ),
                mock.patch.object(runner, "_audit_binary", side_effect=audit_binary),
                mock.patch.object(
                    runner,
                    "_audited_command",
                    return_value={"stdoutPath": str(plist_output)},
                ) as audited_command,
            ):
                runner._audit_package_and_harness(
                    manifest,
                    root,
                    REPOSITORY_ROOT,
                    Path("/Users/example/Qt/6.12.0/macos"),
                    release_build,
                    debug_build,
                    app,
                    test_executable,
                    60,
                    sdk,
                )

            arguments = audited_command.call_args.args
            self.assertIs(arguments[0], manifest)
            self.assertEqual(arguments[1], root)
            self.assertEqual(arguments[2], REPOSITORY_ROOT)
            self.assertEqual(arguments[4], "plist-minimum-version")
            self.assertEqual(arguments[6], root / "packaging-audit" / "info-plist")
            self.assertEqual(manifest["packagingAudit"]["status"], "passed")

    def test_finished_build_continuation_runs_self_check_routes_and_validation_once(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="phase0-macos-finish-built-",
            dir="/private/tmp",
        ) as temporary:
            root = Path(temporary)
            manifest = {
                "status": "running",
                "failures": [],
                "commands": [],
                "routeExecution": {
                    "status": "not-started",
                    "requestedRouteCount": 24,
                    "executedRouteIds": [],
                },
            }
            success = {
                "processFinished": True,
                "exitStatus": "normal",
                "exitCode": 0,
                "timedOut": False,
                "stdoutPath": str(root / "stdout.txt"),
                "stderrPath": str(root / "stderr.txt"),
            }
            with (
                mock.patch.object(runner, "_invoke", return_value=success) as invoke,
                mock.patch.object(runner, "_execute_routes", return_value=[]) as execute_routes,
            ):
                result = runner._finish_built_run(
                    manifest,
                    root,
                    REPOSITORY_ROOT,
                    Path("/Users/example/Qt/6.12.0/macos"),
                    root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr",
                    root / "build" / "debug" / "ClassMngrStartupPerformanceTests",
                    60,
                    "/Xcode/SDKs/MacOSX26.5.sdk",
                )

            self.assertEqual(result, 0)
            self.assertEqual(invoke.call_count, 2)
            self.assertEqual(execute_routes.call_count, 1)
            self.assertEqual(manifest["status"], "completed")
            self.assertEqual(manifest["routeExecution"]["executedRouteIds"], list(runner.ALL_ROUTE_IDS))

    def test_vtool_audit_requires_one_acceptable_minimum_per_slice(self) -> None:
        output = (
            "binary (architecture x86_64):\n"
            "  Load command 1\n"
            "    platform MACOS\n"
            "    minos 14.4\n"
            "binary (architecture arm64):\n"
            "  Load command 1\n"
            "    platform MACOS\n"
            "    minos 14.4\n"
        )
        minimums = runner._check_minimum_versions(
            output,
            {"arm64", "x86_64"},
            "test binary",
        )
        self.assertEqual(minimums, ["14.4", "14.4"])
        with self.assertRaises(runner.RunnerError):
            runner._check_minimum_versions(
                output.replace("minos 14.4", "minos 15.0", 1),
                {"arm64", "x86_64"},
                "too-new binary",
            )


if __name__ == "__main__":
    unittest.main()
