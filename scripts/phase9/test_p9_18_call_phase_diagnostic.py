#!/usr/bin/env python3
"""Focused contract checks for the P9-18 call-phase diagnostic trace."""

from __future__ import annotations

import argparse
import json
import re
import unittest
from pathlib import Path
from typing import Any


RUNNER = Path(__file__).with_name("run_packaged_next_p9_18_campus_map.ps1")
TRACE_SCHEMA = "classmngr-phase9-p9-18-call-phase-memory-diagnostic-v1"
CALL_KINDS = ("uia-targeted-controls", "screenshot")
METRICS = ("workingSetBytes", "privateUsageBytes", "peakWorkingSetBytes")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?m)^function\s+{re.escape(name)}\s*\(", source)
    if match is None:
        raise AssertionError(f"Missing PowerShell function {name}.")
    following = re.search(r"(?m)^function\s+\w+\s*\(", source[match.end() :])
    end = match.end() + following.start() if following else len(source)
    return source[match.start() : end]


def is_number(value: Any) -> bool:
    return type(value) in (int, float)


def validate_trace(manifest: dict[str, Any]) -> dict[str, int]:
    diagnostic = manifest.get("callPhaseDiagnostic")
    if not isinstance(diagnostic, dict):
        raise AssertionError("Manifest is missing callPhaseDiagnostic.")
    if diagnostic.get("schema") != TRACE_SCHEMA or diagnostic.get("diagnosticOnly") is not True:
        raise AssertionError("Call-phase diagnostic schema or diagnostic-only marker is invalid.")
    samples = diagnostic.get("samples")
    windows = (manifest.get("memory") or {}).get("windows")
    process = manifest.get("process") or {}
    if not isinstance(samples, list) or not isinstance(windows, list) or not windows:
        raise AssertionError("Call-phase samples or memory windows are missing.")
    if diagnostic.get("recordCount") != len(samples):
        raise AssertionError("Call-phase recordCount differs from the trace length.")
    target_pid = process.get("processId")
    if type(target_pid) is not int:
        raise AssertionError("Manifest does not contain the measured process ID.")
    if [sample.get("sequence") for sample in samples] != list(range(len(samples))):
        raise AssertionError("Call-phase trace sequence numbers are missing or reordered.")

    counts = {"uia-targeted-controls": 0, "screenshot": 0, "after-two-second-settle": 0}
    confirmation_window_ids = {
        "warmup-open": "warmup-map-open",
        "warmup-return": "warmup-map-suspended",
    }
    for window in windows:
        window_id = window.get("windowId")
        confirmation = window.get("stateConfirmation") or {}
        for call_kind in CALL_KINDS:
            reference_key = "targetedUiaArtifact" if call_kind == "uia-targeted-controls" else "screenshot"
            expected_reference = confirmation.get(reference_key) or {}
            records = [
                sample for sample in samples
                if sample.get("recordType") == "call-bracket"
                and sample.get("callKind") == call_kind
                and (sample.get("evidenceReference") or {}).get("relativePath") == expected_reference.get("relativePath")
                and (sample.get("evidenceReference") or {}).get("sha256") == expected_reference.get("sha256")
            ]
            if len(records) != 1:
                raise AssertionError(f"{window_id} has {len(records)} {call_kind} brackets; expected one.")
            record = records[0]
            counts[call_kind] += 1
            context = record.get("context") or {}
            if context.get("processId") != target_pid or context.get("phase") != window.get("phase"):
                raise AssertionError(f"{window_id} {call_kind} is not bound to the measured PID and phase.")
            if context.get("windowId") != confirmation_window_ids.get(window_id, window_id):
                raise AssertionError(f"{window_id} {call_kind} has the wrong confirmation identifier.")
            if context.get("cycleNumber") != window.get("cycleNumber"):
                raise AssertionError(f"{window_id} {call_kind} has the wrong cycle number.")
            if context.get("pageTitle") != confirmation.get("pageTitle"):
                raise AssertionError(f"{window_id} {call_kind} has the wrong confirmed page title.")
            if record.get("operationOutcome") not in ("succeeded", "failed"):
                raise AssertionError(f"{window_id} {call_kind} has no operation outcome.")
            if not is_number(record.get("callDurationNanoseconds")) or record["callDurationNanoseconds"] < 0:
                raise AssertionError(f"{window_id} {call_kind} has an invalid call duration.")
            if not isinstance(record.get("metricErrors"), list):
                raise AssertionError(f"{window_id} {call_kind} has no counter error list.")
            before, after = record.get("before"), record.get("after")
            if not isinstance(before, dict) or not isinstance(after, dict):
                raise AssertionError(f"{window_id} {call_kind} lacks raw before/after counters.")
            for point in (before, after):
                if point.get("processId") != target_pid or not isinstance(point.get("metricErrors"), list):
                    raise AssertionError(f"{window_id} {call_kind} counter point lacks PID or errors.")
            delta = record.get("delta")
            if not isinstance(delta, dict):
                raise AssertionError(f"{window_id} {call_kind} has no signed deltas.")
            for metric in METRICS:
                before_value, after_value = before.get(metric), after.get(metric)
                expected = after_value - before_value if is_number(before_value) and is_number(after_value) else None
                if delta.get(metric) != expected:
                    raise AssertionError(f"{window_id} {call_kind} {metric} delta does not match raw values.")
            reference = record.get("evidenceReference") or {}
            if reference.get("relativePath") != expected_reference.get("relativePath") or reference.get("sha256") != expected_reference.get("sha256"):
                raise AssertionError(f"{window_id} {call_kind} is not bound to its existing route evidence.")

        settle_records = [
            sample for sample in samples
            if sample.get("recordType") == "counter-point" and sample.get("point") == "after-two-second-settle"
            and isinstance(sample.get("context"), dict) and sample["context"].get("windowId") == window_id
        ]
        if len(settle_records) != 1:
            raise AssertionError(f"{window_id} has {len(settle_records)} post-settle points; expected one.")
        settle = settle_records[0]
        counts["after-two-second-settle"] += 1
        context = settle.get("context") or {}
        point = settle.get("counterSnapshot") or {}
        if context.get("processId") != target_pid or context.get("phase") != window.get("phase") or context.get("cycleNumber") != window.get("cycleNumber"):
            raise AssertionError(f"{window_id} post-settle point is not bound to its window context.")
        if point.get("processId") != target_pid or not isinstance(point.get("metricErrors"), list):
            raise AssertionError(f"{window_id} post-settle point lacks raw counters or errors.")
        if not is_number(settle.get("settleDurationMilliseconds")) or settle["settleDurationMilliseconds"] < 1900:
            raise AssertionError(f"{window_id} post-settle point precedes the existing settle interval.")
    if counts["uia-targeted-controls"] != len(windows) or counts["screenshot"] != len(windows) or counts["after-two-second-settle"] != len(windows):
        raise AssertionError("Call-phase trace does not contain one confirmation pair and post-settle point per memory window.")
    return counts


def make_valid_manifest() -> dict[str, Any]:
    process_id = 22
    window_id = "cold-baseline"
    snapshot_reference = {"relativePath": "uia/target-controls-cold-baseline.json", "sha256": "a" * 64}
    screenshot_reference = {"relativePath": "screenshots/state-cold-baseline.png", "sha256": "b" * 64}
    context = {"windowId": window_id, "phase": "cold-baseline", "cycleNumber": None, "pageTitle": "Office Information", "processId": process_id}
    before = {
        "processId": process_id, "metricErrors": [],
        "workingSetBytes": 1000, "privateUsageBytes": 2000, "peakWorkingSetBytes": 3000,
    }
    after = {
        "processId": process_id, "metricErrors": [],
        "workingSetBytes": 1100, "privateUsageBytes": 2200, "peakWorkingSetBytes": 3100,
    }
    delta = {metric: after[metric] - before[metric] for metric in METRICS}
    call_samples = []
    for sequence, (kind, reference_key, reference) in enumerate(
        (
            (CALL_KINDS[0], "targetedUiaArtifact", snapshot_reference),
            (CALL_KINDS[1], "screenshot", screenshot_reference),
        )
    ):
        call_samples.append({
            "sequence": sequence, "recordType": "call-bracket", "callKind": kind,
            "context": context, "operationOutcome": "succeeded", "operationError": None,
            "callDurationNanoseconds": 1000, "before": before, "after": after,
            "delta": delta, "metricErrors": [], "evidenceReference": reference,
        })
    call_samples.append({
        "sequence": 2, "recordType": "counter-point", "point": "after-two-second-settle",
        "context": context, "settleDurationMilliseconds": 2000,
        "counterSnapshot": {**after, "sampleStartedAtUtc": "2026-11-24T00:00:00.000Z"},
    })
    return {
        "process": {"processId": process_id},
        "memory": {"windows": [{
            "windowId": window_id, "phase": "cold-baseline", "cycleNumber": None,
            "stateConfirmation": {
                "targetedUiaArtifact": snapshot_reference, "screenshot": screenshot_reference,
                "pageTitle": "Office Information",
            },
        }]},
        "callPhaseDiagnostic": {
            "schema": TRACE_SCHEMA, "diagnosticOnly": True,
            "recordCount": len(call_samples), "samples": call_samples,
        },
    }


class CallPhaseDiagnosticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runner = RUNNER.read_text(encoding="utf-8")

    def test_confirmation_brackets_existing_uia_and_screenshot_calls(self) -> None:
        confirmation = function_body(self.runner, "Get-P918Confirmation")
        uia_call = confirmation.index("-CallKind 'uia-targeted-controls'")
        screenshot_call = confirmation.index("-CallKind 'screenshot'")
        self.assertLess(uia_call, screenshot_call)
        self.assertEqual(confirmation.count("Save-P918TargetedUiaArtifact"), 1)
        self.assertEqual(confirmation.count("Capture-P914Screenshot"), 1)
        self.assertIn("$targetedUia.manifestRecord", confirmation)

        targeted = function_body(self.runner, "Save-P918TargetedUiaArtifact")
        self.assertIn("$cache.Activate()", targeted)
        self.assertEqual(targeted.count("Get-P918TargetedUiaElements"), 3)
        condition = function_body(self.runner, "Get-P918TargetedCondition")
        self.assertIn("AutomationIdProperty", condition)
        self.assertIn("ControlTypeProperty", condition)
        self.assertIn("if ($null -ne $Name)", condition)
        self.assertIn("Get-P918TargetedCondition $MapImageCardAutomationId 'ControlType.Custom' $null", targeted)
        self.assertIn("Get-P918TargetedCondition $MapImageAutomationId 'ControlType.Image' $null", targeted)
        self.assertIn("$Root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $Condition)", function_body(self.runner, "Get-P918TargetedUiaElements"))
        self.assertNotIn("Save-P914UiaDescendantSnapshot", targeted)
        self.assertNotIn("FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)", self.runner)

    def test_post_settle_point_keeps_existing_wait_before_memory_sampling(self) -> None:
        memory_window = function_body(self.runner, "Invoke-P918MemoryWindow")
        wait = memory_window.index("Start-Sleep -Seconds 2")
        point = memory_window.index("-PointName 'after-two-second-settle'")
        sampling_start = memory_window.index("$samplingStarted = [DateTimeOffset]::UtcNow")
        sample_context = memory_window.index("Set-P914SamplerContext")
        first_sample_sleep = memory_window.index("Start-Sleep -Milliseconds 100")
        self.assertLess(wait, point)
        self.assertLess(point, sampling_start)
        self.assertLess(sampling_start, sample_context)
        self.assertLess(sample_context, first_sample_sleep)
        self.assertEqual(memory_window.count("Start-Sleep -Seconds 2"), 1)

    def test_counter_contract_records_raw_metrics_failures_and_call_duration(self) -> None:
        counter = function_body(self.runner, "Get-P918DiagnosticProcessCounterSnapshot")
        call = function_body(self.runner, "Invoke-P918DiagnosticCall")
        self.assertIn("$Context.process.WorkingSet64", counter)
        self.assertIn("$Context.process.PrivateMemorySize64", counter)
        self.assertIn("$Context.process.PeakWorkingSet64", counter)
        self.assertIn("metricErrors", counter)
        self.assertIn("counterReadDurationNanoseconds", counter)
        self.assertLess(call.index("$before = Get-P918DiagnosticProcessCounterSnapshot"), call.index("$result = & $Operation"))
        self.assertLess(call.index("$result = & $Operation"), call.index("$after = Get-P918DiagnosticProcessCounterSnapshot"))
        self.assertIn("callDurationNanoseconds", call)
        self.assertIn("operationError", call)

    def test_exact_build_overlay_allows_only_the_updated_runner(self) -> None:
        receipt_check = function_body(self.runner, "Assert-P918BuildReceipt")
        self.assertIn("$currentSource.sha256 -cne [string]$receipt.sourceSha256", receipt_check)
        self.assertIn("$receipt.releaseExecutableSha256 -cne (Get-P914FileSha256", receipt_check)
        self.assertIn("$path -cne $CallPhaseRunnerPath", receipt_check)
        self.assertIn("$changedHarnessPaths.Count -gt 1", receipt_check)

    def test_trace_validator_rejects_missing_call_durations(self) -> None:
        manifest = make_valid_manifest()
        manifest["callPhaseDiagnostic"]["samples"][0]["callDurationNanoseconds"] = None
        with self.assertRaisesRegex(AssertionError, "invalid call duration"):
            validate_trace(manifest)

    def test_trace_validator_accepts_raw_counter_pairs_and_settle_point(self) -> None:
        self.assertEqual(
            validate_trace(make_valid_manifest()),
            {"uia-targeted-controls": 1, "screenshot": 1, "after-two-second-settle": 1},
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, help="validate a fresh P9-18 route manifest")
    args = parser.parse_args()
    if args.manifest is not None:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        counts = validate_trace(manifest)
        print(json.dumps({"status": "validated", "traceRecordCount": len(manifest["callPhaseDiagnostic"]["samples"]), "counts": counts}, sort_keys=True))
        return 0
    return unittest.main(argv=[__file__], exit=False).result.wasSuccessful() is False


if __name__ == "__main__":
    raise SystemExit(main())
