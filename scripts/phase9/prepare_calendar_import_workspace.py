#!/usr/bin/env python3
"""Seed a fresh Next workspace from the accepted large-startup SQL fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path


def prepare(blank_workspace: Path, output_workspace: Path, seed_sql: Path) -> dict[str, object]:
    source = blank_workspace.resolve(strict=True)
    output = output_workspace.resolve(strict=False)
    seed = seed_sql.resolve(strict=True)
    if output.exists():
        raise FileExistsError(f"refusing to overwrite workspace {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(source.read_bytes())

    connection = sqlite3.connect(output, timeout=10)
    try:
        connection.execute("PRAGMA busy_timeout = 10000")
        connection.execute("PRAGMA foreign_keys = ON")
        connection.executescript(seed.read_text(encoding="utf-8"))
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        event_count = int(connection.execute("SELECT COUNT(*) FROM calendar_events").fetchone()[0])
        teacher_count = int(connection.execute("SELECT COUNT(*) FROM teachers").fetchone()[0])
        class_count = int(connection.execute("SELECT COUNT(*) FROM classes").fetchone()[0])
        if integrity != "ok":
            raise RuntimeError(f"seeded workspace integrity check failed: {integrity}")
        if (event_count, teacher_count, class_count) != (180, 24, 96):
            raise RuntimeError(
                "large-startup seed counts differ from 180 calendar events, "
                f"24 teachers, and 96 classes: {(event_count, teacher_count, class_count)}"
            )
    finally:
        connection.close()

    data = output.read_bytes()
    return {
        "workspacePath": str(output.resolve(strict=True)),
        "workspaceSizeBytes": len(data),
        "workspaceSha256": hashlib.sha256(data).hexdigest(),
        "seedSqlPath": str(seed),
        "seedSqlSha256": hashlib.sha256(seed.read_bytes()).hexdigest(),
        "integrityCheck": integrity,
        "calendarEventCount": event_count,
        "teacherCount": teacher_count,
        "classCount": class_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--blank-workspace", required=True, type=Path)
    parser.add_argument("--output-workspace", required=True, type=Path)
    parser.add_argument("--seed-sql", required=True, type=Path)
    options = parser.parse_args()
    print(json.dumps(prepare(
        options.blank_workspace,
        options.output_workspace,
        options.seed_sql,
    ), ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
