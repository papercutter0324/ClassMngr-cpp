#!/usr/bin/env python3
"""Prepare and inspect the P9-15 isolated packaged Schedule workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-15-schedule-workspace-v1"
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
CLASS_COUNT = 96
SCHEDULE_ENTRY_COUNT = 768
WEEKDAYS = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")
WEEKEND_DAYS = ("Saturday", "Sunday")


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
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
        )
    ]


def primary_key_order(connection: sqlite3.Connection, table: str, columns: list[str]) -> str:
    details = list(connection.execute(f'PRAGMA table_info("{table}")'))
    primary = [str(row[1]) for row in sorted((row for row in details if int(row[5]) > 0), key=lambda row: int(row[5]))]
    order = primary or columns
    return ", ".join('"' + column.replace('"', '""') + '"' for column in order)


def rows_hash(columns: list[str], rows: list[list[Any]]) -> str:
    return canonical_sha256({"columns": columns, "rows": rows})


def workspace_snapshot(connection: sqlite3.Connection) -> dict[str, Any]:
    tables: dict[str, Any] = {}
    for table in table_names(connection):
        escaped = table.replace('"', '""')
        columns = [str(row[1]) for row in connection.execute(f'PRAGMA table_info("{escaped}")')]
        order = primary_key_order(connection, table, columns)
        values = [json_value(list(row)) for row in connection.execute(f'SELECT * FROM "{escaped}" ORDER BY {order}')]
        tables[table] = {
            "columns": columns,
            "rowCount": len(values),
            "rows": values,
            "rowsSha256": rows_hash(columns, values),
        }
    return {"tables": tables, "tableCount": len(tables), "tablesSha256": canonical_sha256(tables)}


def schedule_fixture(connection: sqlite3.Connection) -> dict[str, Any]:
    classes = int(connection.execute("SELECT COUNT(*) FROM classes").fetchone()[0])
    entries = int(connection.execute("SELECT COUNT(*) FROM class_times").fetchone()[0])
    info = int(connection.execute("SELECT COUNT(*) FROM class_info").fetchone()[0])
    if (classes, info, entries) != (CLASS_COUNT, CLASS_COUNT, SCHEDULE_ENTRY_COUNT):
        raise ValueError(f"large_startup.sql fixture scale differs: classes={classes}, classInfo={info}, regularEntries={entries}")
    names = [str(row[0]) for row in connection.execute("SELECT name FROM classes ORDER BY id")]
    if names != [f"Heavy Fixture Class {index:03d}" for index in range(1, CLASS_COUNT + 1)]:
        raise ValueError("large_startup.sql class identities differ from the pinned 96-class fixture.")
    settings = {
        str(key): str(value)
        for key, value in connection.execute("SELECT key,value FROM app_settings")
    }
    def setting_bool(key: str, default: bool = False) -> bool:
        value = settings.get(key, "true" if default else "false").strip().lower()
        if value not in {"true", "false", "1", "0"}:
            raise ValueError(f"Fixture has an invalid {key} preference: {value!r}")
        return value in {"true", "1"}

    show_english_names = setting_bool("schedule_show_korean_teacher_english_names")
    show_weekends = setting_bool("schedule_show_weekends")
    rows = list(connection.execute(
        "SELECT ct.class_id,ct.day,ct.start_time,ct.end_time,t.teacher_kr,t.teacher_en,t.preferred_name,t.room_number,ci.class_grade,ci.class_level "
        "FROM class_times ct JOIN class_info ci ON ci.class_id=ct.class_id "
        "JOIN teachers t ON t.id=ci.teacher_id ORDER BY ct.class_id,ct.day,ct.start_time,ct.end_time"
    ))
    projection: dict[str, list[dict[str, Any]]] = {}
    all_rows: list[dict[str, Any]] = []
    for row in rows:
        class_id, day, start, end, teacher_kr, teacher_en, preferred, room, grade, level = row
        if str(day) not in WEEKDAYS:
            raise ValueError(f"Unexpected schedule day in fixture: {day!r}")
        parsed = datetime.strptime(str(start), "%I:%M %p")
        row_index = parsed.hour - 15
        if not 0 <= row_index < 7:
            raise ValueError(f"Schedule start time is outside the expected 3 PM to 9 PM table rows: {start!r}")
        preferred_label = preferred if show_english_names else teacher_kr
        teacher = str(preferred_label or teacher_en or teacher_kr or "").strip()
        if not teacher:
            raise ValueError(f"Fixture schedule row {class_id} has no teacher label.")
        teacher_room = f"{teacher} {str(room or '').strip()}".strip()
        class_line = str(grade or "").strip()
        if str(level or "").strip():
            class_line = f"{class_line} - {str(level).strip()}" if class_line else str(level).strip()
        item = {
            "classId": int(class_id), "day": str(day), "startTime": str(start), "endTime": str(end),
            "row": row_index, "column": WEEKDAYS.index(str(day)) + 1,
            "teacherRoom": teacher_room, "classLine": class_line,
        }
        key = f"{row_index},{item['column']}"
        projection.setdefault(key, []).append(item)
        all_rows.append(item)
    if len(all_rows) != SCHEDULE_ENTRY_COUNT or len(projection) < 10:
        raise ValueError("Fixture schedule projection is incomplete or too small.")
    displayed_days = (*WEEKDAYS, *WEEKEND_DAYS) if show_weekends else WEEKDAYS
    default_slot_states: dict[str, str] = {}
    for row_index in range(7):
        hour = 15 + row_index
        for day_index, _day in enumerate(displayed_days):
            column = day_index + 1
            key = f"{row_index},{column}"
            if key in projection:
                continue
            # Regular weekday slots after 3 PM default to Essay; weekends
            # default to Empty. Source entries replace the slot label.
            default_slot_states[key] = "Essay" if day_index < 5 and hour > 15 else "Empty"
    return {
        "classCount": classes,
        "classInfoCount": info,
        "scheduleEntryCount": entries,
        "scheduleRows": all_rows,
        "showEnglishNames": show_english_names,
        "showWeekends": show_weekends,
        "uiGrid": {"headers": ["Time", *displayed_days], "rowCount": 7, "columnCount": 1 + len(displayed_days), "sourceRowOffset": 15},
        "fixtureProjection": projection,
        "defaultSlotStates": default_slot_states,
        "projectionRule": "regular class_times rows grouped by weekday and parsed start hour; teacher label follows the seeded showEnglishNames display preference; accessible pair is teacher/room plus grade-level",
    }


def check_database(connection: sqlite3.Connection, phase: str, preparation: dict[str, Any] | None = None) -> dict[str, Any]:
    integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
    foreign_keys = [json_value(list(row)) for row in connection.execute("PRAGMA foreign_key_check")]
    if integrity != "ok" or foreign_keys:
        raise ValueError(f"{phase} workspace integrity failed: integrity={integrity}, foreignKeyErrors={foreign_keys}")
    snapshot = workspace_snapshot(connection)
    schedule = schedule_fixture(connection)
    if preparation is not None:
        if snapshot["tablesSha256"] != preparation["baselineTablesSha256"]:
            raise ValueError(f"{phase} workspace rows differ from the prepared baseline.")
        if snapshot["tables"] != preparation["baselineTables"]:
            raise ValueError(f"{phase} full workspace table rows differ from the prepared baseline.")
        if schedule["scheduleRows"] != preparation.get("schedule", {}).get("scheduleRows"):
            raise ValueError(f"{phase} regular schedule rows differ from the prepared large_startup fixture.")
    return {
        "schema": SCHEMA, "status": "passed", "phase": phase,
        "integrityCheck": integrity, "foreignKeyErrors": foreign_keys,
        "workspaceTables": snapshot["tables"], "workspaceTableCount": snapshot["tableCount"],
        "workspaceTablesSha256": snapshot["tablesSha256"],
        "schedule": schedule,
    }


def prepare(base: Path, seed: Path, output: Path) -> dict[str, Any]:
    base = base.resolve(strict=True)
    seed = seed.resolve(strict=True)
    if sha256_file(base) != BASE_SHA256 or sha256_file(seed) != SEED_SHA256:
        raise ValueError("Master blank workspace or large_startup.sql differs from its accepted fixture fingerprint.")
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
        connection.commit()
        baseline = check_database(connection, "prepared")
    finally:
        connection.close()
    for suffix in ("-wal", "-shm", "-journal"):
        Path(str(output) + suffix).unlink(missing_ok=True)
    return {
        "schema": SCHEMA, "status": "passed", "kind": "schedule-locale-theme",
        "workspace": str(output), "workspaceSha256": sha256_file(output), "workspaceBytes": output.stat().st_size,
        "baseWorkspace": str(base), "baseWorkspaceSha256": sha256_file(base),
        "seedSql": str(seed), "seedSqlSha256": sha256_file(seed),
        "baselineTables": baseline["workspaceTables"],
        "baselineTablesSha256": baseline["workspaceTablesSha256"],
        "schedule": baseline["schedule"],
        "identityRule": "all non-internal SQLite table columns and rows in primary-key order are retained in the preparation receipt and compared before and after the route",
    }


def snapshot(phase: str, workspace: Path, receipt_path: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    preparation = json.loads(receipt_path.read_text(encoding="utf-8"))
    if preparation.get("schema") != SCHEMA or preparation.get("status") != "passed" or preparation.get("kind") != "schedule-locale-theme":
        raise ValueError("Preparation receipt does not match the P9-15 Schedule fixture.")
    if Path(str(preparation.get("workspace"))).resolve() != workspace:
        raise ValueError("Snapshot workspace path differs from the preparation receipt.")
    if phase == "before" and sha256_file(workspace) != preparation.get("workspaceSha256"):
        raise ValueError("Before snapshot workspace bytes differ from the prepared fixture.")
    for suffix in ("-wal", "-shm", "-journal"):
        if Path(str(workspace) + suffix).exists():
            raise ValueError(f"Workspace SQLite sidecar remains before {phase} snapshot: {suffix}")
    connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
    try:
        connection.execute("PRAGMA query_only=ON")
        result = check_database(connection, phase, preparation)
    finally:
        connection.close()
    result.update({"workspace": str(workspace), "workspaceSha256": sha256_file(workspace)})
    return result


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
    snapshot_parser = commands.add_parser("snapshot")
    snapshot_parser.add_argument("--phase", choices=("before", "after"), required=True)
    snapshot_parser.add_argument("--workspace", type=Path, required=True)
    snapshot_parser.add_argument("--preparation-receipt", type=Path, required=True)
    snapshot_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            receipt = prepare(args.base_workspace, args.seed_sql, args.output)
            write_new_json(args.receipt, receipt)
            result = {
                "schema": receipt["schema"], "status": receipt["status"], "kind": receipt["kind"],
                "workspace": receipt["workspace"], "workspaceSha256": receipt["workspaceSha256"],
                "classes": receipt["schedule"]["classCount"],
                "scheduleEntries": receipt["schedule"]["scheduleEntryCount"],
                "receipt": str(args.receipt.resolve()), "receiptSha256": sha256_file(args.receipt.resolve()),
            }
        else:
            snapshot_result = snapshot(args.phase, args.workspace, args.preparation_receipt)
            write_new_json(args.output, snapshot_result)
            result = {
                "schema": SCHEMA, "status": "passed", "phase": args.phase,
                "snapshot": str(args.output.resolve()), "snapshotSha256": sha256_file(args.output.resolve()),
                "classes": snapshot_result["schedule"]["classCount"],
                "scheduleEntries": snapshot_result["schedule"]["scheduleEntryCount"],
            }
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"P9-15 workspace preparation/inspection failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
