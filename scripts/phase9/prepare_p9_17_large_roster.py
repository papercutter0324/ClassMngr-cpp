#!/usr/bin/env python3
"""Prepare and inspect the P9-17 100-row by 156-column roster workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sqlite3
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-17-large-roster-fixture-v1"
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
CLASS_COUNT = 96
CLASS_INFO_COUNT = 96
SCHEDULE_ENTRY_COUNT = 768
TARGET_CLASS_ID = 1
TARGET_CLASS_NAME = "Heavy Fixture Class 001"
DISPLAYED_BASE_COLUMNS = 6
CUSTOM_STRESS_COLUMN_COUNT = 150
STORED_ROSTER_COLUMNS = DISPLAYED_BASE_COLUMNS + CUSTOM_STRESS_COLUMN_COUNT
ROSTER_ROW_COUNT = 100
BASE_ROSTER_COLUMNS = (
    ("English", 170),
    ("Korean", 120),
    ("Winter", 130),
    ("Speech Contest", 130),
    ("Summer", 130),
    ("Fall", 130),
)
CUSTOM_ROSTER_COLUMN_OFFSET = len(BASE_ROSTER_COLUMNS)
PAYLOAD_PROBE_COORDINATES = ((0, 6), (49, 77), (99, 155))
CUSTOM_CELL_RECORD_COUNT = ROSTER_ROW_COUNT * CUSTOM_STRESS_COLUMN_COUNT
SPARSE_PAYLOAD_SENTINEL = "x"
SPARSE_PAYLOAD_SENTINEL_COUNT = CUSTOM_CELL_RECORD_COUNT - len(PAYLOAD_PROBE_COORDINATES)
ENGLISH_FIRST_NAMES = (
    "Alex", "Avery", "Bailey", "Blake", "Casey", "Devon", "Elliot", "Harper", "Jordan", "Morgan"
)
ENGLISH_LAST_NAMES = (
    "Archer", "Baker", "Carter", "Davis", "Ellis", "Foster", "Garcia", "Hayes", "Ives", "Jones"
)
KOREAN_SURNAMES = ("김", "이", "박", "최", "정", "강", "조", "윤", "장", "임")
KOREAN_GIVEN_NAMES = ("민지", "서연", "지우", "하은", "수빈", "예린", "유진", "은서", "지민", "채원")
CLASSES_NAVIGATION_DEFAULTS = (
    ("classes_navigation_visibility_scope", "active_schedule"),
    ("classes_navigation_day_filter_reset_policy", "on_application_close"),
    ("classes_navigation_class_selection_reset_policy", "on_application_close"),
)
TARGET_VALUES = {
    "start": {
        "row": 0,
        "modelColumn": 6,
        "storedColumn": 6,
        "value": "P917_START_R001_C007",
    },
    "middle": {
        "row": 49,
        "modelColumn": 77,
        "storedColumn": 77,
        "value": "P917_MIDDLE_R050_C078",
    },
    "end": {
        "row": 99,
        "modelColumn": 155,
        "storedColumn": 155,
        "value": "P917_END_R100_C156",
    },
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_sha256(value: Any) -> str:
    raw = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def json_value(value: Any) -> Any:
    if isinstance(value, bytes):
        return {"blobHex": value.hex()}
    if isinstance(value, tuple):
        return [json_value(item) for item in value]
    if isinstance(value, list):
        return [json_value(item) for item in value]
    if isinstance(value, dict):
        return {str(key): json_value(item) for key, item in value.items()}
    return value


def table_names(connection: sqlite3.Connection) -> list[str]:
    return [
        str(row[0])
        for row in connection.execute(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
        )
    ]


def schema_objects(connection: sqlite3.Connection) -> list[list[Any]]:
    rows = connection.execute(
        "SELECT type,name,tbl_name,sql FROM sqlite_master "
        "WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name"
    )
    return [json_value(list(row)) for row in rows]


def primary_key_order(connection: sqlite3.Connection, table: str, columns: list[str]) -> str:
    escaped = table.replace('"', '""')
    details = list(connection.execute(f'PRAGMA table_info("{escaped}")'))
    primary = [
        str(row[1])
        for row in sorted(
            (row for row in details if int(row[5]) > 0), key=lambda row: int(row[5])
        )
    ]
    order = primary or columns
    return ", ".join('"' + column.replace('"', '""') + '"' for column in order)


def workspace_snapshot(connection: sqlite3.Connection) -> dict[str, Any]:
    tables: dict[str, Any] = {}
    for table in table_names(connection):
        escaped = table.replace('"', '""')
        details = list(connection.execute(f'PRAGMA table_info("{escaped}")'))
        columns = [str(row[1]) for row in details]
        declaration = [
            {
                "cid": int(row[0]),
                "name": str(row[1]),
                "type": str(row[2]),
                "notNull": int(row[3]),
                "default": json_value(row[4]),
                "primaryKeyOrder": int(row[5]),
            }
            for row in details
        ]
        order = primary_key_order(connection, table, columns)
        values = [
            json_value(list(row))
            for row in connection.execute(f'SELECT * FROM "{escaped}" ORDER BY {order}')
        ]
        tables[table] = {
            "columns": columns,
            "declaration": declaration,
            "rowCount": len(values),
            "rows": values,
            "rowsSha256": canonical_sha256({"columns": columns, "rows": values}),
        }
    schema = schema_objects(connection)
    return {
        "schemaObjects": schema,
        "schemaObjectsSha256": canonical_sha256(schema),
        "tables": tables,
        "tableCount": len(tables),
        "tablesSha256": canonical_sha256(tables),
    }


def remove_sidecars(workspace: Path, phase: str) -> None:
    for suffix in ("-wal", "-shm", "-journal"):
        sidecar = Path(str(workspace) + suffix)
        if sidecar.exists():
            raise ValueError(f"Workspace SQLite sidecar remains during {phase} inspection: {suffix}")


def roster_value(row: int, column: int) -> str:
    if column == 0:
        return f"Student {TARGET_CLASS_ID:03d}-{row + 1:02d}"
    if column == 1:
        return f"School {1 + (TARGET_CLASS_ID % 12):02d}"
    if column == 2:
        return f"Heavy roster note {TARGET_CLASS_ID:03d}-{row + 1:02d}"
    return f"LargeRoster-{TARGET_CLASS_ID:03d}-R{row + 1:03d}-C{column + 1:03d}"


def custom_column_name(column: int) -> str:
    if column == 0:
        return "Student"
    if column == 1:
        return "School"
    if column == 2:
        return "Notes"
    return f"Stress Field {column + 1:03d}"


def student_names(row: int) -> tuple[str, str]:
    first_name = ENGLISH_FIRST_NAMES[row % len(ENGLISH_FIRST_NAMES)]
    last_name = ENGLISH_LAST_NAMES[row // len(ENGLISH_FIRST_NAMES)]
    korean_surname = KOREAN_SURNAMES[row // len(KOREAN_GIVEN_NAMES)]
    korean_given_name = KOREAN_GIVEN_NAMES[row % len(KOREAN_GIVEN_NAMES)]
    return f"{first_name} {last_name}", f"{korean_surname}{korean_given_name}"


_WEEKDAY_RANK = {
    "Monday": 0,
    "Tuesday": 1,
    "Wednesday": 2,
    "Thursday": 3,
    "Friday": 4,
    "Saturday": 5,
    "Sunday": 6,
}
_WEEKDAY_CODE = {
    "Monday": "M",
    "Tuesday": "T",
    "Wednesday": "W",
    "Thursday": "Th",
    "Friday": "F",
    "Saturday": "Sat",
    "Sunday": "Sun",
}


def compact_start_time(value: str) -> str:
    text = value.strip()
    match = re.fullmatch(r"(\d{1,2}):(\d{2})(?:\s*(AM|PM))?", text, re.IGNORECASE)
    if match is None:
        return text.removesuffix(" AM").removesuffix(" PM").strip()
    hour, minute = int(match.group(1)), match.group(2)
    meridiem = match.group(3)
    if meridiem:
        hour = hour % 12 or 12
    return f"{hour}:{minute}"


def compressed_days(days: list[str]) -> str:
    ordered = sorted(dict.fromkeys(days), key=lambda day: _WEEKDAY_RANK.get(day, 99))
    codes = [_WEEKDAY_CODE.get(day, day) for day in ordered if day]
    if codes in (["M", "W"], ["M", "F"], ["M", "W", "F"], ["T", "Th"]):
        return {"M/W": "M/W", "M/F": "M/F", "M/W/F": "M/W/F", "T/Th": "T/Th"}["/".join(codes)]
    return "/".join(codes)


def preferred_schedule_text(connection: sqlite3.Connection, class_id: int) -> str:
    regular = list(
        connection.execute(
            "SELECT day,start_time FROM class_times WHERE class_id=? ORDER BY id",
            (class_id,),
        )
    )
    intensive = not regular
    rows = regular or list(
        connection.execute(
            "SELECT day,start_time FROM class_intensive_times WHERE class_id=? ORDER BY id",
            (class_id,),
        )
    )
    if not rows:
        return "No time"
    groups: dict[str, list[str]] = {}
    for day, start_time in rows:
        compact = compact_start_time(str(start_time))
        if compact:
            groups.setdefault(compact, []).append(str(day))
    labels = [f"{compressed_days(days)} {start}" for start, days in groups.items()]
    schedule = "; ".join(labels) if labels else "No time"
    return f"Int {schedule}" if intensive else schedule


def target_class_list_accessible_name(connection: sqlite3.Connection) -> str:
    """Derive the exact Classes ListItem.Name used by the accepted Next UI."""
    candidates: list[dict[str, Any]] = []
    classes = connection.execute(
        "SELECT c.id,c.name,ci.class_grade,ci.class_level,t.teacher_en,t.teacher_kr "
        "FROM classes c LEFT JOIN class_info ci ON ci.class_id=c.id "
        "LEFT JOIN teachers t ON t.id=ci.teacher_id ORDER BY c.id"
    )
    for class_id, class_name, grade, level, teacher_english, teacher_korean in classes:
        details_available = grade is not None or level is not None or teacher_english is not None or teacher_korean is not None
        if grade and level:
            name = f"{grade} {level}"
        elif level:
            name = str(level)
        elif grade:
            name = str(grade)
        else:
            name = str(class_name or f"Class {class_id}")
        base = f"{name} • {preferred_schedule_text(connection, int(class_id))}" if details_available else name
        teacher = str(teacher_english or teacher_korean or "")
        candidates.append({"id": int(class_id), "base": base, "teacher": teacher})

    target = next((item for item in candidates if item["id"] == TARGET_CLASS_ID), None)
    if target is None:
        raise ValueError("Could not derive the exact Classes ListItem.Name for target class 1.")
    base_count = sum(item["base"] == target["base"] for item in candidates)
    label = target["base"]
    if base_count > 1:
        expanded = f"{target['base']} • {target['teacher']}" if target["teacher"] else target["base"]
        teacher_duplicate = any(
            other["id"] != target["id"]
            and other["base"] == target["base"]
            and (f"{other['base']} • {other['teacher']}" if other["teacher"] else other["base"]) == expanded
            for other in candidates
        )
        label = f"{expanded} #{target['id']}" if not target["teacher"] or teacher_duplicate else expanded
    return label


def fixture_identity(
    connection: sqlite3.Connection,
    expected_custom_record_count: int = CUSTOM_CELL_RECORD_COUNT,
) -> dict[str, Any]:
    class_count = int(connection.execute("SELECT COUNT(*) FROM classes").fetchone()[0])
    class_info_count = int(connection.execute("SELECT COUNT(*) FROM class_info").fetchone()[0])
    schedule_count = int(connection.execute("SELECT COUNT(*) FROM class_times").fetchone()[0])
    target = connection.execute(
        "SELECT name FROM classes WHERE id=?", (TARGET_CLASS_ID,)
    ).fetchone()
    if (class_count, class_info_count, schedule_count) != (
        CLASS_COUNT,
        CLASS_INFO_COUNT,
        SCHEDULE_ENTRY_COUNT,
    ):
        raise ValueError(
            "Pinned large workspace identity differs: "
            f"classes={class_count}, classInfo={class_info_count}, schedules={schedule_count}."
        )
    if target is None or str(target[0]) != TARGET_CLASS_NAME:
        raise ValueError("Target class does not match the pinned Heavy Fixture Class 001 identity.")
    columns = list(
        connection.execute(
            "SELECT position,name,width FROM roster_columns "
            "WHERE class_id=? ORDER BY position", (TARGET_CLASS_ID,)
        )
    )
    cell_rows = int(
        connection.execute("SELECT COUNT(*) FROM roster_data WHERE class_id=?", (TARGET_CLASS_ID,)).fetchone()[0]
    )
    distinct_rows = int(
        connection.execute(
            "SELECT COUNT(DISTINCT row_index) FROM roster_data WHERE class_id=?",
            (TARGET_CLASS_ID,),
        ).fetchone()[0]
    )
    if len(columns) != STORED_ROSTER_COLUMNS or [int(row[0]) for row in columns] != list(range(STORED_ROSTER_COLUMNS)):
        raise ValueError("Target roster does not contain the exact 156 contiguous stored columns.")
    expected_target_rows = ROSTER_ROW_COUNT * 2 + expected_custom_record_count
    if cell_rows != expected_target_rows or distinct_rows != ROSTER_ROW_COUNT:
        raise ValueError(
            "Target roster record count differs from the declared custom-row fixture: "
            f"targetRows={cell_rows}, customRows={expected_custom_record_count}."
        )
    expected_columns = [
        (name, width) for name, width in BASE_ROSTER_COLUMNS
    ] + [
        (custom_column_name(column), 180 if column == 0 else 160 if column == 1 else 260 if column == 2 else 140)
        for column in range(CUSTOM_STRESS_COLUMN_COUNT)
    ]
    if [(str(row[1]), int(row[2])) for row in columns] != expected_columns:
        raise ValueError("Target roster's six fixed-base and 150 custom-column schema differs from the fixture contract.")
    english_names: set[str] = set()
    korean_names: set[str] = set()
    base_name_rows = connection.execute(
        "SELECT row_index,col_index,value FROM roster_data "
        "WHERE class_id=? AND col_index IN (0,1) ORDER BY row_index,col_index",
        (TARGET_CLASS_ID,),
    )
    observed_names: dict[tuple[int, int], str] = {}
    for row_index, column_index, value in base_name_rows:
        observed_names[(int(row_index), int(column_index))] = "" if value is None else str(value)
    if len(observed_names) != ROSTER_ROW_COUNT * 2:
        raise ValueError("Target roster does not contain exactly two required name cells for every row.")
    for row in range(ROSTER_ROW_COUNT):
        expected_english, expected_korean = student_names(row)
        if observed_names.get((row, 0)) != expected_english or observed_names.get((row, 1)) != expected_korean:
            raise ValueError(f"Target roster required names differ from the valid deterministic pair at row {row}.")
        english_names.add(expected_english)
        korean_names.add(expected_korean)
    if len(english_names) != ROSTER_ROW_COUNT or len(korean_names) != ROSTER_ROW_COUNT:
        raise ValueError("Target roster contains duplicate English or Korean fixture names.")
    evaluation_value_count = int(connection.execute(
        "SELECT COUNT(*) FROM roster_data WHERE class_id=? AND col_index BETWEEN 2 AND 5",
        (TARGET_CLASS_ID,),
    ).fetchone()[0])
    if evaluation_value_count != 0:
        raise ValueError("Target roster's four evaluation base fields must remain empty and unmodified.")
    return {
        "classCount": class_count,
        "classInfoCount": class_info_count,
        "scheduleEntryCount": schedule_count,
        "targetClassId": TARGET_CLASS_ID,
        "targetClassName": TARGET_CLASS_NAME,
        "targetClassListAccessibleName": target_class_list_accessible_name(connection),
        "targetRosterRowCount": distinct_rows,
        "targetStoredRosterColumnCount": len(columns),
        "targetFixedBaseRosterColumnCount": DISPLAYED_BASE_COLUMNS,
        "targetCustomRosterColumnCount": CUSTOM_STRESS_COLUMN_COUNT,
        "targetDisplayedRosterColumnCount": len(columns),
        "targetRosterCellCount": cell_rows,
        "targetEnglishNameCellCount": len(english_names),
        "targetKoreanNameCellCount": len(korean_names),
        "targetCustomRosterCellCount": expected_custom_record_count,
        "targetEvaluationRosterCellCount": evaluation_value_count,
        "targetRosterGridCellCapacity": ROSTER_ROW_COUNT * len(columns),
        "databaseRosterCellCount": int(connection.execute("SELECT COUNT(*) FROM roster_data").fetchone()[0]),
    }


def custom_payload_counts(connection: sqlite3.Connection) -> dict[str, int]:
    rows = list(connection.execute(
        "SELECT row_index,col_index,value FROM roster_data "
        "WHERE class_id=? AND col_index BETWEEN ? AND ? ORDER BY row_index,col_index",
        (TARGET_CLASS_ID, CUSTOM_ROSTER_COLUMN_OFFSET,
         CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT - 1),
    ))
    expected_keys = {
        (row, column)
        for row in range(ROSTER_ROW_COUNT)
        for column in range(CUSTOM_ROSTER_COLUMN_OFFSET,
                            CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT)
    }
    observed_keys = {(int(row), int(column)) for row, column, _ in rows}
    if len(rows) != CUSTOM_CELL_RECORD_COUNT or observed_keys != expected_keys:
        raise ValueError("Custom roster records differ from the exact 100x150 in-place payload contract.")
    blank_count = sum(value == "" for _, _, value in rows)
    nonblank_count = sum(value is not None and value != "" for _, _, value in rows)
    compact_sentinel_count = sum(value == SPARSE_PAYLOAD_SENTINEL for _, _, value in rows)
    return {
        "customCellRecordCount": len(rows),
        "customCellValueCount": nonblank_count,
        "blankCustomCellValueCount": blank_count,
        "compactSentinelValueCount": compact_sentinel_count,
    }


def validate_payload_values(
    connection: sqlite3.Connection,
    payload_mode: str,
    expected_probe_values: dict[tuple[int, int], str],
    phase: str,
) -> None:
    rows = connection.execute(
        "SELECT row_index,col_index,value FROM roster_data "
        "WHERE class_id=? AND col_index BETWEEN ? AND ? ORDER BY row_index,col_index",
        (TARGET_CLASS_ID, CUSTOM_ROSTER_COLUMN_OFFSET,
         CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT - 1),
    )
    for row_index, column_index, value in rows:
        coordinate = (int(row_index), int(column_index))
        if coordinate in expected_probe_values:
            expected = expected_probe_values[coordinate]
        elif payload_mode == "sparse":
            expected = SPARSE_PAYLOAD_SENTINEL
        else:
            expected = roster_value(coordinate[0], coordinate[1] - CUSTOM_ROSTER_COLUMN_OFFSET)
        if value is None or str(value) != expected:
            raise ValueError(f"{phase.capitalize()} snapshot custom payload differs from its declared value at {coordinate!r}.")


def payload_diagnostic_record(
    connection: sqlite3.Connection,
    payload_mode: str,
    base_workspace: Path,
    seed_sql: Path,
    preparer_path: Path,
) -> dict[str, Any]:
    if payload_mode not in ("filled", "sparse"):
        raise ValueError(f"Unsupported payload mode: {payload_mode}")
    counts = custom_payload_counts(connection)
    expected_sentinel_count = SPARSE_PAYLOAD_SENTINEL_COUNT if payload_mode == "sparse" else 0
    if (
        counts["customCellRecordCount"] != CUSTOM_CELL_RECORD_COUNT
        or counts["blankCustomCellValueCount"] != 0
        or counts["customCellValueCount"] != CUSTOM_CELL_RECORD_COUNT
        or counts["compactSentinelValueCount"] != expected_sentinel_count
    ):
        raise ValueError(f"{payload_mode} payload does not have the exact custom-value and compact-sentinel counts.")
    probes = []
    for name, target in TARGET_VALUES.items():
        coordinate = (int(target["row"]), int(target["storedColumn"]))
        if coordinate not in PAYLOAD_PROBE_COORDINATES:
            continue
        result = connection.execute(
            "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
            (TARGET_CLASS_ID, *coordinate),
        ).fetchone()
        if result is None or result[0] is None or str(result[0]) == "":
            raise ValueError(f"Payload probe cell is blank or missing at {coordinate!r}.")
        probes.append({
            "name": name,
            "row": coordinate[0],
            "modelColumn": int(target["modelColumn"]),
            "storedColumn": coordinate[1],
            "beforeValue": str(result[0]),
            "editValue": str(target["value"]),
        })
    if len(probes) != len(PAYLOAD_PROBE_COORDINATES):
        raise ValueError("Payload diagnostic does not identify the exact three preserved probe cells.")
    validate_payload_values(
        connection,
        payload_mode,
        {
            (int(probe["row"]), int(probe["storedColumn"])): str(probe["beforeValue"])
            for probe in probes
        },
        "prepared",
    )
    return {
        "schema": "classmngr-phase9-p9-17-payload-fixture-diagnostic-v1",
        "diagnosticOnly": True,
        "accepted": False,
        "payloadMode": payload_mode,
        **counts,
        "compactSentinelValue": SPARSE_PAYLOAD_SENTINEL if payload_mode == "sparse" else None,
        "preservedProbeCellCount": len(probes),
        "probeCells": probes,
        "fixtureSource": {
            "baseWorkspaceSha256": sha256_file(base_workspace),
            "seedSqlSha256": sha256_file(seed_sql),
            "preparerSha256": sha256_file(preparer_path),
        },
    }


RECORD_COUNT_FIXTURE_SCHEMA = "classmngr-phase9-p9-17-roster-record-count-fixture-v1"
RECORD_COUNT_DIAGNOSTIC_SCHEMA = "classmngr-phase9-p9-17-roster-record-count-diagnostic-v1"
REDUCED_CUSTOM_RECORD_COUNT = len(PAYLOAD_PROBE_COORDINATES)


def roster_record_count_values(mode: str) -> tuple[int, int]:
    if mode == "filled":
        return CUSTOM_CELL_RECORD_COUNT, 0
    if mode == "reduced-rows":
        return REDUCED_CUSTOM_RECORD_COUNT, CUSTOM_CELL_RECORD_COUNT - REDUCED_CUSTOM_RECORD_COUNT
    raise ValueError(f"Unsupported roster record-count mode: {mode}")


def validate_record_count_rows(
    connection: sqlite3.Connection,
    mode: str,
    expected_probe_values: dict[tuple[int, int], str],
    phase: str,
) -> dict[str, int]:
    expected_count, removed_count = roster_record_count_values(mode)
    rows = list(connection.execute(
        "SELECT row_index,col_index,value FROM roster_data "
        "WHERE class_id=? AND col_index BETWEEN ? AND ? ORDER BY row_index,col_index",
        (TARGET_CLASS_ID, CUSTOM_ROSTER_COLUMN_OFFSET,
         CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT - 1),
    ))
    coordinates = {(int(row), int(column)) for row, column, _ in rows}
    if mode == "filled":
        expected_coordinates = {
            (row, column)
            for row in range(ROSTER_ROW_COUNT)
            for column in range(CUSTOM_ROSTER_COLUMN_OFFSET,
                                CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT)
        }
    else:
        expected_coordinates = set(PAYLOAD_PROBE_COORDINATES)
    if len(rows) != expected_count or coordinates != expected_coordinates:
        raise ValueError(
            f"{phase.capitalize()} {mode} custom roster rows differ from the declared exact coordinate set."
        )
    if set(expected_probe_values) != set(PAYLOAD_PROBE_COORDINATES):
        raise ValueError("Record-count fixture must retain exactly the three declared probe coordinates.")
    for row_index, column_index, value in rows:
        coordinate = (int(row_index), int(column_index))
        if coordinate in expected_probe_values:
            expected = expected_probe_values[coordinate]
        else:
            expected = roster_value(coordinate[0], coordinate[1] - CUSTOM_ROSTER_COLUMN_OFFSET)
        if value is None or str(value) == "" or str(value) != expected:
            raise ValueError(
                f"{phase.capitalize()} {mode} custom roster value differs at {coordinate!r}."
            )
    return {
        "customCellRecordCount": len(rows),
        "customCellValueCount": len(rows),
        "blankCustomCellValueCount": 0,
        "removedCustomCellRecordCount": removed_count,
    }


def record_count_diagnostic_record(
    connection: sqlite3.Connection,
    mode: str,
    base_workspace: Path,
    seed_sql: Path,
    preparer_path: Path,
) -> dict[str, Any]:
    expected_count, removed_count = roster_record_count_values(mode)
    probe_cells = []
    expected_values: dict[tuple[int, int], str] = {}
    for name, target in TARGET_VALUES.items():
        coordinate = (int(target["row"]), int(target["storedColumn"]))
        if coordinate not in PAYLOAD_PROBE_COORDINATES:
            continue
        value = connection.execute(
            "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
            (TARGET_CLASS_ID, *coordinate),
        ).fetchone()
        if value is None or value[0] is None or str(value[0]) == "":
            raise ValueError(f"Record-count probe cell is blank or missing at {coordinate!r}.")
        before_value = str(value[0])
        expected_values[coordinate] = before_value
        probe_cells.append({
            "name": name,
            "row": coordinate[0],
            "modelColumn": int(target["modelColumn"]),
            "storedColumn": coordinate[1],
            "beforeValue": before_value,
            "editValue": str(target["value"]),
        })
    counts = validate_record_count_rows(connection, mode, expected_values, "prepared")
    if len(probe_cells) != REDUCED_CUSTOM_RECORD_COUNT:
        raise ValueError("Record-count fixture does not identify exactly three preserved probe cells.")
    return {
        "schema": RECORD_COUNT_FIXTURE_SCHEMA,
        "diagnosticOnly": True,
        "accepted": False,
        "recordCountMode": mode,
        **counts,
        "retainedCustomRecordCoordinates": [list(value) for value in sorted(PAYLOAD_PROBE_COORDINATES)],
        "preservedProbeCellCount": len(probe_cells),
        "probeCells": probe_cells,
        "fixtureSource": {
            "baseWorkspaceSha256": sha256_file(base_workspace),
            "seedSqlSha256": sha256_file(seed_sql),
            "preparerSha256": sha256_file(preparer_path),
        },
        "comparisonDeclaration": {
            "removedCustomCellRecordCount": removed_count,
            "removedCoordinates": "all class 1 custom roster coordinates except the three named probes",
            "allowedDifference": "only the declared custom roster_data rows at the 14997 non-probe coordinates",
        },
    }


def inspect_database(
    connection: sqlite3.Connection,
    phase: str,
    expected_custom_record_count: int = CUSTOM_CELL_RECORD_COUNT,
) -> dict[str, Any]:
    integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
    foreign_keys = [json_value(list(row)) for row in connection.execute("PRAGMA foreign_key_check")]
    if integrity != "ok" or foreign_keys:
        raise ValueError(f"{phase} integrity failed: {integrity}; foreign key errors={foreign_keys!r}")
    identity = fixture_identity(connection, expected_custom_record_count)
    snapshot = workspace_snapshot(connection)
    return {
        "schema": SCHEMA,
        "status": "passed",
        "phase": phase,
        "integrityCheck": integrity,
        "foreignKeyErrors": foreign_keys,
        "identity": identity,
        "schemaObjects": snapshot["schemaObjects"],
        "schemaObjectsSha256": snapshot["schemaObjectsSha256"],
        "workspaceTables": snapshot["tables"],
        "workspaceTableCount": snapshot["tableCount"],
        "workspaceTablesSha256": snapshot["tablesSha256"],
    }


def read_receipt(path: Path, workspace: Path) -> dict[str, Any]:
    receipt = json.loads(path.read_text(encoding="utf-8"))
    if receipt.get("schema") != SCHEMA or receipt.get("status") != "passed" or receipt.get("kind") != "large-roster-editing":
        raise ValueError("Preparation receipt does not identify the passed P9-17 large-roster fixture.")
    if Path(str(receipt.get("workspace", ""))).resolve() != workspace.resolve():
        raise ValueError("Requested workspace path differs from the preparation receipt.")
    record_mode = receipt.get("recordCountMode")
    expected_custom_count = (
        roster_record_count_values(str(record_mode))[0]
        if record_mode is not None
        else CUSTOM_CELL_RECORD_COUNT
    )
    if record_mode is not None and record_mode not in ("filled", "reduced-rows"):
        raise ValueError("Preparation receipt has an unsupported roster record-count mode.")
    if receipt.get("identity") != fixture_identity_from_receipt(receipt):
        raise ValueError("Preparation receipt identity is internally inconsistent.")
    if receipt["identity"].get("targetCustomRosterCellCount") != expected_custom_count:
        raise ValueError("Preparation receipt identity custom-row count differs from its declared record-count mode.")
    if "payloadMode" in receipt:
        payload = receipt.get("payloadDiagnostic")
        expected_sentinel_count = SPARSE_PAYLOAD_SENTINEL_COUNT if receipt.get("payloadMode") == "sparse" else 0
        expected_sentinel = SPARSE_PAYLOAD_SENTINEL if receipt.get("payloadMode") == "sparse" else None
        if (
            receipt.get("payloadMode") not in ("filled", "sparse")
            or not isinstance(payload, dict)
            or payload.get("payloadMode") != receipt.get("payloadMode")
            or payload.get("diagnosticOnly") is not True
            or payload.get("accepted") is not False
            or payload.get("customCellRecordCount") != CUSTOM_CELL_RECORD_COUNT
            or payload.get("customCellValueCount") != CUSTOM_CELL_RECORD_COUNT
            or payload.get("blankCustomCellValueCount") != 0
            or payload.get("compactSentinelValueCount") != expected_sentinel_count
            or payload.get("compactSentinelValue") != expected_sentinel
        ):
            raise ValueError("Payload diagnostic receipt has inconsistent mode or diagnostic-only classification.")
    if record_mode is not None:
        record = receipt.get("recordCountDiagnostic")
        expected_count, removed_count = roster_record_count_values(str(record_mode))
        if (
            not isinstance(record, dict)
            or record.get("schema") != RECORD_COUNT_FIXTURE_SCHEMA
            or record.get("recordCountMode") != record_mode
            or record.get("diagnosticOnly") is not True
            or record.get("accepted") is not False
            or record.get("customCellRecordCount") != expected_count
            or record.get("customCellValueCount") != expected_count
            or record.get("blankCustomCellValueCount") != 0
            or record.get("removedCustomCellRecordCount") != removed_count
            or record.get("preservedProbeCellCount") != REDUCED_CUSTOM_RECORD_COUNT
            or record.get("retainedCustomRecordCoordinates")
            != [list(value) for value in sorted(PAYLOAD_PROBE_COORDINATES)]
        ):
            raise ValueError("Record-count receipt has inconsistent mode, row counts, or diagnostic-only classification.")
    return receipt


def fixture_identity_from_receipt(receipt: dict[str, Any]) -> dict[str, Any]:
    expected = receipt.get("identity")
    if not isinstance(expected, dict):
        raise ValueError("Preparation receipt identity is missing.")
    required = {
        "classCount", "classInfoCount", "scheduleEntryCount", "targetClassId",
        "targetClassName", "targetClassListAccessibleName", "targetRosterRowCount", "targetStoredRosterColumnCount",
        "targetFixedBaseRosterColumnCount", "targetCustomRosterColumnCount",
        "targetDisplayedRosterColumnCount", "targetRosterCellCount", "targetEnglishNameCellCount",
        "targetKoreanNameCellCount", "targetCustomRosterCellCount", "targetEvaluationRosterCellCount",
        "targetRosterGridCellCapacity", "databaseRosterCellCount",
    }
    if set(expected) != required:
        raise ValueError("Preparation receipt identity has unexpected fields.")
    return expected


def read_only_connection(workspace: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
    connection.execute("PRAGMA query_only=ON")
    return connection


def prepare(
    base: Path,
    seed: Path,
    output: Path,
    payload_mode: str | None = None,
    record_count_mode: str | None = None,
) -> dict[str, Any]:
    if payload_mode is not None and record_count_mode is not None:
        raise ValueError("Payload and roster record-count diagnostic modes must be prepared separately.")
    if record_count_mode is not None:
        roster_record_count_values(record_count_mode)
    base = base.resolve(strict=True)
    seed = seed.resolve(strict=True)
    if sha256_file(base) != BASE_SHA256:
        raise ValueError("Master blank workspace differs from its pinned P9-16 fingerprint.")
    if sha256_file(seed) != SEED_SHA256:
        raise ValueError("large_startup.sql differs from its pinned fingerprint.")
    output = output.resolve()
    if output.exists():
        raise ValueError(f"Refusing to overwrite fixture workspace {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base.read_bytes())
    connection = sqlite3.connect(str(output))
    try:
        connection.execute("PRAGMA foreign_keys=ON")
        connection.execute("PRAGMA journal_mode=DELETE")
        connection.executescript(seed.read_text(encoding="utf-8"))
        connection.execute("BEGIN")
        existing_navigation_settings = {
            str(row[0])
            for row in connection.execute(
                "SELECT key FROM app_settings WHERE key IN (?,?,?)",
                tuple(key for key, _ in CLASSES_NAVIGATION_DEFAULTS),
            )
        }
        if existing_navigation_settings:
            raise ValueError(
                "Pinned startup seed unexpectedly contains P9-17 Classes navigation settings: "
                f"{sorted(existing_navigation_settings)!r}."
            )
        connection.executemany(
            "INSERT INTO app_settings (key,value) VALUES (?,?)",
            CLASSES_NAVIGATION_DEFAULTS,
        )
        current_name = connection.execute(
            "SELECT name FROM classes WHERE id=?", (TARGET_CLASS_ID,)
        ).fetchone()
        if current_name is None or str(current_name[0]) != TARGET_CLASS_NAME:
            raise ValueError("Seeded class 1 is not the exact target class.")
        original_columns = list(
            connection.execute(
                "SELECT position,name,width FROM roster_columns "
                "WHERE class_id=? ORDER BY position", (TARGET_CLASS_ID,)
            )
        )
        if [tuple(row) for row in original_columns] != [
            (0, "Student", 180), (1, "School", 160), (2, "Notes", 260)
        ]:
            raise ValueError("Seeded target roster columns differ from the pinned three-column shape.")
        original_cells = int(
            connection.execute(
                "SELECT COUNT(*) FROM roster_data WHERE class_id=?", (TARGET_CLASS_ID,)
            ).fetchone()[0]
        )
        if original_cells != 25 * 3:
            raise ValueError(f"Seeded target roster contains {original_cells} cells, expected 75.")
        for row in range(25):
            for column in range(3):
                observed = connection.execute(
                    "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
                    (TARGET_CLASS_ID, row, column),
                ).fetchone()
                if observed is None or str(observed[0]) != roster_value(row, column):
                    raise ValueError(f"Pinned source roster value changed at {row},{column}.")

        connection.execute(
            "UPDATE roster_columns SET position=position+? WHERE class_id=?",
            (DISPLAYED_BASE_COLUMNS, TARGET_CLASS_ID),
        )
        connection.execute(
            "UPDATE roster_data SET col_index=col_index+? WHERE class_id=?",
            (DISPLAYED_BASE_COLUMNS, TARGET_CLASS_ID),
        )
        connection.executemany(
            "INSERT INTO roster_columns (class_id,name,position,width) VALUES (?,?,?,?)",
            (
                (TARGET_CLASS_ID, name, position, width)
                for position, (name, width) in enumerate(BASE_ROSTER_COLUMNS)
            ),
        )

        connection.executemany(
            "INSERT INTO roster_columns (class_id,name,position,width) VALUES (?,?,?,?)",
            (
                (TARGET_CLASS_ID, custom_column_name(column), CUSTOM_ROSTER_COLUMN_OFFSET + column,
                 180 if column == 0 else 160 if column == 1 else 260 if column == 2 else 140)
                for column in range(3, CUSTOM_STRESS_COLUMN_COUNT)
            ),
        )
        connection.executemany(
            "INSERT INTO roster_data (class_id,row_index,col_index,value) VALUES (?,?,?,?)",
            (
                (TARGET_CLASS_ID, row, column, value)
                for row in range(ROSTER_ROW_COUNT)
                for column, value in enumerate(student_names(row))
            ),
        )
        connection.executemany(
            "INSERT INTO roster_data (class_id,row_index,col_index,value) VALUES (?,?,?,?)",
            (
                (TARGET_CLASS_ID, row, CUSTOM_ROSTER_COLUMN_OFFSET + column, roster_value(row, column))
                for row in range(ROSTER_ROW_COUNT)
                for column in range(CUSTOM_STRESS_COLUMN_COUNT)
                if row >= 25 or column >= 3
            ),
        )
        if payload_mode == "sparse":
            connection.execute(
                "UPDATE roster_data SET value=? WHERE class_id=? AND col_index BETWEEN ? AND ? "
                "AND NOT ((row_index=? AND col_index=?) OR "
                "(row_index=? AND col_index=?) OR (row_index=? AND col_index=?))",
                (SPARSE_PAYLOAD_SENTINEL, TARGET_CLASS_ID, CUSTOM_ROSTER_COLUMN_OFFSET,
                 CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT - 1,
                 *PAYLOAD_PROBE_COORDINATES[0], *PAYLOAD_PROBE_COORDINATES[1],
                 *PAYLOAD_PROBE_COORDINATES[2]),
            )
        elif payload_mode not in (None, "filled"):
            raise ValueError(f"Unsupported payload mode: {payload_mode}")
        if record_count_mode == "reduced-rows":
            probe_predicate = " OR ".join("(row_index=? AND col_index=?)" for _ in PAYLOAD_PROBE_COORDINATES)
            connection.execute(
                "DELETE FROM roster_data WHERE class_id=? AND col_index BETWEEN ? AND ? AND NOT ("
                + probe_predicate
                + ")",
                (
                    TARGET_CLASS_ID,
                    CUSTOM_ROSTER_COLUMN_OFFSET,
                    CUSTOM_ROSTER_COLUMN_OFFSET + CUSTOM_STRESS_COLUMN_COUNT - 1,
                    *(coordinate for pair in PAYLOAD_PROBE_COORDINATES for coordinate in pair),
                ),
            )
        connection.commit()
        expected_custom_count = (
            roster_record_count_values(record_count_mode)[0]
            if record_count_mode is not None
            else CUSTOM_CELL_RECORD_COUNT
        )
        baseline = inspect_database(connection, "prepared", expected_custom_count)
        payload_record = (
            payload_diagnostic_record(
                connection, payload_mode, base, seed, Path(__file__).resolve()
            )
            if payload_mode is not None
            else None
        )
        record_count_record = (
            record_count_diagnostic_record(
                connection, record_count_mode, base, seed, Path(__file__).resolve()
            )
            if record_count_mode is not None
            else None
        )
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()
    remove_sidecars(output, "prepared")
    before_values = []
    for name, target in TARGET_VALUES.items():
        before = read_target_cell(output, target["row"], target["storedColumn"])
        before_values.append(
            {
                "name": name,
                "classId": TARGET_CLASS_ID,
                "row": target["row"],
                "modelColumn": target["modelColumn"],
                "storedColumn": target["storedColumn"],
                "beforeValue": before,
                "afterValue": target["value"],
            }
        )
    receipt = {
        "schema": SCHEMA,
        "status": "passed",
        "kind": "large-roster-editing",
        "workspace": str(output),
        "workspaceSha256": sha256_file(output),
        "workspaceBytes": output.stat().st_size,
        "baseWorkspace": str(base),
        "baseWorkspaceSha256": sha256_file(base),
        "seedSql": str(seed),
        "seedSqlSha256": sha256_file(seed),
        "preparerPath": str(Path(__file__).resolve()),
        "preparerSha256": sha256_file(Path(__file__).resolve()),
        "dimensionContract": {
            "targetClassId": TARGET_CLASS_ID,
            "targetClassName": TARGET_CLASS_NAME,
            "rosterRows": ROSTER_ROW_COUNT,
            "fixedBaseColumns": DISPLAYED_BASE_COLUMNS,
            "customStressColumns": CUSTOM_STRESS_COLUMN_COUNT,
            "storedRosterColumns": STORED_ROSTER_COLUMNS,
            "displayedBaseColumns": DISPLAYED_BASE_COLUMNS,
            "displayedCustomColumns": CUSTOM_STRESS_COLUMN_COUNT,
            "displayedRosterColumns": STORED_ROSTER_COLUMNS,
            "targetRosterGridCellCapacity": ROSTER_ROW_COUNT * STORED_ROSTER_COLUMNS,
            "targetNonEmptyRosterDatabaseCellCount": ROSTER_ROW_COUNT * 2 + expected_custom_count,
        },
        "expectedEditTargets": before_values,
        "baselineSchemaObjects": baseline["schemaObjects"],
        "baselineSchemaObjectsSha256": baseline["schemaObjectsSha256"],
        "baselineTables": baseline["workspaceTables"],
        "baselineTablesSha256": baseline["workspaceTablesSha256"],
        "integrityCheck": baseline["integrityCheck"],
        "foreignKeyErrors": baseline["foreignKeyErrors"],
        "identity": baseline["identity"],
        "identityRule": (
            "the pinned workspace has the exact target class roster shape; after the UI route, "
            "only the three listed roster_data values may differ and every other schema, row, "
            "column, and table must remain byte-value-equivalent in canonical snapshots"
        ),
    }
    if payload_record is not None:
        receipt["payloadMode"] = payload_mode
        receipt["payloadDiagnostic"] = payload_record
    if record_count_record is not None:
        receipt["recordCountMode"] = record_count_mode
        receipt["recordCountDiagnostic"] = record_count_record
    return receipt


def read_target_cell(workspace: Path, row: int, column: int) -> str:
    connection = read_only_connection(workspace)
    try:
        result = connection.execute(
            "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
            (TARGET_CLASS_ID, row, column),
        ).fetchone()
        if result is None:
            raise ValueError(f"Target roster cell is missing: class={TARGET_CLASS_ID}, row={row}, column={column}.")
        return "" if result[0] is None else str(result[0])
    finally:
        connection.close()


def snapshot(phase: str, workspace: Path, receipt_path: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    receipt = read_receipt(receipt_path.resolve(strict=True), workspace)
    if phase == "before" and sha256_file(workspace) != receipt.get("workspaceSha256"):
        raise ValueError("Before-snapshot database bytes differ from the newly prepared fixture.")
    remove_sidecars(workspace, phase)
    connection = read_only_connection(workspace)
    try:
        expected_custom_count = (
            roster_record_count_values(str(receipt["recordCountMode"]))[0]
            if "recordCountMode" in receipt
            else CUSTOM_CELL_RECORD_COUNT
        )
        current = inspect_database(connection, phase, expected_custom_count)
        payload = None
        record_count = None
        if "payloadMode" in receipt:
            payload = custom_payload_counts(connection)
            expected_sentinel_count = (
                SPARSE_PAYLOAD_SENTINEL_COUNT if receipt["payloadMode"] == "sparse" else 0
            )
            if (
                payload["customCellRecordCount"] != CUSTOM_CELL_RECORD_COUNT
                or payload["blankCustomCellValueCount"] != 0
                or payload["customCellValueCount"] != CUSTOM_CELL_RECORD_COUNT
                or payload["compactSentinelValueCount"] != expected_sentinel_count
            ):
                raise ValueError(f"{phase.capitalize()} snapshot custom-cell payload counts differ from the declared mode.")
            expected_probe_values = {
                (int(target["row"]), int(target["storedColumn"])): str(
                    target["beforeValue"] if phase == "before" else target["afterValue"]
                )
                for target in receipt["expectedEditTargets"]
            }
            validate_payload_values(connection, str(receipt["payloadMode"]), expected_probe_values, phase)
            payload["probeCells"] = []
            for probe in receipt["payloadDiagnostic"]["probeCells"]:
                value = connection.execute(
                    "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
                    (TARGET_CLASS_ID, int(probe["row"]), int(probe["storedColumn"])),
                ).fetchone()
                if value is None or value[0] is None:
                    raise ValueError(f"{phase.capitalize()} snapshot probe cell is missing or NULL.")
                snapshot_value = str(value[0])
                expected_value_key = "beforeValue" if phase == "before" else "editValue"
                if snapshot_value != str(probe[expected_value_key]):
                    raise ValueError(f"{phase.capitalize()} snapshot probe value differs from its declared target.")
                payload["probeCells"].append({**probe, "snapshotValue": snapshot_value})
            expected_sentinel = (
                SPARSE_PAYLOAD_SENTINEL if receipt["payloadMode"] == "sparse" else None
            )
            if receipt["payloadDiagnostic"].get("compactSentinelValue") != expected_sentinel:
                raise ValueError(f"{phase.capitalize()} snapshot compact payload sentinel differs from its receipt.")
        if "recordCountMode" in receipt:
            record_mode = str(receipt["recordCountMode"])
            expected_probe_values = {
                (int(target["row"]), int(target["storedColumn"])): str(
                    target["beforeValue"] if phase == "before" else target["afterValue"]
                )
                for target in receipt["expectedEditTargets"]
            }
            record_counts = validate_record_count_rows(
                connection, record_mode, expected_probe_values, phase
            )
            snapshot_probe_cells = []
            for probe in receipt["recordCountDiagnostic"]["probeCells"]:
                coordinate = (int(probe["row"]), int(probe["storedColumn"]))
                result = connection.execute(
                    "SELECT value FROM roster_data WHERE class_id=? AND row_index=? AND col_index=?",
                    (TARGET_CLASS_ID, *coordinate),
                ).fetchone()
                expected_key = "beforeValue" if phase == "before" else "editValue"
                if result is None or result[0] is None or str(result[0]) != str(probe[expected_key]):
                    raise ValueError(f"{phase.capitalize()} record-count probe value differs at {coordinate!r}.")
                snapshot_probe_cells.append({**probe, "snapshotValue": str(result[0])})
            record_count = {
                **receipt["recordCountDiagnostic"],
                "snapshotCounts": record_counts,
                "snapshotProbeCells": snapshot_probe_cells,
            }
    finally:
        connection.close()
    snapshot_value = {
        "schema": SCHEMA,
        "status": "passed",
        "phase": phase,
        "workspace": str(workspace),
        "workspaceSha256": sha256_file(workspace),
        **current,
    }
    if payload is not None:
        snapshot_value["payloadMode"] = str(receipt["payloadMode"])
        snapshot_value["payloadDiagnostic"] = {
            **receipt["payloadDiagnostic"],
            "snapshotCounts": {
                key: value for key, value in payload.items() if key != "probeCells"
            },
            "snapshotProbeCells": payload["probeCells"],
        }
    if record_count is not None:
        snapshot_value["recordCountMode"] = str(receipt["recordCountMode"])
        snapshot_value["recordCountDiagnostic"] = record_count
    return snapshot_value


def write_new_json(path: Path, value: dict[str, Any]) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        raise ValueError(f"Refusing to overwrite evidence file {path}")
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--base-workspace", type=Path, required=True)
    prepare_parser.add_argument("--seed-sql", type=Path, required=True)
    prepare_parser.add_argument("--output", type=Path, required=True)
    prepare_parser.add_argument("--receipt", type=Path, required=True)
    prepare_parser.add_argument("--payload-mode", choices=("filled", "sparse"))
    prepare_parser.add_argument("--record-count-mode", choices=("filled", "reduced-rows"))
    snapshot_parser = commands.add_parser("snapshot")
    snapshot_parser.add_argument("--phase", choices=("before", "after"), required=True)
    snapshot_parser.add_argument("--workspace", type=Path, required=True)
    snapshot_parser.add_argument("--preparation-receipt", type=Path, required=True)
    snapshot_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            receipt = prepare(
                args.base_workspace,
                args.seed_sql,
                args.output,
                args.payload_mode,
                args.record_count_mode,
            )
            write_new_json(args.receipt, receipt)
            result = {
                "schema": SCHEMA,
                "status": "passed",
                "kind": receipt["kind"],
                "workspace": receipt["workspace"],
                "workspaceSha256": receipt["workspaceSha256"],
                "receipt": str(args.receipt.resolve()),
                "receiptSha256": sha256_file(args.receipt.resolve()),
                "identity": receipt["identity"],
            }
            if "payloadMode" in receipt:
                result["payloadMode"] = receipt["payloadMode"]
                result["payloadDiagnostic"] = receipt["payloadDiagnostic"]
            if "recordCountMode" in receipt:
                result["recordCountMode"] = receipt["recordCountMode"]
                result["recordCountDiagnostic"] = receipt["recordCountDiagnostic"]
        else:
            snapshot_value = snapshot(args.phase, args.workspace, args.preparation_receipt)
            write_new_json(args.output, snapshot_value)
            result = {
                "schema": SCHEMA,
                "status": "passed",
                "phase": args.phase,
                "snapshot": str(args.output.resolve()),
                "snapshotSha256": sha256_file(args.output.resolve()),
                "workspaceSha256": snapshot_value["workspaceSha256"],
                "identity": snapshot_value["identity"],
            }
            if "recordCountMode" in snapshot_value:
                result["recordCountMode"] = snapshot_value["recordCountMode"]
                result["recordCountDiagnostic"] = snapshot_value["recordCountDiagnostic"]
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"P9-17 fixture preparation/inspection failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
