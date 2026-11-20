#!/usr/bin/env python3
"""Focused fail-closed contract tests for the P9-14 lifecycle validator."""

from __future__ import annotations

import sys
import unittest
import copy
import tempfile
from pathlib import Path
from typing import Any


SCRIPT_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_ROOT))

import validate_packaged_next_p9_14_lifecycle as validator  # noqa: E402
import prepare_p9_14_staff_ai_workspaces as preparer  # noqa: E402


def staff_fixture() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    native = [
        {"id": i, "name": f"Native {i:03d}", "position": "NET", "phoneNumber": f"010-{i:04d}", "birthday": "01-01", "nationality": "fixture", "email": f"n{i}@example.test"}
        for i in range(1, 97)
    ]
    gs = [
        {"id": i, "name": f"GS {i:03d}", "koreanName": f"GS {i:03d}", "position": "GS Team", "phoneNumber": f"010-{i:04d}", "birthday": "01-01"}
        for i in range(1, 97)
    ]
    preparation = {"kind": "staff-directory", "nativeEnglish": {"rows": native}, "gsTeam": {"rows": gs}}
    tables = {
        "nativeEnglish": {"rowCount": 96, "rows": native},
        "gsTeam": {"rowCount": 96, "rows": gs},
    }
    return preparation, {"tables": copy.deepcopy(tables)}, {"tables": copy.deepcopy(tables)}


def ai_fixture() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    students = validator.expected_ai_rows()
    columns = ["evaluation_id", "row_index", *[f"col_{i}" for i in range(11)]]
    data_rows: list[list[Any]] = []
    snapshot_students: list[dict[str, Any]] = []
    for student in students:
        notes = f"[Did Well]\n{student['didWell']}\n[Needs Improvement]\n{student['needsImprovement']}"
        data = [1, student["rowIndex"], "", student["englishName"], student["koreanName"], "", "", "", "", "", "", "", notes]
        data_rows.append(data)
        snapshot_students.append({"rowIndex": student["rowIndex"], "englishName": student["englishName"], "koreanName": student["koreanName"], "comment": "", "notes": notes})
    data_rows.append([2, 0, "", "Other Class Student", "Other", "preserve", "", "", "", "", "", "", "other evaluation notes"])
    preparation = {
        "kind": "speaking-evaluation-ai-batch",
        "rows": students,
        "baselineSpeakingEvalDataColumns": columns,
        "baselineSpeakingEvalDataRows": data_rows,
        "speakingEvalDataRowsSha256": "baseline-hash",
    }
    baseline = {
        "evaluation": {"id": 1, "classId": 1, "name": "Winter", "grade": "E4"},
        "rowCount": 100,
        "students": snapshot_students,
        "allDataColumns": columns,
        "allDataRows": data_rows,
        "speakingEvalDataRowsSha256": "baseline-hash",
    }
    after = {
        "evaluation": dict(baseline["evaluation"]),
        "rowCount": 100,
        "students": [dict(row) for row in snapshot_students],
        "allDataColumns": list(columns),
        "allDataRows": [list(row) for row in data_rows],
        "speakingEvalDataRowsSha256": "after-hash",
    }
    for index in range(25):
        comment = validator.deterministic_comment(index + 1)
        after["students"][index]["comment"] = comment
        after["allDataRows"][index][11] = comment
    return preparation, baseline, after


def staff_table_snapshot(rows: list[dict[str, Any]], headers: list[str]) -> dict[str, Any]:
    fixture_index_by_name = {row["name"]: index for index, row in enumerate(rows)}
    runtime_rows = list(reversed(rows))
    identity_checks = [
        {
            "row": index,
            "fixtureRowIndex": fixture_index_by_name[row["name"]],
            "column": 0,
            "expectedIdentity": row["name"],
            "text": row["name"],
            "controlType": "ControlType.DataItem",
            "processId": 71,
            "automationId": "staffTableCell",
            "runtimeId": f"1,{index},0",
            "isOffscreen": index not in (0, 48, 95),
            "rectangle": {"x": 120, "y": 240 + index, "width": 100, "height": 20},
        }
        for index, row in enumerate(runtime_rows)
    ]
    header_rows = [
        {
            "column": index,
            "text": header,
            "control": {
                "processId": 71,
                "ownerWindowProcessId": 71,
                "ownerWindowHandle": 9001,
                "isOffscreen": False,
            },
        }
        for index, header in enumerate(headers)
    ]
    samples = [
        {
            "row": index,
            "fixtureRowIndex": fixture_index_by_name[runtime_rows[index]["name"]],
            "expectedIdentity": runtime_rows[index]["name"],
            "text": runtime_rows[index]["name"],
            "processId": 71,
            "runtimeId": f"1,{index},0",
            "isOffscreen": False,
            "rectangle": {"x": 120, "y": 240, "width": 100, "height": 20},
        }
        for index in (0, 48, 95)
    ]
    return {
        "gridPattern": {"columnCount": len(headers)},
        "staffTableEvidence": {
            "source": "UIA GridPattern.GetItem row identity plus UIA TablePattern.GetColumnHeaders",
            "rowCount": 96,
            "columnCount": len(headers),
            "headers": header_rows,
            "identityCellChecks": identity_checks,
            "allGridRowsExposed": True,
            "fixtureIdentitySetVerified": True,
            "renderedIdentitySamples": samples,
        },
    }


class P914ValidatorTests(unittest.TestCase):
    def test_staff_fixture_requires_exact_96_row_identities_before_and_after(self) -> None:
        preparation, before, after = staff_fixture()
        self.assertTrue(validator.validate_staff_snapshots(preparation, before, after)["identitiesVerifiedBeforeAndAfter"])
        after["tables"]["gsTeam"]["rows"][95]["phoneNumber"] = "changed"
        with self.assertRaisesRegex(validator.ValidationError, "after gsTeam"):
            validator.validate_staff_snapshots(preparation, before, after)

    def test_staff_preparer_copies_the_pinned_96_row_workspace_fixture(self) -> None:
        source = Path(__file__).resolve().parents[2] / "docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/generated-large-staff-directory.tps"
        with tempfile.TemporaryDirectory(prefix="p9-14-staff-prep-") as temporary:
            output = Path(temporary) / "staff.tps"
            receipt = preparer.prepare_staff_from_fixture(source, output)
            self.assertEqual(receipt["sourceWorkspaceSha256"], preparer.STAFF_SOURCE_WORKSPACE_SHA256)
            self.assertEqual(receipt["workspaceSha256"], receipt["sourceWorkspaceSha256"])
            self.assertEqual(receipt["nativeEnglish"]["rows"][47]["name"], "Native Staff AV")
            self.assertEqual(receipt["gsTeam"]["rows"][95]["name"], "GS Staff CR")

    def test_staff_ui_table_headers_and_every_identity_cell_are_pinned(self) -> None:
        preparation, _, _ = staff_fixture()
        rows = preparation["nativeEnglish"]["rows"]
        headers = ["Name", "Position", "Phone Number", "Email", "Birthday", "Nationality"]
        snapshot = staff_table_snapshot(rows, headers)
        summary = validator.validate_staff_table_evidence(snapshot, "native_english_teachers", rows, 71, "native-english-first")
        self.assertEqual(summary["columnCount"], 6)
        self.assertEqual(summary["identityCellCount"], 96)
        snapshot["staffTableEvidence"]["identityCellChecks"][48]["text"] = "different row"
        with self.assertRaisesRegex(validator.ValidationError, "identity differs"):
            validator.validate_staff_table_evidence(snapshot, "native_english_teachers", rows, 71, "native-english-first")
        snapshot["staffTableEvidence"]["identityCellChecks"][48]["text"] = rows[48]["name"]
        snapshot["staffTableEvidence"]["headers"][0]["text"] = "Different"
        with self.assertRaisesRegex(validator.ValidationError, "TablePattern headers"):
            validator.validate_staff_table_evidence(snapshot, "native_english_teachers", rows, 71, "native-english-first")

    def test_staff_runtime_page_title_and_leaf_table_ids_are_bound_to_uia_snapshots(self) -> None:
        title_snapshot = {
            "control": {
                "name": "Native English Teachers",
                "automationId": "QApplication.host.QWidget.pageTitle",
                "controlType": "ControlType.Text",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
            }
        }
        self.assertEqual(validator.validate_staff_title_snapshot(title_snapshot, "Native English Teachers", 71, 9001)["automationId"], "QApplication.host.QWidget.pageTitle")
        with self.assertRaisesRegex(validator.ValidationError, "PID/HWND"):
            validator.validate_staff_title_snapshot(title_snapshot, "Native English Teachers", 71, 9002)
        with self.assertRaisesRegex(validator.ValidationError, "exact Text control"):
            validator.validate_staff_title_snapshot(title_snapshot, "GS Team", 71, 9001)

        table_snapshot = {
            "control": {
                "automationId": "QApplication.host.QWidget.nativeEnglishTeachersTable",
                "controlType": "ControlType.Table",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
            }
        }
        validator.validate_staff_table_runtime_identity(table_snapshot, "QApplication.host.QWidget.pageTitle", "nativeEnglishTeachersTable", "QApplication.host.QWidget.nativeEnglishTeachersTable", 71, 9001)
        with self.assertRaisesRegex(validator.ValidationError, "exact UIA table ID"):
            validator.validate_staff_table_runtime_identity(table_snapshot, "QApplication.host.QWidget.pageTitle", "nativeEnglishTeachersTable", "QApplication...nativeEnglishTeachersTable", 71, 9001)

    def test_staff_post_route_snapshot_pins_actual_title_and_table_automation_ids(self) -> None:
        snapshot = {
            "root": {
                "automationId": "QApplication.classMngrNextApplicationShell",
                "controlType": "ControlType.Window",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
            },
            "descendants": [
                {"automationId": "QApplication.host.QWidget.pageTitle", "controlType": "ControlType.Text", "name": "Native English Teachers", "processId": 71},
                {"automationId": "QApplication.host.QWidget.nativeEnglishTeachersTable", "controlType": "ControlType.Table", "name": "", "processId": 71, "gridPattern": {"available": True, "rowCount": 96, "columnCount": 6}},
            ],
        }
        self.assertEqual(validator.validate_staff_post_route_snapshot(snapshot, 71, 9001), {"pageTitleAutomationId": "QApplication.host.QWidget.pageTitle", "tableAutomationId": "QApplication.host.QWidget.nativeEnglishTeachersTable"})
        snapshot["descendants"][1]["gridPattern"]["rowCount"] = 95
        with self.assertRaisesRegex(validator.ValidationError, "exact 96 by 6"):
            validator.validate_staff_post_route_snapshot(snapshot, 71, 9001)

    def test_ai_fixture_preserves_all_unselected_and_other_cells(self) -> None:
        preparation, before, after = ai_fixture()
        result = validator.validate_ai_snapshots(preparation, before, after)
        self.assertEqual(result["selectedCommentRows"], list(range(25)))
        self.assertTrue(result["allOtherSpeakingEvaluationCellsPreserved"])
        after["allDataRows"][25][5] = "unexpected change"
        with self.assertRaisesRegex(validator.ValidationError, "nonselected comment or any other"):
            validator.validate_ai_snapshots(preparation, before, after)

    def test_ai_fixture_rejects_comment_change_in_unselected_row(self) -> None:
        preparation, before, after = ai_fixture()
        after["students"][25]["comment"] = "unexpected"
        with self.assertRaisesRegex(validator.ValidationError, "comment state differs at row 25"):
            validator.validate_ai_snapshots(preparation, before, after)

    def test_reopened_matrix_descendant_snapshot_requires_exact_next_100_by_11_table(self) -> None:
        snapshot = {
            "root": {"automationId": "QApplication.classMngrNextApplicationShell", "processId": 71, "ownerWindowProcessId": 71, "ownerWindowHandle": 9001},
            "descendants": [{
                "automationId": "QApplication.classMngrNextApplicationShell.QWidget.speakingEvaluationMatrix",
                "controlType": "ControlType.Table",
                "processId": 71,
                "gridPattern": {"available": True, "rowCount": 100, "columnCount": 11},
            }],
        }
        self.assertEqual(validator.validate_reopened_matrix_descendants(snapshot, 71)["rowCount"], 100)
        snapshot["descendants"][0]["gridPattern"]["rowCount"] = 99
        with self.assertRaisesRegex(validator.ValidationError, "100-row by 11-column"):
            validator.validate_reopened_matrix_descendants(snapshot, 71)

    def test_workspace_hash_transition_pins_before_and_after_without_freezing_database(self) -> None:
        transition = validator.validate_workspace_hash_transition(
            {"workspaceSha256": "before"},
            {"workspaceSha256": "before"},
            {"workspaceSha256": "after"},
            "after",
        )
        self.assertTrue(transition["workspaceChangedDuringRoute"])
        with self.assertRaisesRegex(validator.ValidationError, "Final fixture workspace bytes"):
            validator.validate_workspace_hash_transition(
                {"workspaceSha256": "before"},
                {"workspaceSha256": "before"},
                {"workspaceSha256": "after"},
                "unexpected",
            )

    def test_response_requires_exact_ordered_25_valid_length_blocks(self) -> None:
        blocks = []
        for index in range(1, 26):
            student_id = f"STUDENT_{index:02d}"
            blocks.append(f"<<<{student_id}>>>\n{validator.deterministic_comment(index)}\n<<<END_{student_id}>>>")
        payload = "\n".join(blocks)
        self.assertEqual(len(validator.validate_response_payload(payload)), 25)
        with self.assertRaisesRegex(validator.ValidationError, "exactly 25"):
            validator.validate_response_payload(payload.rsplit("\n", 1)[0])
        with self.assertRaisesRegex(validator.ValidationError, "differs from the deterministic"):
            validator.validate_response_payload(payload.replace("clear central idea", "clear idea", 1))

    def test_ai_review_status_accepts_valid_ready_rows_with_or_without_name_placeholder(self) -> None:
        self.assertTrue(validator.is_valid_ai_review_status("Ready"))
        self.assertTrue(validator.is_valid_ai_review_status("Ready — name placeholder was omitted"))
        self.assertFalse(validator.is_valid_ai_review_status("Invalid response block"))

    def test_uia_click_requires_exact_nested_target_identity_and_measured_bounds(self) -> None:
        event_line = "ClassMngrNext navigation event=page-route outcome=completed cause=selection from=my_workspace to=speaking_evaluation catalog_document_count=30"
        route_event = {
            "arrivalIndex": 8,
            "targetPageId": "speaking_evaluation",
            "processId": 71,
            "line": event_line,
        }
        action = {
            "targetPageId": "speaking_evaluation",
            "verifiedByFollowingPageRoute": True,
            "pageRouteEvent": dict(route_event),
            "target": {
                "name": "Speaking Evaluation",
                "automationId": "navigation-tree",
                "controlType": "ControlType.TreeItem",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
                "rectangle": {"x": 120, "y": 240, "width": 100, "height": 40},
            },
            "pointerAction": {
                "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                "screenX": 170,
                "screenY": 260,
                "targetProcessId": 71,
                "windowHandle": 9001,
                "windowProcessId": 71,
                "sourceTarget": {
                    "name": "Speaking Evaluation",
                    "automationId": "navigation-tree",
                    "controlType": "ControlType.TreeItem",
                    "rectangle": {"x": 120, "y": 240, "width": 100, "height": 40},
                },
                "measuredMainWindowBounds": {"x": 100, "y": 200, "width": 500, "height": 400},
            },
        }
        validator.validate_uia_window_click(action, 71, [route_event])
        action["pageRouteEvent"]["arrivalIndex"] = 9
        with self.assertRaisesRegex(validator.ValidationError, "matching observed Next page-route event"):
            validator.validate_uia_window_click(action, 71, [route_event])
        action["pageRouteEvent"]["arrivalIndex"] = 8
        action["pointerAction"]["screenX"] = 175
        with self.assertRaisesRegex(validator.ValidationError, "center"):
            validator.validate_uia_window_click(action, 71, [route_event])

    def test_uia_prompt_button_fallback_requires_measured_bounds_and_state_transition(self) -> None:
        action = {
            "target": {
                "name": "Create Class Prompt",
                "automationId": "QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchCreatePrompt",
                "controlType": "ControlType.Button",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
                "rectangle": {"x": 120, "y": 240, "width": 100, "height": 40},
            },
            "pointerAction": {
                "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                "screenX": 170,
                "screenY": 260,
                "targetProcessId": 71,
                "windowHandle": 9001,
                "windowProcessId": 71,
                "sourceTarget": {
                    "name": "Create Class Prompt",
                    "automationId": "QApplication.speakingEvaluationAiBatchDialog.speakingEvaluationAiBatchCreatePrompt",
                    "controlType": "ControlType.Button",
                    "rectangle": {"x": 120, "y": 240, "width": 100, "height": 40},
                },
                "measuredDialogBounds": {"x": 100, "y": 200, "width": 500, "height": 400},
            },
            "stateBefore": {"copyPromptEnabled": False},
            "stateAfter": {"copyPromptEnabled": True},
            "verifiedByFollowingUiAStateTransition": True,
        }
        validator.validate_uia_button_click(action, 71)
        action["pointerAction"]["windowHandle"] = 9002
        with self.assertRaisesRegex(validator.ValidationError, "HWND"):
            validator.validate_uia_button_click(action, 71)

    def test_staff_group_expansion_fallback_requires_measured_tree_item_and_state_transition(self) -> None:
        rectangle = {"x": 120, "y": 240, "width": 100, "height": 40}
        tree_rectangle = {"x": 110, "y": 220, "width": 200, "height": 200}
        screenshot = {
            "name": "campus-staff-navigation-before-disclosure-click",
            "relativePath": "screenshots/campus-staff-navigation-before-disclosure-click.png",
            "sha256": "a" * 64,
            "captureMethod": "PrintWindow(PW_RENDERFULLCONTENT)",
            "processId": 71,
            "windowHandle": 9001,
            "windowProcessId": 71,
            "width": 500,
            "height": 400,
            "screenBounds": {"x": 100, "y": 200, "width": 500, "height": 400},
        }
        action = {
            "method": "PrintWindow-inspected disclosure indicator with screenshot-derived point bounded by UIA parent Tree and measured main HWND; bounded WM mouse click",
            "expansionStateBefore": "Collapsed",
            "expansionStateAfter": "Expanded",
            "target": {
                "name": "Campus Staff",
                "automationId": "navigation-tree",
                "controlType": "ControlType.TreeItem",
                "processId": 71,
                "ownerWindowHandle": 9001,
                "ownerWindowProcessId": 71,
                "rectangle": rectangle,
            },
            "pointerAction": {
                "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                "screenX": 115,
                "screenY": 260,
                "targetProcessId": 71,
                "windowHandle": 9001,
                "windowProcessId": 71,
                "parentTreeTarget": {
                    "name": "",
                    "automationId": "QApplication.classMngrNextNavigationTree",
                    "controlType": "ControlType.Tree",
                    "processId": 71,
                    "ownerWindowHandle": 9001,
                    "ownerWindowProcessId": 71,
                    "rectangle": tree_rectangle,
                },
                "disclosurePointDerivation": {
                    "method": "operator-inspected fresh PrintWindow pixel converted through captured main-window screen origin and checked against the fresh UIA parent Tree bounds",
                    "screenshotRelativePath": screenshot["relativePath"],
                    "screenshotSha256": screenshot["sha256"],
                    "screenshotCapturedAtUtc": "2026-11-20T17:00:00Z",
                    "screenshotProcessId": 71,
                    "screenshotWindowHandle": 9001,
                    "screenshotWindowProcessId": 71,
                    "screenshotWidth": 500,
                    "screenshotHeight": 400,
                    "screenshotScreenBounds": {"x": 100, "y": 200, "width": 500, "height": 400},
                    "selectedImagePixel": {"x": 15, "y": 60},
                    "uiaTreeBoundsAtInspection": dict(tree_rectangle),
                    "uiaTreeItemBoundsAtInspection": dict(rectangle),
                },
                "sourceTarget": {
                    "name": "Campus Staff",
                    "automationId": "navigation-tree",
                    "controlType": "ControlType.TreeItem",
                    "rectangle": dict(rectangle),
                },
                "measuredMainWindowBounds": {"x": 100, "y": 200, "width": 500, "height": 400},
            },
        }
        validator.validate_navigation_group_click(action, 71, [screenshot])
        action["pointerAction"]["screenX"] = 130
        with self.assertRaisesRegex(validator.ValidationError, "does not map from the inspected PrintWindow pixel"):
            validator.validate_navigation_group_click(action, 71, [screenshot])
        action["pointerAction"]["screenX"] = 115
        action["pointerAction"]["parentTreeTarget"]["rectangle"]["x"] = 116
        action["pointerAction"]["disclosurePointDerivation"]["uiaTreeBoundsAtInspection"]["x"] = 116
        with self.assertRaisesRegex(validator.ValidationError, "outside the UIA parent Tree bounds"):
            validator.validate_navigation_group_click(action, 71, [screenshot])
        action["pointerAction"]["parentTreeTarget"]["rectangle"]["x"] = 110
        action["pointerAction"]["disclosurePointDerivation"]["uiaTreeBoundsAtInspection"]["x"] = 110
        action["expansionStateAfter"] = "Collapsed"
        with self.assertRaisesRegex(validator.ValidationError, "state transition"):
            validator.validate_navigation_group_click(action, 71, [screenshot])

    def test_ai_cell_action_rejects_nested_identity_or_out_of_bounds_point(self) -> None:
        target = {
            "row": 4,
            "column": 0,
            "cellIdentity": {"name": "Select", "automationId": "reviewTable", "controlType": "ControlType.Custom", "processId": 71, "runtimeId": "1,2,3"},
            "cellControl": {"name": "Select", "automationId": "reviewTable", "controlType": "ControlType.Custom", "processId": 71, "ownerWindowHandle": 9001, "ownerWindowProcessId": 71, "rectangle": {"x": 120, "y": 240, "width": 20, "height": 20}},
            "cellRectangle": {"x": 120, "y": 240, "width": 20, "height": 20, "isOffscreen": False},
            "inputPoint": {"x": 130, "y": 250, "basis": "center of UIA GridPattern cell; Next custom delegate paints its check indicator at the centered cell position"},
            "ownerWindow": {"processId": 71, "windowHandle": 9001, "automationId": "QApplication.speakingEvaluationAiBatchDialog", "bounds": {"x": 100, "y": 200, "width": 500, "height": 400}},
        }
        validator.validate_ai_cell_target(target, 4, 0, 71, "test target")
        target["cellControl"]["automationId"] = "different"
        with self.assertRaisesRegex(validator.ValidationError, "identity"):
            validator.validate_ai_cell_target(target, 4, 0, 71, "test target")

    def test_ai_cell_click_fallback_requires_nested_identity_bounds_and_pattern_state(self) -> None:
        target = {
            "row": 4,
            "column": 0,
            "cellIdentity": {"name": "Select", "automationId": "reviewTable", "controlType": "ControlType.Custom", "processId": 71, "runtimeId": "1,2,3"},
            "cellControl": {"name": "Select", "automationId": "reviewTable", "controlType": "ControlType.Custom", "processId": 71, "ownerWindowHandle": 9001, "ownerWindowProcessId": 71, "rectangle": {"x": 120, "y": 240, "width": 20, "height": 20}},
            "cellRectangle": {"x": 120, "y": 240, "width": 20, "height": 20, "isOffscreen": False},
            "inputPoint": {"x": 130, "y": 250, "basis": "center of UIA GridPattern cell; Next custom delegate paints its check indicator at the centered cell position"},
            "ownerWindow": {"processId": 71, "windowHandle": 9001, "automationId": "QApplication.speakingEvaluationAiBatchDialog", "bounds": {"x": 100, "y": 200, "width": 500, "height": 400}},
        }
        action = {
            "method": "UIA-derived cell BoundingRectangle bounded WM mouse click",
            "row": 4,
            "column": 0,
            "target": copy.deepcopy(target["cellControl"]),
            "cellTargetBefore": copy.deepcopy(target),
            "cellTargetAfter": copy.deepcopy(target),
            "stateBefore": {"available": True, "state": "Off", "source": "LegacyIAccessiblePattern.State"},
            "stateAfter": {"available": True, "state": "On", "source": "LegacyIAccessiblePattern.State"},
            "togglePatternAvailable": False,
            "toggleReturned": False,
            "toggleReturnedWithoutTransition": False,
            "pointerAction": {
                "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                "screenX": 130,
                "screenY": 250,
                "sourceTarget": copy.deepcopy(target["cellIdentity"]),
                "sourceRectangle": copy.deepcopy(target["cellRectangle"]),
                "targetProcessId": 71,
                "windowHandle": 9001,
                "windowProcessId": 71,
                "measuredDialogBounds": copy.deepcopy(target["ownerWindow"]["bounds"]),
            },
        }
        validator.validate_ai_cell_action(action, 4, 71)
        action["pointerAction"]["sourceTarget"]["runtimeId"] = "wrong"
        with self.assertRaisesRegex(validator.ValidationError, "identity"):
            validator.validate_ai_cell_action(action, 4, 71)
        action["pointerAction"]["sourceTarget"]["runtimeId"] = "1,2,3"
        action["togglePatternAvailable"] = True
        action["toggleReturned"] = True
        action["toggleReturnedWithoutTransition"] = True
        action["method"] = "UIA-derived cell BoundingRectangle bounded WM mouse click after TogglePattern produced no state transition"
        validator.validate_ai_cell_action(action, 4, 71)
        action["toggleReturnedWithoutTransition"] = False
        with self.assertRaisesRegex(validator.ValidationError, "returned without a state transition"):
            validator.validate_ai_cell_action(action, 4, 71)


if __name__ == "__main__":
    unittest.main(verbosity=2)
