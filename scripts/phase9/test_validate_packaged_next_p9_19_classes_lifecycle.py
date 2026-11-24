"""Focused P9-19 validator contract tests."""

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("validate_packaged_next_p9_19_classes_lifecycle.py")
SPEC = importlib.util.spec_from_file_location("p919_validator", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)


class ValidateP919LifecycleTests(unittest.TestCase):
    def test_build_source_snapshot_verifies_only_pinned_resource_crlf_normalization(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            repository = base / "repo"
            evidence = base / "evidence"
            snapshot = evidence / "build-source"
            for root in (repository, snapshot):
                (root / "docs/qt-rewrite").mkdir(parents=True)
                (root / "resources").mkdir()
                (root / "src").mkdir()
            canonical = b'{"label":"fixture"}\n'
            raw = canonical.replace(b"\n", b"\r\n")
            resource = {
                "source_path": "resources/fixture.json",
                "source_file": {"bytes": len(canonical), "sha256": validator.sha256_bytes(canonical)},
            }
            qml_canonical = b"Item { objectName: \"fixture\" }\n"
            qml_raw = qml_canonical.replace(b"\n", b"\r\n")
            qml_source = {
                "source_path": "src/calendar.qml",
                "source_file": {"bytes": len(qml_canonical), "sha256": validator.sha256_bytes(qml_canonical)},
            }
            inventory_bytes = json.dumps({"resources": [resource], "calendar_qml_sources": [qml_source]}, separators=(",", ":")).encode("utf-8")
            (repository / "docs/qt-rewrite/phase-4-resource-inventory.json").write_bytes(inventory_bytes)
            (snapshot / "docs/qt-rewrite/phase-4-resource-inventory.json").write_bytes(inventory_bytes)
            (repository / "resources/fixture.json").write_bytes(raw)
            (snapshot / "resources/fixture.json").write_bytes(canonical)
            (repository / "src/calendar.qml").write_bytes(qml_raw)
            (snapshot / "src/calendar.qml").write_bytes(qml_canonical)
            cpp = b"int main() { return 0; }\r\n"
            (repository / "src/entry.cpp").write_bytes(cpp)
            (snapshot / "src/entry.cpp").write_bytes(cpp)
            roots = ["docs/qt-rewrite/phase-4-resource-inventory.json", "resources", "src"]
            current = validator.source_inventory(repository, roots)
            built = validator.source_inventory(snapshot, roots)
            source_inventory_path = evidence / "source-files.json"
            build_inventory_path = evidence / "build-source-files.json"
            source_inventory_path.write_text(json.dumps(current), encoding="utf-8")
            build_inventory_path.write_text(json.dumps(built), encoding="utf-8")
            transformations = [
                {
                    "path": "resources/fixture.json",
                    "originalRawBytes": len(raw),
                    "originalRawSha256": validator.sha256_bytes(raw),
                    "buildSourceBytes": len(canonical),
                    "buildSourceSha256": validator.sha256_bytes(canonical),
                    "normalization": "CRLF-to-LF only; exact pinned source bytes and SHA-256 verified",
                },
                {
                    "path": "src/calendar.qml",
                    "originalRawBytes": len(qml_raw),
                    "originalRawSha256": validator.sha256_bytes(qml_raw),
                    "buildSourceBytes": len(qml_canonical),
                    "buildSourceSha256": validator.sha256_bytes(qml_canonical),
                    "normalization": "CRLF-to-LF only; exact pinned source bytes and SHA-256 verified",
                },
            ]
            receipt = {
                "sourceIncludedRoots": roots,
                "sourceInventoryPath": str(source_inventory_path),
                "sourceInventoryFileSha256": validator.sha256_file(source_inventory_path),
                "sourceSha256": current["sha256"],
                "sourceFileCount": current["fileCount"],
                "buildSourceRoot": str(snapshot),
                "buildSourceInventoryPath": str(build_inventory_path),
                "buildSourceInventoryFileSha256": validator.sha256_file(build_inventory_path),
                "buildSourceSha256": built["sha256"],
                "buildSourceFileCount": built["fileCount"],
                "buildSourceNormalization": {
                    "root": str(snapshot),
                    "normalizedResourceCount": len(transformations),
                    "normalizedResources": transformations,
                },
            }
            proof = validator.validate_build_source_snapshot(receipt, repository, evidence)
            self.assertEqual(proof["normalizedResourceCount"], 2)

            (snapshot / "src/entry.cpp").write_bytes(cpp + b"// changed\n")
            changed = validator.source_inventory(snapshot, roots)
            build_inventory_path.write_text(json.dumps(changed), encoding="utf-8")
            receipt["buildSourceSha256"] = changed["sha256"]
            receipt["buildSourceFileCount"] = changed["fileCount"]
            receipt["buildSourceInventoryFileSha256"] = validator.sha256_file(build_inventory_path)
            with self.assertRaisesRegex(validator.ValidationError, "changed a non-resource source file"):
                validator.validate_build_source_snapshot(receipt, repository, evidence)

    def test_fixed_sequence_includes_pre_route_cold_baseline_and_all_measured_windows(self) -> None:
        windows = validator.expected_window_ids()
        self.assertEqual(len(windows), 28)
        self.assertEqual(windows[:8], [
            "cold-baseline", "warmup-classes", "warmup-return",
            "plateau-baseline-01", "plateau-baseline-02", "plateau-baseline-03",
            "plateau-baseline-04", "plateau-baseline-05",
        ])
        self.assertEqual(windows[-2:], ["cycle-10-classes", "cycle-10-return"])
        self.assertEqual(len(validator.expected_transition_pairs()), 22)

    def test_memory_formulas_delegate_to_the_unchanged_p918_calculation(self) -> None:
        cold = [100, 102, 101, 99, 100, 103, 100, 101, 99, 100]
        first_return = [101, 101, 102, 102, 101, 100, 101, 101, 102, 100]
        baselines = [[100 + baseline + (sample % 2) for sample in range(10)] for baseline in range(5)]
        returns = [[101 + (cycle % 2) + (sample % 2) for sample in range(10)] for cycle in range(10)]
        result = validator.p918.compute_cleanup_analysis(cold, first_return, baselines, returns)
        self.assertTrue(result["coldBaselineToFirstReturn"]["passed"])
        self.assertIn("postPrimingPlateauAndCycleReturns", result)
        with self.assertRaisesRegex(ValueError, "five ten-sample"):
            validator.p918.compute_cleanup_analysis(cold, first_return, baselines[:4], returns)

    def test_sweep_requires_compact_pid_hwnd_bound_per_row_and_matching_app_checkpoint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            rows = []
            for class_id, name in enumerate(validator.CLASS_NAMES, start=1):
                rows.append({
                    "actionKind": "select-fixture-class",
                    "sweepId": "class-selections-warmup",
                    "cycleNumber": None,
                    "classId": class_id,
                    "className": name,
                    "selectedListItemName": name,
                    "selectionItemPatternIsSelected": True,
                    "target": {
                        "name": name,
                        "automationId": f"classListItem_{class_id}",
                        "controlType": "ControlType.ListItem",
                        "processId": 222,
                        "ownerWindowHandle": 333,
                        "ownerWindowProcessId": 222,
                    },
                })
            line = "\n".join(json.dumps(row, separators=(",", ":")) for row in rows) + "\n"
            raw_readbacks = line.encode("utf-8")
            (root / "readbacks.jsonl").write_bytes(raw_readbacks)
            details = {
                "schema": validator.SWEEP_SCHEMA,
                "class_count": 96,
                "visible_class_count": 96,
                "selected_unique_class_count": 96,
                "selection_observation_count": 96,
                "selected_class_editor_count_minimum": 1,
                "selected_class_editor_count_maximum": 1,
                "selected_class_editor_instance_generation": 1,
                "selected_class_editor_stable_within_sweep": True,
                "selected_class_editor_stable_across_sweeps": True,
                "non_roster_widget_count_maximum": 249,
                "class_id_child_widget_count_maximum": 0,
            }
            callback = {
                "event": "page-action-checkpoint",
                "page_key": "classes",
                "checkpoint": "p9-19-class-selection-sweep",
                "outcome": "completed",
                "details": details,
            }
            manifest = {
                "route": {
                    "lifecycleEvents": [{
                        "arrivalIndex": 41,
                        "rawLine": "ClassMngrNext page lifecycle checkpoint=" + json.dumps(callback, separators=(",", ":")),
                    }]
                }
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            import hashlib

            sweep = {
                "sweepId": "class-selections-warmup",
                "phase": "warmup",
                "cycleNumber": None,
                "classCount": 96,
                "selectionActionCount": 96,
                "allSelectionReadbacksConfirmed": True,
                "listItemNameHash": hashlib.sha256("\n".join(validator.CLASS_NAMES).encode("utf-8")).hexdigest(),
                "selectionReadbackPath": "readbacks.jsonl",
                "selectionReadbackSha256": hashlib.sha256(raw_readbacks).hexdigest(),
                "selectionReadbackBytes": len(raw_readbacks),
                "appCheckpointArrivalIndex": 41,
                "appCheckpointDetails": details,
            }
            summary = validator.validate_sweep_readbacks(sweep, root, 222, 333, None)
            self.assertEqual(summary["readbackCount"], 96)

    def test_sweep_rejects_a_wrong_class_readback_and_editor_growth(self) -> None:
        details = {
            "schema": validator.SWEEP_SCHEMA,
            "class_count": 96,
            "visible_class_count": 96,
            "selected_unique_class_count": 96,
            "selection_observation_count": 96,
            "selected_class_editor_count_minimum": 1,
            "selected_class_editor_count_maximum": 2,
            "selected_class_editor_instance_generation": 2,
            "selected_class_editor_stable_within_sweep": False,
            "selected_class_editor_stable_across_sweeps": False,
            "non_roster_widget_count_maximum": 250,
            "class_id_child_widget_count_maximum": 1,
        }
        sweep = {
            "classCount": 96,
            "selectionActionCount": 96,
            "allSelectionReadbacksConfirmed": True,
            "appCheckpointDetails": details,
        }
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(validator.ValidationError, "SelectedClassEditor/widget-count"):
                validator.validate_sweep_readbacks(sweep, Path(temporary), 222, 333, None)

    def test_manifest_gate_rejects_incomplete_or_failed_route_status(self) -> None:
        with self.assertRaisesRegex(validator.ValidationError, "Route did not complete"):
            validator.validate_manifest_identity({"schema": validator.SCHEMA, "taskId": validator.TASK_ID, "status": "failed", "failure": None})


if __name__ == "__main__":
    unittest.main()
