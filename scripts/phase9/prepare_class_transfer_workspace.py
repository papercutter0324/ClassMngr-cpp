#!/usr/bin/env python3
"""Create isolated, fully seeded P9-11 Class Transfer workspace copies."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sqlite3
from pathlib import Path
from typing import Any


CLASS_ID_OFFSET = 10_000
TEACHER_ID_OFFSET = 20_000
EVALUATION_ID_OFFSET = 30_000
LOCAL_PREFIX = "Destination Copy "
SOURCE_SELECTED_CLASS_MIN = 1
SOURCE_SELECTED_CLASS_MAX = 48
SOURCE_RETAINED_REGULAR_CLASS_IDS = (1, 9)
SOURCE_TEACHER_PROFILE_COLUMNS = (
    "teacher_kr",
    "teacher_en",
    "preferred_romanization",
    "preferred_name",
    "birthday",
)
SOURCE_TEACHER_KOREAN_BASE = "선생님"
SOURCE_TEACHER_ENGLISH_BASE = "Teacher"
SOURCE_CLASS_DETAIL_COLUMNS = ("class_level", "reading_book", "essay_book")
SOURCE_LEVELS_BY_GRADE = {
    "E4": ("Theseus", "Perseus", "Odysseus", "Hercules"),
    "E5": ("Artemis", "Hermes", "Apollo", "Zeus", "Athena"),
    "M1": ("Elephantus", "Galaxia", "Solis", "Major", "Song's"),
    "M2": ("Ursa", "Leo", "Tigris", "Major", "Song's"),
}
SOURCE_CLASS_DETAIL_RULES = {
    "catalogPath": "src/domain/config/class_details_catalog.h",
    "levelMapping": "parse Level NN; select levelsForGrade(grade)[(NN-1) modulo level count]",
    "readingBook": "blank string, first allowed catalog value for every supported grade-level pair",
    "essayBook": "blank string for E4/E5; N/A for M1/M2",
}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical_json(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def _json_sha256(value: Any) -> str:
    return _sha256(_canonical_json(value))


def _count(connection: sqlite3.Connection, table: str) -> int:
    escaped = table.replace('"', '""')
    return int(connection.execute(f'SELECT COUNT(*) FROM "{escaped}"').fetchone()[0])


def _column_tables(connection: sqlite3.Connection, column: str) -> list[str]:
    tables = [
        str(row[0])
        for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%' ORDER BY name"
        )
    ]
    matches: list[str] = []
    for table in tables:
        escaped = table.replace('"', '""')
        columns = {
            str(row[1])
            for row in connection.execute(f'PRAGMA table_info("{escaped}")')
        }
        if column in columns:
            matches.append(table)
    return matches


def _increment_column(
    connection: sqlite3.Connection,
    table: str,
    column: str,
    offset: int,
) -> int:
    escaped_table = table.replace('"', '""')
    escaped_column = column.replace('"', '""')
    cursor = connection.execute(
        f'UPDATE "{escaped_table}" SET "{escaped_column}" = '
        f'CAST("{escaped_column}" AS INTEGER) + ? '
        f'WHERE "{escaped_column}" IS NOT NULL',
        (offset,),
    )
    return int(cursor.rowcount)


def _set_sequence(connection: sqlite3.Connection, table: str) -> None:
    escaped = table.replace('"', '""')
    maximum = int(connection.execute(f'SELECT COALESCE(MAX(id), 0) FROM "{escaped}"').fetchone()[0])
    row = connection.execute(
        "SELECT seq FROM sqlite_sequence WHERE name = ?", (table,)
    ).fetchone()
    if row is None:
        connection.execute(
            "INSERT INTO sqlite_sequence(name, seq) VALUES (?, ?)",
            (table, maximum),
        )
    else:
        connection.execute(
            "UPDATE sqlite_sequence SET seq = MAX(seq, ?) WHERE name = ?",
            (maximum, table),
        )


def _minutes(value: str) -> int:
    pieces = value.strip().upper().split()
    if len(pieces) != 2 or pieces[1] not in {"AM", "PM"}:
        raise ValueError(f"unsupported schedule time {value!r}")
    hour_text, minute_text = pieces[0].split(":", 1)
    hour = int(hour_text) % 12
    if pieces[1] == "PM":
        hour += 12
    return hour * 60 + int(minute_text)


def _schedule_conflicts(
    connection: sqlite3.Connection,
    table: str,
    class_ids: tuple[int, ...],
) -> list[dict[str, Any]]:
    day_index = {
        name: index
        for index, name in enumerate(
            ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday")
        )
    }
    escaped = table.replace('"', '""')
    placeholders = ",".join("?" for _ in class_ids)
    rows = connection.execute(
        f'SELECT class_id, day, start_time, end_time FROM "{escaped}" '
        f"WHERE class_id IN ({placeholders}) ORDER BY class_id, day, start_time, end_time",
        class_ids,
    ).fetchall()
    intervals: list[tuple[int, int, int, str, str, str]] = []
    for class_id, day, start, end in rows:
        if day not in day_index:
            raise ValueError(f"unsupported schedule day {day!r} in {table}")
        start_minute = day_index[day] * 1440 + _minutes(str(start))
        end_minute = day_index[day] * 1440 + _minutes(str(end))
        if end_minute <= start_minute:
            raise ValueError(f"non-positive schedule interval in {table}: {start!r}–{end!r}")
        intervals.append(
            (start_minute, end_minute, int(class_id), str(day), str(start), str(end))
        )
    conflicts: list[dict[str, Any]] = []
    week = 7 * 1440
    for index, first in enumerate(intervals):
        for second in intervals[index + 1 :]:
            overlaps = any(
                first[0] < second[1] + offset and second[0] + offset < first[1]
                for offset in (-week, 0, week)
            )
            if overlaps:
                conflicts.append(
                    {
                        "first": {
                            "classId": first[2],
                            "day": first[3],
                            "startTime": first[4],
                            "endTime": first[5],
                        },
                        "second": {
                            "classId": second[2],
                            "day": second[3],
                            "startTime": second[4],
                            "endTime": second[5],
                        },
                    }
                )
    return conflicts


def apply_source_schedule_transform(
    connection: sqlite3.Connection,
) -> dict[str, Any]:
    """Retain the approved conflict-free incoming schedule subset verbatim."""
    retained_ids = SOURCE_RETAINED_REGULAR_CLASS_IDS
    placeholders = ",".join("?" for _ in retained_ids)
    cursor = connection.execute(
        "DELETE FROM class_times WHERE class_id BETWEEN ? AND ? "
        f"AND class_id NOT IN ({placeholders})",
        (
            SOURCE_SELECTED_CLASS_MIN,
            SOURCE_SELECTED_CLASS_MAX,
            *retained_ids,
        ),
    )
    removed_regular_rows = int(cursor.rowcount)
    selected_regular_rows = int(
        connection.execute(
            "SELECT COUNT(*) FROM class_times WHERE class_id BETWEEN ? AND ?",
            (SOURCE_SELECTED_CLASS_MIN, SOURCE_SELECTED_CLASS_MAX),
        ).fetchone()[0]
    )
    selected_intensive_rows = int(
        connection.execute(
            "SELECT COUNT(*) FROM class_intensive_times "
            "WHERE class_id BETWEEN ? AND ?",
            (SOURCE_SELECTED_CLASS_MIN, SOURCE_SELECTED_CLASS_MAX),
        ).fetchone()[0]
    )
    regular_conflicts = _schedule_conflicts(
        connection,
        "class_times",
        tuple(range(SOURCE_SELECTED_CLASS_MIN, SOURCE_SELECTED_CLASS_MAX + 1)),
    )
    intensive_conflicts = _schedule_conflicts(
        connection,
        "class_intensive_times",
        tuple(range(SOURCE_SELECTED_CLASS_MIN, SOURCE_SELECTED_CLASS_MAX + 1)),
    )
    if (removed_regular_rows, selected_regular_rows, selected_intensive_rows) != (
        368,
        16,
        24,
    ):
        raise RuntimeError(
            "source schedule transform shape changed: expected "
            "368 removed regular, 16 retained selected regular, and "
            f"24 selected intensive rows; got {removed_regular_rows}, "
            f"{selected_regular_rows}, {selected_intensive_rows}"
        )
    if regular_conflicts or intensive_conflicts:
        raise RuntimeError(
            "selected source package still has schedule conflicts: "
            f"regular={len(regular_conflicts)}, intensive={len(intensive_conflicts)}"
        )
    return {
        "selectedClassIds": [SOURCE_SELECTED_CLASS_MIN, SOURCE_SELECTED_CLASS_MAX],
        "retainedRegularClassIds": list(retained_ids),
        "removedRegularRows": removed_regular_rows,
        "retainedSelectedRegularRows": selected_regular_rows,
        "retainedSelectedIntensiveRows": selected_intensive_rows,
        "regularIncomingConflictCount": len(regular_conflicts),
        "intensiveIncomingConflictCount": len(intensive_conflicts),
        "retainedScheduleValuesInvented": False,
    }


def _normalized_source_teacher_profile(teacher_id: int) -> dict[str, str]:
    if not 1 <= teacher_id <= 24:
        raise RuntimeError(f"source teacher ID {teacher_id} is outside the approved range 1..24")
    suffix = chr(ord("A") + teacher_id - 1)
    teacher_kr = f"{SOURCE_TEACHER_KOREAN_BASE}({suffix})"
    teacher_en = f"{SOURCE_TEACHER_ENGLISH_BASE} {suffix}"
    romanization = f"{SOURCE_TEACHER_ENGLISH_BASE}-{suffix}"
    birthday = f"01-{teacher_id:02d}"
    if not re.fullmatch(r"^[\s\uAC00-\uD7A3]+(?:\s*\([A-Za-z]\))?\s*$", teacher_kr):
        raise RuntimeError(f"normalized Korean source teacher name failed its validator shape: {teacher_kr}")
    if len(SOURCE_TEACHER_KOREAN_BASE) != 3:
        raise RuntimeError("normalized Korean source teacher base must have three Hangul syllables")
    if any(len(value) > 20 or not re.fullmatch(r"[A-Za-z .-]+", value) for value in (teacher_en, romanization)):
        raise RuntimeError("normalized English source teacher name failed its validator shape")
    if not re.fullmatch(r"\d{2}-\d{2}", birthday) or int(birthday[3:]) > 31:
        raise RuntimeError(f"normalized source teacher birthday failed its MM-dd shape: {birthday}")
    return {
        "teacher_kr": teacher_kr,
        "teacher_en": teacher_en,
        "preferred_romanization": romanization,
        "preferred_name": teacher_en,
        "birthday": birthday,
    }


def apply_source_teacher_profile_transform(
    connection: sqlite3.Connection,
) -> dict[str, Any]:
    """Use validator-compliant synthetic names in the prepared source clone."""
    columns = [
        str(row[1])
        for row in connection.execute('PRAGMA table_info("teachers")')
    ]
    missing = {"id", *SOURCE_TEACHER_PROFILE_COLUMNS} - set(columns)
    if missing:
        raise RuntimeError(f"source teacher table lacks required profile columns: {sorted(missing)}")
    selected_columns = ", ".join(f'"{column}"' for column in columns)
    before_rows = [
        dict(zip(columns, row, strict=True))
        for row in connection.execute(
            f'SELECT {selected_columns} FROM "teachers" ORDER BY "id"'
        )
    ]
    if [int(row["id"]) for row in before_rows] != list(range(1, 25)):
        raise RuntimeError("source teacher profile normalization requires fixture IDs 1..24")

    for row in before_rows:
        profile = _normalized_source_teacher_profile(int(row["id"]))
        connection.execute(
            "UPDATE teachers SET teacher_kr=?, teacher_en=?, "
            "preferred_romanization=?, preferred_name=?, birthday=? WHERE id=?",
            (
                profile["teacher_kr"],
                profile["teacher_en"],
                profile["preferred_romanization"],
                profile["preferred_name"],
                profile["birthday"],
                int(row["id"]),
            ),
        )

    after_rows = [
        dict(zip(columns, row, strict=True))
        for row in connection.execute(
            f'SELECT {selected_columns} FROM "teachers" ORDER BY "id"'
        )
    ]
    if len(after_rows) != len(before_rows):
        raise RuntimeError("source teacher profile normalization changed the teacher row count")
    changed_columns = set(SOURCE_TEACHER_PROFILE_COLUMNS)
    row_evidence: list[dict[str, Any]] = []
    unchanged_projection_rows: list[dict[str, Any]] = []
    before_profile_rows: list[dict[str, Any]] = []
    after_profile_rows: list[dict[str, Any]] = []
    for before, after in zip(before_rows, after_rows, strict=True):
        teacher_id = int(before["id"])
        expected_profile = _normalized_source_teacher_profile(teacher_id)
        if after != {**before, **expected_profile}:
            raise RuntimeError(f"source teacher {teacher_id} changed outside the approved profile values")
        before_profile = {column: before[column] for column in SOURCE_TEACHER_PROFILE_COLUMNS}
        after_profile = {column: after[column] for column in SOURCE_TEACHER_PROFILE_COLUMNS}
        unchanged_values = {
            column: before[column]
            for column in columns
            if column not in changed_columns
        }
        unchanged_projection_rows.append({"teacherId": teacher_id, "values": unchanged_values})
        before_profile_rows.append({"teacherId": teacher_id, "values": before_profile})
        after_profile_rows.append({"teacherId": teacher_id, "values": after_profile})
        row_evidence.append(
            {
                "teacherId": teacher_id,
                "beforeProfile": before_profile,
                "afterProfile": after_profile,
                "beforeRowSha256": _json_sha256(before),
                "afterRowSha256": _json_sha256(after),
                "unchangedColumnsSha256": _json_sha256(unchanged_values),
            }
        )

    return {
        "table": "teachers",
        "modifiedColumns": list(SOURCE_TEACHER_PROFILE_COLUMNS),
        "rowCount": len(row_evidence),
        "koreanNameRule": "선생님(A)..선생님(X); three Hangul syllables with the supported A-X suffix",
        "englishNameRule": "Teacher A..Teacher X; ASCII letters and spaces, maximum 20 characters",
        "preferredRomanizationRule": "Teacher-A..Teacher-X; ASCII letters and hyphens, maximum 20 characters",
        "preferredNameRule": "equals teacher_en and is therefore an allowed preferred-name choice",
        "birthdayRule": "synthetic MM-dd values 01-01..01-24; valid under TeacherValidator MM-dd parsing",
        "beforeFullRowsSha256": _json_sha256(before_rows),
        "afterFullRowsSha256": _json_sha256(after_rows),
        "beforeProfileRowsSha256": _json_sha256(before_profile_rows),
        "afterProfileRowsSha256": _json_sha256(after_profile_rows),
        "unchangedColumnsSha256": _json_sha256(unchanged_projection_rows),
        "unchangedColumnsPreserved": True,
        "rows": row_evidence,
    }


def _normalized_source_class_details(
    class_id: int, grade: str, original_level: str
) -> dict[str, str]:
    if not 1 <= class_id <= 48:
        raise RuntimeError(f"source class ID {class_id} is outside the approved range 1..48")
    levels = SOURCE_LEVELS_BY_GRADE.get(grade)
    if levels is None:
        raise RuntimeError(f"source class {class_id} has unsupported catalog grade {grade!r}")
    match = re.fullmatch(r"Level (?P<number>\d{2})", original_level)
    if match is None:
        raise RuntimeError(f"source class {class_id} level does not use the seeded Level NN value: {original_level!r}")
    level_number = int(match.group("number"))
    if level_number < 1:
        raise RuntimeError(f"source class {class_id} has invalid seeded level number {level_number}")
    level = levels[(level_number - 1) % len(levels)]
    essay_book = "" if grade in {"E4", "E5"} else "N/A"
    return {"class_level": level, "reading_book": "", "essay_book": essay_book}


def apply_source_class_details_transform(
    connection: sqlite3.Connection,
) -> dict[str, Any]:
    """Normalize the selected classes' catalog-bound profile fields only."""
    columns = [
        str(row[1])
        for row in connection.execute('PRAGMA table_info("class_info")')
    ]
    missing = {"class_id", "teacher_id", *SOURCE_CLASS_DETAIL_COLUMNS} - set(columns)
    if missing:
        raise RuntimeError(f"source class_info table lacks required columns: {sorted(missing)}")
    selected_columns = ", ".join(f'"{column}"' for column in columns)
    before_rows = [
        dict(zip(columns, row, strict=True))
        for row in connection.execute(
            f'SELECT {selected_columns} FROM "class_info" ORDER BY "class_id"'
        )
    ]
    before_by_id = {int(row["class_id"]): row for row in before_rows}
    if len(before_by_id) != len(before_rows) or set(before_by_id) != set(range(1, 97)):
        raise RuntimeError("source class details normalization requires fixture class IDs 1..96")

    for class_id in range(1, 49):
        before = before_by_id[class_id]
        normalized = _normalized_source_class_details(
            class_id,
            str(before["class_grade"]),
            str(before["class_level"]),
        )
        connection.execute(
            "UPDATE class_info SET class_level=?, reading_book=?, essay_book=? "
            "WHERE class_id=?",
            (
                normalized["class_level"],
                normalized["reading_book"],
                normalized["essay_book"],
                class_id,
            ),
        )

    after_rows = [
        dict(zip(columns, row, strict=True))
        for row in connection.execute(
            f'SELECT {selected_columns} FROM "class_info" ORDER BY "class_id"'
        )
    ]
    after_by_id = {int(row["class_id"]): row for row in after_rows}
    if len(after_by_id) != len(after_rows) or set(after_by_id) != set(before_by_id):
        raise RuntimeError("source class details normalization changed class IDs or row count")

    changed_columns = set(SOURCE_CLASS_DETAIL_COLUMNS)
    unchanged_columns = [column for column in columns if column not in changed_columns]
    row_evidence: list[dict[str, Any]] = []
    before_detail_rows: list[dict[str, Any]] = []
    after_detail_rows: list[dict[str, Any]] = []
    unchanged_projection_rows: list[dict[str, Any]] = []
    relation_rows_before: list[dict[str, Any]] = []
    relation_rows_after: list[dict[str, Any]] = []
    untouched_rows: list[dict[str, Any]] = []
    for class_id in range(1, 97):
        before = before_by_id[class_id]
        after = after_by_id[class_id]
        if class_id <= 48:
            expected = {**before, **_normalized_source_class_details(
                class_id,
                str(before["class_grade"]),
                str(before["class_level"]),
            )}
            if after != expected:
                raise RuntimeError(f"source class {class_id} changed outside the approved class-detail fields")
            before_profile = {column: before[column] for column in SOURCE_CLASS_DETAIL_COLUMNS}
            after_profile = {column: after[column] for column in SOURCE_CLASS_DETAIL_COLUMNS}
            unchanged_values = {column: before[column] for column in unchanged_columns}
            before_detail_rows.append({"classId": class_id, "values": before_profile})
            after_detail_rows.append({"classId": class_id, "values": after_profile})
            unchanged_projection_rows.append({"classId": class_id, "values": unchanged_values})
            row_evidence.append(
                {
                    "classId": class_id,
                    "beforeProfile": before_profile,
                    "afterProfile": after_profile,
                    "beforeRowSha256": _json_sha256(before),
                    "afterRowSha256": _json_sha256(after),
                    "unchangedColumnsSha256": _json_sha256(unchanged_values),
                }
            )
        elif after != before:
            raise RuntimeError(f"source class {class_id} outside the selected range was modified")
        relation_rows_before.append(
            {"classId": class_id, "teacherId": before["teacher_id"]}
        )
        relation_rows_after.append(
            {"classId": class_id, "teacherId": after["teacher_id"]}
        )
        if class_id > 48:
            untouched_rows.append(before)

    if relation_rows_before != relation_rows_after:
        raise RuntimeError("source class-to-teacher relationships changed during class detail normalization")
    return {
        "table": "class_info",
        "selectedClassIds": [1, 48],
        "modifiedColumns": list(SOURCE_CLASS_DETAIL_COLUMNS),
        "rowCount": len(row_evidence),
        "rules": SOURCE_CLASS_DETAIL_RULES,
        "catalogLevelOrderByGrade": {
            grade: list(levels) for grade, levels in SOURCE_LEVELS_BY_GRADE.items()
        },
        "beforeFullRowsSha256": _json_sha256(before_rows),
        "afterFullRowsSha256": _json_sha256(after_rows),
        "beforeProfileRowsSha256": _json_sha256(before_detail_rows),
        "afterProfileRowsSha256": _json_sha256(after_detail_rows),
        "unchangedColumnsSha256": _json_sha256(unchanged_projection_rows),
        "untouchedClassRowsSha256": _json_sha256(untouched_rows),
        "classTeacherRelationsSha256": _json_sha256(relation_rows_before),
        "classTeacherRelationsPreserved": True,
        "untouchedClassesPreserved": True,
        "rows": row_evidence,
    }


def prepare(
    blank_workspace: Path,
    output_workspace: Path,
    seed_sql: Path,
    role: str,
) -> dict[str, Any]:
    source = blank_workspace.resolve(strict=True)
    output = output_workspace.resolve(strict=False)
    seed = seed_sql.resolve(strict=True)
    if role not in {"source", "apply_destination", "cancel_destination", "destination"}:
        raise ValueError(f"unsupported workspace role: {role}")
    if output.exists():
        raise FileExistsError(f"refusing to overwrite workspace {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    blank_bytes = source.read_bytes()
    output.write_bytes(blank_bytes)

    transformations: dict[str, Any] = {
        "role": role,
        "sourceScheduleTransformation": None,
        "sourceTeacherProfileTransformation": None,
        "sourceClassDetailTransformation": None,
        "destinationScheduleTransformation": None,
        "classIdOffset": 0,
        "teacherIdOffset": 0,
        "evaluationIdOffset": 0,
        "classNamePrefix": "",
        "classGradePrefix": "",
        "classLevelPrefix": "",
        "teacherNamePrefix": "",
        "updatedClassIdTables": {},
        "updatedTeacherIdTables": {},
        "updatedEvaluationIdTables": {},
    }
    connection = sqlite3.connect(output, timeout=20)
    try:
        connection.execute("PRAGMA busy_timeout = 20000")
        connection.execute("PRAGMA foreign_keys = ON")
        connection.executescript(seed.read_text(encoding="utf-8"))
        seeded_counts = {
            name: _count(connection, name)
            for name in (
                "classes",
                "teachers",
                "class_info",
                "class_times",
                "class_intensive_times",
                "roster_columns",
                "roster_data",
                "speaking_evaluations",
                "speaking_eval_data",
                "calendar_events",
            )
        }
        expected_counts = {
            "classes": 96,
            "teachers": 24,
            "class_info": 96,
            "class_times": 768,
            "class_intensive_times": 24,
            "roster_columns": 288,
            "roster_data": 7200,
            "speaking_evaluations": 20,
            "speaking_eval_data": 600,
            "calendar_events": 180,
        }
        if seeded_counts != expected_counts:
            raise RuntimeError(
                "large-startup fixture shape changed: "
                f"expected {expected_counts}, got {seeded_counts}"
            )

        if role == "source":
            connection.execute("BEGIN IMMEDIATE")
            try:
                schedule_transformation = apply_source_schedule_transform(connection)
                teacher_profile_transformation = apply_source_teacher_profile_transform(
                    connection
                )
                class_detail_transformation = apply_source_class_details_transform(
                    connection
                )
                connection.commit()
            except Exception:
                connection.rollback()
                raise
            transformations["sourceScheduleTransformation"] = schedule_transformation
            transformations["sourceTeacherProfileTransformation"] = teacher_profile_transformation
            transformations["sourceClassDetailTransformation"] = class_detail_transformation

        if role in {"apply_destination", "destination"}:
            connection.execute("PRAGMA foreign_keys = OFF")
            connection.execute("BEGIN IMMEDIATE")

            for table in _column_tables(connection, "class_id"):
                transformations["updatedClassIdTables"][table] = _increment_column(
                    connection, table, "class_id", CLASS_ID_OFFSET
                )
            for table in _column_tables(connection, "teacher_id"):
                transformations["updatedTeacherIdTables"][table] = _increment_column(
                    connection, table, "teacher_id", TEACHER_ID_OFFSET
                )
            for table in _column_tables(connection, "evaluation_id"):
                transformations["updatedEvaluationIdTables"][table] = _increment_column(
                    connection, table, "evaluation_id", EVALUATION_ID_OFFSET
                )

            connection.execute(
                "UPDATE classes SET id = id + ?, name = ? || name",
                (CLASS_ID_OFFSET, LOCAL_PREFIX),
            )
            connection.execute(
                "UPDATE teachers SET id = id + ?, "
                "teacher_kr = 'Destination ' || teacher_kr, "
                "teacher_en = 'Destination ' || teacher_en, "
                "preferred_name = 'Destination ' || preferred_name, "
                "preferred_romanization = 'Destination ' || preferred_romanization",
                (TEACHER_ID_OFFSET,),
            )
            connection.execute(
                "UPDATE speaking_evaluations SET id = id + ?",
                (EVALUATION_ID_OFFSET,),
            )
            connection.execute(
                "UPDATE class_info SET class_grade = 'Destination ' || class_grade, "
                "class_level = 'Destination ' || class_level"
            )
            if role == "apply_destination":
                regular_removed = int(
                    connection.execute("DELETE FROM class_times").rowcount
                )
                intensive_removed = int(
                    connection.execute("DELETE FROM class_intensive_times").rowcount
                )
                transformations["destinationScheduleTransformation"] = {
                    "clearedTables": ["class_times", "class_intensive_times"],
                    "removedRows": {
                        "class_times": regular_removed,
                        "class_intensive_times": intensive_removed,
                    },
                }
            for table in ("classes", "teachers", "speaking_evaluations"):
                _set_sequence(connection, table)
            connection.commit()
            transformations.update(
                {
                    "classIdOffset": CLASS_ID_OFFSET,
                    "teacherIdOffset": TEACHER_ID_OFFSET,
                    "evaluationIdOffset": EVALUATION_ID_OFFSET,
                    "classNamePrefix": LOCAL_PREFIX,
                    "classGradePrefix": "Destination ",
                    "classLevelPrefix": "Destination ",
                    "teacherNamePrefix": "Destination ",
                }
            )

        connection.execute("PRAGMA foreign_keys = ON")
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        foreign_key_errors = [
            list(row) for row in connection.execute("PRAGMA foreign_key_check")
        ]
        if integrity != "ok" or foreign_key_errors:
            raise RuntimeError(
                "workspace integrity failed: "
                f"integrity={integrity}, foreignKeyErrors={foreign_key_errors[:10]}"
            )
        final_counts = {
            name: _count(connection, name) for name in expected_counts
        }
        required_final_counts = dict(expected_counts)
        if role == "source":
            required_final_counts["class_times"] = 400
        elif role == "apply_destination":
            required_final_counts["class_times"] = 0
            required_final_counts["class_intensive_times"] = 0
        if final_counts != required_final_counts:
            raise RuntimeError(
                "workspace table row counts changed during identity transformation: "
                f"expected {required_final_counts}, got {final_counts}"
            )
        if role in {"apply_destination", "destination"}:
            class_id_min, class_id_max = connection.execute(
                "SELECT MIN(id), MAX(id) FROM classes"
            ).fetchone()
            teacher_id_min, teacher_id_max = connection.execute(
                "SELECT MIN(id), MAX(id) FROM teachers"
            ).fetchone()
            if (class_id_min, class_id_max) != (
                CLASS_ID_OFFSET + 1,
                CLASS_ID_OFFSET + 96,
            ):
                raise RuntimeError("destination class primary keys were not fully remapped")
            if (teacher_id_min, teacher_id_max) != (
                TEACHER_ID_OFFSET + 1,
                TEACHER_ID_OFFSET + 24,
            ):
                raise RuntimeError("destination teacher primary keys were not fully remapped")
    finally:
        connection.close()

    data = output.read_bytes()
    transformation_bytes = json.dumps(
        transformations,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return {
        "schema": "classmngr-phase9-class-transfer-workspace-preparation-v1",
        "role": role,
        "workspacePath": str(output.resolve(strict=True)),
        "workspaceSizeBytes": len(data),
        "workspaceSha256": _sha256(data),
        "blankWorkspacePath": str(source),
        "blankWorkspaceSha256": _sha256(blank_bytes),
        "seedSqlPath": str(seed),
        "seedSqlSha256": _sha256(seed.read_bytes()),
        "integrityCheck": integrity,
        "foreignKeyErrors": foreign_key_errors,
        "seededCounts": seeded_counts,
        "finalCounts": final_counts,
        "transformations": transformations,
        "transformationSha256": _sha256(transformation_bytes),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--blank-workspace", required=True, type=Path)
    parser.add_argument("--output-workspace", required=True, type=Path)
    parser.add_argument("--seed-sql", required=True, type=Path)
    parser.add_argument(
        "--role",
        required=True,
        choices=("source", "apply_destination", "cancel_destination", "destination"),
    )
    options = parser.parse_args()
    print(
        json.dumps(
            prepare(
                options.blank_workspace,
                options.output_workspace,
                options.seed_sql,
                options.role,
            ),
            # Keep native Windows stdout ASCII-safe; the runner parses this
            # line through PowerShell's process-pipe code-page conversion.
            ensure_ascii=True,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
