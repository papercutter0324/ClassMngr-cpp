#!/usr/bin/env python3
"""Prepare and inspect the pinned P9-16 repeated workspace route fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-16-repeated-workspace-fixture-v1"
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
CLASS_COUNT = 96
CLASS_INFO_COUNT = 96
SCHEDULE_ENTRY_COUNT = 768
PROFILE_IDENTITIES = {
    "tiny": {"classCount": 0, "classInfoCount": 0, "scheduleEntryCount": 0},
    "large": {"classCount": CLASS_COUNT, "classInfoCount": CLASS_INFO_COUNT, "scheduleEntryCount": SCHEDULE_ENTRY_COUNT},
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


def schema_objects(connection: sqlite3.Connection) -> list[list[Any]]:
    rows = connection.execute(
        "SELECT type,name,tbl_name,sql FROM sqlite_master "
        "WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name"
    )
    return [json_value(list(row)) for row in rows]


def table_names(connection: sqlite3.Connection) -> list[str]:
    return [
        str(row[0])
        for row in connection.execute(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"
        )
    ]


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


def fixture_identity(connection: sqlite3.Connection, profile_size: str = "large") -> dict[str, int]:
    identity = {
        "classCount": int(connection.execute("SELECT COUNT(*) FROM classes").fetchone()[0]),
        "classInfoCount": int(connection.execute("SELECT COUNT(*) FROM class_info").fetchone()[0]),
        "scheduleEntryCount": int(connection.execute("SELECT COUNT(*) FROM class_times").fetchone()[0]),
    }
    expected = PROFILE_IDENTITIES.get(profile_size)
    if expected is None:
        raise ValueError(f"Unsupported P9-16 workspace profile size: {profile_size!r}")
    if identity != expected:
        raise ValueError(f"Prepared {profile_size} workspace profile identity differs: {identity!r}")
    names = [str(row[0]) for row in connection.execute("SELECT name FROM classes ORDER BY id")]
    if profile_size == "large":
        expected_names = [f"Heavy Fixture Class {index:03d}" for index in range(1, CLASS_COUNT + 1)]
        if names != expected_names:
            raise ValueError("The seeded class names do not match the pinned 96-class fixture.")
    elif names:
        raise ValueError("The tiny current-schema profile must retain an empty class catalog.")
    return identity


def inspect_database(
    connection: sqlite3.Connection,
    phase: str,
    preparation: dict[str, Any] | None = None,
    profile_size: str = "large",
) -> dict[str, Any]:
    integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
    foreign_keys = [json_value(list(row)) for row in connection.execute("PRAGMA foreign_key_check")]
    if integrity != "ok" or foreign_keys:
        raise ValueError(f"{phase} integrity failed: {integrity}; foreign key errors={foreign_keys!r}")
    effective_profile_size = str(preparation.get("profileSize", profile_size)) if preparation else profile_size
    identity = fixture_identity(connection, effective_profile_size)
    snapshot = workspace_snapshot(connection)
    if preparation is not None:
        if snapshot["schemaObjects"] != preparation["baselineSchemaObjects"]:
            raise ValueError(f"{phase} SQLite schema objects differ from the prepared baseline.")
        if snapshot["tables"] != preparation["baselineTables"]:
            raise ValueError(f"{phase} table schemas/rows differ from the prepared baseline.")
        if snapshot["tablesSha256"] != preparation["baselineTablesSha256"]:
            raise ValueError(f"{phase} table fingerprint differs from the prepared baseline.")
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
    if (
        receipt.get("schema") != SCHEMA
        or receipt.get("status") != "passed"
        or receipt.get("kind") != "repeated-workspace-open-close"
    ):
        raise ValueError("Preparation receipt does not identify the P9-16 repeated workspace fixture.")
    if receipt.get("profileSize") not in PROFILE_IDENTITIES or receipt.get("identity") != PROFILE_IDENTITIES[receipt["profileSize"]]:
        raise ValueError("Preparation receipt has an unsupported profile size or identity.")
    if Path(str(receipt.get("workspace", ""))).resolve() != workspace:
        raise ValueError("Requested workspace path differs from the preparation receipt.")
    return receipt


def read_only_connection(workspace: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
    connection.execute("PRAGMA query_only=ON")
    return connection


def prepare(base: Path, seed: Path | None, output: Path, profile_size: str = "large") -> dict[str, Any]:
    base = base.resolve(strict=True)
    if profile_size not in PROFILE_IDENTITIES:
        raise ValueError(f"Unsupported P9-16 workspace profile size: {profile_size!r}")
    if sha256_file(base) != BASE_SHA256:
        raise ValueError("Master blank workspace differs from its pinned fingerprint.")
    if profile_size == "large":
        if seed is None:
            raise ValueError("The large fixture profile requires the pinned large_startup.sql source.")
        seed = seed.resolve(strict=True)
        if sha256_file(seed) != SEED_SHA256:
            raise ValueError("large_startup.sql differs from its pinned fingerprint.")
    elif seed is not None:
        raise ValueError("The tiny fixture profile must be derived directly from the pinned current-schema blank workspace.")
    output = output.resolve()
    if output.exists():
        raise ValueError(f"Refusing to overwrite fixture workspace {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base.read_bytes())
    if profile_size == "large":
        assert seed is not None
        connection = sqlite3.connect(str(output))
        try:
            connection.execute("PRAGMA foreign_keys=ON")
            connection.execute("PRAGMA journal_mode=DELETE")
            connection.executescript(seed.read_text(encoding="utf-8"))
            connection.commit()
            baseline = inspect_database(connection, "prepared", profile_size=profile_size)
        finally:
            connection.close()
    else:
        connection = read_only_connection(output)
        try:
            baseline = inspect_database(connection, "prepared", profile_size=profile_size)
        finally:
            connection.close()
    remove_sidecars(output, "prepared")
    return {
        "schema": SCHEMA,
        "status": "passed",
        "kind": "repeated-workspace-open-close",
        "profileSize": profile_size,
        "workspace": str(output),
        "workspaceSha256": sha256_file(output),
        "workspaceBytes": output.stat().st_size,
        "baseWorkspace": str(base),
        "baseWorkspaceSha256": sha256_file(base),
        "seedSql": str(seed) if seed is not None else None,
        "seedSqlSha256": sha256_file(seed) if seed is not None else None,
        "baselineSchemaObjects": baseline["schemaObjects"],
        "baselineSchemaObjectsSha256": baseline["schemaObjectsSha256"],
        "baselineTables": baseline["workspaceTables"],
        "baselineTablesSha256": baseline["workspaceTablesSha256"],
        "identity": baseline["identity"],
        "identityRule": (
            "all non-internal SQLite schema objects and every table's declared columns and rows "
            "in primary-key order are retained and compared before and after the route"
        ),
    }


def remove_sidecars(workspace: Path, phase: str) -> None:
    for suffix in ("-wal", "-shm", "-journal"):
        sidecar = Path(str(workspace) + suffix)
        if sidecar.exists():
            raise ValueError(f"Workspace SQLite sidecar remains during {phase} inspection: {suffix}")


def snapshot(phase: str, workspace: Path, receipt_path: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    preparation = read_receipt(receipt_path.resolve(strict=True), workspace)
    if phase == "before" and sha256_file(workspace) != preparation.get("workspaceSha256"):
        raise ValueError("Before snapshot database bytes differ from the newly prepared fixture.")
    remove_sidecars(workspace, phase)
    connection = read_only_connection(workspace)
    try:
        result = inspect_database(connection, phase, preparation)
    finally:
        connection.close()
    result.update({"workspace": str(workspace), "workspaceSha256": sha256_file(workspace)})
    return result


def identity(workspace: Path, receipt_path: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    preparation = read_receipt(receipt_path.resolve(strict=True), workspace)
    remove_sidecars(workspace, "identity")
    connection = read_only_connection(workspace)
    try:
        current = inspect_database(connection, "identity", preparation)
    finally:
        connection.close()
    return {
        "schema": SCHEMA,
        "status": "passed",
        "workspace": str(workspace),
        "workspaceSha256": sha256_file(workspace),
        "identity": current["identity"],
        "workspaceTableCount": current["workspaceTableCount"],
        "workspaceTablesSha256": current["workspaceTablesSha256"],
        "schemaObjectsSha256": current["schemaObjectsSha256"],
        "integrityCheck": current["integrityCheck"],
    }


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
    prepare_parser.add_argument("--seed-sql", type=Path)
    prepare_parser.add_argument("--profile-size", choices=tuple(PROFILE_IDENTITIES), default="large")
    prepare_parser.add_argument("--output", type=Path, required=True)
    prepare_parser.add_argument("--receipt", type=Path, required=True)
    snapshot_parser = commands.add_parser("snapshot")
    snapshot_parser.add_argument("--phase", choices=("before", "after"), required=True)
    snapshot_parser.add_argument("--workspace", type=Path, required=True)
    snapshot_parser.add_argument("--preparation-receipt", type=Path, required=True)
    snapshot_parser.add_argument("--output", type=Path, required=True)
    identity_parser = commands.add_parser("identity")
    identity_parser.add_argument("--workspace", type=Path, required=True)
    identity_parser.add_argument("--preparation-receipt", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            receipt = prepare(args.base_workspace, args.seed_sql, args.output, args.profile_size)
            write_new_json(args.receipt, receipt)
            result = {
                "schema": receipt["schema"],
                "status": receipt["status"],
                "kind": receipt["kind"],
                "profileSize": receipt["profileSize"],
                "workspace": receipt["workspace"],
                "workspaceSha256": receipt["workspaceSha256"],
                **receipt["identity"],
                "receipt": str(args.receipt.resolve()),
                "receiptSha256": sha256_file(args.receipt.resolve()),
            }
        elif args.command == "snapshot":
            snapshot_result = snapshot(args.phase, args.workspace, args.preparation_receipt)
            write_new_json(args.output, snapshot_result)
            result = {
                "schema": SCHEMA,
                "status": "passed",
                "phase": args.phase,
                "snapshot": str(args.output.resolve()),
                "snapshotSha256": sha256_file(args.output.resolve()),
                **snapshot_result["identity"],
            }
        else:
            result = identity(args.workspace, args.preparation_receipt)
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"P9-16 workspace preparation/inspection failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
