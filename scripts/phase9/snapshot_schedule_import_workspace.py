#!/usr/bin/env python3
"""Capture a complete, stable SQLite table snapshot for P9-09 evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path
from typing import Any


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
    uri = resolved.as_uri() + "?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=10)
    try:
        connection.execute("PRAGMA query_only = ON")
        connection.execute("PRAGMA busy_timeout = 10000")
        connection.execute("BEGIN")
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        tables = [
            str(row[0])
            for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
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
            name: {
                "schema": value["schema"],
                "rows": value["rows"],
            }
            for name, value in table_rows.items()
        }
        file_digest = hashlib.sha256(resolved.read_bytes()).hexdigest()
        return {
            "schema": "classmngr-phase9-schedule-import-workspace-snapshot-v1",
            "workspacePath": str(resolved),
            "workspaceFileSizeBytes": resolved.stat().st_size,
            "workspaceFileSha256": file_digest,
            "integrityCheck": integrity,
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
    payload = snapshot(options.workspace)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps({
        "workspacePath": payload["workspacePath"],
        "integrityCheck": payload["integrityCheck"],
        "tableCount": payload["tableCount"],
        "tableContentSha256": payload["tableContentSha256"],
        "output": str(options.output.resolve()),
    }, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
