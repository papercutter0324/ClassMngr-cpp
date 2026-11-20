#!/usr/bin/env python3
"""Focused tests for P9-15 fixture and fail-closed harness validation."""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any


SCRIPT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_ROOT))

import prepare_p9_15_schedule_workspace as preparer  # noqa: E402
import validate_packaged_next_p9_15_schedule_locale_theme as validator  # noqa: E402


def fixture_schedule() -> dict[str, Any]:
    projection: dict[str, list[dict[str, Any]]] = {}
    schedule_rows = []
    index = 0
    for class_id in range(1, 97):
        for occurrence in range(8):
            row = occurrence % 4
            column = 1 + ((class_id + occurrence) % 5)
            key = f"{row},{column}"
            projection.setdefault(key, []).append({
                "classId": class_id,
                "teacherRoom": f"Teacher {((class_id - 1) % 24) + 1:02d} KR {300 + ((class_id - 1) % 24) + 1}",
                "classLine": f"E{4 + (class_id % 2)} - Level {1 + (class_id % 8):02d}",
            })
            schedule_rows.append({"classId": class_id, "occurrence": occurrence, "row": row, "column": column})
            index += 1
    defaults = {}
    for row in range(7):
        for day in range(7):
            column = day + 1
            key = f"{row},{column}"
            if key not in projection:
                defaults[key] = "Essay" if day < 5 and row > 0 else "Empty"
    return {
        "classCount": 96,
        "classInfoCount": 96,
        "scheduleEntryCount": index,
        "scheduleRows": schedule_rows,
        "showEnglishNames": True,
        "showWeekends": True,
        "uiGrid": {"headers": validator.ENGLISH_HEADERS, "rowCount": 7, "columnCount": 8, "sourceRowOffset": 15},
        "fixtureProjection": projection,
        "defaultSlotStates": defaults,
    }


def ui_snapshot(schedule: dict[str, Any], locale: str = "English", pid: int = 71, hwnd: int = 9001) -> dict[str, Any]:
    cells = []
    projection = schedule["fixtureProjection"]
    for row in range(7):
        for column in range(8):
            if column == 0:
                text = f"{3 + row}:00 - {3 + row}:50 PM"
            else:
                entries = projection.get(f"{row},{column}", [])
                text = "\n".join(line for entry in entries for line in (entry["teacherRoom"], entry["classLine"]))
                if not entries and schedule["defaultSlotStates"][f"{row},{column}"] == "Essay":
                    text = "Essay" if locale == "English" else validator.KOREAN_ESSAY_LABEL
            cells.append({
                "row": row,
                "column": column,
                "text": text,
                "control": {"processId": pid, "ownerWindowHandle": hwnd, "ownerWindowProcessId": pid},
            })
    headers = validator.ENGLISH_HEADERS if locale == "English" else validator.KOREAN_HEADERS
    return {
        "processId": pid,
        "mainWindow": {"processId": pid, "windowHandle": hwnd},
        "schedulePage": {"accessibleName": validator.ENGLISH_TITLE if locale == "English" else validator.KOREAN_TITLE,
                         "processId": pid, "ownerWindowHandle": hwnd},
        "table": {"control": {"automationId": "QApplication.classMngrNextWindow.QWidget.nextScheduleTable", "controlType": "ControlType.Table",
                               "processId": pid, "ownerWindowHandle": hwnd, "ownerWindowProcessId": pid},
                  "gridPattern": {"available": True, "rowCount": 7, "columnCount": 8}},
        "headers": [{"column": index, "text": text, "processId": pid, "ownerWindowHandle": hwnd,
                     "ownerWindowProcessId": pid} for index, text in enumerate(headers)],
        "cells": cells,
    }


def lifecycle_event(pid: int = 71) -> dict[str, Any]:
    return {
        "event": "page-lifecycle",
        "page_key": "schedule",
        "state": "activated",
        "sample_available": True,
        "page_instance_available": True,
        "page": {
            "widget_count": 120,
            "widget_scan_truncated": False,
            "item_model_count": 2,
            "item_model_root_rows": 10,
            "item_models": [{"class_name": "SchedulePageModel", "root_rows": 7}],
            "item_models_truncated": False,
            "table_widget_count": 0,
            "table_item_count": 0,
            "inspected_table_cells": 42,
            "table_scan_truncated": False,
            "view_index_widget_count": 0,
            "view_index_widget_scan_truncated": False,
            "editor_control_widget_count": 0,
        },
        "processId": pid,
    }


class P915HarnessValidationTests(unittest.TestCase):
    def test_large_startup_fixture_prepares_exact_96_classes_and_768_entries(self) -> None:
        repository = SCRIPT_ROOT.parents[1]
        base = repository / "artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps"
        seed = repository / "tests/fixtures/workspaces/large_startup.sql"
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary) / "schedule.tps"
            receipt = preparer.prepare(base, seed, workspace)
            self.assertEqual(receipt["schedule"]["classCount"], 96)
            self.assertEqual(receipt["schedule"]["scheduleEntryCount"], 768)
            self.assertEqual(receipt["schedule"]["classInfoCount"], 96)
            self.assertTrue(receipt["schedule"]["showEnglishNames"])
            self.assertTrue(receipt["schedule"]["showWeekends"])
            self.assertEqual(receipt["schedule"]["uiGrid"], {
                "headers": validator.ENGLISH_HEADERS, "rowCount": 7, "columnCount": 8, "sourceRowOffset": 15,
            })
            self.assertGreater(len(receipt["baselineTables"]), 0)
            receipt_path = Path(temporary) / "preparation.json"
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
            before = preparer.snapshot("before", workspace, receipt_path)
            after = preparer.snapshot("after", workspace, receipt_path)
            self.assertEqual(before["workspaceTables"], after["workspaceTables"])
            self.assertEqual(before["schedule"]["scheduleRows"], after["schedule"]["scheduleRows"])

    def test_rendered_schedule_cells_match_the_fixture_projection(self) -> None:
        schedule = fixture_schedule()
        snapshot = ui_snapshot(schedule)
        result = validator.validate_cell_projection(schedule, snapshot, "English")
        self.assertEqual(result["fixtureEntriesMatchedInRenderedCells"], 768)
        self.assertEqual(result["uiacellCount"], 56)
        self.assertEqual(result["headers"], validator.ENGLISH_HEADERS)

    def test_missing_rendered_entry_is_rejected(self) -> None:
        schedule = fixture_schedule()
        snapshot = ui_snapshot(schedule)
        first_data_cell = next(cell for cell in snapshot["cells"] if cell["column"] > 0 and cell["text"])
        first_data_cell["text"] = ""
        with self.assertRaisesRegex(validator.ValidationError, "Schedule cell"):
            validator.validate_cell_projection(schedule, snapshot, "English")

    def test_korean_capture_requires_korean_headers_and_page_title(self) -> None:
        schedule = fixture_schedule()
        snapshot = ui_snapshot(schedule, locale="Korean")
        result = validator.validate_cell_projection(schedule, snapshot, "Korean")
        self.assertEqual(result["headers"], validator.KOREAN_HEADERS)
        self.assertEqual(result["pageTitle"], validator.KOREAN_TITLE)
        snapshot["headers"][0]["text"] = "Time"
        with self.assertRaisesRegex(validator.ValidationError, "headers do not match"):
            validator.validate_cell_projection(schedule, snapshot, "Korean")

    def test_preferences_require_pid_hwnd_bound_selected_values(self) -> None:
        state = {"locale": "Korean", "theme": "Dark", "preferences": {
            "dialogProcessId": 71,
            "dialogWindowHandle": 9002,
            "dialogAutomationId": "",
            "dialogName": validator.KOREAN_PREFERENCES_TITLE,
            "dialogClassName": "QDialog",
            "dialogControlType": "ControlType.Window",
            "dialogIdentityMatchedBy": "localized Preferences window title plus measured PID/HWND/Qt class",
            "language": {"selectedItem": "Korean", "control": {"processId": 71, "ownerWindowHandle": 9001, "ownerWindowProcessId": 71}},
            "theme": {"selectedItem": "Dark", "control": {"processId": 71, "ownerWindowHandle": 9001, "ownerWindowProcessId": 71}},
        }}
        state["preferences"]["language"]["control"]["ownerWindowHandle"] = 9002
        state["preferences"]["theme"]["control"]["ownerWindowHandle"] = 9002
        validator.validate_preference_state(state, "Korean", "Dark", 71, 9001)
        state["preferences"]["theme"]["control"]["ownerWindowHandle"] = 12
        with self.assertRaisesRegex(validator.ValidationError, "not bound"):
            validator.validate_preference_state(state, "Korean", "Dark", 71, 9001)

    def test_uia_action_validator_counts_successful_fallback_selections(self) -> None:
        main = {"processId": 71, "ownerWindowHandle": 9001, "ownerWindowProcessId": 71}
        dialog = {"processId": 71, "ownerWindowHandle": 9002, "ownerWindowProcessId": 71}
        actions = [
            {"actionKind": "open-edit-preferences", "method": "InvokePattern.Invoke", "target": main},
            {"actionKind": "close-preferences-dialog", "method": "InvokePattern.Invoke", "target": dialog},
            {"actionKind": "set-language-korean-value-pattern-no-selection-change", "method": "ValuePattern.SetValue", "target": dialog,
             "result": "no-selection-change"},
            {"actionKind": "set-language-korean-activate-option", "method": "UIA-targeted window click", "target": dialog,
             "result": "succeeded", "selectedValue": "Korean"},
            {"actionKind": "set-theme-dark", "method": "ValuePattern.SetValue", "target": dialog,
             "result": "succeeded", "selectedValue": "Dark"},
            {"actionKind": "set-language-english", "method": "ValuePattern.SetValue", "target": dialog,
             "result": "succeeded", "selectedValue": "English"},
            {"actionKind": "set-theme-light-select-option", "method": "SelectionItemPattern.Select", "target": dialog,
             "result": "succeeded", "selectedValue": "Light"},
        ]
        actions.extend({"actionKind": "navigate-schedule", "method": "UIA-targeted window click", "target": main} for _ in range(6))
        manifest = {"route": {"actions": actions, "scheduleStates": [{"preferences": {"dialogWindowHandle": 9002}}]}}
        result = validator.validate_actions(manifest, 71, 9001)
        self.assertEqual(result["languageChanges"], 2)
        self.assertEqual(result["themeChanges"], 2)

    def test_lifecycle_metrics_must_report_scan_truncation_and_not_claim_schedule_count(self) -> None:
        event = lifecycle_event()
        result = validator.validate_generic_lifecycle(event, 71)
        self.assertFalse(result["scheduleSpecificEntryMetricClaimed"])
        event["page"]["widget_scan_truncated"] = True
        with self.assertRaisesRegex(validator.ValidationError, "was truncated"):
            validator.validate_generic_lifecycle(event, 71)

    def test_workspace_row_contract_detects_mutation(self) -> None:
        tables = {"classes": {"rowCount": 96, "rows": [[i, f"Class {i}"] for i in range(1, 97)]}}
        schedule = fixture_schedule()
        preparation = {"kind": "schedule-locale-theme", "status": "passed", "schedule": schedule,
                       "baselineTables": tables, "baselineTablesSha256": "baseline"}
        before = {"schema": validator.PREPARATION_SCHEMA, "status": "passed", "phase": "before", "integrityCheck": "ok",
                  "foreignKeyErrors": [], "workspaceTables": copy.deepcopy(tables), "workspaceTablesSha256": "baseline",
                  "schedule": {"scheduleRows": schedule.get("scheduleRows", [])}}
        after = copy.deepcopy(before)
        after["phase"] = "after"
        validator.validate_workspace_rows(preparation, before, after)
        after["workspaceTables"]["classes"]["rows"][0][1] = "changed"
        with self.assertRaisesRegex(validator.ValidationError, "rows differ"):
            validator.validate_workspace_rows(preparation, before, after)


if __name__ == "__main__":
    unittest.main()
