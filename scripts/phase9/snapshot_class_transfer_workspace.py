#!/usr/bin/env python3
"""Capture every SQLite table and row for packaged Class Transfer evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path
from typing import Any


SNAPSHOT_SCHEMA = "classmngr-phase9-class-transfer-workspace-snapshot-v1"


def _json_value(value: Any) -> Any:
    if isinstance(value, bytes):
        return {"$bytesHex": value.hex()}
    return value


def _canonical(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def snapshot(path: Path) -> dict[str, Any]:
    resolved = path.resolve(strict=True)
    connection = sqlite3.connect(
        resolved.as_uri() + "?mode=ro", uri=True, timeout=20
    )
    try:
        connection.execute("PRAGMA query_only = ON")
        connection.execute("PRAGMA busy_timeout = 20000")
        connection.execute("BEGIN")
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        foreign_key_errors = [
            [_json_value(value) for value in row]
            for row in connection.execute("PRAGMA foreign_key_check")
        ]
        tables = [
            str(row[0])
            for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table' "
                "AND name NOT LIKE 'sqlite_%' ORDER BY name"
            )
        ]
        table_rows: dict[str, dict[str, Any]] = {}
        for name in tables:
            escaped = name.replace('"', '""')
            schema = [
                [_json_value(value) for value in row]
                for row in connection.execute(f'PRAGMA table_info("{escaped}")')
            ]
            rows = [
                [_json_value(value) for value in row]
                for row in connection.execute(f'SELECT * FROM "{escaped}"')
            ]
            rows.sort(key=_canonical)
            contents = {"schema": schema, "rows": rows}
            table_rows[name] = {
                **contents,
                "rowCount": len(rows),
                "contentSha256": hashlib.sha256(_canonical(contents)).hexdigest(),
            }
        content = {
            name: {"schema": item["schema"], "rows": item["rows"]}
            for name, item in table_rows.items()
        }
        data = resolved.read_bytes()
        return {
            "schema": SNAPSHOT_SCHEMA,
            "workspacePath": str(resolved),
            "workspaceFileSizeBytes": len(data),
            "workspaceFileSha256": hashlib.sha256(data).hexdigest(),
            "integrityCheck": integrity,
            "foreignKeyErrors": foreign_key_errors,
            "tableCount": len(table_rows),
            "tableContentSha256": hashlib.sha256(_canonical(content)).hexdigest(),
            "tables": table_rows,
        }
    finally:
        connection.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    options = parser.parse_args()
    document = snapshot(options.workspace)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(
        json.dumps(document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    counts = {
        name: document["tables"].get(name, {}).get("rowCount")
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
    print(
        json.dumps(
            {
                "workspacePath": document["workspacePath"],
                "integrityCheck": document["integrityCheck"],
                "foreignKeyErrorCount": len(document["foreignKeyErrors"]),
                "tableCount": document["tableCount"],
                "tableContentSha256": document["tableContentSha256"],
                "counts": counts,
                "output": str(options.output.resolve()),
            },
            ensure_ascii=False,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
