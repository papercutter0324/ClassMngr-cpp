#!/usr/bin/env python3
"""Focused fail-closed memory and provenance-shape tests for P9-16."""

from __future__ import annotations

import importlib.util
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("validate_packaged_next_p9_16_repeated_workspace.py")
SPEC = importlib.util.spec_from_file_location("p916_validator", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


def make_window(
    window_id: str,
    state: str,
    phase: str,
    value: int,
    *,
    cycle: int | None = None,
) -> dict:
    return {
        "windowId": window_id,
        "state": state,
        "phase": phase,
        "cycleNumber": cycle,
        "processId": 4321,
        "mainWindowHandle": 5678,
        "samples": [
            {
                "elapsedMilliseconds": index * 100,
                "workingSetBytes": value,
                "privateBytes": value * 2,
                "handles": 240 + (index % 2),
                "threadCount": 18,
            }
            for index in range(1, 11)
        ],
    }


def memory_manifest(
    open_baseline: list[int] | None = None,
    open_cycles: list[int] | None = None,
    closed_cycles: list[int] | None = None,
    cycle_count: int = 10,
) -> dict:
    windows = [
        make_window("warmup-open", "open", "warmup", 100_000_000),
        make_window("warmup-closed", "closed", "warmup", 80_000_000),
    ]
    open_baseline = open_baseline or [100_000_000, 102_000_000, 100_000_000, 98_000_000, 100_000_000]
    closed_baseline = [80_000_000, 81_000_000, 80_000_000, 79_000_000, 80_000_000]
    for state, values in (("closed", closed_baseline), ("open", open_baseline)):
        for index, value in enumerate(values, 1):
            windows.append(make_window(f"baseline-{state}-{index}", state, "baseline", value))
    open_cycles = open_cycles or [100_000_000] * cycle_count
    closed_cycles = closed_cycles or [80_000_000] * cycle_count
    for cycle in range(1, cycle_count + 1):
        windows.append(make_window(f"cycle-{cycle}-closed", "closed", "cycle", closed_cycles[cycle - 1], cycle=cycle))
        windows.append(make_window(f"cycle-{cycle}-open", "open", "cycle", open_cycles[cycle - 1], cycle=cycle))
    return {
        "memory": {"windows": windows},
        "process": {},
        "route": {"measuredCycleCount": cycle_count, "primingCycleCount": 3 if cycle_count == 20 else 0, "baselineWindowsPerState": 5},
    }


class P916MemoryValidationTests(unittest.TestCase):
    def analyze(self, manifest: dict, directory: Path) -> dict:
        metrics = directory / "process-metrics.jsonl"
        metrics.write_text(
            "\n".join(
                json.dumps(row)
                for row in (
                    {"pid": 4321, "category": "operation", "workingSetBytes": 115_000_000, "peakWorkingSetBytes": 130_000_000},
                    {"pid": 4321, "category": "normal-use", "workingSetBytes": 100_000_000, "peakWorkingSetBytes": 130_000_000},
                )
            )
            + "\n",
            encoding="utf-8",
        )
        manifest["_evidenceRoot"] = str(directory)
        manifest["process"]["metrics"] = {
            "path": "process-metrics.jsonl",
            "sha256": validator.sha256_file(metrics),
        }
        return validator.analyze_memory(manifest, 4321, 5678)

    def test_empirical_envelopes_and_cycle_upper_bounds_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(), Path(directory))
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["workingSetNoGrowthStatus"], "passed")
        self.assertTrue(result["states"]["open"]["workingSetBytes"]["sameStateNoGrowthPass"])
        self.assertEqual(result["states"]["open"]["workingSetBytes"]["noiseEnvelope"], 2_000_000)
        self.assertEqual(result["operationPeakWorkingSetBytes"], 115_000_000)
        self.assertEqual(result["lifetimePeakWorkingSetBytesDiagnosticOnly"], 130_000_000)

    def test_positive_cycle_trend_beyond_noise_fails(self) -> None:
        growth = [100_000_000 + 3_000_000 * index for index in range(10)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=growth), Path(directory))
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["workingSetNoGrowthStatus"], "failed")
        self.assertFalse(result["states"]["open"]["workingSetBytes"]["cycleMediansWithinUpperBound"])
        self.assertGreater(
            result["states"]["open"]["workingSetBytes"]["cycleRegressionFittedChangeDiagnostic"],
            0,
        )

    def test_twenty_cycle_fixture_size_experiment_is_summarized(self) -> None:
        growth = [100_000_000 + 2_000_000 * index for index in range(20)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=growth, cycle_count=20), Path(directory))
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["windowPolicy"]["primingCyclesBeforeBaseline"], 3)
        self.assertEqual(result["windowPolicy"]["cycleWindowsPerState"], 20)
        self.assertEqual(len(result["windowSummaries"]), 52)
        self.assertEqual(len(result["states"]["open"]["workingSetBytes"]["cycleMedians"]), 20)

    def test_decreases_below_the_symmetric_envelope_pass_the_one_sided_rule(self) -> None:
        lower = [95_000_000] * 10
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=lower), Path(directory))
        open_working_set = result["states"]["open"]["workingSetBytes"]
        self.assertEqual(result["status"], "passed")
        self.assertGreater(open_working_set["noiseEnvelope"], 0)
        self.assertLess(open_working_set["cycleMedians"][0], open_working_set["baselineMedian"] - open_working_set["noiseEnvelope"])
        self.assertTrue(open_working_set["cycleMediansWithinUpperBound"])

    def test_transient_above_baseline_checkpoints_do_not_fail_when_final_value_declines(self) -> None:
        transient = [110_000_000, 112_000_000, 98_000_000, 97_000_000, 96_000_000,
                     95_000_000, 94_000_000, 93_000_000, 92_000_000, 91_000_000]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=transient), Path(directory))
        open_working_set = result["states"]["open"]["workingSetBytes"]
        self.assertEqual(result["status"], "passed")
        self.assertEqual(open_working_set["status"], "passed")
        self.assertEqual([item["cycleNumber"] for item in open_working_set["cycleMediansAboveUpperBound"]], [1, 2])
        self.assertFalse(open_working_set["finalMedianAboveUpperBound"])
        self.assertLess(open_working_set["cycleRegressionFittedChangeDiagnostic"], 0)

    def test_final_above_upper_bound_with_negative_trend_is_diagnostic_only(self) -> None:
        declining = [110_000_000, 109_000_000, 108_000_000, 107_000_000, 106_000_000,
                     105_000_000, 104_000_000, 103_000_000, 102_500_000, 102_250_000]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=declining), Path(directory))
        open_working_set = result["states"]["open"]["workingSetBytes"]
        self.assertEqual(result["status"], "passed")
        self.assertTrue(open_working_set["finalMedianAboveUpperBound"])
        self.assertFalse(open_working_set["positiveCycleTrend"])
        self.assertFalse(open_working_set["cumulativeUpwardGrowth"])

    def test_positive_slope_inside_upper_bound_is_diagnostic_only(self) -> None:
        increasing = [100_000_000 + 200_000 * index for index in range(10)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_cycles=increasing), Path(directory))
        open_working_set = result["states"]["open"]["workingSetBytes"]
        self.assertEqual(result["status"], "passed")
        self.assertGreater(open_working_set["cycleRegressionFittedChangeDiagnostic"], 0)
        self.assertTrue(open_working_set["cycleMediansWithinUpperBound"])

    def test_final_working_set_above_upper_bound_with_trend_inside_envelope_passes(self) -> None:
        series = [99_000_000, 100_000_000, 100_000_000, 100_000_000, 100_000_000,
                  100_000_000, 100_000_000, 100_000_000, 100_000_000, 102_100_000]
        baseline = [100_000_000, 102_000_000, 100_000_000, 98_000_000, 100_000_000]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_baseline=baseline, open_cycles=series), Path(directory))
        open_working_set = result["states"]["open"]["workingSetBytes"]
        self.assertEqual(result["workingSetNoGrowthStatus"], "passed")
        self.assertTrue(open_working_set["finalMedianAboveUpperBound"])
        self.assertLessEqual(
            open_working_set["cycleRegressionFittedChangeDiagnostic"],
            open_working_set["noiseEnvelope"],
        )
        self.assertFalse(open_working_set["positiveCycleTrendBeyondNoiseEnvelope"])

    def test_private_bytes_growth_is_reported_but_does_not_fail_working_set_gate(self) -> None:
        manifest = memory_manifest()
        private_series = [210_000_000 + 1_000_000 * index for index in range(10)]
        for window in manifest["memory"]["windows"]:
            if window["phase"] != "cycle" or window["state"] != "open":
                continue
            private_value = private_series[int(window["cycleNumber"]) - 1]
            for sample in window["samples"]:
                sample["privateBytes"] = private_value
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(manifest, Path(directory))
        self.assertEqual(result["workingSetNoGrowthStatus"], "passed")
        self.assertEqual(result["strictWorkingSetCapStatus"], "passed")
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["privateBytesTrend"]["status"], "unresolved-positive-trend")
        self.assertFalse(result["privateBytesTrend"]["affectsPrimaryMemoryStatus"])
        self.assertEqual(len(result["privateBytesTrend"]["states"]["open"]["cycleMedians"]), 10)
        self.assertTrue(result["privateBytesTrend"]["states"]["open"]["cumulativeUpwardGrowth"])

    def test_directionally_drifting_baseline_is_inconclusive(self) -> None:
        drift = [100_000_000 + 1_000_000 * index for index in range(5)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_baseline=drift), Path(directory))
        self.assertEqual(result["status"], "inconclusive")
        self.assertEqual(
            result["states"]["open"]["workingSetBytes"]["baselineReason"],
            "baseline-has-directional-drift-beyond-its-envelope",
        )

    def test_negative_directional_baseline_drift_is_inconclusive(self) -> None:
        drift = [100_000_000 - 1_000_000 * index for index in range(5)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_baseline=drift), Path(directory))
        self.assertEqual(result["status"], "inconclusive")
        self.assertEqual(
            result["states"]["open"]["workingSetBytes"]["baselineReason"],
            "baseline-has-directional-drift-beyond-its-envelope",
        )

    def test_stable_growth_failure_is_not_masked_by_another_baselines_drift(self) -> None:
        drift = [100_000_000 + 1_000_000 * index for index in range(5)]
        growth = [80_000_000 + 3_000_000 * index for index in range(10)]
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(memory_manifest(open_baseline=drift, closed_cycles=growth), Path(directory))
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["workingSetNoGrowthStatus"], "failed")
        self.assertEqual(result["states"]["open"]["workingSetBytes"]["status"], "inconclusive")
        self.assertEqual(result["states"]["closed"]["privateBytes"]["status"], "diagnostic-only")
        self.assertEqual(result["privateBytesTrend"]["status"], "unresolved-positive-trend")
        self.assertFalse(result["privateBytesTrend"]["affectsPrimaryMemoryStatus"])
        self.assertTrue(result["inconclusiveReasons"])

    def test_settled_working_set_cap_violation_is_reported_as_memory_failure(self) -> None:
        manifest = memory_manifest()
        manifest["memory"]["windows"][2]["samples"][0]["workingSetBytes"] = validator.STRICT_WS_LIMIT
        with tempfile.TemporaryDirectory() as directory:
            result = self.analyze(manifest, Path(directory))
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["strictWorkingSetCapStatus"], "failed")
        self.assertFalse(result["strictCapPassedForEverySettledSample"])
        self.assertEqual(result["settledWorkingSetCapFailures"][0]["windowId"], "baseline-closed-1")

    def test_window_rejects_wrong_sample_count(self) -> None:
        window = make_window("short", "open", "baseline", 100_000_000)
        window["samples"].pop()
        with self.assertRaisesRegex(validator.ValidationError, "exactly 10 samples"):
            validator.summarize_window(window)

    def test_initial_setup_route_is_not_counted_as_a_workspace_close(self) -> None:
        events = [
            {"fromPageId": "none", "targetPageId": "initial_setup", "cause": "initial"},
            {"fromPageId": "initial_setup", "targetPageId": "my_workspace", "cause": "selection"},
            {"fromPageId": "my_workspace", "targetPageId": "initial_setup", "cause": "selection"},
        ]
        opened, closed = validator.workspace_route_transitions(events)
        self.assertEqual(len(opened), 1)
        self.assertEqual(len(closed), 1)
        self.assertEqual(closed[0]["fromPageId"], "my_workspace")

    def test_passing_recent_arm_remains_diagnostic_only(self) -> None:
        self.assertEqual(validator.overall_run_status("recent", "passed"), "diagnostic-only")
        self.assertEqual(validator.overall_run_status("dialog", "passed"), "passed")
        self.assertEqual(validator.overall_run_status("recent", "failed"), "failed")

    def test_absent_recent_action_is_zero_for_dialog_arm(self) -> None:
        actions = [{"actionKind": "open-teacher-profile"}, {"actionKind": "close-teacher-profile"}]
        self.assertEqual(validator.action_kind_count(actions, "open-recent-workspace"), 0)


class P916CliFailureReportTests(unittest.TestCase):
    def test_failed_manifest_with_complete_evidence_prints_report_before_memory_gate_exit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "manifest.json").write_text(
                json.dumps({
                    "schema": validator.SCHEMA,
                    "taskId": validator.TASK_ID,
                    "status": "failed",
                    "failure": None,
                    "openMode": "dialog",
                }),
                encoding="utf-8",
            )
            memory = {
                "status": "failed",
                "workingSetNoGrowthStatus": "failed",
                "strictWorkingSetCapStatus": "passed",
                "privateBytesTrend": {"status": "unresolved-positive-trend", "diagnosticOnly": True},
                "windowSummaries": [{"windowId": "cycle-10-open"}],
            }
            stdout = io.StringIO()
            stderr = io.StringIO()
            patches = (
                patch.object(validator, "validate_package_provenance", return_value={"status": "passed"}),
                patch.object(validator, "validate_process", return_value={"processId": 4321, "mainWindowHandle": 5678}),
                patch.object(validator, "validate_fixture", return_value={"workspace": str(root / "fixture.tps")}),
                patch.object(validator, "validate_settings", return_value={"status": "passed"}),
                patch.object(validator, "validate_route", return_value={"status": "passed"}),
                patch.object(validator, "analyze_memory", return_value=memory),
                patch.object(validator, "validate_workspace_sidecars", return_value=None),
                patch.object(validator, "analyze_application_diagnostics", return_value={"status": "captured"}),
                patch.object(sys, "argv", ["p9_16_validator", "--evidence-dir", str(root)]),
            )
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7], patches[8]:
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    exit_code = validator.main()
            report = json.loads(stdout.getvalue())

        self.assertEqual(exit_code, 1)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["evidenceIntegrityAndProvenanceStatus"], "passed")
        self.assertEqual(report["evidenceValidationStatus"], "passed")
        self.assertEqual(report["memoryStatus"], "failed")
        self.assertEqual(report["workingSetNoGrowthStatus"], "failed")
        self.assertEqual(report["strictWorkingSetCapStatus"], "passed")
        self.assertEqual(report["privateBytesTrend"]["status"], "unresolved-positive-trend")
        self.assertEqual(report["memory"]["windowSummaries"][0]["windowId"], "cycle-10-open")

    def test_execution_failure_manifest_remains_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "manifest.json").write_text(
                json.dumps({
                    "schema": validator.SCHEMA,
                    "taskId": validator.TASK_ID,
                    "status": "failed",
                    "failure": {"message": "route did not complete"},
                }),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(validator.ValidationError, "execution/evidence-collection failure"):
                validator.validate_run(root)

    def test_application_diagnostics_report_lifecycle_truncation_and_sqlite_warning(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = {
                "event": "page-lifecycle",
                "page_key": "my_workspace",
                "state": "activated",
                "page_instance_available": True,
                "page": {
                    "item_model_count": 2,
                    "item_model_root_rows": 768,
                    "item_models": [{"class_name": "QAbstractTableModel", "root_rows": 768}],
                    "item_models_truncated": False,
                    "widget_scan_truncated": True,
                },
            }
            warning = "QSqlDatabase::removeDatabase: connection 'workspace' is still in use, all queries will cease to work."
            app_output = root / "application-output.jsonl"
            app_output.write_text(
                "\n".join(
                    json.dumps(row)
                    for row in (
                        {"arrivalIndex": 1, "stream": "stderr", "line": f"{validator.PAGE_LIFECYCLE_MARKER}{json.dumps(checkpoint)}"},
                        {"arrivalIndex": 2, "stream": "stderr", "line": warning},
                    )
                ) + "\n",
                encoding="utf-8",
            )
            stderr = root / "process-stderr.log"
            stderr.write_text(warning + "\n", encoding="utf-8")
            manifest = {
                "applicationDiagnostics": {
                    "applicationOutput": {"relativePath": app_output.name, "sha256": validator.sha256_file(app_output)},
                    "processStderr": {"relativePath": stderr.name, "sha256": validator.sha256_file(stderr)},
                },
                "process": {"stderrPath": stderr.name},
            }
            result = validator.analyze_application_diagnostics(root, manifest)

        self.assertEqual(result["pageLifecycleCheckpointCount"], 1)
        self.assertEqual(result["pageInstanceAndLifecycleEventCounts"]["my_workspace"]["page-lifecycle:activated"], 1)
        self.assertEqual(result["pageModelMetricRangesAndObservationCounts"]["my_workspace"]["metricRanges"]["item_model_root_rows"], [768, 768])
        self.assertEqual(result["truncationTrueCounts"]["widget_scan_truncated"], 1)
        self.assertEqual(len(result["sqliteRemoveDatabaseInUseWarnings"]), 2)


if __name__ == "__main__":
    unittest.main()
