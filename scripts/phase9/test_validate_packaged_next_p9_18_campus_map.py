#!/usr/bin/env python3
"""Focused tests for P9-18 memory thresholds and cleanup formulas."""

from __future__ import annotations

import importlib.util
import json
import re
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path


SCRIPT = Path(__file__).with_name("validate_packaged_next_p9_18_campus_map.py")
RUNNER_SCRIPT = Path(__file__).with_name("run_packaged_next_p9_18_campus_map.ps1")
SPEC = importlib.util.spec_from_file_location("p9_18_validator", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


def cleanup_inputs(return_excess: int = 0):
    cold = [200_000_000] * 10
    first_return = [200_000_000] * 10
    plateau_values = [200_000_000, 202_000_000, 200_000_000, 202_000_000, 200_000_000]
    baselines = [[value] * 10 for value in plateau_values]
    cycle_values = [200_000_000, 202_000_000] * 5
    cycle_values[-1] += return_excess
    returns = [[value] * 10 for value in cycle_values]
    return cold, first_return, baselines, returns


def make_window(window_id: str, value: int, process_id: int, hwnd: int, index: int) -> dict:
    confirmed = datetime(2026, 1, 1, tzinfo=timezone.utc) + timedelta(minutes=index)
    sampling = confirmed + timedelta(seconds=2)
    samples = []
    for sample_index in range(10):
        captured = sampling + timedelta(milliseconds=sample_index * 100)
        samples.append(
            {
                "sampleIndex": sample_index,
                "capturedAtUtc": captured.isoformat().replace("+00:00", "Z"),
                "elapsedMilliseconds": sample_index * 100,
                "processId": process_id,
                "processState": "running",
                "workingSetBytes": value,
                "privateBytes": value,
                "peakWorkingSetBytes": value,
                "handles": 100,
                "threadCount": 10,
                "metricErrors": [],
            }
        )
    return {
        "windowId": window_id,
        "processId": process_id,
        "mainWindowHandle": hwnd,
        "stateConfirmedBeforeSampling": True,
        "settleDelaySeconds": 2,
        "sampleIntervalMilliseconds": 100,
        "samplingStartedAtUtc": sampling.isoformat().replace("+00:00", "Z"),
        "samplingCompleted": True,
        "samplingErrorCount": 0,
        "samplingErrors": [],
        "sampleCount": 10,
        "stateConfirmation": {"confirmedAtUtc": confirmed.isoformat().replace("+00:00", "Z")},
        "samples": samples,
    }


def make_memory_manifest(inclusive_ceiling: int = 300_000_000, return_excess: int = 0) -> dict:
    process_id = 43123
    hwnd = 987654
    ids = validator.expected_window_ids()
    values = {
        "cold-baseline": 200_000_000,
        "warmup-open": 220_000_000,
        "warmup-return": 200_000_000,
    }
    plateau = [200_000_000, 202_000_000, 200_000_000, 202_000_000, 200_000_000]
    for index, value in enumerate(plateau, start=1):
        values[f"plateau-baseline-{index:02d}"] = value
    for index in range(1, 11):
        values[f"cycle-{index:02d}-map-open"] = 220_000_000
        values[f"cycle-{index:02d}-return"] = [200_000_000, 202_000_000][(index - 1) % 2]
    values["cycle-10-map-open"] = inclusive_ceiling
    if return_excess:
        values["cycle-10-return"] += return_excess
    windows = [make_window(name, values[name], process_id, hwnd, index) for index, name in enumerate(ids)]
    for window in windows:
        samples = window["samples"]
        value = samples[0]["workingSetBytes"]
        target_indexes = list(range(10)) if value >= validator.WORKING_SET_TARGET else []
        ceiling_indexes = list(range(10)) if value > validator.WORKING_SET_CEILING else []
        window["workingSetTargetExceededSampleIndexes"] = target_indexes
        window["workingSetTargetMet"] = not target_indexes
        window["workingSetCeilingExceededSampleIndexes"] = ceiling_indexes
        window["workingSetCeilingPassed"] = not ceiling_indexes
    return {
        "process": {"processId": process_id, "mainWindowHandle": hwnd},
        "route": {"warmupCount": 1, "baselineWindowsPerState": 5, "measuredCycleCount": 10},
        "memory": {
            "windows": windows,
            "coldBaselineWindowId": "cold-baseline",
            "coldReturnWindowId": "warmup-return",
            "plateauBaselineWindowIds": [f"plateau-baseline-{i:02d}" for i in range(1, 6)],
            "workingSetTargetBytes": validator.WORKING_SET_TARGET,
            "workingSetTargetExclusive": True,
            "workingSetCeilingBytes": validator.WORKING_SET_CEILING,
            "workingSetCeilingExclusive": False,
            "operationPeakAdvisoryLimitBytes": validator.OPERATION_PEAK_ADVISORY,
            "operationPeakAdvisoryOnly": True,
            "operationPeakWorkingSetBytes": validator.OPERATION_PEAK_ADVISORY,
        },
    }


def make_bundang_fixture_image() -> dict:
    image_hash = "a" * 64
    resource_id = "resource:assets/campuses/bundang/bundang_map.png"
    return {
        "resourceId": resource_id,
        "sizeBytes": 615_603,
        "widthPixels": 1230,
        "heightPixels": 1150,
        "sha256": image_hash,
        "catalogRecord": {
            "location": "resource_file",
            "resource_kind": "campus_map_image",
            "expected_type": "png",
            "logical_id": resource_id,
            "package_relative_path": "assets/campuses/bundang/bundang_map.png",
            "provenance": {
                "source": {"sha256": image_hash},
                "baked": {"sha256": image_hash},
            },
        },
    }


def make_packaged_map_load(fixture_image: dict) -> dict:
    return {
        "event": "resource-load",
        "resource_id": fixture_image["resourceId"],
        "outcome": "succeeded",
        "request_scope": {"kind": "page", "owner": "campus_map"},
        "catalog_resource": {"location": "package_file", "kind": "campus_map_image", "type": "png"},
        "image": {
            "source_dimensions": {"width_pixels": fixture_image["widthPixels"], "height_pixels": fixture_image["heightPixels"]},
            "decoded_dimensions": {"width_pixels": fixture_image["widthPixels"], "height_pixels": fixture_image["heightPixels"]},
        },
        "payload_accounting": {
            "source_catalog_bytes": fixture_image["sizeBytes"],
            "decoded_payload_bytes": 5_658_000,
        },
    }


def make_targeted_uia_artifact(title: str = "Maps", map_control_count: int = 2) -> dict:
    process_id = 43123
    hwnd = 987654
    contract = validator.expected_targeted_uia_contract(title)

    def elements(key: str, offscreen: bool = False) -> list[dict]:
        target = contract["controls"][key]
        count = 1 if key == "pageTitle" else map_control_count
        return [
            {
                "name": target["name"],
                "automationId": target["automationId"],
                "controlType": target["controlType"],
                "processId": process_id,
                "nativeWindowHandle": 0,
                "ownerWindowHandle": hwnd,
                "ownerWindowProcessId": process_id,
                "className": "QLabel",
                "isEnabled": True,
                "isOffscreen": offscreen,
                "rectangle": {"x": 0, "y": 0, "width": 10, "height": 10},
            }
            for _ in range(count)
        ]

    controls = {
        key: {
            **target,
            "count": len(elements(key)),
            "elements": elements(key),
        }
        for key, target in contract["controls"].items()
    }
    if title == "Office Information":
        for key in ("mapImageCard", "mapImage"):
            controls[key]["count"] = 0
            controls[key]["elements"] = []
    return {
        "schema": validator.TARGETED_UIA_SCHEMA,
        "taskId": validator.TASK_ID,
        "name": "state-cold-baseline",
        "processId": process_id,
        "mainWindowHandle": hwnd,
        "windowProcessId": process_id,
        "rootNativeWindowHandle": hwnd,
        "expectedPageTitle": title,
        "root": {
            "name": "ClassMngr Next",
            "automationId": validator.MAIN_WINDOW_AUTOMATION_ID,
            "controlType": "ControlType.Window",
            "processId": process_id,
            "ownerWindowHandle": hwnd,
            "ownerWindowProcessId": process_id,
            "isOffscreen": False,
        },
        "query": {
            "method": "AutomationElement.FindAll with property-constrained AndCondition and CacheRequest.Activate",
            "treeScope": "Descendants",
            "cacheRequestUsed": True,
            "cacheTreeScope": "Element",
            "cachedProperties": validator.TARGETED_UIA_CACHED_PROPERTIES,
            "targetPageObjectAutomationId": contract["pageObjectAutomationId"],
            "targets": contract["queryTargets"],
        },
        "controls": controls,
    }


class CleanupFormulaTests(unittest.TestCase):
    def test_phase9_cleanup_formula_uses_baseline_range_and_fitted_trends(self) -> None:
        cold, first_return, baselines, returns = cleanup_inputs()
        analysis = validator.compute_cleanup_analysis(cold, first_return, baselines, returns)

        self.assertTrue(analysis["passed"])
        cold_result = analysis["coldBaselineToFirstReturn"]
        self.assertEqual(cold_result["maximumAllowedReturnMedianBytes"], 200_000_000)
        plateau = analysis["postPrimingPlateauAndCycleReturns"]
        self.assertEqual(plateau["baselineMedianWorkingSetBytes"], 200_000_000)
        self.assertEqual(plateau["baselineNoiseEnvelopeBytes"], 2_000_000)
        self.assertEqual(plateau["maximumAllowedReturnMedianBytes"], 202_000_000)
        self.assertTrue(plateau["cycleTrendWithinBaselineEnvelope"])

    def test_cold_and_plateau_overruns_fail_without_changing_allowances(self) -> None:
        cold, first_return, baselines, returns = cleanup_inputs(return_excess=2_000_001)
        cold[0] -= 2_000_000
        first_return = [value + 4_000_000 for value in first_return]
        analysis = validator.compute_cleanup_analysis(cold, first_return, baselines, returns)

        self.assertFalse(analysis["passed"])
        self.assertFalse(analysis["coldBaselineToFirstReturn"]["passed"])
        self.assertFalse(analysis["postPrimingPlateauAndCycleReturns"]["passed"])

    def test_strict_target_miss_and_inclusive_ceiling_are_reported_separately(self) -> None:
        manifest = make_memory_manifest(validator.WORKING_SET_CEILING)
        summary = validator.validate_memory(manifest)

        self.assertTrue(summary["passed"])
        self.assertEqual(summary["maximumSettledWorkingSetBytes"], validator.WORKING_SET_CEILING)
        self.assertIn("cycle-10-map-open", summary["windowsWithTargetMisses"])
        self.assertFalse(summary["settledCeilingExceedances"])
        self.assertEqual(summary["operationPeakWorkingSetBytes"], validator.OPERATION_PEAK_ADVISORY)
        self.assertTrue(summary["operationPeakAdvisoryExceeded"])

    def test_one_byte_above_inclusive_ceiling_fails(self) -> None:
        manifest = make_memory_manifest(validator.WORKING_SET_CEILING + 1)

        with self.assertRaisesRegex(validator.ValidationError, "inclusive 450 MiB ceiling"):
            validator.validate_memory(manifest)

    def test_failed_cold_and_plateau_cleanup_details_fail_the_validation_gate(self) -> None:
        manifest = make_memory_manifest()
        windows = {window["windowId"]: window for window in manifest["memory"]["windows"]}
        for window_id, value in (("warmup-return", 205_000_000), ("cycle-01-return", 205_000_000)):
            for sample in windows[window_id]["samples"]:
                sample["workingSetBytes"] = value

        details = validator.validate_memory(manifest)
        self.assertFalse(details["passed"])
        self.assertFalse(details["cleanupGates"]["coldBaselineToFirstReturn"]["passed"])
        self.assertFalse(details["cleanupGates"]["postPrimingPlateauAndCycleReturns"]["passed"])

        report = {"gates": {}}
        validator.add_gate(report, "settledMemoryAndCleanupGates", lambda: details)
        gate = report["gates"]["settledMemoryAndCleanupGates"]
        self.assertFalse(gate["passed"])
        self.assertEqual(gate["details"], details)
        self.assertTrue(any(reason.startswith("Cold cleanup gate failed:") for reason in gate["failureReasons"]))
        self.assertTrue(any(reason.startswith("Post-suspension returns exceed") for reason in gate["failureReasons"]))
        self.assertFalse(all(item["passed"] for item in report["gates"].values()))

    def test_gate_does_not_treat_non_boolean_reported_pass_as_success(self) -> None:
        report = {"gates": {}}
        details = {"passed": 1, "evidence": "malformed status"}

        validator.add_gate(report, "contract", lambda: details)

        gate = report["gates"]["contract"]
        self.assertFalse(gate["passed"])
        self.assertEqual(gate["details"], details)
        self.assertIn("non-boolean passed field", gate["failureReasons"][0])


class TargetedUiaArtifactTests(unittest.TestCase):
    def test_memory_window_ids_map_to_existing_warmup_confirmation_artifact_names(self) -> None:
        self.assertEqual(
            validator.targeted_uia_artifact_name_for_window("warmup-open"),
            "state-warmup-map-open",
        )
        self.assertEqual(
            validator.targeted_uia_artifact_name_for_window("warmup-return"),
            "state-warmup-map-suspended",
        )
        self.assertEqual(
            validator.targeted_uia_artifact_name_for_window("cycle-10-return"),
            "state-cycle-10-return",
        )

    def test_maps_and_information_artifacts_prove_exact_conditional_control_counts(self) -> None:
        maps = make_targeted_uia_artifact("Maps")
        self.assertNotIn("name", maps["query"]["targets"]["mapImageCard"])
        self.assertNotIn("name", maps["query"]["targets"]["mapImage"])
        self.assertEqual(maps["controls"]["mapImageCard"]["name"], "")
        self.assertEqual(maps["controls"]["mapImage"]["name"], "")
        self.assertEqual(
            validator.validate_targeted_uia_artifact(maps, "Maps", 43123, 987654),
            {"pageTitle": 1, "mapImageCard": 2, "mapImage": 2},
        )

        maps_without_exposed_map_controls = make_targeted_uia_artifact("Maps", map_control_count=0)
        self.assertEqual(
            validator.validate_targeted_uia_artifact(maps_without_exposed_map_controls, "Maps", 43123, 987654),
            {"pageTitle": 1, "mapImageCard": 0, "mapImage": 0},
        )

        information = make_targeted_uia_artifact("Office Information")
        self.assertEqual(
            validator.validate_targeted_uia_artifact(information, "Office Information", 43123, 987654),
            {"pageTitle": 1, "mapImageCard": 0, "mapImage": 0},
        )

    def test_artifact_rejects_wrong_pid_hwnd_and_page_title(self) -> None:
        mutations = (
            ("processId", 99999, "measured PID and main HWND"),
            ("mainWindowHandle", 123, "measured PID and main HWND"),
            ("windowProcessId", 99999, "measured PID and main HWND"),
            ("rootNativeWindowHandle", 123, "measured PID and main HWND"),
            ("expectedPageTitle", "Office Information", "page title differs"),
        )
        for key, value, message in mutations:
            with self.subTest(key=key):
                artifact = make_targeted_uia_artifact("Maps")
                artifact[key] = value
                with self.assertRaisesRegex(validator.ValidationError, message):
                    validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        wrong_root = make_targeted_uia_artifact("Maps")
        wrong_root["root"]["ownerWindowProcessId"] = 99999
        with self.assertRaisesRegex(validator.ValidationError, "main-window root identity"):
            validator.validate_targeted_uia_artifact(wrong_root, "Maps", 43123, 987654)

    def test_artifact_rejects_missing_extra_hidden_and_malformed_controls(self) -> None:
        artifact = make_targeted_uia_artifact("Maps")
        del artifact["controls"]["mapImage"]
        with self.assertRaisesRegex(validator.ValidationError, "missing or extra control groups"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Maps")
        artifact["controls"]["unrelated"] = {}
        with self.assertRaisesRegex(validator.ValidationError, "missing or extra control groups"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Maps")
        artifact["controls"]["mapImage"]["count"] = 1
        with self.assertRaisesRegex(validator.ValidationError, "count is malformed"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Maps", map_control_count=1)
        with self.assertRaisesRegex(validator.ValidationError, "count must be zero or exactly two"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Maps")
        artifact["controls"]["mapImage"]["elements"][0]["isOffscreen"] = True
        with self.assertRaisesRegex(validator.ValidationError, "not all visible"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Maps")
        artifact["controls"]["pageTitle"]["elements"][0]["isOffscreen"] = True
        with self.assertRaisesRegex(validator.ValidationError, "one exact visible page title"):
            validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        artifact = make_targeted_uia_artifact("Office Information")
        artifact["controls"]["mapImageCard"] = {
            **artifact["controls"]["mapImageCard"],
            "count": 1,
            "elements": [make_targeted_uia_artifact("Maps")["controls"]["mapImageCard"]["elements"][0]],
        }
        with self.assertRaisesRegex(validator.ValidationError, "Information state still exposes"):
            validator.validate_targeted_uia_artifact(artifact, "Office Information", 43123, 987654)

    def test_artifact_checks_exact_automationids_types_names_and_process_per_element(self) -> None:
        mutations = (
            ("automationId", "wrong-control", "AutomationId, type, or name"),
            ("controlType", "ControlType.Button", "AutomationId, type, or name"),
            ("name", "unexpected name", "AutomationId, type, or name"),
            ("processId", 99999, "malformed or foreign element"),
            ("ownerWindowHandle", 123, "malformed or foreign element"),
        )
        for key, value, message in mutations:
            with self.subTest(key=key):
                artifact = make_targeted_uia_artifact("Maps")
                artifact["controls"]["mapImage"]["elements"][0][key] = value
                with self.assertRaisesRegex(validator.ValidationError, message):
                    validator.validate_targeted_uia_artifact(artifact, "Maps", 43123, 987654)

        wrong_query = make_targeted_uia_artifact("Maps")
        wrong_query["query"]["targets"]["mapImage"]["automationId"] = "unbounded-or-wrong-id"
        with self.assertRaisesRegex(validator.ValidationError, "exact bounded cached property queries"):
            validator.validate_targeted_uia_artifact(wrong_query, "Maps", 43123, 987654)

        wrong_query = make_targeted_uia_artifact("Maps")
        wrong_query["query"]["targets"]["mapImage"]["name"] = ""
        with self.assertRaisesRegex(validator.ValidationError, "exact bounded cached property queries"):
            validator.validate_targeted_uia_artifact(wrong_query, "Maps", 43123, 987654)


class HarnessContractTests(unittest.TestCase):
    def test_fixture_source_resource_file_and_packaged_runtime_package_file_are_bound_separately(self) -> None:
        fixture_image = make_bundang_fixture_image()
        catalog = fixture_image["catalogRecord"]
        validator.validate_fixture_catalog_record(
            catalog,
            fixture_image["resourceId"],
            "assets/campuses/bundang/bundang_map.png",
            fixture_image["sha256"],
        )

        runtime_load = make_packaged_map_load(fixture_image)
        validator.validate_packaged_map_resource_load(runtime_load, fixture_image, "warmup")

        wrong_runtime_location = dict(runtime_load)
        wrong_runtime_location["catalog_resource"] = dict(runtime_load["catalog_resource"], location="resource_file")
        with self.assertRaisesRegex(validator.ValidationError, "packaged catalog type/location"):
            validator.validate_packaged_map_resource_load(wrong_runtime_location, fixture_image, "warmup")

        wrong_fixture_location = dict(catalog, location="package_file")
        with self.assertRaisesRegex(validator.ValidationError, "does not bind the current Bundang image bytes"):
            validator.validate_fixture_catalog_record(
                wrong_fixture_location,
                fixture_image["resourceId"],
                "assets/campuses/bundang/bundang_map.png",
                fixture_image["sha256"],
            )

        wrong_fixture_hash = {
            **catalog,
            "provenance": {**catalog["provenance"], "source": {"sha256": "b" * 64}},
        }
        with self.assertRaisesRegex(validator.ValidationError, "does not bind the current Bundang image bytes"):
            validator.validate_fixture_catalog_record(
                wrong_fixture_hash,
                fixture_image["resourceId"],
                "assets/campuses/bundang/bundang_map.png",
                fixture_image["sha256"],
            )

    def test_packaged_map_load_requires_exact_payload_and_decoded_dimensions(self) -> None:
        fixture_image = make_bundang_fixture_image()
        runtime_load = make_packaged_map_load(fixture_image)

        wrong_decoded_dimensions = {
            **runtime_load,
            "image": {
                **runtime_load["image"],
                "decoded_dimensions": {"width_pixels": 1229, "height_pixels": 1150},
            },
        }
        with self.assertRaisesRegex(validator.ValidationError, "source/decoded dimensions differ"):
            validator.validate_packaged_map_resource_load(wrong_decoded_dimensions, fixture_image, "warmup")

        wrong_source_bytes = {
            **runtime_load,
            "payload_accounting": {**runtime_load["payload_accounting"], "source_catalog_bytes": 615_602},
        }
        with self.assertRaisesRegex(validator.ValidationError, "decoded payload and source bytes"):
            validator.validate_packaged_map_resource_load(wrong_source_bytes, fixture_image, "warmup")

        missing_decoded_payload = {
            **runtime_load,
            "payload_accounting": {**runtime_load["payload_accounting"], "decoded_payload_bytes": 0},
        }
        with self.assertRaisesRegex(validator.ValidationError, "decoded payload and source bytes"):
            validator.validate_packaged_map_resource_load(missing_decoded_payload, fixture_image, "warmup")

        wrong_resource_id = {**runtime_load, "resource_id": "resource:assets/campuses/other/map.png"}
        with self.assertRaisesRegex(validator.ValidationError, "Runtime resource ID differs"):
            validator.validate_packaged_map_resource_load(wrong_resource_id, fixture_image, "warmup")

        wrong_resource_kind = {
            **runtime_load,
            "catalog_resource": {**runtime_load["catalog_resource"], "kind": "generic_image"},
        }
        with self.assertRaisesRegex(validator.ValidationError, "packaged catalog type/location"):
            validator.validate_packaged_map_resource_load(wrong_resource_kind, fixture_image, "warmup")

    def test_inventory_row_comparison_ignores_filesystem_enumeration_order(self) -> None:
        rows = [
            {"path": "zeta/file.bin", "sizeBytes": 4, "sha256": "B" * 64},
            {"path": "Alpha/file.bin", "sizeBytes": 3, "sha256": "a" * 64},
        ]
        self.assertEqual(
            validator.canonical_inventory_rows(rows),
            validator.canonical_inventory_rows(list(reversed(rows))),
        )

    def test_inventory_row_comparison_rejects_changed_path_or_hash(self) -> None:
        baseline = [{"path": "resources/image.png", "sizeBytes": 9, "sha256": "a" * 64}]
        changed_path = [{"path": "resources/other.png", "sizeBytes": 9, "sha256": "a" * 64}]
        changed_hash = [{"path": "resources/image.png", "sizeBytes": 9, "sha256": "b" * 64}]

        self.assertNotEqual(
            validator.canonical_inventory_rows(baseline),
            validator.canonical_inventory_rows(changed_path),
        )
        self.assertNotEqual(
            validator.canonical_inventory_rows(baseline),
            validator.canonical_inventory_rows(changed_hash),
        )

    def test_runner_never_assigns_powershell_pid_automatic_variable(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        self.assertNotRegex(runner, r"(?i)\$pid\s*(?:=|\+=|-=|\+\+|--)")
        self.assertIn("$measuredProcessId", runner)
        self.assertIn("windowGeometry=@ByteArray(", runner)

    def test_campus_directory_fallback_uses_screenshot_disclosure_pixel(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        start = runner.index("function Expand-P918CampusDirectory")
        end = runner.index("function Get-P918LifecycleInfo", start)
        expand = runner[start:end]

        self.assertIn("Capture-P914Screenshot $Context $Manifest $Directory 'campus-directory-navigation-before-disclosure-click'", expand)
        self.assertIn("Read-Host 'Measured disclosure pixel x,y'", expand)
        self.assertIn("if ($x -ge $rectangle.X -or $y -lt $rectangle.Y -or $y -ge ($rectangle.Y+$rectangle.Height))", expand)
        self.assertIn("if ($x -lt $treeRectangle.X -or $x -ge ($treeRectangle.X+$treeRectangle.Width)", expand)
        self.assertIn("if (($freshBounds -join ',') -cne ($bounds -join ','))", expand)
        self.assertIn("$fallback.expansionStateAfter = 'Expanded'", expand)
        self.assertNotIn("$rectangle.X + $rectangle.Width / 2", expand)

    def test_runtime_resource_assertion_separates_source_catalog_and_package_load_metadata(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        start = runner.index("function Assert-P918MapResourceLoads")
        end = runner.index("function Invoke-P918MemoryWindow", start)
        assertion = runner[start:end]
        self.assertIn("$fixtureImage[0].catalogRecord.location -cne 'resource_file'", assertion)
        self.assertIn("$record.catalog_resource.location -cne 'package_file'", assertion)
        self.assertIn("$record.payload_accounting.source_catalog_bytes -ne [Int64]$fixtureImage[0].sizeBytes", assertion)
        self.assertIn("$decodedDimensions.width_pixels -ne [int]$fixtureImage[0].widthPixels", assertion)
        self.assertIn("$decodedDimensions.height_pixels -ne [int]$fixtureImage[0].heightPixels", assertion)

    def test_route_and_lifecycle_guards_bind_map_activation_before_return_baselines(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        route_start = runner.index("function Invoke-P918Route")
        route = runner[route_start:]
        baseline_loop = route.index("for ($index = 1; $index -le 5; $index++)")

        for guard in (
            "Assert-P918RouteTarget $initialRoute 'campus_information'",
            "Assert-P918LifecycleEvent $infoActivated 'campus_information' 'activated'",
        ):
            self.assertLess(route.index(guard), route.index("$coldWindow = Invoke-P918MemoryWindow"))
        for guard in (
            "Assert-P918RouteTarget $warmOpenRoute 'campus_map'",
            "Assert-P918LifecycleEvent $warmMapActivated 'campus_map' 'activated'",
            "Assert-P918LifecycleEvent $warmInfoSuspended 'campus_information' 'suspended'",
        ):
            self.assertLess(route.index(guard), baseline_loop)

    def test_lifecycle_state_spelling_matches_case_sensitive_emitter_and_validator(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        common = (RUNNER_SCRIPT.parent / "p9_14_packaged_lifecycle_common.ps1").read_text(encoding="utf-8")
        lifecycle_states = re.findall(
            r"Wait-P914Lifecycle\s+\$Context\s+\$Directory\s+'[^']+'\s+'([^']+)'",
            runner,
        )
        self.assertGreater(len(lifecycle_states), 0)
        self.assertEqual(set(lifecycle_states), {"created", "activated", "suspended"})
        self.assertIn("[string]$_.record.state -ceq $State", common)

        lowercase_events = [
            {"pageKey": "campus_map", "state": "activated", "pageInstanceAvailable": True},
            {"pageKey": "campus_information", "state": "suspended", "pageInstanceAvailable": True},
        ]
        validator.validate_lifecycle_pair(
            lowercase_events, "campus_map", "activated", "campus_information", "suspended", "lowercase route"
        )
        uppercase_events = [dict(event, state=event["state"].capitalize()) for event in lowercase_events]
        with self.assertRaisesRegex(validator.ValidationError, "Lifecycle confirmation is incomplete"):
            validator.validate_lifecycle_pair(
                uppercase_events, "campus_map", "activated", "campus_information", "suspended", "uppercase route"
            )

    def test_first_warmup_binds_created_suspend_activate_event_order(self) -> None:
        runner = RUNNER_SCRIPT.read_text(encoding="utf-8")
        route = runner[runner.index("function Invoke-P918Route") :]
        baseline_loop = route.index("for ($index = 1; $index -le 5; $index++)")
        self.assertIn("$expectedInstanceAvailable = $State -cne 'created'", runner)
        self.assertIn("$instanceAvailableProperty.Value -isnot [bool]", runner)
        self.assertIn("$sampleAvailableProperty.Value -isnot [bool]", runner)
        for evidence in (
            "$warmMapCreated = Wait-P914Lifecycle $Context $Directory 'campus_map' 'created'",
            "$warmActivationLife = @($warmMapCreated,$warmInfoSuspended,$warmMapActivated)",
            "Assert-P918LifecycleSequence $warmActivationLife @('campus_map/created','campus_information/suspended','campus_map/activated')",
        ):
            self.assertLess(route.index(evidence), baseline_loop)

        first_activation = [
            {"pageKey": "campus_map", "state": "created", "arrivalIndex": 40, "pageInstanceAvailable": False, "sampleAvailable": True},
            {"pageKey": "campus_information", "state": "suspended", "arrivalIndex": 41, "pageInstanceAvailable": True, "sampleAvailable": True},
            {"pageKey": "campus_map", "state": "activated", "arrivalIndex": 42, "pageInstanceAvailable": True, "sampleAvailable": True},
        ]
        expected = [
            ("campus_map", "created"),
            ("campus_information", "suspended"),
            ("campus_map", "activated"),
        ]
        validator.validate_lifecycle_sequence(first_activation, expected, "first map warm-up activation")
        with self.assertRaisesRegex(validator.ValidationError, "Lifecycle sequence differs"):
            validator.validate_lifecycle_sequence(list(reversed(first_activation)), expected, "reordered map warm-up")
        incorrect_created_availability = [dict(event) for event in first_activation]
        incorrect_created_availability[0]["pageInstanceAvailable"] = True
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state"):
            validator.validate_lifecycle_sequence(incorrect_created_availability, expected, "created instance must be absent")
        missing_process_sample = [dict(event) for event in first_activation]
        missing_process_sample[0]["sampleAvailable"] = False
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state or process sample"):
            validator.validate_lifecycle_sequence(missing_process_sample, expected, "missing lifecycle process sample")
        missing_instance_property = [dict(event) for event in first_activation]
        del missing_instance_property[0]["pageInstanceAvailable"]
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state or process sample"):
            validator.validate_lifecycle_sequence(missing_instance_property, expected, "missing lifecycle instance property")
        untyped_instance_property = [dict(event) for event in first_activation]
        untyped_instance_property[0]["pageInstanceAvailable"] = "false"
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state or process sample"):
            validator.validate_lifecycle_sequence(untyped_instance_property, expected, "untyped lifecycle instance property")
        untyped_sample_property = [dict(event) for event in first_activation]
        untyped_sample_property[0]["sampleAvailable"] = "true"
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state or process sample"):
            validator.validate_lifecycle_sequence(untyped_sample_property, expected, "untyped lifecycle sample property")
        missing_sample_property = [dict(event) for event in first_activation]
        del missing_sample_property[0]["sampleAvailable"]
        with self.assertRaisesRegex(validator.ValidationError, "page-instance state or process sample"):
            validator.validate_lifecycle_sequence(missing_sample_property, expected, "missing lifecycle sample property")
        for index in (1, 2):
            missing_page_instance = [dict(event) for event in first_activation]
            missing_page_instance[index]["pageInstanceAvailable"] = False
            with self.subTest(index=index), self.assertRaisesRegex(validator.ValidationError, "page-instance state"):
                validator.validate_lifecycle_sequence(missing_page_instance, expected, "activated/suspended instance required")

        singleton_information_activation = {
            "pageKey": "campus_information",
            "state": "activated",
            "arrivalIndex": 39,
            "pageInstanceAvailable": True,
            "sampleAvailable": True,
        }
        validator.validate_lifecycle_sequence(
            singleton_information_activation,
            [("campus_information", "activated")],
            "cold or plateau singleton lifecycle event",
        )
        with self.assertRaisesRegex(validator.ValidationError, "malformed shape"):
            validator.validate_lifecycle_sequence(
                ["campus_information"],
                [("campus_information", "activated")],
                "malformed singleton lifecycle evidence",
            )


if __name__ == "__main__":
    unittest.main()
