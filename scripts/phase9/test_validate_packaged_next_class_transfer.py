#!/usr/bin/env python3
"""Focused fail-closed tests for P9-11 workspace preparation validation."""

from __future__ import annotations

import unittest
import copy
from typing import Any

import validate_packaged_next_class_transfer as validator


def _snapshot(tables: dict[str, tuple[list[str], list[list[Any]]]]) -> dict[str, Any]:
    result: dict[str, Any] = {"tables": {}}
    for name, (columns, rows) in tables.items():
        schema = [[index, column, "TEXT", 0, None, 0] for index, column in enumerate(columns)]
        result["tables"][name] = {"schema": schema, "rows": copy.deepcopy(rows), "rowCount": len(rows)}
    return result


def _seeded_pair() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]]:
    classes = [[class_id, f"Class {class_id}"] for class_id in range(1, 97)]
    regular = []
    row_id = 1
    for class_id in range(1, 49):
        for meeting in range(8):
            regular.append([row_id, class_id, "Monday", f"{meeting}:00 AM", f"{meeting}:50 AM"])
            row_id += 1
    for class_id in range(49, 97):
        row_id += 1
        regular.append([row_id, class_id, "Friday", "5:00 PM", "5:50 PM"])
    intensive = [[index, index, "Tuesday", f"{index}:00 AM", f"{index}:50 AM"] for index in range(1, 25)]
    grades = ("E4", "E5", "M1", "M2")
    class_info_columns = ["class_id", "teacher_id", "class_grade", "class_level", "reading_book", "essay_book", "class_color", "notes", "time_filler"]
    class_info_rows = [
        [
            class_id,
            ((class_id - 1) % 24) + 1,
            grades[(class_id - 1) % len(grades)],
            f"Level {((class_id - 1) % 8) + 1:02d}",
            f"Heavy Reading {class_id:03d}",
            f"Heavy Essay {class_id:03d}",
            f"#{class_id:06X}",
            f"Fixture class {class_id:02d}",
            class_id % 2,
        ]
        for class_id in range(1, 97)
    ]
    base = {
        "classes": (["id", "name"], classes),
        "class_times": (["id", "class_id", "day", "start_time", "end_time"], regular),
        "class_intensive_times": (["id", "class_id", "day", "start_time", "end_time"], intensive),
        "class_info": (class_info_columns, class_info_rows),
        "teachers": (
            ["id", "teacher_kr", "teacher_en", "preferred_romanization", "preferred_name", "birthday", "phone_number"],
            [
                [i, f"Teacher {i:02d} KR", f"Teacher {i:02d}", f"Teacher-{i:02d}", f"Teacher {i:02d}", f"1981-02-{i:02d}", f"010-2000-{i:04d}"]
                for i in range(1, 25)
            ],
        ),
    }
    cancel = _snapshot(base)
    source_regular = [row for row in regular if not (1 <= row[1] <= 48 and row[1] not in {1, 9})]
    source_class_info = []
    for baseline in class_info_rows:
        row = dict(zip(class_info_columns, baseline, strict=True))
        if int(row["class_id"]) <= 48:
            row.update(validator._expected_source_class_details(
                int(row["class_id"]), str(row["class_grade"]), str(row["class_level"])
            ))
        source_class_info.append([row[column] for column in class_info_columns])
    teacher_columns = base["teachers"][0]
    source_teachers = []
    for baseline in base["teachers"][1]:
        row = dict(zip(teacher_columns, baseline, strict=True))
        row.update(validator._expected_source_teacher_profile(int(row["id"])))
        source_teachers.append([row[column] for column in teacher_columns])
    source = _snapshot({
        **base,
        "class_times": (base["class_times"][0], source_regular),
        "teachers": (teacher_columns, source_teachers),
        "class_info": (class_info_columns, source_class_info),
    })
    teacher_transform = validator._expected_source_teacher_profile_transformation(source, cancel)
    class_detail_transform = validator._expected_source_class_detail_transformation(source, cancel)
    return source, cancel, teacher_transform, class_detail_transform


def _apply_clone(cancel: dict[str, Any]) -> dict[str, Any]:
    apply_tables: dict[str, tuple[list[str], list[list[Any]]]] = {}
    for table, value in cancel["tables"].items():
        columns = [column[1] for column in value["schema"]]
        if table in {"class_times", "class_intensive_times"}:
            apply_tables[table] = (columns, [])
            continue
        rows = [list(validator._expected_apply_preparation_row(table, columns, row)) for row in value["rows"]]
        apply_tables[table] = (columns, rows)
    return _snapshot(apply_tables)


class ValidatePackagedNextClassTransferTests(unittest.TestCase):
    @staticmethod
    def _script_only_inventory_pair() -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
        build_rows = [
            {"path": "src/next/ui/class_transfer_workflow_page.cpp", "sizeBytes": 10, "sha256": "a" * 64},
            {"path": "scripts/phase9/prepare_class_transfer_workspace.py", "sizeBytes": 20, "sha256": "b" * 64},
            {"path": "scripts/phase9/run_packaged_next_class_transfer.ps1", "sizeBytes": 30, "sha256": "c" * 64},
            {"path": "scripts/phase9/test_prepare_class_transfer_workspace.py", "sizeBytes": 40, "sha256": "d" * 64},
            {"path": "scripts/phase9/validate_packaged_next_class_transfer.py", "sizeBytes": 50, "sha256": "e" * 64},
            {"path": "scripts/phase9/test_validate_packaged_next_class_transfer.py", "sizeBytes": 60, "sha256": "f" * 64},
        ]
        current_rows = copy.deepcopy(build_rows)
        for index, row in enumerate(current_rows[1:], start=1):
            row["sizeBytes"] += index
            row["sha256"] = str(index) * 64
        return build_rows, current_rows

    def test_script_only_build_derivation_accepts_exact_harness_delta(self) -> None:
        build_rows, current_rows = self._script_only_inventory_pair()
        changes = [
            {
                "path": build_row["path"],
                "buildSizeBytes": build_row["sizeBytes"],
                "buildSha256": build_row["sha256"],
                "currentSizeBytes": current_row["sizeBytes"],
                "currentSha256": current_row["sha256"],
            }
            for build_row, current_row in zip(build_rows[1:], current_rows[1:], strict=True)
        ]
        changes.sort(key=lambda row: str(row["path"]).casefold())
        product_hash = validator.inventory_fingerprint([build_rows[0]])
        validated = validator.validate_script_only_source_delta(
            build_rows,
            current_rows,
            changes,
            build_source_sha256=validator.inventory_fingerprint(build_rows),
            current_source_sha256=validator.inventory_fingerprint(current_rows),
            product_source_sha256=product_hash,
            allowed_paths=sorted(validator.ALLOWED_SCRIPT_ONLY_BUILD_DELTA),
        )
        self.assertEqual([row["path"] for row in validated], sorted(validator.ALLOWED_SCRIPT_ONLY_BUILD_DELTA))

    def test_script_only_build_derivation_rejects_product_source_change(self) -> None:
        build_rows, current_rows = self._script_only_inventory_pair()
        current_rows[0]["sha256"] = "1" * 64
        with self.assertRaisesRegex(validator.ValidationFailure, "product/compiler source changed"):
            validator.validate_script_only_source_delta(
                build_rows,
                current_rows,
                [],
                build_source_sha256=validator.inventory_fingerprint(build_rows),
                current_source_sha256=validator.inventory_fingerprint(current_rows),
                product_source_sha256=validator.inventory_fingerprint([build_rows[0]]),
                allowed_paths=sorted(validator.ALLOWED_SCRIPT_ONLY_BUILD_DELTA),
            )

    def test_natural_exit_requires_explicit_unforced_zero_exit_evidence(self) -> None:
        route = {
            "routeId": "route-1-next-export",
            "normalExit": True,
            "processExitCode": 0,
            "processExit": {"exitCode": 0, "waitCompleted": True, "forcedTerminationForCleanup": False},
        }
        validator.validate_process_exit(route)
        route["processExit"]["forcedTerminationForCleanup"] = True
        with self.assertRaisesRegex(validator.ValidationFailure, "did not exit naturally"):
            validator.validate_process_exit(route)
        del route["processExit"]["forcedTerminationForCleanup"]
        with self.assertRaisesRegex(validator.ValidationFailure, "did not exit naturally"):
            validator.validate_process_exit(route)

    def test_evaluation_cell_count_excludes_blank_fixed_schema_padding(self) -> None:
        rows = [
            [f"row-{row}-cell-{column}" for column in range(11)]
            for row in range(30)
        ] + [[""] * 11 for _ in range(70)]

        self.assertEqual(len(rows) * 20, 2000)
        self.assertEqual(330, validator._count_nonempty_evaluation_cells(rows))
        self.assertEqual(
            6600,
            sum(validator._count_nonempty_evaluation_cells(rows) for _ in range(20)),
        )

    def test_apply_delta_expects_only_three_classes_navigator_default_rows(self) -> None:
        self.assertEqual(
            {
                ("classes_navigation_visibility_scope", "active_schedule"): 1,
                ("classes_navigation_day_filter_reset_policy", "on_application_close"): 1,
                ("classes_navigation_class_selection_reset_policy", "on_application_close"): 1,
            },
            dict(validator._expected_classes_navigator_default_rows()),
        )

    def test_import_teacher_projection_matches_product_romanization_normalization(self) -> None:
        columns = [
            "teacher_en",
            "preferred_romanization",
            "preferred_name",
            "birthday",
        ]
        for suffix in "ABCDEFGHIJKLMNOPQRSTUVWX":
            profile = {
                "teacher_en": f"Teacher {suffix}",
                "preferred_romanization": f"Teacher-{suffix}",
                "preferred_name": f"Teacher {suffix}",
                "birthday": "01-01",
            }
            projected = validator._expected_import_teacher_profile(profile, columns)
            self.assertEqual(projected["preferred_romanization"], f"Teacher-{suffix.lower()}")
            for column in columns:
                if column != "preferred_romanization":
                    self.assertEqual(projected[column], profile[column])

        malformed = {
            "teacher_en": "Teacher A",
            "preferred_romanization": "Teacher-B",
            "preferred_name": "Teacher A",
            "birthday": "01-01",
        }
        with self.assertRaisesRegex(validator.ValidationFailure, "differs from its approved"):
            validator._expected_import_teacher_profile(malformed, columns)

    def test_lifecycle_trace_allows_one_calendar_cycle_after_apply_navigation(self) -> None:
        def record(checkpoint: str, sequence: int, timestamp: str) -> dict[str, Any]:
            return {
                "checkpoint": checkpoint,
                "sequence": sequence,
                "captured_at_utc": timestamp,
                "metrics": {"page_instance_token": "page-1", "imports_applied": 1},
            }

        diagnostics = [
            record("activated", 1, "t1"),
            record("suspended", 2, "t2"),
            record("activated", 3, "t3"),
            record("suspended", 4, "t4"),
            record("activated", 5, "t5"),
            record("suspended", 6, "t6"),
        ]
        route = {
            "routeId": "route-2-next-apply",
            "lifecycle": {
                "suspendCount": 1,
                "reentryCount": 1,
                "applyAutomaticSuspension": diagnostics[1],
                "preCalendarReentry": diagnostics[2],
                "shutdownSuspension": diagnostics[5],
                "applyToClassesRouteLogLine": "navigation event=page-route outcome=completed cause=selection from=class_transfer to=classes",
                "classesToClassTransferRouteLogLine": "navigation event=page-route outcome=completed cause=selection from=classes to=class_transfer",
            },
        }
        validator.validate_lifecycle_checkpoints(route, diagnostics)
        misordered = copy.deepcopy(diagnostics)
        misordered[3]["sequence"] = 2
        with self.assertRaisesRegex(validator.ValidationFailure, "misordered"):
            validator.validate_lifecycle_checkpoints(route, misordered)
        missing_manual_suspension = diagnostics[:-2]
        with self.assertRaisesRegex(validator.ValidationFailure, "expected app transition"):
            validator.validate_lifecycle_checkpoints(route, missing_manual_suspension)
        route["lifecycle"]["shutdownSuspension"] = diagnostics[4]
        with self.assertRaisesRegex(validator.ValidationFailure, "terminal page suspension"):
            validator.validate_lifecycle_checkpoints(route, diagnostics)
        route["lifecycle"]["shutdownSuspension"] = diagnostics[5]
        route["lifecycle"]["classesToClassTransferRouteLogLine"] = ""
        with self.assertRaisesRegex(validator.ValidationFailure, "restore route log line"):
            validator.validate_lifecycle_checkpoints(route, diagnostics)

    def test_review_tables_require_visible_pid_bound_uia_evidence(self) -> None:
        route = {
            "routeId": "route-2-next-apply",
            "uia": {
                "classReviewTable": {
                    "control": {"automationId": "QApplication.classTransferClassReviewTable", "controlType": "ControlType.Table", "processId": 1234, "offscreen": False, "rectangle": {"width": 400, "height": 300}},
                    "descendantCount": 12,
                },
                "teacherReviewTable": {
                    "control": {"automationId": "QApplication.classTransferTeacherReviewTable", "controlType": "ControlType.Table", "processId": 1234, "offscreen": False, "rectangle": {"width": 400, "height": 300}},
                    "descendantCount": 7,
                },
            },
        }
        validator.validate_review_table_uia(route, 1234)
        route["uia"]["teacherReviewTable"]["control"]["processId"] = 9999
        with self.assertRaisesRegex(validator.ValidationFailure, "not a visible PID-bound UIA table"):
            validator.validate_review_table_uia(route, 1234)

    def test_post_apply_classes_page_matches_first_exported_class(self) -> None:
        pid = 1234
        route = {
            "routeId": "route-2-next-apply",
            "processIdentity": {"pid": pid},
            "uia": {
                "postApplyPage": {
                    "root": {
                        "automationId": "QApplication.classesNavigatorPage",
                        "controlType": "ControlType.Group",
                        "processId": pid,
                        "offscreen": False,
                        "rectangle": {"width": 1000, "height": 700},
                    },
                    "title": {
                        "automationId": "QApplication.classesNavigatorTitle",
                        "controlType": "ControlType.Text",
                        "processId": pid,
                        "offscreen": False,
                        "name": "Classes",
                    },
                    "selectedClassDetails": {
                        "classGrade": {"automationId": "QApplication.classEditorClassGrade", "controlType": "ControlType.ComboBox", "processId": pid, "offscreen": False, "enabled": True, "value": "E4"},
                        "classLevel": {"automationId": "QApplication.classEditorClassLevel", "controlType": "ControlType.ComboBox", "processId": pid, "offscreen": False, "enabled": True, "value": "Perseus"},
                        "readingBook": {"automationId": "QApplication.classEditorReadingBook", "controlType": "ControlType.ComboBox", "processId": pid, "offscreen": False, "enabled": True, "value": ""},
                        "essayBook": {"automationId": "QApplication.classEditorEssayBook", "controlType": "ControlType.ComboBox", "processId": pid, "offscreen": False, "enabled": True, "value": ""},
                    },
                }
            },
        }
        package = {"classes": [{"info": {"class_grade": "E4", "class_level": "Perseus", "reading_book": "", "essay_book": ""}}]}
        validator.validate_post_apply_selection_matches_package(route, package)
        route["uia"]["postApplyPage"]["selectedClassDetails"]["classLevel"]["value"] = "Apollo"
        with self.assertRaisesRegex(validator.ValidationFailure, "differs from the first exported class"):
            validator.validate_post_apply_selection_matches_package(route, package)
        route["uia"]["postApplyPage"]["selectedClassDetails"]["classLevel"]["value"] = "Perseus"
        route["uia"]["postApplyPage"]["root"]["processId"] = 9999
        with self.assertRaisesRegex(validator.ValidationFailure, "not a visible PID-bound UIA group"):
            validator.validate_post_apply_selection_matches_package(route, package)

    def test_source_transform_allows_only_approved_regular_schedule_removal(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        result = validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)
        self.assertEqual(result["removedRegularRows"], 368)
        self.assertEqual(result["retainedSelectedRegularRows"], 16)
        self.assertEqual(result["retainedSelectedIntensiveRows"], 24)
        self.assertEqual(result["normalizedTeacherRows"], 24)
        self.assertEqual(result["normalizedTeacherColumns"], list(validator.SOURCE_TEACHER_PROFILE_COLUMNS))
        self.assertEqual(result["normalizedClassDetailRows"], 48)
        self.assertEqual(result["normalizedClassDetailColumns"], list(validator.SOURCE_CLASS_DETAIL_COLUMNS))
        self.assertTrue(result["classTeacherRelationsSha256"])

    def test_source_transform_rejects_unapproved_table_change(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        source["tables"]["class_info"]["rows"][0][7] = "changed note"
        with self.assertRaisesRegex(validator.ValidationFailure, "outside the approved class-detail fields"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_source_profile_transform_rejects_unapproved_profile_value(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        teacher_columns = [column[1] for column in source["tables"]["teachers"]["schema"]]
        english_index = teacher_columns.index("teacher_en")
        source["tables"]["teachers"]["rows"][0][english_index] = "Changed Teacher"
        with self.assertRaisesRegex(validator.ValidationFailure, "outside the approved profile normalization"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_source_profile_transform_rejects_unapproved_nonprofile_value(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        teacher_columns = [column[1] for column in source["tables"]["teachers"]["schema"]]
        phone_index = teacher_columns.index("phone_number")
        source["tables"]["teachers"]["rows"][0][phone_index] = "changed phone"
        with self.assertRaisesRegex(validator.ValidationFailure, "outside the approved profile normalization"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_source_profile_transform_rejects_tampered_before_after_hashes(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        teacher_transform["rows"][0]["beforeRowSha256"] = "0" * 64
        with self.assertRaisesRegex(validator.ValidationFailure, "rows or hashes differ"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_source_class_detail_transform_rejects_unapproved_profile_value(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        class_columns = [column[1] for column in source["tables"]["class_info"]["schema"]]
        source["tables"]["class_info"]["rows"][0][class_columns.index("class_level")] = "Zeus"
        with self.assertRaisesRegex(validator.ValidationFailure, "outside the approved class-detail fields"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_source_class_detail_transform_rejects_tampered_hashes(self) -> None:
        source, cancel, teacher_transform, detail_transform = _seeded_pair()
        detail_transform["rows"][0]["unchangedColumnsSha256"] = "0" * 64
        with self.assertRaisesRegex(validator.ValidationFailure, "rows or hashes differ"):
            validator.validate_source_preparation(source, cancel, teacher_transform, detail_transform)

    def test_apply_clone_namespaces_relations_and_clears_both_schedule_tables(self) -> None:
        _, cancel, _, _ = _seeded_pair()
        apply = _apply_clone(cancel)
        result = validator.validate_apply_preparation(apply, cancel)
        self.assertTrue(result["preservedRowsAndRelations"])
        self.assertEqual(result["scheduleTablesCleared"], ["class_times", "class_intensive_times"])

    def test_apply_clone_rejects_unapproved_field_change(self) -> None:
        _, cancel, _, _ = _seeded_pair()
        apply = _apply_clone(cancel)
        apply["tables"]["classes"]["rows"][0][1] = "wrong target"
        with self.assertRaisesRegex(validator.ValidationFailure, "unapproved rows or references in classes"):
            validator.validate_apply_preparation(apply, cancel)

    def test_inventory_fingerprint_is_path_and_hash_bound(self) -> None:
        rows = [
            {"path": "src/z.cpp", "sizeBytes": 2, "sha256": "b" * 64},
            {"path": "src/a.cpp", "sizeBytes": 1, "sha256": "a" * 64},
        ]
        original = validator.inventory_fingerprint(rows)
        self.assertEqual(original, validator.inventory_fingerprint(list(reversed(rows))))
        changed = [*rows, {"path": "src/new.cpp", "sizeBytes": 3, "sha256": "c" * 64}]
        self.assertNotEqual(original, validator.inventory_fingerprint(changed))

    @staticmethod
    def _uia_export_route(names: list[str]) -> dict[str, Any]:
        pid = 1234
        rows = [
            {
                "index": index,
                "expectedClassName": name,
                "identity": {
                    "name": name,
                    "automationId": "QApplication.classTransferExportClassList",
                    "processId": pid,
                    "controlType": "ControlType.ListItem",
                    "offscreen": False,
                    "rectangle": {"x": 500, "y": 100 + index * 30, "width": 240, "height": 30},
                },
                "method": "uia-listitem-pid-bound-window-input",
                "toggleStateAfter": "On",
                "nativeActivation": {
                    "expectedProcessId": pid,
                    "windowProcessId": pid,
                    "screenX": 512,
                    "screenY": 115 + index * 30,
                    "leftButtonDownPosted": True,
                    "leftButtonUpPosted": True,
                },
            }
            for index, name in enumerate(names)
        ]
        return {
            "routeId": "route-1-next-export",
            "processIdentity": {"pid": pid},
            "uia": {
                "exportSelection": rows,
                "exportListScroll": [
                    {
                        "actionKind": "scroll-export-class-list",
                        "method": "uia-list-bound-pid-verified-window-wheel",
                        "targetList": {
                            "processId": pid,
                            "controlType": "ControlType.List",
                            "automationId": "QApplication.classTransferExportClassList",
                        },
                        "messageResult": {
                            "expectedProcessId": pid,
                            "windowProcessId": pid,
                            "wheelPosted": True,
                            "wheelDelta": -120,
                        },
                    }
                ],
            },
        }

    def test_uia_export_selection_requires_the_exact_ordered_pid_bound_rows(self) -> None:
        names = [f"Class {index:03d}" for index in range(1, 49)]
        route = self._uia_export_route(names)
        validator.validate_export_uia_selection(route, names)

    def test_uia_export_selection_rejects_wrong_row_order_and_scroll_pid(self) -> None:
        names = [f"Class {index:03d}" for index in range(1, 49)]
        route = self._uia_export_route(names)
        route["uia"]["exportSelection"][1]["identity"]["processId"] = 9999
        with self.assertRaisesRegex(validator.ValidationFailure, "PID-bound ListItem"):
            validator.validate_export_uia_selection(route, names)
        route = self._uia_export_route(names)
        route["uia"]["exportListScroll"][0]["messageResult"]["windowProcessId"] = 9999
        with self.assertRaisesRegex(validator.ValidationFailure, "PID-bound window evidence"):
            validator.validate_export_uia_selection(route, names)


if __name__ == "__main__":
    unittest.main(verbosity=2)
