#!/usr/bin/env python3
"""Unit tests for the dependency-free Phase 2 report validator."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from phase2_exit_gate import (  # noqa: E402
    DATABASE_ENGINE_TEST,
    ENGINE_LANES,
    REQUIRED_COVERAGE,
    REQUIRED_LANES,
    RETAINED_QT_GENERATOR,
    RETAINED_QT_TEST,
    parse_json_inventory,
    validate_aggregate,
)


def make_report(root: Path, lane_id: str) -> None:
    lane_dir = root / lane_id
    logs_dir = lane_dir / "logs"
    logs_dir.mkdir(parents=True)
    artifact_names = (
        "configure.log",
        "build.log",
        "ctest.log",
        "ctest.junit.xml",
        "inventory-json.log",
        "inventory.json",
    )
    for name in artifact_names:
        (logs_dir / name).write_text("evidence\n", encoding="utf-8")
    engine = lane_id in ENGINE_LANES
    required_test = DATABASE_ENGINE_TEST if engine else RETAINED_QT_TEST
    artifact_keys = {
        "configure.log": "configure_log",
        "build.log": "build_log",
        "ctest.log": "ctest_log",
        "ctest.junit.xml": "ctest_junit",
        "inventory-json.log": "inventory_log",
        "inventory.json": "inventory_json",
    }
    qt_prefix_env, qt_architecture = {
        "windows-qt-6.12-x64": ("QT_MSVC_X64_PREFIX", "x64"),
        "linux-qt-6.12-x64": ("QT_LINUX_PREFIX", "x64"),
        "macos-qt-6.12-universal": ("QT_MACOS_PREFIX", "universal"),
    }.get(lane_id, (None, None))
    report = {
        "format": "phase2-exit-gate-report-v1",
        "lane_id": lane_id,
        "lane_type": "engine" if engine else "retained-qt",
        "status": "PASS",
        "evidence_class": "runtime-tested",
        "git_commit": "synthetic-commit",
        "source_stable_during_run": True,
        "artifacts": {artifact_keys[name]: f"logs/{name}" for name in artifact_names},
        "results": {
            "configure_exit_code": 0,
            "build_exit_code": 0,
            "inventory_exit_code": 0,
            "test_exit_code": 0,
            "test_pattern": r"^ClassMngrEngine" if engine else rf"^{RETAINED_QT_TEST}$",
            "registered_tests": [required_test],
            "registered_engine_tests": [DATABASE_ENGINE_TEST] if engine else [],
            "registered_engine_inventory": [
                {"name": DATABASE_ENGINE_TEST, "executable_present": True}
            ] if engine else [],
            "missing_engine_executables": [],
            "unexecuted_engine_tests": [],
            "required_test_executable_present": True,
            "tests": {"test_names": [required_test], "failures": 0, "errors": 0},
        },
        "coverage": {
            category: {"status": "covered", "evidence_tests": [DATABASE_ENGINE_TEST]}
            for category in REQUIRED_COVERAGE
        }
        if engine
        else {},
        "qt": None
        if engine
        else {
            "required_version": "6.12.0",
            "version": "6.12.0",
            "detected_version": "6.12.0",
            "architecture": qt_architecture,
            "prefix_env": qt_prefix_env,
            "prefix": "Qt/6.12.0",
            "version_probe": "Qt/6.12.0/bin/qmake",
            "runtime_tested": True,
            "compile_only": False,
            "blocked": False,
        },
    }
    (lane_dir / f"{lane_id}.json").write_text(json.dumps(report), encoding="utf-8")


class Phase2ExitGateTests(unittest.TestCase):
    def test_retained_qt_uses_fixture_generator_target(self) -> None:
        self.assertEqual(RETAINED_QT_GENERATOR, "ClassMngrDatabasePortFixtureGenerator")

    def test_retained_qt_resolves_fixture_generator_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build_dir = Path(temporary)
            executable = build_dir / f"{RETAINED_QT_GENERATOR}.exe"
            executable.write_text("fixture generator\n", encoding="utf-8")
            inventory = parse_json_inventory(
                json.dumps(
                    {
                        "tests": [
                            {
                                "name": RETAINED_QT_TEST,
                                "command": [
                                    str(executable),
                                    "--verify-directory",
                                    "tests/fixtures/database-port",
                                ],
                            }
                        ]
                    }
                ),
                build_dir,
            )
            self.assertIsNotNone(inventory)
            self.assertTrue(inventory[0]["executable_present"])
            self.assertEqual(str(executable.resolve()), inventory[0]["executable"])

    def test_complete_synthetic_matrix_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for lane_id in REQUIRED_LANES:
                make_report(root, lane_id)
            output = root / "aggregate.json"
            self.assertEqual(validate_aggregate(root, output), 0)
            self.assertEqual(json.loads(output.read_text())["status"], "PASS")

    def test_aggregate_output_inside_reports_dir_is_excluded(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for lane_id in REQUIRED_LANES:
                make_report(root, lane_id)
            output = root / "aggregate.json"
            self.assertEqual(validate_aggregate(root, output), 0)
            self.assertEqual(validate_aggregate(root, output), 0)

    def test_missing_artifact_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for lane_id in REQUIRED_LANES:
                make_report(root, lane_id)
            (root / ENGINE_LANES[0] / "logs" / "build.log").unlink()
            self.assertNotEqual(validate_aggregate(root, root / "aggregate.json"), 0)

    def test_compile_only_qt_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for lane_id in REQUIRED_LANES:
                make_report(root, lane_id)
            qt_report_path = root / "windows-qt-6.12-x64" / "windows-qt-6.12-x64.json"
            qt_report = json.loads(qt_report_path.read_text())
            qt_report["qt"]["runtime_tested"] = False
            qt_report["qt"]["compile_only"] = True
            qt_report_path.write_text(json.dumps(qt_report), encoding="utf-8")
            self.assertNotEqual(validate_aggregate(root, root / "aggregate.json"), 0)


if __name__ == "__main__":
    unittest.main()
