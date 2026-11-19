from __future__ import annotations

import importlib.util
import hashlib
import json
import sys
import tempfile
import unittest
from collections import Counter
from datetime import date
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIRECTORY))
SPEC = importlib.util.spec_from_file_location(
    "validate_packaged_next_calendar_import",
    SCRIPT_DIRECTORY / "validate_packaged_next_calendar_import.py",
)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)

REPOSITORY_ROOT = SCRIPT_DIRECTORY.parents[1]
SUCCESS_FIXTURE = REPOSITORY_ROOT / "docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/generated-large-calendar-import.xlsx"
ERROR_FIXTURE = REPOSITORY_ROOT / "docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/malformed-calendar-import-response.bin"


class CalendarImportValidatorTests(unittest.TestCase):
    def test_success_fixture_bytes_define_the_exact_234_27_event_split(self) -> None:
        expected = validator.parse_success_fixture(SUCCESS_FIXTURE)
        events = expected["events"]

        self.assertEqual(expected["worksheetCellCount"], 382)
        self.assertEqual(expected["calendarWorksheetCellCount"], 381)
        self.assertEqual(expected["archiveWorksheetCellCount"], 1)
        self.assertEqual(expected["dateCount"], 365)
        self.assertEqual(expected["skippedCount"], 104)
        self.assertEqual(len(events), 261)
        self.assertEqual(expected["eventTypeCounts"], {"Workshop": 234, "Holiday": 27})
        self.assertEqual(Counter(row[1] for row in events), {"Workshop": 234, "Holiday": 27})
        self.assertEqual(Counter(row[0] for row in events), {"DYB Workshop": 234, "Red Day": 27})
        self.assertEqual(len({row[5] for row in events}), 261)
        for row in events:
            self.assertEqual(row[5], row[7])
            self.assertEqual(row[6], row[8], "all fixture events must be date-to-date")
            self.assertTrue(row[5].startswith("2026-"))
            self.assertLess(date.fromisoformat(row[5]).weekday(), 5)
            if row[1] == "Workshop":
                self.assertEqual((row[2], row[4]), ("Unknown", "0"))
            else:
                self.assertEqual((row[2], row[4]), ("Timed", "1"))

    def test_candidate_hash_preserves_every_row_and_duplicate_multiplicity(self) -> None:
        expected = validator.parse_success_fixture(SUCCESS_FIXTURE)
        rows = expected["events"]

        self.assertEqual(validator._candidate_hash(rows), validator._candidate_hash(list(reversed(rows))))
        self.assertNotEqual(validator._candidate_hash(rows), validator._candidate_hash(rows + [rows[0]]))

    def test_error_fixture_identity_is_exact(self) -> None:
        self.assertEqual(ERROR_FIXTURE.stat().st_size, 69)
        self.assertEqual(validator._sha256(ERROR_FIXTURE), validator.ERROR_FIXTURE_SHA256)

    def test_normal_use_working_set_limit_is_strict_and_peak_is_advisory(self) -> None:
        required_points = (
            "startup-ready-checkpoint", "startup-ready-external", "startup-settled",
            "post-cleanup",
        )
        route = {
            "routeId": "route-a",
            "memory": {
                "normalUseSamples": [
                    {
                        "point": point,
                        "sampleIndex": index if point.endswith("tail") else 0,
                        "workingSetBytes": 262_143_999,
                        "privateBytes": 100,
                        "commitBytes": 100,
                        "peakWorkingSetBytes": 600_000_000,
                        "peakCommitBytes": 100,
                        "handles": 20,
                        "threads": 10,
                    }
                    for point in required_points
                    for index in (0,)
                ]
                + [
                    {
                        "point": "post-cleanup-idle-tail",
                        "sampleIndex": index,
                        "workingSetBytes": 262_143_999,
                        "privateBytes": 100,
                        "commitBytes": 100,
                        "peakWorkingSetBytes": 600_000_000,
                        "peakCommitBytes": 100,
                        "handles": 20,
                        "threads": 10,
                    }
                    for index in range(10)
                ],
                "operationSamples": [
                    {"point": "parse-completed", "peakWorkingSetBytes": 600_000_000}
                ],
            },
        }

        result = validator._validate_memory(route)
        self.assertTrue(result["passed"])
        self.assertFalse(result["operationPeaks"][0]["underAdvisoryCeiling"])
        route["memory"]["normalUseSamples"][0]["workingSetBytes"] = 262_144_000
        result = validator._validate_memory(route)
        self.assertFalse(result["passed"])
        self.assertEqual(len(result["violations"]), 1)

    def test_shutdown_milestones_match_captured_quit_signal_order_and_fail_closed(self) -> None:
        captured = list(validator.APPLICATION_SHUTDOWN_MILESTONES)
        shell_release = captured.index("application_bootstrap.finalization.shell_release_started")
        captured.insert(shell_release, validator.APPLICATION_FINALIZATION_STARTED)
        exec_returned = captured.index("application.exec.returned")
        captured.insert(exec_returned + 1, validator.APPLICATION_FINALIZATION_STARTED)

        validator._validate_ordered_shutdown_milestones(captured, "route-a")

        missing = list(captured)
        missing.remove("application.about_to_quit")
        with self.assertRaisesRegex(validator.EvidenceError, "lacks milestone application.about_to_quit"):
            validator._validate_ordered_shutdown_milestones(missing, "route-a")

        duplicated = list(captured)
        duplicated.insert(
            duplicated.index("application.about_to_quit") + 1,
            "application.about_to_quit",
        )
        with self.assertRaisesRegex(validator.EvidenceError, "duplicates milestone application.about_to_quit"):
            validator._validate_ordered_shutdown_milestones(duplicated, "route-a")

        misordered = list(captured)
        about_to_quit = misordered.index("application.about_to_quit")
        quit_returned = misordered.index("application_bootstrap.finalization.application_quit_returned")
        misordered[about_to_quit], misordered[quit_returned] = misordered[quit_returned], misordered[about_to_quit]
        with self.assertRaisesRegex(validator.EvidenceError, "misorders milestone application_bootstrap.finalization.application_quit_returned"):
            validator._validate_ordered_shutdown_milestones(misordered, "route-a")

    def test_post_measurement_validator_revision_keeps_the_measured_inventory_bound(self) -> None:
        measured_bytes = b"measured source revision"
        current_validator_bytes = b"corrected validator revision"
        current_test_bytes = b"validator regression test revision"
        unchanged_bytes = b"unchanged measured source"
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            validator_path = root / "scripts/phase9/validate_packaged_next_calendar_import.py"
            test_path = root / "scripts/phase9/test_validate_packaged_next_calendar_import.py"
            unchanged_path = root / "src/unchanged.cpp"
            validator_path.parent.mkdir(parents=True)
            unchanged_path.parent.mkdir(parents=True)
            validator_path.write_bytes(current_validator_bytes)
            test_path.write_bytes(current_test_bytes)
            unchanged_path.write_bytes(unchanged_bytes)
            rows = [
                {
                    "path": "scripts/phase9/validate_packaged_next_calendar_import.py",
                    "sizeBytes": len(measured_bytes),
                    "sha256": hashlib.sha256(measured_bytes).hexdigest(),
                },
                {
                    "path": "scripts/phase9/test_validate_packaged_next_calendar_import.py",
                    "sizeBytes": len(measured_bytes),
                    "sha256": hashlib.sha256(measured_bytes).hexdigest(),
                },
                {
                    "path": "src/unchanged.cpp",
                    "sizeBytes": len(unchanged_bytes),
                    "sha256": hashlib.sha256(unchanged_bytes).hexdigest(),
                },
            ]
            revision = {
                "schema": "classmngr-phase9-post-measurement-validator-revision-v1",
                "measuredSourceSha256": "a" * 64,
                "reason": "validator chronology correction after route measurement; route/process evidence unchanged",
                "files": [
                    {
                        "path": "scripts/phase9/validate_packaged_next_calendar_import.py",
                        "measuredSha256": rows[0]["sha256"],
                        "measuredSizeBytes": rows[0]["sizeBytes"],
                        "currentSha256": hashlib.sha256(current_validator_bytes).hexdigest(),
                        "currentSizeBytes": len(current_validator_bytes),
                    },
                    {
                        "path": "scripts/phase9/test_validate_packaged_next_calendar_import.py",
                        "measuredSha256": rows[1]["sha256"],
                        "measuredSizeBytes": rows[1]["sizeBytes"],
                        "currentSha256": hashlib.sha256(current_test_bytes).hexdigest(),
                        "currentSizeBytes": len(current_test_bytes),
                    },
                ],
            }

            validator._verify_source_inventory_with_validator_revision(
                rows, root, "a" * 64, revision
            )

            validator_path.write_bytes(b"unrecorded mutation")
            with self.assertRaisesRegex(validator.EvidenceError, "validator revision size mismatch"):
                validator._verify_source_inventory_with_validator_revision(
                    rows, root, "a" * 64, revision
                )

    def test_shutdown_dump_relocation_is_path_only_and_fail_closed(self) -> None:
        dump_bytes = b"MDMP" + b"thread-info-dump-body"
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            original_root = base / "original-run"
            corrected_root = base / "corrected-run"
            other_root = base / "other-run"
            for root in (original_root, corrected_root, other_root):
                root.mkdir()
            relative_dump = Path("runtime/route-a/diagnostics/shutdown.dmp")
            corrected_dump = corrected_root / relative_dump
            corrected_dump.parent.mkdir(parents=True)
            corrected_dump.write_bytes(dump_bytes)
            digest = hashlib.sha256(dump_bytes).hexdigest()
            observation = {
                "schema": "classmngr-phase9-shutdown-minidump-v1",
                "status": "captured",
                "processId": 1234,
                "triggerElapsedSeconds": 12,
                "sizeBytes": len(dump_bytes),
                "sha256": digest,
                "dumpKind": "MiniDumpNormal | MiniDumpWithThreadInfo",
                "signature": "MDMP",
                "path": str(original_root / relative_dump),
            }
            manifest_link = dict(observation)
            manifest_link["path"] = str(corrected_dump)
            relocation = {
                "originalRoot": str(original_root),
                "correctedRoot": str(corrected_root),
            }

            normalized = validator._relocate_shutdown_dump_link(
                observation, manifest_link, corrected_root, relocation, "route-a"
            )
            self.assertEqual(Path(normalized["path"]), corrected_dump.resolve())
            self.assertEqual(
                validator._verify_shutdown_dump_file(normalized, corrected_root, "route-a"),
                corrected_dump.resolve(),
            )

            with self.assertRaisesRegex(validator.EvidenceError, "differs from the process observation"):
                validator._relocate_shutdown_dump_link(
                    observation, manifest_link, corrected_root, None, "route-a"
                )
            with self.assertRaisesRegex(validator.EvidenceError, "outside the declared original root"):
                validator._relocate_shutdown_dump_link(
                    observation,
                    manifest_link,
                    corrected_root,
                    {**relocation, "originalRoot": str(other_root)},
                    "route-a",
                )
            with self.assertRaisesRegex(validator.EvidenceError, "targets another evidence directory"):
                validator._relocate_shutdown_dump_link(
                    observation,
                    manifest_link,
                    corrected_root,
                    {**relocation, "correctedRoot": str(other_root)},
                    "route-a",
                )
            changed_path = dict(manifest_link)
            changed_path["path"] = str(corrected_root / "runtime/other-route/diagnostics/shutdown.dmp")
            with self.assertRaisesRegex(validator.EvidenceError, "does not preserve the original relative path"):
                validator._relocate_shutdown_dump_link(
                    observation, changed_path, corrected_root, relocation, "route-a"
                )
            for key, value in (("processId", 5678), ("sha256", "0" * 64), ("sizeBytes", len(dump_bytes) + 1)):
                altered_identity = dict(manifest_link)
                altered_identity[key] = value
                with self.subTest(field=key), self.assertRaisesRegex(
                    validator.EvidenceError, "dump identity differs from the process observation"
                ):
                    validator._relocate_shutdown_dump_link(
                        observation, altered_identity, corrected_root, relocation, "route-a"
                    )

            corrected_dump.unlink()
            with self.assertRaisesRegex(validator.EvidenceError, "minidump file is missing"):
                validator._verify_shutdown_dump_file(normalized, corrected_root, "route-a")

    def test_derived_evidence_requires_original_manifest_and_matching_relocation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            original_root = base / "original-run"
            corrected_root = base / "corrected-run"
            original_root.mkdir()
            corrected_root.mkdir()
            original_manifest = original_root / "manifest.json"
            preserved_manifest = corrected_root / "manifest-original-invalid.json"
            original_manifest.write_text("{\"status\":\"invalid\"}\n", encoding="utf-8")
            preserved_manifest.write_bytes(original_manifest.read_bytes())
            manifest_hash = validator._sha256(original_manifest)
            manifest = {
                "derivedFrom": {"root": str(original_root), "manifestSha256": manifest_hash},
                "relocation": {
                    "schema": "classmngr-phase9-evidence-relocation-v1",
                    "originalRoot": str(original_root),
                    "correctedRoot": str(corrected_root),
                },
            }

            self.assertEqual(
                validator._validate_evidence_relocation(manifest, corrected_root)["correctedRoot"],
                str(corrected_root.resolve()),
            )
            missing = dict(manifest)
            del missing["relocation"]
            with self.assertRaisesRegex(validator.EvidenceError, "lacks its origin or relocation declaration"):
                validator._validate_evidence_relocation(missing, corrected_root)
            wrong_root = {**manifest, "relocation": {**manifest["relocation"], "correctedRoot": str(original_root)}}
            with self.assertRaisesRegex(validator.EvidenceError, "corrected root differs"):
                validator._validate_evidence_relocation(wrong_root, corrected_root)
            wrong_hash = {**manifest, "derivedFrom": {**manifest["derivedFrom"], "manifestSha256": "0" * 64}}
            with self.assertRaisesRegex(validator.EvidenceError, "original manifest changed"):
                validator._validate_evidence_relocation(wrong_hash, corrected_root)

    def test_final_route_reporting_sets_parser_error_imported_count_to_zero(self) -> None:
        summary = {
            "routes": {
                "route-a-large-calendar-success": {
                    "pid": 12,
                    "outcome": "large-calendar-import-applied",
                    "parsedEventCount": 261,
                    "importedEventCount": 261,
                    "skippedEventCount": 104,
                    "stagedWorkbookRemoved": True,
                    "rowLevelWorkspace": {
                        "beforeEventCount": 180,
                        "afterEventCount": 441,
                        "importedEventCount": 261,
                        "rowLevelContentsUnchanged": False,
                    },
                    "memory": {"passed": True},
                    "shutdown": {"processExitCode": 0},
                },
                "route-b-malformed-calendar-error": {
                    "pid": 34,
                    "outcome": "malformed-workbook-rejected-with-no-writes",
                    "parsedEventCount": 0,
                    "importedEventCount": 0,
                    "skippedEventCount": 0,
                    "stagedWorkbookRemoved": True,
                    "rowLevelWorkspace": {
                        "beforeEventCount": 180,
                        "afterEventCount": 180,
                        "rowLevelContentsUnchanged": True,
                    },
                    "memory": {"passed": True},
                    "shutdown": {"processExitCode": 0},
                },
            }
        }

        rows = {row["routeId"]: row for row in validator._build_final_route_reporting_rows(summary)}
        error = rows["route-b-malformed-calendar-error"]
        self.assertEqual(error["parsedEventCount"], 0)
        self.assertEqual(error["appliedEventCount"], 0)
        self.assertEqual(error["importedCount"], 0)
        self.assertTrue(error["rowLevelContentsUnchanged"])
        self.assertEqual((error["beforeEventCount"], error["afterEventCount"]), (180, 180))

        summary["routes"]["route-b-malformed-calendar-error"]["importedEventCount"] = 1
        with self.assertRaisesRegex(validator.EvidenceError, "parser-error final report must record zero"):
            validator._build_final_route_reporting_rows(summary)

    def test_revalidation_report_requires_history_validator_output_and_linked_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            logs = root / "logs"
            tools = root / "tools"
            logs.mkdir()
            tools.mkdir()
            validator_script = tools / "validator.py"
            validator_script.write_text("validator revision", encoding="utf-8")
            metadata_tool = tools / "finalizer.ps1"
            metadata_tool.write_text("metadata tool revision", encoding="utf-8")
            source_sha = "a" * 64
            package_sha = "b" * 64
            application_sha = "c" * 64
            corrected_validator_sha = validator._sha256(validator_script)
            metadata_tool_revision = {
                "path": str(metadata_tool),
                "sha256": validator._sha256(metadata_tool),
                "measuredSourceSha256": source_sha,
            }

            routes = {
                "route-a-large-calendar-success": {
                    "pid": 12,
                    "outcome": "large-calendar-import-applied",
                    "parsedEventCount": 261,
                    "importedEventCount": 261,
                    "skippedEventCount": 104,
                    "stagedWorkbookRemoved": True,
                    "rowLevelWorkspace": {
                        "beforeEventCount": 180,
                        "afterEventCount": 441,
                        "importedEventCount": 261,
                        "rowLevelContentsUnchanged": False,
                    },
                    "memory": {"passed": True},
                    "shutdown": {"processExitCode": 0},
                },
                "route-b-malformed-calendar-error": {
                    "pid": 34,
                    "outcome": "malformed-workbook-rejected-with-no-writes",
                    "parsedEventCount": 0,
                    "importedEventCount": 0,
                    "skippedEventCount": 0,
                    "stagedWorkbookRemoved": True,
                    "rowLevelWorkspace": {
                        "beforeEventCount": 180,
                        "afterEventCount": 180,
                        "rowLevelContentsUnchanged": True,
                    },
                    "memory": {"passed": True},
                    "shutdown": {"processExitCode": 0},
                },
            }
            summary = {
                "status": "passed",
                "routeCount": 2,
                "evidenceDirectory": str(root),
                "sourceFingerprintSha256": source_sha,
                "packageSha256": package_sha,
                "applicationSha256": application_sha,
                "routes": routes,
            }
            summary_path = root / "summary.json"
            summary_path.write_text(json.dumps(summary), encoding="utf-8")
            summary_sha = validator._sha256(summary_path)
            original_validation = {
                "status": "failed",
                "exitCode": 1,
                "validatorSha256": "d" * 64,
                "result": {"status": "invalid-evidence", "error": "initial inventory mismatch"},
                "logSha256": "",
            }
            initial_log = logs / "initial-invalid.txt"
            initial_log.write_text(json.dumps(original_validation["result"]), encoding="utf-8")
            original_validation["logSha256"] = validator._sha256(initial_log)
            original_manifest = {"validation": original_validation}
            original_manifest_path = root / "manifest-original-invalid.json"
            original_manifest_path.write_text(json.dumps(original_manifest), encoding="utf-8")
            original_manifest_sha = validator._sha256(original_manifest_path)
            final_output = logs / "final-validation.txt"
            output_value = {
                "status": "passed",
                "routeCount": 2,
                "summaryPath": str(summary_path),
            }
            final_output.write_text(json.dumps(output_value), encoding="utf-8")
            output_sha = validator._sha256(final_output)
            route_rows = validator._build_final_route_reporting_rows(summary)
            final_checks = {
                "finalStatus": "passed",
                "measuredSourceSha256": source_sha,
                "summarySha256": summary_sha,
                "metadataToolRevision": metadata_tool_revision,
                "routeResults": route_rows,
            }
            final_checks_path = logs / "final-checks.json"
            final_checks_path.write_text(json.dumps(final_checks), encoding="utf-8")

            manifest = {
                "source": {"sha256": source_sha},
                "package": {"sha256": package_sha},
                "application": {"sha256": application_sha},
                "validatorIdentity": {"path": str(validator_script), "sha256": corrected_validator_sha},
                "validation": {
                    "status": "passed",
                    "exitCode": 0,
                    "validatorPath": str(validator_script),
                    "validatorSha256": corrected_validator_sha,
                    "outputPath": str(final_output),
                    "outputSha256": output_sha,
                    "summaryPath": str(summary_path),
                    "summarySha256": summary_sha,
                },
                "validationHistory": [{
                    "stage": "original-attempt-initial-validation",
                    "status": "invalid-evidence",
                    "exitCode": 1,
                    "validatorSha256": original_validation["validatorSha256"],
                    "result": original_validation["result"],
                    "originalManifestSha256": original_manifest_sha,
                    "outputPath": str(initial_log),
                    "outputSha256": original_validation["logSha256"],
                }],
                "derivedFrom": {"manifestSha256": original_manifest_sha},
                "metadataToolRevision": metadata_tool_revision,
                "finalChecks": {
                    "path": str(final_checks_path),
                    "sha256": validator._sha256(final_checks_path),
                },
            }
            validator._validate_revalidation_reporting(manifest, root, summary)

            bad_validator = manifest["validation"]["validatorSha256"]
            manifest["validation"]["validatorSha256"] = "0" * 64
            with self.assertRaisesRegex(validator.EvidenceError, "differs from the corrected validator identity"):
                validator._validate_revalidation_reporting(manifest, root, summary)
            manifest["validation"]["validatorSha256"] = bad_validator

            error_row = next(
                row for row in final_checks["routeResults"]
                if row["routeId"] == "route-b-malformed-calendar-error"
            )
            error_row["importedCount"] = 1
            final_checks_path.write_text(json.dumps(final_checks), encoding="utf-8")
            manifest["finalChecks"]["sha256"] = validator._sha256(final_checks_path)
            with self.assertRaisesRegex(validator.EvidenceError, "final-checks route reporting differs"):
                validator._validate_revalidation_reporting(manifest, root, summary)

            manifest["validationHistory"] = []
            with self.assertRaisesRegex(validator.EvidenceError, "preserve its single original invalid validation result"):
                validator._validate_revalidation_reporting(manifest, root, summary)


if __name__ == "__main__":
    unittest.main()
