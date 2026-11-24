"""Focused tests for the P9-17 large-roster fixture preparation."""

from __future__ import annotations

import importlib.util
import json
import sqlite3
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("prepare_p9_17_large_roster.py")
SPEC = importlib.util.spec_from_file_location("p917_fixture", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)

REPOSITORY = Path(__file__).resolve().parents[2]
BASE = (
    REPOSITORY
    / "artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps"
)
SEED = REPOSITORY / "tests/fixtures/workspaces/large_startup.sql"


class PrepareP917LargeRosterTests(unittest.TestCase):
    def test_default_prepare_has_the_same_filled_database_bytes_as_explicit_filled_mode(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            default_workspace = root / "default.tps"
            filled_workspace = root / "filled.tps"
            default_receipt = fixture.prepare(BASE, SEED, default_workspace)
            filled_receipt = fixture.prepare(BASE, SEED, filled_workspace, "filled")

            self.assertEqual(default_workspace.read_bytes(), filled_workspace.read_bytes())
            self.assertEqual(default_receipt["workspaceSha256"], filled_receipt["workspaceSha256"])
            self.assertEqual(default_receipt["identity"], filled_receipt["identity"])
            self.assertEqual(default_receipt["baselineTables"], filled_receipt["baselineTables"])
            self.assertNotIn("payloadMode", default_receipt)
            self.assertEqual(filled_receipt["payloadMode"], "filled")
            self.assertTrue(filled_receipt["payloadDiagnostic"]["diagnosticOnly"])
            self.assertFalse(filled_receipt["payloadDiagnostic"]["accepted"])
            self.assertEqual(filled_receipt["payloadDiagnostic"]["customCellRecordCount"], 15000)
            self.assertEqual(filled_receipt["payloadDiagnostic"]["customCellValueCount"], 15000)
            self.assertEqual(filled_receipt["payloadDiagnostic"]["blankCustomCellValueCount"], 0)
            self.assertEqual(filled_receipt["payloadDiagnostic"]["compactSentinelValue"], None)
            self.assertEqual(filled_receipt["payloadDiagnostic"]["compactSentinelValueCount"], 0)

    def test_sparse_mode_uses_compact_values_and_preserves_probe_and_grid_contracts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "sparse.tps"
            receipt_path = root / "preparation-receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace, "sparse")
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
            before = fixture.snapshot("before", workspace, receipt_path)

            self.assertEqual(receipt["payloadMode"], "sparse")
            payload = receipt["payloadDiagnostic"]
            self.assertTrue(payload["diagnosticOnly"])
            self.assertFalse(payload["accepted"])
            self.assertEqual(payload["customCellRecordCount"], 15000)
            self.assertEqual(payload["customCellValueCount"], 15000)
            self.assertEqual(payload["blankCustomCellValueCount"], 0)
            self.assertEqual(payload["compactSentinelValue"], fixture.SPARSE_PAYLOAD_SENTINEL)
            self.assertEqual(payload["compactSentinelValueCount"], 14997)
            self.assertEqual(payload["preservedProbeCellCount"], 3)
            self.assertEqual(
                [(cell["row"], cell["storedColumn"]) for cell in payload["probeCells"]],
                [(0, 6), (49, 77), (99, 155)],
            )
            self.assertEqual(receipt["identity"]["targetRosterRowCount"], 100)
            self.assertEqual(receipt["identity"]["targetStoredRosterColumnCount"], 156)
            self.assertEqual(receipt["identity"]["targetEnglishNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["targetKoreanNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["classCount"], 96)
            self.assertEqual(receipt["identity"]["classInfoCount"], 96)
            self.assertEqual(receipt["identity"]["scheduleEntryCount"], 768)
            self.assertEqual(before["workspaceTables"]["roster_data"]["rowCount"], 22325)
            self.assertEqual(before["payloadMode"], "sparse")
            self.assertEqual(before["payloadDiagnostic"]["snapshotCounts"]["customCellRecordCount"], 15000)
            self.assertEqual(before["payloadDiagnostic"]["snapshotCounts"]["compactSentinelValueCount"], 14997)

            connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
            try:
                custom_rows = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index BETWEEN 6 AND 155 ORDER BY row_index,col_index"
                ).fetchall()
                names = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index IN (0,1) ORDER BY row_index,col_index"
                ).fetchall()
                columns = connection.execute(
                    "SELECT position,name,width FROM roster_columns WHERE class_id=1 ORDER BY position"
                ).fetchall()
                total_roster_rows = connection.execute(
                    "SELECT COUNT(*) FROM roster_data"
                ).fetchone()[0]
            finally:
                connection.close()

            self.assertEqual(len(custom_rows), 15000)
            self.assertEqual(sum(value == fixture.SPARSE_PAYLOAD_SENTINEL for _, _, value in custom_rows), 14997)
            self.assertEqual(sum(value == "" for _, _, value in custom_rows), 0)
            self.assertEqual(sum(value != "" for _, _, value in custom_rows), 15000)
            probe_coordinates = set(fixture.PAYLOAD_PROBE_COORDINATES)
            self.assertTrue(
                all(
                    value == fixture.SPARSE_PAYLOAD_SENTINEL
                    for row, column, value in custom_rows
                    if (row, column) not in probe_coordinates
                )
            )
            self.assertEqual(
                {(row, column): value for row, column, value in custom_rows if (row, column) in probe_coordinates},
                {
                    (int(probe["row"]), int(probe["storedColumn"])): probe["beforeValue"]
                    for probe in payload["probeCells"]
                },
            )
            self.assertEqual(len(names), 200)
            self.assertEqual(len({value for _, column, value in names if column == 0}), 100)
            self.assertEqual(len({value for _, column, value in names if column == 1}), 100)
            self.assertEqual(len(columns), 156)
            self.assertEqual(columns[0], (0, "English", 170))
            self.assertEqual(columns[5], (5, "Fall", 130))
            self.assertEqual(columns[6], (6, "Student", 180))
            self.assertEqual(columns[-1], (155, "Stress Field 150", 140))
            self.assertEqual(total_roster_rows, 22325)

            for target in receipt["expectedEditTargets"]:
                connection = sqlite3.connect(workspace)
                try:
                    connection.execute(
                        "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                        (target["afterValue"], target["classId"], target["row"], target["storedColumn"]),
                    )
                    connection.commit()
                finally:
                    connection.close()
            after = fixture.snapshot("after", workspace, receipt_path)
            self.assertEqual(after["payloadMode"], "sparse")
            self.assertEqual(after["identity"]["targetRosterCellCount"], 15200)
            self.assertEqual(after["workspaceTables"]["roster_data"]["rowCount"], 22325)
            self.assertEqual(after["payloadDiagnostic"]["snapshotCounts"]["customCellRecordCount"], 15000)
            self.assertEqual(after["payloadDiagnostic"]["snapshotCounts"]["customCellValueCount"], 15000)
            self.assertEqual(after["payloadDiagnostic"]["snapshotCounts"]["blankCustomCellValueCount"], 0)
            self.assertEqual(after["payloadDiagnostic"]["snapshotCounts"]["compactSentinelValueCount"], 14997)
            self.assertEqual(
                [cell["snapshotValue"] for cell in after["payloadDiagnostic"]["snapshotProbeCells"]],
                [target["afterValue"] for target in receipt["expectedEditTargets"]],
            )
            connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
            try:
                after_target_roster_rows = connection.execute(
                    "SELECT COUNT(*) FROM roster_data WHERE class_id=1"
                ).fetchone()[0]
                after_custom_rows = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index BETWEEN 6 AND 155 ORDER BY row_index,col_index"
                ).fetchall()
            finally:
                connection.close()
            self.assertEqual(after_target_roster_rows, 15200)
            self.assertEqual(len(after_custom_rows), 15000)
            self.assertEqual(
                sum(value == fixture.SPARSE_PAYLOAD_SENTINEL for _, _, value in after_custom_rows),
                14997,
            )
            self.assertEqual(sum(value not in (None, "") for _, _, value in after_custom_rows), 15000)
            self.assertEqual(
                {(row, column): value for row, column, value in after_custom_rows if (row, column) in probe_coordinates},
                {
                    (int(target["row"]), int(target["storedColumn"])): target["afterValue"]
                    for target in receipt["expectedEditTargets"]
                },
            )

    def test_reduced_rows_mode_retains_only_probes_and_preserves_non_target_database(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_workspace = root / "filled.tps"
            reduced_workspace = root / "reduced.tps"
            receipt_path = root / "preparation-receipt.json"
            filled_receipt = fixture.prepare(BASE, SEED, filled_workspace, record_count_mode="filled")
            receipt = fixture.prepare(BASE, SEED, reduced_workspace, record_count_mode="reduced-rows")
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
            before = fixture.snapshot("before", reduced_workspace, receipt_path)

            self.assertEqual(receipt["recordCountMode"], "reduced-rows")
            diagnostic = receipt["recordCountDiagnostic"]
            self.assertTrue(diagnostic["diagnosticOnly"])
            self.assertFalse(diagnostic["accepted"])
            self.assertEqual(diagnostic["customCellRecordCount"], 3)
            self.assertEqual(diagnostic["customCellValueCount"], 3)
            self.assertEqual(diagnostic["blankCustomCellValueCount"], 0)
            self.assertEqual(diagnostic["removedCustomCellRecordCount"], 14997)
            self.assertEqual(diagnostic["retainedCustomRecordCoordinates"], [[0, 6], [49, 77], [99, 155]])
            self.assertEqual(receipt["identity"]["targetRosterCellCount"], 203)
            self.assertEqual(receipt["identity"]["targetCustomRosterCellCount"], 3)
            self.assertEqual(receipt["identity"]["databaseRosterCellCount"], 7328)
            self.assertEqual(receipt["identity"]["targetRosterRowCount"], 100)
            self.assertEqual(receipt["identity"]["targetEnglishNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["targetKoreanNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["scheduleEntryCount"], 768)
            self.assertEqual(receipt["identity"]["targetStoredRosterColumnCount"], 156)
            self.assertEqual(receipt["identity"]["targetDisplayedRosterColumnCount"], 156)
            self.assertEqual(before["workspaceTables"]["roster_data"]["rowCount"], 7328)
            self.assertEqual(before["recordCountDiagnostic"]["snapshotCounts"]["customCellRecordCount"], 3)
            self.assertEqual(before["integrityCheck"], "ok")
            self.assertEqual(before["foreignKeyErrors"], [])

            connection = sqlite3.connect(reduced_workspace)
            try:
                custom = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index BETWEEN 6 AND 155 ORDER BY row_index,col_index"
                ).fetchall()
                names = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index IN (0,1) ORDER BY row_index,col_index"
                ).fetchall()
                columns = connection.execute(
                    "SELECT position,name,width FROM roster_columns WHERE class_id=1 ORDER BY position"
                ).fetchall()
                integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
                foreign_keys = connection.execute("PRAGMA foreign_key_check").fetchall()
            finally:
                connection.close()
            self.assertEqual([(row, column) for row, column, _ in custom], [(0, 6), (49, 77), (99, 155)])
            self.assertTrue(all(value not in (None, "", fixture.SPARSE_PAYLOAD_SENTINEL) for _, _, value in custom))
            self.assertEqual(len(names), 200)
            self.assertEqual(len({value for _, column, value in names if column == 0}), 100)
            self.assertEqual(len({value for _, column, value in names if column == 1}), 100)
            self.assertEqual(len(columns), 156)
            filled_column_rows = filled_receipt["baselineTables"]["roster_columns"]["rows"]
            self.assertEqual(
                columns,
                [
                    (row[3], row[2], row[4])
                    for row in sorted(
                        (row for row in filled_column_rows if row[1] == 1),
                        key=lambda row: row[3],
                    )
                ],
            )
            self.assertEqual(integrity, "ok")
            self.assertEqual(foreign_keys, [])

            for table_name in filled_receipt["baselineTables"]:
                filled_table = filled_receipt["baselineTables"][table_name]
                reduced_table = receipt["baselineTables"][table_name]
                if table_name != "roster_data":
                    self.assertEqual(filled_table, reduced_table, table_name)
                    continue
                columns_by_name = {name: index for index, name in enumerate(filled_table["columns"])}
                identity_indexes = [columns_by_name[name] for name in ("class_id", "row_index", "col_index")]
                filled_rows = {
                    tuple(row[index] for index in identity_indexes): row
                    for row in filled_table["rows"]
                }
                reduced_rows = {
                    tuple(row[index] for index in identity_indexes): row
                    for row in reduced_table["rows"]
                }
                removed = set(filled_rows) - set(reduced_rows)
                self.assertEqual(set(reduced_rows) - set(filled_rows), set())
                self.assertEqual(len(removed), 14997)
                self.assertTrue(all(key[0] == 1 and 6 <= key[2] <= 155 and key[1:] not in {(0, 6), (49, 77), (99, 155)} for key in removed))
                self.assertTrue(all(filled_rows[key] == reduced_rows[key] for key in reduced_rows))

            for target in receipt["expectedEditTargets"]:
                connection = sqlite3.connect(reduced_workspace)
                try:
                    connection.execute(
                        "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                        (target["afterValue"], target["classId"], target["row"], target["storedColumn"]),
                    )
                    connection.commit()
                finally:
                    connection.close()
            after = fixture.snapshot("after", reduced_workspace, receipt_path)
            self.assertEqual(after["identity"]["targetRosterCellCount"], 203)
            self.assertEqual(after["identity"]["databaseRosterCellCount"], 7328)
            self.assertEqual(after["workspaceTables"]["roster_data"]["rowCount"], 7328)
            self.assertEqual(after["recordCountDiagnostic"]["snapshotCounts"]["customCellRecordCount"], 3)
            self.assertEqual(
                [cell["snapshotValue"] for cell in after["recordCountDiagnostic"]["snapshotProbeCells"]],
                [target["afterValue"] for target in receipt["expectedEditTargets"]],
            )

    def test_reduced_rows_snapshots_reject_unexpected_custom_rows_and_changed_probe_or_name(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "reduced.tps"
            receipt_path = root / "preparation-receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace, record_count_mode="reduced-rows")
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")

            connection = sqlite3.connect(workspace)
            try:
                connection.execute(
                    "INSERT INTO roster_data (class_id,row_index,col_index,value) VALUES (1,1,7,'unexpected')"
                )
                connection.commit()
            finally:
                connection.close()
            with self.assertRaisesRegex(ValueError, "record count differs|exact coordinate set"):
                fixture.snapshot("after", workspace, receipt_path)

            connection = sqlite3.connect(workspace)
            try:
                connection.execute("DELETE FROM roster_data WHERE class_id=1 AND row_index=1 AND col_index=7")
                connection.execute("UPDATE roster_data SET value='unexpected-probe' WHERE class_id=1 AND row_index=0 AND col_index=6")
                connection.commit()
            finally:
                connection.close()
            with self.assertRaisesRegex(ValueError, "custom roster value differs"):
                fixture.snapshot("after", workspace, receipt_path)

            connection = sqlite3.connect(workspace)
            try:
                connection.execute("UPDATE roster_data SET value='unexpected-name' WHERE class_id=1 AND row_index=0 AND col_index=0")
                connection.commit()
            finally:
                connection.close()
            with self.assertRaisesRegex(ValueError, "required names differ"):
                fixture.snapshot("after", workspace, receipt_path)

    def test_prepare_pins_full_roster_dimensions_and_snapshots_all_tables(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "large-roster.tps"
            receipt_path = root / "preparation-receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace)
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")

            self.assertEqual(receipt["identity"]["targetClassId"], 1)
            self.assertEqual(receipt["identity"]["targetClassName"], "Heavy Fixture Class 001")
            self.assertEqual(
                receipt["identity"]["targetClassListAccessibleName"],
                "E4 Level 02 • M/F 5:00; M/T 6:00; T/W 3:00; W/Th 4:00 • Teacher 01",
            )
            self.assertEqual(receipt["identity"]["targetRosterRowCount"], 100)
            self.assertEqual(receipt["identity"]["targetStoredRosterColumnCount"], 156)
            self.assertEqual(receipt["identity"]["targetFixedBaseRosterColumnCount"], 6)
            self.assertEqual(receipt["identity"]["targetCustomRosterColumnCount"], 150)
            self.assertEqual(receipt["identity"]["targetDisplayedRosterColumnCount"], 156)
            self.assertEqual(receipt["identity"]["targetEnglishNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["targetKoreanNameCellCount"], 100)
            self.assertEqual(receipt["identity"]["targetCustomRosterCellCount"], 15000)
            self.assertEqual(receipt["identity"]["targetEvaluationRosterCellCount"], 0)
            self.assertEqual(receipt["identity"]["targetRosterCellCount"], 15200)
            self.assertEqual(receipt["identity"]["targetRosterGridCellCapacity"], 15600)
            self.assertEqual(receipt["identity"]["databaseRosterCellCount"], 22325)
            self.assertEqual(receipt["dimensionContract"]["fixedBaseColumns"], 6)
            self.assertEqual(receipt["dimensionContract"]["customStressColumns"], 150)
            self.assertEqual(receipt["dimensionContract"]["storedRosterColumns"], 156)
            self.assertEqual(receipt["dimensionContract"]["displayedRosterColumns"], 156)
            settings = dict(receipt["baselineTables"]["app_settings"]["rows"])
            self.assertEqual(settings["classes_navigation_visibility_scope"], "active_schedule")
            self.assertEqual(settings["classes_navigation_day_filter_reset_policy"], "on_application_close")
            self.assertEqual(settings["classes_navigation_class_selection_reset_policy"], "on_application_close")

            before = fixture.snapshot("before", workspace, receipt_path)
            self.assertEqual(before["integrityCheck"], "ok")
            self.assertEqual(before["foreignKeyErrors"], [])
            self.assertEqual(before["workspaceTableCount"], 17)
            self.assertEqual(before["workspaceTables"]["roster_data"]["rowCount"], 22325)
            self.assertEqual(len(receipt["expectedEditTargets"]), 3)
            self.assertEqual(
                [(target["modelColumn"], target["storedColumn"]) for target in receipt["expectedEditTargets"]],
                [(6, 6), (77, 77), (155, 155)],
            )
            self.assertEqual(
                [target["beforeValue"] for target in receipt["expectedEditTargets"]],
                [
                    "Student 001-01",
                    "LargeRoster-001-R050-C072",
                    "LargeRoster-001-R100-C150",
                ],
            )
            connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
            try:
                rows = connection.execute(
                    "SELECT row_index,col_index,value FROM roster_data "
                    "WHERE class_id=1 AND col_index IN (0,1) ORDER BY row_index,col_index"
                ).fetchall()
            finally:
                connection.close()
            self.assertEqual(len(rows), 200)
            for row in range(100):
                self.assertEqual(
                    (rows[row * 2][2], rows[row * 2 + 1][2]),
                    fixture.student_names(row),
                )

    def test_prepare_refuses_to_overwrite_existing_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory) / "preserve.tps"
            workspace.write_bytes(b"keep this workspace")
            with self.assertRaisesRegex(ValueError, "Refusing to overwrite"):
                fixture.prepare(BASE, SEED, workspace)
            self.assertEqual(workspace.read_bytes(), b"keep this workspace")

    def test_snapshot_requires_the_exact_three_cell_targets_and_integrity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "large-roster.tps"
            receipt_path = root / "preparation-receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace)
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
            before = fixture.snapshot("before", workspace, receipt_path)

            connection = sqlite3.connect(workspace)
            try:
                for target in receipt["expectedEditTargets"]:
                    connection.execute(
                        "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                        (
                            target["afterValue"],
                            target["classId"],
                            target["row"],
                            target["storedColumn"],
                        ),
                    )
                connection.commit()
            finally:
                connection.close()

            after = fixture.snapshot("after", workspace, receipt_path)
            self.assertEqual(after["integrityCheck"], "ok")
            self.assertEqual(before["schemaObjects"], after["schemaObjects"])
            self.assertEqual(
                before["workspaceTables"]["classes"], after["workspaceTables"]["classes"]
            )

    def test_snapshot_detects_unapproved_table_changes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "large-roster.tps"
            receipt_path = root / "preparation-receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace)
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
            fixture.snapshot("before", workspace, receipt_path)

            connection = sqlite3.connect(workspace)
            try:
                connection.execute("UPDATE classes SET name='Unexpected' WHERE id=2")
                connection.commit()
            finally:
                connection.close()

            after = fixture.snapshot("after", workspace, receipt_path)
            self.assertNotEqual(
                receipt["baselineTables"]["classes"]["rowsSha256"],
                after["workspaceTables"]["classes"]["rowsSha256"],
            )


if __name__ == "__main__":
    unittest.main()
