#!/usr/bin/env python3
"""Focused tests for the P9-11 source schedule preparation boundary."""

from __future__ import annotations

import importlib.util
import sqlite3
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("prepare_class_transfer_workspace.py")
SPEC = importlib.util.spec_from_file_location("prepare_class_transfer_workspace", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"could not load {MODULE_PATH}")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def _slot(index: int) -> tuple[str, str, str]:
    days = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday")
    day = days[index // 4]
    hour = 8 + (index % 4) * 2
    return day, f"{hour}:00 AM" if hour < 12 else f"{hour - 12}:00 PM", (
        f"{hour}:50 AM" if hour < 12 else f"{hour - 12}:50 PM"
    )


class PrepareClassTransferWorkspaceTests(unittest.TestCase):
    def _connection(self) -> sqlite3.Connection:
        connection = sqlite3.connect(":memory:")
        connection.execute(
            "CREATE TABLE class_times "
            "(id INTEGER PRIMARY KEY, class_id INTEGER, day TEXT, "
            "start_time TEXT, end_time TEXT)"
        )
        connection.execute(
            "CREATE TABLE class_intensive_times "
            "(id INTEGER PRIMARY KEY, class_id INTEGER, day TEXT, "
            "start_time TEXT, end_time TEXT)"
        )
        row_id = 1
        for class_id in range(1, 49):
            for meeting in range(8):
                if class_id in (1, 9):
                    slot = meeting + (8 if class_id == 9 else 0)
                    day, start, end = _slot(slot)
                else:
                    day, start, end = "Monday", "3:00 PM", "3:50 PM"
                connection.execute(
                    "INSERT INTO class_times VALUES (?, ?, ?, ?, ?)",
                    (row_id, class_id, day, start, end),
                )
                row_id += 1
        connection.execute(
            "INSERT INTO class_times VALUES (?, ?, ?, ?, ?)",
            (row_id, 49, "Sunday", "9:00 PM", "9:50 PM"),
        )
        for class_id in range(1, 25):
            day, start, end = _slot(class_id - 1)
            connection.execute(
                "INSERT INTO class_intensive_times VALUES (?, ?, ?, ?, ?)",
                (class_id, class_id, day, start, end),
            )
        connection.commit()
        return connection

    def _teacher_connection(self) -> sqlite3.Connection:
        connection = sqlite3.connect(":memory:")
        connection.execute(
            "CREATE TABLE teachers ("
            "id INTEGER PRIMARY KEY, teacher_kr TEXT, teacher_en TEXT, "
            "preferred_romanization TEXT, preferred_name TEXT, birthday TEXT, "
            "room_number TEXT, phone_number TEXT, notes TEXT)"
        )
        connection.execute(
            "CREATE TABLE class_info (class_id INTEGER PRIMARY KEY, teacher_id INTEGER)"
        )
        for teacher_id in range(1, 25):
            connection.execute(
                "INSERT INTO teachers VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (
                    teacher_id,
                    f"Teacher {teacher_id:02d} KR",
                    f"Teacher {teacher_id:02d}",
                    f"Teacher-{teacher_id:02d}",
                    f"Teacher {teacher_id:02d}",
                    f"1981-01-{teacher_id:02d}",
                    f"Room {teacher_id:02d}",
                    f"010-2000-{teacher_id:04d}",
                    f"Fixture teacher {teacher_id:02d}",
                ),
            )
            connection.execute(
                "INSERT INTO class_info VALUES (?, ?)", (teacher_id, teacher_id)
            )
        connection.commit()
        return connection

    def _class_detail_connection(self) -> sqlite3.Connection:
        connection = sqlite3.connect(":memory:")
        connection.execute(
            "CREATE TABLE class_info ("
            "class_id INTEGER PRIMARY KEY, teacher_id INTEGER, class_grade TEXT, "
            "class_level TEXT, reading_book TEXT, essay_book TEXT, class_color TEXT, "
            "notes TEXT, time_filler INTEGER)"
        )
        grades = ("E4", "E5", "M1", "M2")
        for class_id in range(1, 97):
            grade = grades[(class_id - 1) % len(grades)]
            level_number = ((class_id - 1) % 8) + 1
            connection.execute(
                "INSERT INTO class_info VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (
                    class_id,
                    ((class_id - 1) % 24) + 1,
                    grade,
                    f"Level {level_number:02d}",
                    f"Heavy Reading {class_id:03d}",
                    f"Heavy Essay {class_id:03d}",
                    f"#{class_id:06X}",
                    f"Fixture class {class_id:02d}",
                    class_id % 2,
                ),
            )
        connection.commit()
        return connection

    def test_source_transform_retains_approved_values_and_removes_only_other_selected_regular_rows(self) -> None:
        connection = self._connection()
        try:
            retained_regular_before = connection.execute(
                "SELECT class_id, day, start_time, end_time FROM class_times "
                "WHERE class_id IN (1, 9) ORDER BY class_id, id"
            ).fetchall()
            outside_regular_before = connection.execute(
                "SELECT class_id, day, start_time, end_time FROM class_times "
                "WHERE class_id=49"
            ).fetchall()
            intensive_before = connection.execute(
                "SELECT class_id, day, start_time, end_time "
                "FROM class_intensive_times ORDER BY class_id, id"
            ).fetchall()

            result = MODULE.apply_source_schedule_transform(connection)

            self.assertEqual(result["removedRegularRows"], 368)
            self.assertEqual(result["retainedSelectedRegularRows"], 16)
            self.assertEqual(result["retainedSelectedIntensiveRows"], 24)
            self.assertEqual(result["regularIncomingConflictCount"], 0)
            self.assertEqual(result["intensiveIncomingConflictCount"], 0)
            self.assertFalse(result["retainedScheduleValuesInvented"])
            self.assertEqual(
                connection.execute(
                    "SELECT class_id, day, start_time, end_time FROM class_times "
                    "WHERE class_id IN (1, 9) ORDER BY class_id, id"
                ).fetchall(),
                retained_regular_before,
            )
            self.assertEqual(
                connection.execute(
                    "SELECT class_id, day, start_time, end_time FROM class_times "
                    "WHERE class_id=49"
                ).fetchall(),
                outside_regular_before,
            )
            self.assertEqual(
                connection.execute(
                    "SELECT class_id, day, start_time, end_time "
                    "FROM class_intensive_times ORDER BY class_id, id"
                ).fetchall(),
                intensive_before,
            )
        finally:
            connection.close()

    def test_source_transform_rejects_any_retained_overlap(self) -> None:
        connection = self._connection()
        try:
            connection.execute(
                "UPDATE class_times SET day='Monday', start_time='8:00 AM', "
                "end_time='8:50 AM' WHERE class_id=9 AND id=("
                "SELECT MIN(id) FROM class_times WHERE class_id=9)"
            )
            with self.assertRaisesRegex(RuntimeError, "still has schedule conflicts"):
                MODULE.apply_source_schedule_transform(connection)
        finally:
            connection.close()

    def test_source_teacher_profile_transform_changes_only_approved_columns(self) -> None:
        connection = self._teacher_connection()
        try:
            columns = [row[1] for row in connection.execute('PRAGMA table_info("teachers")')]
            before_rows = connection.execute(
                "SELECT * FROM teachers ORDER BY id"
            ).fetchall()
            relations_before = connection.execute(
                "SELECT * FROM class_info ORDER BY class_id"
            ).fetchall()

            result = MODULE.apply_source_teacher_profile_transform(connection)

            after_rows = connection.execute(
                "SELECT * FROM teachers ORDER BY id"
            ).fetchall()
            relations_after = connection.execute(
                "SELECT * FROM class_info ORDER BY class_id"
            ).fetchall()
            self.assertEqual(len(after_rows), 24)
            self.assertEqual(relations_after, relations_before)
            self.assertEqual(result["rowCount"], 24)
            self.assertEqual(
                result["modifiedColumns"],
                [
                    "teacher_kr",
                    "teacher_en",
                    "preferred_romanization",
                    "preferred_name",
                    "birthday",
                ],
            )
            self.assertTrue(result["unchangedColumnsPreserved"])
            self.assertEqual(len(result["rows"]), 24)
            self.assertEqual(
                [row["afterProfile"]["teacher_kr"] for row in result["rows"]],
                [f"선생님({chr(ord('A') + index)})" for index in range(24)],
            )
            self.assertEqual(
                [row["afterProfile"]["teacher_en"] for row in result["rows"]],
                [f"Teacher {chr(ord('A') + index)}" for index in range(24)],
            )
            self.assertEqual(
                [row["afterProfile"]["preferred_romanization"] for row in result["rows"]],
                [f"Teacher-{chr(ord('A') + index)}" for index in range(24)],
            )
            self.assertEqual(
                [row["afterProfile"]["birthday"] for row in result["rows"]],
                [f"01-{index:02d}" for index in range(1, 25)],
            )
            for row in result["rows"]:
                for digest in (
                    row["beforeRowSha256"],
                    row["afterRowSha256"],
                    row["unchangedColumnsSha256"],
                ):
                    self.assertEqual(len(digest), 64)
            self.assertEqual(result["rows"][0]["teacherId"], 1)
            self.assertEqual(
                result["rows"][0]["afterProfile"],
                {
                    "teacher_kr": "선생님(A)",
                    "teacher_en": "Teacher A",
                    "preferred_romanization": "Teacher-A",
                    "preferred_name": "Teacher A",
                    "birthday": "01-01",
                },
            )
            self.assertEqual(
                result["rows"][-1]["afterProfile"],
                {
                    "teacher_kr": "선생님(X)",
                    "teacher_en": "Teacher X",
                    "preferred_romanization": "Teacher-X",
                    "preferred_name": "Teacher X",
                    "birthday": "01-24",
                },
            )
            target_columns = set(result["modifiedColumns"])
            for before, after in zip(before_rows, after_rows, strict=True):
                before_map = dict(zip(columns, before, strict=True))
                after_map = dict(zip(columns, after, strict=True))
                self.assertEqual(before_map["id"], after_map["id"])
                self.assertEqual(
                    {key: value for key, value in before_map.items() if key not in target_columns},
                    {key: value for key, value in after_map.items() if key not in target_columns},
                )
            self.assertEqual(
                [row[0] for row in after_rows],
                list(range(1, 25)),
            )
            self.assertEqual(len(result["beforeFullRowsSha256"]), 64)
            self.assertEqual(len(result["afterFullRowsSha256"]), 64)
        finally:
            connection.close()

    def test_source_teacher_profile_transform_rejects_unexpected_teacher_ids(self) -> None:
        connection = self._teacher_connection()
        try:
            connection.execute("UPDATE teachers SET id=25 WHERE id=24")
            with self.assertRaisesRegex(RuntimeError, "IDs 1..24"):
                MODULE.apply_source_teacher_profile_transform(connection)
        finally:
            connection.close()

    def test_source_class_detail_transform_changes_only_approved_catalog_columns(self) -> None:
        connection = self._class_detail_connection()
        try:
            columns = [row[1] for row in connection.execute('PRAGMA table_info("class_info")')]
            before_rows = connection.execute("SELECT * FROM class_info ORDER BY class_id").fetchall()
            relations_before = connection.execute(
                "SELECT class_id, teacher_id FROM class_info ORDER BY class_id"
            ).fetchall()

            result = MODULE.apply_source_class_details_transform(connection)

            after_rows = connection.execute("SELECT * FROM class_info ORDER BY class_id").fetchall()
            relations_after = connection.execute(
                "SELECT class_id, teacher_id FROM class_info ORDER BY class_id"
            ).fetchall()
            self.assertEqual(result["table"], "class_info")
            self.assertEqual(result["selectedClassIds"], [1, 48])
            self.assertEqual(result["rowCount"], 48)
            self.assertEqual(result["modifiedColumns"], ["class_level", "reading_book", "essay_book"])
            self.assertTrue(result["classTeacherRelationsPreserved"])
            self.assertTrue(result["untouchedClassesPreserved"])
            self.assertEqual(relations_after, relations_before)
            self.assertEqual(len(result["rows"]), 48)
            target_columns = set(result["modifiedColumns"])
            column_indexes = {column: index for index, column in enumerate(columns)}
            for class_id, (before, after) in enumerate(zip(before_rows, after_rows, strict=True), start=1):
                before_map = dict(zip(columns, before, strict=True))
                after_map = dict(zip(columns, after, strict=True))
                if class_id <= 48:
                    grade = before_map["class_grade"]
                    level_number = ((class_id - 1) % 8) + 1
                    levels = MODULE.SOURCE_LEVELS_BY_GRADE[grade]
                    expected_level = levels[(level_number - 1) % len(levels)]
                    expected_essay = "" if grade in {"E4", "E5"} else "N/A"
                    self.assertEqual(after_map["class_level"], expected_level)
                    self.assertEqual(after_map["reading_book"], "")
                    self.assertEqual(after_map["essay_book"], expected_essay)
                    self.assertEqual(
                        {key: value for key, value in before_map.items() if key not in target_columns},
                        {key: value for key, value in after_map.items() if key not in target_columns},
                    )
                else:
                    self.assertEqual(after_map, before_map)
            self.assertEqual(result["rows"][0]["classId"], 1)
            self.assertEqual(result["rows"][-1]["classId"], 48)
            for row in result["rows"]:
                for digest in (row["beforeRowSha256"], row["afterRowSha256"], row["unchangedColumnsSha256"]):
                    self.assertEqual(len(digest), 64)
            for key in ("beforeFullRowsSha256", "afterFullRowsSha256", "unchangedColumnsSha256", "untouchedClassRowsSha256", "classTeacherRelationsSha256"):
                self.assertEqual(len(result[key]), 64)
            self.assertEqual(column_indexes["teacher_id"], 1)
        finally:
            connection.close()

    def test_source_class_detail_transform_rejects_unsupported_grade(self) -> None:
        connection = self._class_detail_connection()
        try:
            connection.execute("UPDATE class_info SET class_grade='E6' WHERE class_id=1")
            with self.assertRaisesRegex(RuntimeError, "unsupported catalog grade"):
                MODULE.apply_source_class_details_transform(connection)
        finally:
            connection.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
