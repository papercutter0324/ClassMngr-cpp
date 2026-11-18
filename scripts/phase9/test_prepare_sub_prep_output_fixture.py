from __future__ import annotations

import hashlib
import sqlite3
import tempfile
import unittest
from pathlib import Path

from prepare_sub_prep_output_fixture import (
    FixturePreparationError,
    TRANSFORM_ID,
    WEEKDAYS,
    prepare_fixture,
)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _create_fixture(path: Path) -> None:
    connection = sqlite3.connect(path)
    try:
        connection.executescript(
            """
            CREATE TABLE classes (id INTEGER PRIMARY KEY);
            CREATE TABLE teachers (id INTEGER PRIMARY KEY);
            CREATE TABLE roster_data (class_id INTEGER, student_id INTEGER, value TEXT);
            CREATE TABLE roster_columns (position INTEGER PRIMARY KEY, name TEXT);
            CREATE TABLE class_times (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                class_id INTEGER NOT NULL,
                day TEXT NOT NULL,
                start_time TEXT NOT NULL,
                end_time TEXT NOT NULL
            );
            """
        )
        connection.executemany("INSERT INTO classes(id) VALUES (?)", ((value,) for value in range(1, 97)))
        connection.executemany("INSERT INTO teachers(id) VALUES (?)", ((value,) for value in range(1, 25)))
        connection.executemany(
            "INSERT INTO roster_data(class_id, student_id, value) VALUES (?, ?, ?)",
            ((class_id, student_id, "x") for class_id in range(1, 97) for student_id in range(1, 76)),
        )
        connection.executemany(
            "INSERT INTO roster_columns(position, name) VALUES (?, ?)",
            ((position, f"Column {position}") for position in range(288)),
        )
        connection.executemany(
            "INSERT INTO class_times(class_id, day, start_time, end_time) VALUES (?, ?, ?, ?)",
            ((class_id, "Friday", "5:00 PM", "5:50 PM") for class_id in range(1, 97)),
        )
        connection.commit()
    finally:
        connection.close()


class SubPrepOutputFixturePreparationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.fixture_path = Path(self.temporary_directory.name) / "fixture.tps"
        _create_fixture(self.fixture_path)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def test_transforms_full_fixture_into_conflict_free_weekday_route(self) -> None:
        source_sha256 = _sha256(self.fixture_path)
        result = prepare_fixture(self.fixture_path, source_sha256)

        self.assertEqual(result["transformId"], TRANSFORM_ID)
        self.assertEqual(result["sourceSha256"], source_sha256)
        self.assertEqual(result["preparedSha256"], _sha256(self.fixture_path))
        self.assertNotEqual(result["preparedSha256"], source_sha256)
        self.assertEqual(result["classCount"], 96)
        self.assertEqual(result["teacherCount"], 24)
        self.assertEqual(result["rosterCellCount"], 7200)
        self.assertEqual(result["classTimeCount"], 96)
        self.assertEqual(result["weekdayClassCount"], 30)
        self.assertEqual(result["saturdayClassCount"], 66)
        self.assertEqual(result["selectedPackageDays"], list(WEEKDAYS))
        self.assertEqual(result["requestedPackageClassCount"], 30)
        self.assertEqual(result["weekdayTimeConflictCount"], 0)
        self.assertEqual(
            result["dayCounts"],
            {**{day: 6 for day in WEEKDAYS}, "Saturday": 66},
        )
        connection = sqlite3.connect(self.fixture_path)
        try:
            self.assertEqual(connection.execute("PRAGMA integrity_check").fetchone()[0], "ok")
            self.assertEqual(
                connection.execute("SELECT name FROM roster_columns WHERE position=0").fetchone()[0],
                "English",
            )
            self.assertEqual(
                connection.execute("SELECT name FROM roster_columns WHERE position=1").fetchone()[0],
                "Korean",
            )
            self.assertEqual(
                connection.execute("SELECT name FROM roster_columns WHERE position=2").fetchone()[0],
                "Memo",
            )
        finally:
            connection.close()

    def test_rejects_wrong_source_hash_without_modifying_fixture(self) -> None:
        before = self.fixture_path.read_bytes()
        with self.assertRaisesRegex(FixturePreparationError, "does not match"):
            prepare_fixture(self.fixture_path, "0" * 64)
        self.assertEqual(self.fixture_path.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
