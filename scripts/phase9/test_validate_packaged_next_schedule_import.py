from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from validate_packaged_next_schedule_import import (
    EvidenceError,
    _canonical,
    _read_jsonl,
    _same_path,
    _validate_manifest_request_matches_raw_trace,
    _validate_trace_roundtrip,
    _validate_class_associations,
    _validate_memory,
    _validate_uia_actions,
    _verify_snapshot,
)


def _table(columns: list[str], rows: list[list[Any]]) -> dict[str, Any]:
    schema = [[index, name, "TEXT", 0, None, 0] for index, name in enumerate(columns)]
    contents = {"schema": schema, "rows": rows}
    return {
        **contents,
        "rowCount": len(rows),
        "contentSha256": hashlib.sha256(_canonical(contents)).hexdigest(),
    }


def _workspace_and_request() -> tuple[dict[str, Any], dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    teachers = [{"teacherKey": f"key-{index}", "actionName": "create"} for index in range(5)]
    class_decisions = [{"candidateIndex": index, "actionName": "create-new"} for index in range(20)]
    class_rows = [[index + 1, f"Class {index + 1}"] for index in range(20)]
    teacher_rows = [[index + 1, f"Teacher {index + 1}"] for index in range(5)]
    info_rows: list[list[Any]] = []
    time_rows: list[list[Any]] = []

    for index in range(20):
        teacher_number = index % 5 + 1
        teacher_name = f"Teacher {teacher_number}"
        grade = f"Grade {index + 1}"
        level = f"Level {index + 1}"
        time_count = 2 if index < 10 else 1
        times = [
            {
                "day": f"Day {offset + 1}",
                "startTime": f"{index + offset:02d}:00",
                "endTime": f"{index + offset:02d}:50",
            }
            for offset in range(time_count)
        ]
        candidates.append({
            "teacherKey": f"key-{teacher_number - 1}",
            "teacherName": teacher_name,
            "grade": grade,
            "level": level,
            "times": times,
        })
        info_rows.append([index + 1, teacher_number, grade, level])
        for time in times:
            time_rows.append([
                len(time_rows) + 1,
                index + 1,
                time["day"],
                time["startTime"],
                time["endTime"],
            ])

    tables = {
        "classes": _table(["id", "name"], class_rows),
        "teachers": _table(["id", "teacher_kr"], teacher_rows),
        "class_info": _table(["class_id", "teacher_id", "class_grade", "class_level"], info_rows),
        "class_times": _table(["id", "class_id", "day", "start_time", "end_time"], time_rows),
    }
    normalized = {
        name: {"schema": value["schema"], "rows": value["rows"]}
        for name, value in tables.items()
    }
    snapshot = {
        "schema": "classmngr-phase9-schedule-import-workspace-snapshot-v1",
        "integrityCheck": "ok",
        "tableCount": len(tables),
        "tableContentSha256": hashlib.sha256(_canonical(normalized)).hexdigest(),
        "workspaceFileSha256": "a" * 64,
        "workspaceFileSizeBytes": 1024,
        "tables": tables,
    }
    request = {
        "candidateCount": 20,
        "teacherDecisionCount": 5,
        "classDecisionCount": 20,
        "createdTeacherCount": 5,
        "createdClassCount": 20,
        "candidates": candidates,
        "teacherDecisions": teachers,
        "classDecisions": class_decisions,
    }
    return snapshot, request


class ScheduleImportEvidenceValidatorTests(unittest.TestCase):
    def test_korean_apply_request_round_trips_as_utf8_and_mismatch_fails(self) -> None:
        raw_request = {
            "event": "apply-request",
            "candidateCount": 1,
            "candidates": [{"teacherKey": "김선생", "teacherName": "김선생", "grade": "초등", "level": "문학"}],
            "teacherDecisions": [{"teacherKey": "김선생", "actionName": "create"}],
        }
        with tempfile.TemporaryDirectory() as temporary:
            trace_path = Path(temporary) / "schedule-import.jsonl"
            raw_request["processId"] = 1234
            trace_path.write_text(json.dumps(raw_request, ensure_ascii=False) + "\n", encoding="utf-8")
            raw_events = _read_jsonl(trace_path)
            manifest_events = json.loads(json.dumps(raw_events, ensure_ascii=False))
            route = {
                "routeId": "route-b-applicable-apply",
                "processIdentity": {"pid": 1234},
                "tracePath": str(trace_path),
                "traceSha256": hashlib.sha256(trace_path.read_bytes()).hexdigest(),
                "dialogTrace": manifest_events,
            }

            self.assertEqual(_validate_trace_roundtrip(route, Path(temporary)), raw_events)
            _validate_manifest_request_matches_raw_trace(manifest_events[0], raw_events)

            manifest_events[0]["candidates"][0]["teacherName"] = "김�생"
            with self.assertRaisesRegex(EvidenceError, "manifest apply request differs"):
                _validate_manifest_request_matches_raw_trace(manifest_events[0], raw_events)

    def test_windows_trace_paths_match_canonical_backslash_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            canonical = str((Path(temporary) / "fixture.xlsx").resolve())
            trace_path = canonical.replace("\\", "/")

            self.assertTrue(_same_path(trace_path, canonical))

    def test_exact_class_teacher_and_schedule_associations_pass(self) -> None:
        snapshot, request = _workspace_and_request()

        result = _validate_class_associations(snapshot, request)

        self.assertEqual(result["candidateCount"], 20)
        self.assertEqual(result["createdClassCount"], 20)
        self.assertEqual(result["createdTeacherCount"], 5)
        self.assertEqual(result["classTimeRowCount"], 30)
        self.assertEqual(result["actualAssociationCount"], 30)
        self.assertEqual(len(result["persistedClassIdentities"]), 20)

    def test_misassigned_persisted_teacher_fails_even_when_totals_match(self) -> None:
        snapshot, request = _workspace_and_request()
        info = snapshot["tables"]["class_info"]
        info["rows"][0][1] = 2
        info["contentSha256"] = hashlib.sha256(
            _canonical({"schema": info["schema"], "rows": info["rows"]})
        ).hexdigest()

        with self.assertRaisesRegex(EvidenceError, "associations differ"):
            _validate_class_associations(snapshot, request)

    def test_incomplete_schedule_row_fails_even_when_table_counts_match(self) -> None:
        snapshot, request = _workspace_and_request()
        time_table = snapshot["tables"]["class_times"]
        time_table["rows"][0][3] = "wrong-start"
        time_table["contentSha256"] = hashlib.sha256(
            _canonical({"schema": time_table["schema"], "rows": time_table["rows"]})
        ).hexdigest()

        with self.assertRaisesRegex(EvidenceError, "associations differ"):
            _validate_class_associations(snapshot, request)

    def test_snapshot_integrity_recomputes_every_table_and_full_hash(self) -> None:
        snapshot, _ = _workspace_and_request()

        _verify_snapshot(snapshot, "fixture workspace")

        snapshot["tables"]["classes"]["rows"][0][1] = "tampered"
        with self.assertRaisesRegex(EvidenceError, "content hash mismatch"):
            _verify_snapshot(snapshot, "fixture workspace")

    def test_strict_working_set_threshold_is_exclusive_and_peak_is_advisory(self) -> None:
        route = {
            "routeId": "route-a",
            "memory": {
                "normalUseSamples": [
                    {"point": point, "workingSetBytes": 262_143_999, "privateBytes": 100, "commitBytes": 100,
                     "timestampUtc": "2026-11-19T00:00:00Z"}
                    for point in (
                        "startup-ready-checkpoint", "startup-ready-external", "startup-settled",
                        "post-cleanup", "post-cleanup-idle-tail",
                    )
                ],
                "operationSamples": [
                    {"point": "parse-completed", "currentWorkingSetBytes": 600_000_000,
                     "peakWorkingSetBytes": 600_000_000}
                ],
            },
        }

        result = _validate_memory(route)

        self.assertTrue(result["budgetPassed"])
        self.assertEqual(result["strictWorkingSetViolations"], [])
        self.assertFalse(result["operationPeaks"][0]["underAdvisoryCeiling"])

        route["memory"]["normalUseSamples"][0]["workingSetBytes"] = 262_144_000
        result = _validate_memory(route)
        self.assertFalse(result["budgetPassed"])
        self.assertEqual(len(result["strictWorkingSetViolations"]), 1)

    def test_uia_actions_must_remain_process_bound(self) -> None:
        route = {
            "route-a-conflict-cancel": {"processIdentity": {"pid": 1234}},
        }
        action = {
            "routeId": "route-a-conflict-cancel",
            "processInstanceId": "route-a-conflict-cancel",
            "applicationProcessId": 1234,
            "elementProcessId": 9999,
            "windowProcessId": 1234,
            "windowHandle": 55,
            "method": "uia-bound-verified-input",
            "actionKind": "cancel-conflict-review",
            "issuedAtUtc": "2026-11-19T00:00:00Z",
        }

        with self.assertRaisesRegex(EvidenceError, "target belongs to another process"):
            _validate_uia_actions([action], route)


if __name__ == "__main__":
    unittest.main()
