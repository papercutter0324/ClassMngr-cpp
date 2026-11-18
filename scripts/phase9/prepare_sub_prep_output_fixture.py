#!/usr/bin/env python3
"""Prepare the existing 96-class fixture for the supported weekday package route."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any


TRANSFORM_ID = "large-sub-prep-output-weekday-fixture-v1"
WEEKDAYS = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")


class FixturePreparationError(ValueError):
    """Raised when the exact 96-class fixture cannot be prepared safely."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare_fixture(fixture_path: Path, expected_source_sha256: str) -> dict[str, Any]:
    path = fixture_path.resolve(strict=True)
    if len(expected_source_sha256) != 64 or any(ch not in "0123456789abcdef" for ch in expected_source_sha256):
        raise FixturePreparationError("expected source SHA-256 must be lowercase hexadecimal")
    before_sha256 = _sha256(path)
    if before_sha256 != expected_source_sha256:
        raise FixturePreparationError("fixture copy does not match the source-bound master SHA-256")

    connection = sqlite3.connect(path)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("BEGIN IMMEDIATE")
        try:
            for table, expected in (("classes", 96), ("teachers", 24), ("roster_data", 7200)):
                count = connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                if count != expected:
                    raise FixturePreparationError(f"{table} count is {count}, expected {expected}")
            connection.execute("DELETE FROM class_times")
            connection.execute(
                "UPDATE roster_columns SET name = CASE position "
                "WHEN 0 THEN 'English' WHEN 1 THEN 'Korean' ELSE 'Memo' END"
            )
            rows: list[tuple[int, str, str, str]] = []
            for class_id in range(1, 97):
                if class_id <= 30:
                    day = WEEKDAYS[(class_id - 1) % len(WEEKDAYS)]
                    slot = (class_id - 1) // len(WEEKDAYS)
                else:
                    day = "Saturday"
                    slot = 0
                hour = 4 + slot
                rows.append((
                    class_id,
                    day,
                    f"{hour}:00 PM",
                    f"{hour}:50 PM",
                ))
            connection.executemany(
                "INSERT INTO class_times (class_id, day, start_time, end_time) "
                "VALUES (?, ?, ?, ?)",
                rows,
            )
            connection.commit()
        except Exception:
            connection.rollback()
            raise

        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise FixturePreparationError(f"SQLite integrity check returned {integrity!r}")
        counts = {
            "classes": connection.execute("SELECT COUNT(*) FROM classes").fetchone()[0],
            "teachers": connection.execute("SELECT COUNT(*) FROM teachers").fetchone()[0],
            "rosterCells": connection.execute("SELECT COUNT(*) FROM roster_data").fetchone()[0],
            "classTimes": connection.execute("SELECT COUNT(*) FROM class_times").fetchone()[0],
        }
        day_counts = {
            str(day): int(count)
            for day, count in connection.execute(
                "SELECT day, COUNT(DISTINCT class_id) FROM class_times GROUP BY day"
            )
        }
        weekday_conflicts = connection.execute(
            "SELECT day, start_time, COUNT(*) FROM class_times "
            "WHERE day IN (?, ?, ?, ?, ?) GROUP BY day, start_time HAVING COUNT(*) > 1",
            WEEKDAYS,
        ).fetchall()
        if counts != {"classes": 96, "teachers": 24, "rosterCells": 7200, "classTimes": 96}:
            raise FixturePreparationError(f"prepared fixture shape changed unexpectedly: {counts}")
        expected_days = {day: 6 for day in WEEKDAYS} | {"Saturday": 66}
        if day_counts != expected_days or weekday_conflicts:
            raise FixturePreparationError(
                f"weekday output schedule is not conflict-free: days={day_counts}, conflicts={weekday_conflicts}"
            )
    except sqlite3.Error as error:
        raise FixturePreparationError(f"fixture database preparation failed: {error}") from error
    finally:
        connection.close()

    after_sha256 = _sha256(path)
    if after_sha256 == before_sha256:
        raise FixturePreparationError("fixture transform did not change the source database")
    return {
        "transformId": TRANSFORM_ID,
        "fixturePath": str(path),
        "sourceSha256": before_sha256,
        "preparedSha256": after_sha256,
        "classCount": counts["classes"],
        "teacherCount": counts["teachers"],
        "rosterCellCount": counts["rosterCells"],
        "classTimeCount": counts["classTimes"],
        "weekdayClassCount": sum(day_counts.get(day, 0) for day in WEEKDAYS),
        "saturdayClassCount": day_counts.get("Saturday", 0),
        "dayCounts": day_counts,
        "selectedPackageDays": list(WEEKDAYS),
        "requestedPackageClassCount": 30,
        "weekdayTimeConflictCount": 0,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--expected-source-sha256", required=True)
    arguments = parser.parse_args()
    try:
        result = prepare_fixture(arguments.fixture, arguments.expected_source_sha256)
    except (FixturePreparationError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=True, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
