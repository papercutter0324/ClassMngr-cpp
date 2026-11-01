#!/usr/bin/env python3
"""Build an independent SQL-derived C53 roster text oracle.

The script reads the prepared SQLite snapshot as data. It does not read either
PDF, invoke the application, normalize extracted text, or use PDF-to-PDF text
comparison.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path


WEEKDAYS = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")
SOURCE_FIXTURE = Path("tests/fixtures/workspaces/large_startup.sql")
SOURCE_HEADER = Path("src/shared/roster/roster_template_print_shared_data.inc")
SOURCE_RENDERER = Path("src/shared/roster/roster_template_renderer.cpp")
ROSTER_SOURCE_TABLES = (
    "classes",
    "class_info",
    "teachers",
    "class_times",
    "roster_columns",
    "roster_data",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def class_label(row: sqlite3.Row) -> str:
    """Mirror classLabel from the By Day roster renderer source."""
    label = " ".join(
        value.strip()
        for value in (row["class_grade"], row["class_level"])
        if value and value.strip()
    ) or row["class_name"].strip()
    return label


def teacher_room_label(row: sqlite3.Row) -> str:
    """Mirror teacherRoomLabel from the By Day roster renderer source."""
    teacher = (row["teacher_en"] or "").strip()
    if not teacher:
        teacher = (row["teacher_kr"] or "").strip()
    room = (row["room_number"] or "").strip()
    if not teacher:
        return f"({room})" if room else ""
    return f"{teacher} ({room})" if room else teacher


def verify_legacy_source_tables(
    database: Path, after_legacy_database: Path
) -> dict:
    before_connection = sqlite3.connect(database.as_uri() + "?mode=ro", uri=True)
    after_connection = sqlite3.connect(
        after_legacy_database.as_uri() + "?mode=ro", uri=True
    )
    try:
        for table in ROSTER_SOURCE_TABLES:
            before_cursor = before_connection.execute(f'SELECT * FROM "{table}"')
            after_cursor = after_connection.execute(f'SELECT * FROM "{table}"')
            before_columns = [column[0] for column in before_cursor.description]
            after_columns = [column[0] for column in after_cursor.description]
            if before_columns != after_columns:
                raise ValueError(
                    f"Legacy generation changed the {table} source schema"
                )
            if sorted(before_cursor.fetchall(), key=repr) != sorted(
                after_cursor.fetchall(), key=repr
            ):
                raise ValueError(
                    f"Legacy generation changed rows in roster source table {table}"
                )
    finally:
        before_connection.close()
        after_connection.close()
    return {
        "databasePath": after_legacy_database.as_posix(),
        "databaseSha256": sha256(after_legacy_database),
        "workspaceBytesChanged": sha256(database) != sha256(after_legacy_database),
        "rosterSourceTablesUnchanged": True,
        "tablesChecked": list(ROSTER_SOURCE_TABLES),
    }


def create_oracle(
    database: Path,
    after_legacy_database: Path,
    expected_database_sha256: str,
) -> dict:
    database = database.resolve()
    after_legacy_database = after_legacy_database.resolve()
    actual_database_sha256 = sha256(database)
    if actual_database_sha256.lower() != expected_database_sha256.lower():
        raise ValueError(
            "retained SQL snapshot hash differs from the pre-Legacy-generation hash"
        )
    legacy_after_audit = verify_legacy_source_tables(
        database, after_legacy_database
    )

    uri = database.as_uri() + "?mode=ro"
    connection = sqlite3.connect(uri, uri=True)
    connection.row_factory = sqlite3.Row
    try:
        columns = connection.execute(
            "SELECT class_id, name, position FROM roster_columns "
            "ORDER BY class_id, position"
        ).fetchall()
        if not columns:
            raise ValueError("prepared SQL snapshot contains no roster columns")
        by_class: dict[int, list[tuple[int, str]]] = {}
        for item in columns:
            by_class.setdefault(int(item["class_id"]), []).append(
                (int(item["position"]), str(item["name"]))
            )
        for class_id, entries in by_class.items():
            normalized = [name.strip() for _, name in sorted(entries)]
            if normalized != ["English", "Korean", "Memo"]:
                raise ValueError(
                    f"class {class_id} does not have the Legacy route's prepared columns"
                )

        schedule = connection.execute(
            "SELECT class_id, day, start_time, end_time FROM class_times "
            "ORDER BY CASE day WHEN 'Monday' THEN 0 WHEN 'Tuesday' THEN 1 "
            "WHEN 'Wednesday' THEN 2 WHEN 'Thursday' THEN 3 "
            "WHEN 'Friday' THEN 4 WHEN 'Saturday' THEN 5 ELSE 6 END, "
            "start_time, class_id"
        ).fetchall()
        weekday_schedule = [row for row in schedule if row["day"] in WEEKDAYS]
        saturday_schedule = [row for row in schedule if row["day"] == "Saturday"]
        if len(schedule) != 96 or len(weekday_schedule) != 30 or len(saturday_schedule) != 66:
            raise ValueError(
                "prepared schedule does not match the Legacy 30-weekday/66-Saturday route"
            )
        day_counts = {
            day: sum(1 for row in weekday_schedule if row["day"] == day)
            for day in WEEKDAYS
        }
        if any(count != 6 for count in day_counts.values()):
            raise ValueError("prepared route must have exactly six classes per weekday")

        expected_names: list[str] = []
        expected_header_records: list[dict[str, object]] = []
        selected_class_ids: list[int] = []
        for schedule_row in weekday_schedule:
            class_id = int(schedule_row["class_id"])
            selected_class_ids.append(class_id)
            info = connection.execute(
                "SELECT c.name AS class_name, ci.class_grade, ci.class_level, "
                "t.teacher_en, t.teacher_kr, t.room_number, t.zoom_id, "
                "t.zoom_password "
                "FROM classes c JOIN class_info ci ON ci.class_id = c.id "
                "LEFT JOIN teachers t ON t.id = ci.teacher_id "
                "WHERE c.id = ?",
                (class_id,),
            ).fetchone()
            if info is None:
                raise ValueError(f"missing SQL class/source fields for class {class_id}")

            expected_header_records.append({
                "day": str(schedule_row["day"]),
                "classId": class_id,
                "classLabel": class_label(info),
                "teacherRoomLabel": teacher_room_label(info),
            })

            roster_rows = connection.execute(
                "SELECT row_index, value FROM roster_data "
                "WHERE class_id = ? AND col_index = 0 ORDER BY row_index",
                (class_id,),
            ).fetchall()
            if len(roster_rows) != 25 or [int(row["row_index"]) for row in roster_rows] != list(range(25)):
                raise ValueError(f"class {class_id} does not have 25 ordered roster names")
            expected_names.extend(str(row["value"]).strip() for row in roster_rows)

        if len(expected_names) != 750 or len(set(expected_names)) != 750:
            raise ValueError("SQL-derived weekday roster oracle is not 750 unique names")
        if len(expected_header_records) != 30:
            raise ValueError("SQL-derived By Day header oracle is not 30 records")
        expected_header_text: list[dict[str, object]] = []
        for field in ("classLabel", "teacherRoomLabel"):
            counts: dict[str, int] = {}
            for record in expected_header_records:
                value = str(record[field])
                if value:
                    counts[value] = counts.get(value, 0) + 1
            expected_header_text.extend(
                {
                    "field": field,
                    "value": value,
                    "expectedOccurrences": count,
                }
                for value, count in sorted(counts.items())
            )

        source_fixture = SOURCE_FIXTURE.resolve()
        source_header = SOURCE_HEADER.resolve()
        source_renderer = SOURCE_RENDERER.resolve()
        return {
            "schema": "classmngr-sub-prep-c53-sql-oracle-v2",
            "databasePath": database.as_posix(),
            "databaseSha256BeforeLegacyGeneration": actual_database_sha256,
            "legacyWorkspaceAfterGeneration": legacy_after_audit,
            "sourceFixture": SOURCE_FIXTURE.as_posix(),
            "sourceFixtureSha256": sha256(source_fixture),
            "headerSource": SOURCE_HEADER.as_posix(),
            "headerSourceSha256": sha256(source_header),
            "rendererSource": SOURCE_RENDERER.as_posix(),
            "rendererSourceSha256": sha256(source_renderer),
            "selection": {
                "teacherFolderName": "Teacher 01",
                "startDate": "2026-09-14",
                "endDate": "2026-09-18",
                "weekdays": list(WEEKDAYS),
                "locale": "en_US",
                "rosterTemplate": "By Day",
                "mode": "Regular",
                "selectedClassIds": sorted(selected_class_ids),
            },
            "fixtureSchedule": {
                "classCount": 96,
                "weekdayClassCount": len(weekday_schedule),
                "saturdayClassCount": len(saturday_schedule),
                "weekdayCounts": day_counts,
                "rosterColumns": ["English", "Korean", "Memo"],
            },
            "oracleMethod": {
                "studentNames": "roster_data.value where col_index=0, ordered by row_index, from the retained read-only SQLite snapshot",
                "headerStrings": "classes/class_info/teachers/class_times SQL fields assembled using classLabel and teacherRoomLabel in headerSource for the By Day rows built by rendererSource",
                "pdfInput": False,
                "pdfTextNormalization": False,
                "pdfToPdfComparison": False,
            },
            "expectedStudentCount": len(expected_names),
            "expectedHeaderRecordCount": len(expected_header_records),
            "expectedHeaderText": expected_header_text,
            "expectedHeaderRecords": expected_header_records,
            "expectedBulletCount": 0,
            "expectedStudentNames": expected_names,
        }
    finally:
        connection.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--after-legacy-database", required=True, type=Path)
    parser.add_argument("--expected-database-sha256", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    try:
        oracle = create_oracle(
            args.database,
            args.after_legacy_database,
            args.expected_database_sha256,
        )
    except (OSError, sqlite3.Error, ValueError) as error:
        parser.error(str(error))
        return 2

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(oracle, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
