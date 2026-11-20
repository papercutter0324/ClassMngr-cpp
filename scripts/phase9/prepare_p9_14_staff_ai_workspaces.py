#!/usr/bin/env python3
"""Prepare and inspect the isolated P9-14 packaged lifecycle workspaces."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-14-workspace-v1"
STAFF_ROWS = 96
AI_ROWS = 100
AI_SELECTED_ROWS = 25
AI_EVALUATION_ID = 1
AI_CLASS_ID = 1
AI_EVALUATION_NAME = "Winter"
BASE_WORKSPACE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
LARGE_STARTUP_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
STAFF_SOURCE_WORKSPACE_SHA256 = "d60176721613ef7eef127856a95e51805c7a9b12671353b35289a5e1e8ee7363"
STAFF_SOURCE_WORKSPACE_RELATIVE_PATH = "docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/generated-large-staff-directory.tps"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_sha256(value: Any) -> str:
    encoded = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def table_columns(connection: sqlite3.Connection, table: str) -> list[str]:
    return [str(row[1]) for row in connection.execute(f'PRAGMA table_info("{table}")')]


def rows_for(connection: sqlite3.Connection, table: str, order_by: str) -> tuple[list[str], list[list[Any]]]:
    columns = table_columns(connection, table)
    rows = [list(row) for row in connection.execute(f'SELECT * FROM "{table}" ORDER BY {order_by}')]
    return columns, rows


def rows_hash(columns: list[str], rows: list[list[Any]]) -> str:
    return canonical_sha256({"columns": columns, "rows": rows})


def ai_candidate_name(index: int) -> str:
    letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    zero_based = index - 1
    suffix = letters[zero_based // len(letters)] + letters[zero_based % len(letters)]
    return f"AI Candidate {suffix}"


def deterministic_comment(index: int) -> str:
    return (
        f"P9-14 review for {ai_candidate_name(index)}: your speaking shows a clear central idea and steady organization. "
        "Next, add one precise supporting detail and explain how it connects to your main point. "
        "This will help your listener follow the reasoning and understand why the example matters."
    )


def expected_ai_rows() -> list[dict[str, Any]]:
    hangul = "가나다라마바사아자차"
    return [
        {
            "rowIndex": index - 1,
            "studentId": f"STUDENT_{index:02d}",
            "englishName": ai_candidate_name(index),
            "koreanName": f"{hangul[(index - 1) // 10]}{hangul[(index - 1) % 10]}가",
            "grade": "E4",
            "didWell": f"Explains one idea clearly in candidate {index:03d}.",
            "needsImprovement": f"Add a supporting detail for candidate {index:03d}.",
            "expectedComment": deterministic_comment(index) if index <= AI_SELECTED_ROWS else "",
        }
        for index in range(1, AI_ROWS + 1)
    ]


def verify_workspace_base(base: Path, seed: Path, expected_base_sha256: str, expected_seed_sha256: str) -> tuple[Path, Path]:
    base = base.resolve(strict=True)
    seed = seed.resolve(strict=True)
    if sha256_file(base) != expected_base_sha256.lower():
        raise ValueError("Master blank workspace bytes differ from the accepted P9-11 workspace identity.")
    if sha256_file(seed) != expected_seed_sha256.lower():
        raise ValueError("large_startup.sql bytes differ from the accepted seed identity.")
    if expected_base_sha256.lower() != BASE_WORKSPACE_SHA256 or expected_seed_sha256.lower() != LARGE_STARTUP_SHA256:
        raise ValueError("P9-14 fixture inputs must be pinned to the accepted P9-11 workspace and seed hashes.")
    return base, seed


def seed_base(base: Path, seed_sql: Path, output: Path) -> sqlite3.Connection:
    output = output.resolve()
    if output.exists():
        raise ValueError(f"Refusing to overwrite fixture workspace {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base.read_bytes())
    connection = sqlite3.connect(str(output))
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA journal_mode = DELETE")
        connection.executescript(seed_sql.read_text(encoding="utf-8"))
        connection.commit()
        return connection
    except Exception:
        connection.close()
        output.unlink(missing_ok=True)
        raise


def close_workspace(connection: sqlite3.Connection, output: Path) -> None:
    connection.commit()
    integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
    foreign_key_errors = [list(row) for row in connection.execute("PRAGMA foreign_key_check")]
    connection.close()
    if integrity != "ok" or foreign_key_errors:
        raise ValueError(f"Workspace integrity failed: integrity={integrity}, foreignKeyErrors={foreign_key_errors}")
    for suffix in ("-journal", "-wal", "-shm"):
        Path(str(output) + suffix).unlink(missing_ok=True)


def prepare_staff_from_fixture(source_workspace: Path, output: Path) -> dict[str, Any]:
    source_workspace = source_workspace.resolve(strict=True)
    if sha256_file(source_workspace) != STAFF_SOURCE_WORKSPACE_SHA256:
        raise ValueError("Packaged Staff Directory source fixture differs from its accepted exact workspace fingerprint.")
    if output.exists():
        raise ValueError(f"Refusing to overwrite fixture workspace {output}")

    source_connection = sqlite3.connect(f"file:{source_workspace.as_posix()}?mode=ro", uri=True)
    try:
        source_connection.execute("PRAGMA query_only = ON")
        source_snapshot = snapshot_staff(source_connection)
        integrity = str(source_connection.execute("PRAGMA integrity_check").fetchone()[0])
        foreign_key_errors = [list(row) for row in source_connection.execute("PRAGMA foreign_key_check")]
    finally:
        source_connection.close()
    if integrity != "ok" or foreign_key_errors:
        raise ValueError(f"Accepted Staff Directory source fixture integrity failed: integrity={integrity}, foreignKeyErrors={foreign_key_errors}")
    for key in ("nativeEnglish", "gsTeam"):
        table = source_snapshot["tables"][key]
        if table["rowCount"] != STAFF_ROWS or [int(row["id"]) for row in table["rows"]] != list(range(1, STAFF_ROWS + 1)):
            raise ValueError(f"Accepted Staff Directory source fixture does not contain exact primary keys 1 through 96 for {key}.")

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(source_workspace.read_bytes())
    if sha256_file(output) != STAFF_SOURCE_WORKSPACE_SHA256:
        raise ValueError("Prepared Staff Directory workspace bytes differ from the pinned source fixture.")
    return {
        "schema": SCHEMA,
        "status": "passed",
        "kind": "staff-directory",
        "workspace": str(output),
        "workspaceSha256": sha256_file(output),
        "workspaceBytes": output.stat().st_size,
        "classCount": None,
        "sourceWorkspace": str(source_workspace),
        "sourceWorkspaceSha256": STAFF_SOURCE_WORKSPACE_SHA256,
        "sourceWorkspaceRelativePath": STAFF_SOURCE_WORKSPACE_RELATIVE_PATH,
        "nativeEnglish": {
            "table": "native_english_teachers",
            "rowCount": STAFF_ROWS,
            "rows": source_snapshot["tables"]["nativeEnglish"]["rows"],
            "tableRowsSha256": source_snapshot["tables"]["nativeEnglish"]["rowsSha256"],
        },
        "gsTeam": {
            "table": "gs_team",
            "rowCount": STAFF_ROWS,
            "rows": source_snapshot["tables"]["gsTeam"]["rows"],
            "tableRowsSha256": source_snapshot["tables"]["gsTeam"]["rowsSha256"],
        },
        "identityRule": "source workspace primary key; before/after exact full row identity retained in snapshots",
    }


def prepare_ai(base: Path, seed_sql: Path, output: Path) -> dict[str, Any]:
    connection = seed_base(base, seed_sql, output)
    try:
        info_columns = table_columns(connection, "class_info")
        evaluation_columns = table_columns(connection, "speaking_evaluations")
        data_columns = table_columns(connection, "speaking_eval_data")
        if "class_grade" not in info_columns or evaluation_columns != ["id", "class_id", "evaluation_name"]:
            raise ValueError("Speaking Evaluation fixture schema differs from the accepted workspace schema.")
        if data_columns != ["evaluation_id", "row_index", *[f"col_{index}" for index in range(11)]]:
            raise ValueError("Speaking Evaluation row schema differs from the accepted 11-column workspace schema.")
        update_class = connection.execute(
            "UPDATE class_info SET class_grade = ? WHERE class_id = ?", ("E4", AI_CLASS_ID)
        )
        if update_class.rowcount != 1:
            raise ValueError("Could not set the selected class grade to supported grade E4.")
        update_eval = connection.execute(
            "UPDATE speaking_evaluations SET evaluation_name = ? WHERE id = ? AND class_id = ?",
            (AI_EVALUATION_NAME, AI_EVALUATION_ID, AI_CLASS_ID),
        )
        if update_eval.rowcount != 1:
            raise ValueError("Could not bind the selected class speaking evaluation.")
        connection.execute("DELETE FROM speaking_eval_data WHERE evaluation_id = ?", (AI_EVALUATION_ID,))
        students = expected_ai_rows()
        records = []
        for student in students:
            notes = f"[Did Well]\n{student['didWell']}\n[Needs Improvement]\n{student['needsImprovement']}"
            values = ["", student["englishName"], student["koreanName"], "", "", "", "", "", "", "", notes]
            records.append((AI_EVALUATION_ID, student["rowIndex"], *values))
        connection.executemany(
            "INSERT INTO speaking_eval_data "
            "(evaluation_id,row_index,col_0,col_1,col_2,col_3,col_4,col_5,col_6,col_7,col_8,col_9,col_10) "
            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
            records,
        )
        connection.commit()
        actual_eval = connection.execute(
            "SELECT id,class_id,evaluation_name FROM speaking_evaluations WHERE id=?", (AI_EVALUATION_ID,)
        ).fetchone()
        actual_grade = connection.execute("SELECT class_grade FROM class_info WHERE class_id=?", (AI_CLASS_ID,)).fetchone()[0]
        actual_rows = [list(row) for row in connection.execute(
            "SELECT row_index,col_1,col_2,col_9,col_10 FROM speaking_eval_data WHERE evaluation_id=? ORDER BY row_index",
            (AI_EVALUATION_ID,),
        )]
        if actual_eval != (AI_EVALUATION_ID, AI_CLASS_ID, AI_EVALUATION_NAME) or actual_grade != "E4":
            raise ValueError("Prepared AI evaluation identity or supported class grade is incorrect.")
        if len(actual_rows) != AI_ROWS or [row[0] for row in actual_rows] != list(range(AI_ROWS)):
            raise ValueError("Prepared AI evaluation does not contain exactly 100 ordered candidate rows.")
        for index, row in enumerate(actual_rows, 1):
            student = students[index - 1]
            if row[1] != student["englishName"] or row[2] != student["koreanName"] or row[3] != "":
                raise ValueError(f"Prepared AI candidate {index} name/comment fields are incorrect.")
            if f"[Did Well]\n{student['didWell']}\n[Needs Improvement]\n{student['needsImprovement']}" != row[4]:
                raise ValueError(f"Prepared AI candidate {index} does not have both exact observation sections.")
        all_data_columns, all_data_rows = rows_for(connection, "speaking_eval_data", "evaluation_id, row_index")
        data_hash = rows_hash(all_data_columns, all_data_rows)
        counts = {table: int(connection.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0]) for table in ("classes", "class_info", "speaking_evaluations", "speaking_eval_data")}
    finally:
        close_workspace(connection, output)
    return {
        "schema": SCHEMA,
        "status": "passed",
        "kind": "speaking-evaluation-ai-batch",
        "workspace": str(output.resolve()),
        "workspaceSha256": sha256_file(output),
        "workspaceBytes": output.stat().st_size,
        "counts": counts,
        "evaluation": {"evaluationId": AI_EVALUATION_ID, "classId": AI_CLASS_ID, "name": AI_EVALUATION_NAME, "grade": "E4", "candidateCount": AI_ROWS},
        "selectionBoundary": {"automaticallySelected": AI_SELECTED_ROWS, "remainingEligible": AI_ROWS - AI_SELECTED_ROWS, "maximumPerPrompt": AI_SELECTED_ROWS},
        "rows": students,
        "baselineSpeakingEvalDataColumns": all_data_columns,
        "baselineSpeakingEvalDataRows": all_data_rows,
        "speakingEvalDataRowsSha256": data_hash,
        "identityRule": "evaluation_id plus row_index; before/after full student row identities retained in snapshots",
    }


def snapshot_staff(connection: sqlite3.Connection) -> dict[str, Any]:
    result: dict[str, Any] = {"kind": "staff-directory", "tables": {}}
    specs = (("nativeEnglish", "native_english_teachers"), ("gsTeam", "gs_team"))
    for key, table in specs:
        columns, raw_rows = rows_for(connection, table, "id")
        if table == "native_english_teachers":
            rows = [
                {"id": int(row[0]), "name": str(row[1] or ""), "position": str(row[2] or ""),
                 "phoneNumber": str(row[3] or ""), "birthday": str(row[4] or ""),
                 "nationality": str(row[5] or ""), "email": str(row[6] or "")}
                for row in raw_rows
            ]
        else:
            rows = [
                {"id": int(row[0]), "name": str(row[1] or ""), "koreanName": str(row[2] or ""),
                 "position": str(row[3] or ""), "phoneNumber": str(row[4] or ""), "birthday": str(row[5] or "")}
                for row in raw_rows
            ]
        result["tables"][key] = {"table": table, "rowCount": len(rows), "rows": rows, "rowsSha256": rows_hash(columns, raw_rows)}
    return result


def snapshot_ai(connection: sqlite3.Connection) -> dict[str, Any]:
    evaluation = connection.execute(
        "SELECT id,class_id,evaluation_name FROM speaking_evaluations WHERE id=?", (AI_EVALUATION_ID,)
    ).fetchone()
    grade = connection.execute("SELECT class_grade FROM class_info WHERE class_id=?", (AI_CLASS_ID,)).fetchone()
    rows = [list(row) for row in connection.execute(
        "SELECT row_index,col_1,col_2,col_9,col_10 FROM speaking_eval_data WHERE evaluation_id=? ORDER BY row_index",
        (AI_EVALUATION_ID,),
    )]
    students = []
    for row in rows:
        students.append({"rowIndex": int(row[0]), "englishName": str(row[1] or ""), "koreanName": str(row[2] or ""), "comment": str(row[3] or ""), "notes": str(row[4] or "")})
    all_data_columns, all_data_rows = rows_for(connection, "speaking_eval_data", "evaluation_id, row_index")
    return {
        "kind": "speaking-evaluation-ai-batch",
        "evaluation": {"id": int(evaluation[0]), "classId": int(evaluation[1]), "name": str(evaluation[2]), "grade": str(grade[0])},
        "rowCount": len(students),
        "students": students,
        "allDataColumns": all_data_columns,
        "allDataRows": all_data_rows,
        "speakingEvalDataRowsSha256": rows_hash(all_data_columns, all_data_rows),
    }


def inspect_workspace(kind: str, phase: str, workspace: Path, output: Path, preparation_receipt: Path) -> dict[str, Any]:
    workspace = workspace.resolve(strict=True)
    preparation = json.loads(preparation_receipt.read_text(encoding="utf-8"))
    if preparation.get("schema") != SCHEMA or preparation.get("status") != "passed" or preparation.get("kind") != kind:
        raise ValueError("Preparation receipt does not match the requested P9-14 fixture kind.")
    if Path(str(workspace)).resolve() != Path(str(preparation.get("workspace"))).resolve():
        raise ValueError("Workspace path differs from the immutable preparation receipt.")
    if phase == "before" and sha256_file(workspace) != preparation.get("workspaceSha256"):
        raise ValueError("Before snapshot workspace bytes differ from the preparation receipt.")
    for suffix in ("-wal", "-shm", "-journal"):
        if Path(str(workspace) + suffix).exists():
            raise ValueError(f"SQLite sidecar remains present at {phase} snapshot: {suffix}")
    connection = sqlite3.connect(f"file:{workspace.as_posix()}?mode=ro", uri=True)
    try:
        connection.execute("PRAGMA query_only = ON")
        result = snapshot_staff(connection) if kind == "staff-directory" else snapshot_ai(connection)
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        foreign_key_errors = [list(row) for row in connection.execute("PRAGMA foreign_key_check")]
    finally:
        connection.close()
    if integrity != "ok" or foreign_key_errors:
        raise ValueError(f"{phase} snapshot integrity failed: integrity={integrity}, foreignKeyErrors={foreign_key_errors}")

    if kind == "staff-directory":
        for key, expected_key in (("nativeEnglish", "nativeEnglish"), ("gsTeam", "gsTeam")):
            actual = result["tables"][key]
            expected = preparation[expected_key]["rows"]
            if actual["rowCount"] != STAFF_ROWS or actual["rows"] != expected:
                raise ValueError(f"{phase} {key} rows differ from the exact 96-row identity fixture.")
    else:
        expected_students = preparation["rows"]
        if result["rowCount"] != AI_ROWS or result["evaluation"] != {
            "id": AI_EVALUATION_ID, "classId": AI_CLASS_ID, "name": AI_EVALUATION_NAME, "grade": "E4"
        }:
            raise ValueError(f"{phase} AI evaluation identity/count differs from the accepted fixture.")
        if len(result["students"]) != len(expected_students):
            raise ValueError(f"{phase} AI candidate count differs from the 100-row fixture.")
        for index, (actual, expected) in enumerate(zip(result["students"], expected_students), 1):
            if (actual["rowIndex"], actual["englishName"], actual["koreanName"]) != (
                expected["rowIndex"], expected["englishName"], expected["koreanName"]
            ):
                raise ValueError(f"{phase} AI row identity changed at candidate {index}.")
            expected_comment = "" if phase == "before" else expected["expectedComment"]
            if actual["comment"] != expected_comment:
                raise ValueError(f"{phase} AI comment state differs at candidate {index}.")
            if actual["notes"] != f"[Did Well]\n{expected['didWell']}\n[Needs Improvement]\n{expected['needsImprovement']}":
                raise ValueError(f"{phase} AI observation fields changed at candidate {index}.")
        expected_all_data_rows = [list(row) for row in preparation["baselineSpeakingEvalDataRows"]]
        if phase == "after":
            for row in expected_all_data_rows:
                if int(row[0]) == AI_EVALUATION_ID and 0 <= int(row[1]) < AI_SELECTED_ROWS:
                    row[11] = expected_students[int(row[1])]["expectedComment"]
        if result["allDataColumns"] != preparation["baselineSpeakingEvalDataColumns"] or result["allDataRows"] != expected_all_data_rows:
            raise ValueError(f"{phase} Speaking Evaluation rows changed outside the 25 accepted comment cells.")
    result.update({"schema": SCHEMA, "status": "passed", "phase": phase, "workspace": str(workspace), "workspaceSha256": sha256_file(workspace), "integrityCheck": integrity, "foreignKeyErrors": foreign_key_errors})
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError(f"Refusing to overwrite snapshot {output}")
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    counts = {key: result["tables"][key]["rowCount"] for key in ("nativeEnglish", "gsTeam")} if kind == "staff-directory" else {"candidates": result["rowCount"]}
    return {"snapshot": str(output), "snapshotSha256": sha256_file(output), "rowCounts": counts}


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    staff = subparsers.add_parser("prepare-staff")
    staff.add_argument("--source-workspace", type=Path, required=True)
    staff.add_argument("--expected-source-sha256", required=True)

    ai = subparsers.add_parser("prepare-ai")
    ai.add_argument("--base-workspace", type=Path, required=True)
    ai.add_argument("--seed-sql", type=Path, required=True)
    ai.add_argument("--expected-base-sha256", required=True)
    ai.add_argument("--expected-seed-sha256", required=True)
    for child in (staff, ai):
        child.add_argument("--output", type=Path, required=True)
        child.add_argument("--receipt", type=Path, required=True)
    snapshot = subparsers.add_parser("snapshot")
    snapshot.add_argument("--kind", choices=("staff-directory", "speaking-evaluation-ai-batch"), required=True)
    snapshot.add_argument("--phase", choices=("before", "after"), required=True)
    snapshot.add_argument("--workspace", type=Path, required=True)
    snapshot.add_argument("--output", type=Path, required=True)
    snapshot.add_argument("--preparation-receipt", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "snapshot":
            response = inspect_workspace(args.kind, args.phase, args.workspace, args.output, args.preparation_receipt)
        elif args.command == "prepare-staff":
            if args.expected_source_sha256.lower() != STAFF_SOURCE_WORKSPACE_SHA256:
                raise ValueError("Staff fixture source argument does not match the pinned P9-14 input identity.")
            receipt = prepare_staff_from_fixture(args.source_workspace, args.output)
            receipt["sourceWorkspaceRelativePath"] = STAFF_SOURCE_WORKSPACE_RELATIVE_PATH
            receipt_path = args.receipt.resolve()
            receipt_path.parent.mkdir(parents=True, exist_ok=True)
            if receipt_path.exists():
                raise ValueError(f"Refusing to overwrite preparation receipt {receipt_path}")
            receipt_path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            response = {
                "schema": receipt["schema"], "status": receipt["status"], "kind": receipt["kind"],
                "workspace": receipt["workspace"], "workspaceSha256": receipt["workspaceSha256"],
                "receipt": str(receipt_path), "receiptSha256": sha256_file(receipt_path),
                "sourceWorkspace": receipt["sourceWorkspace"], "sourceWorkspaceSha256": receipt["sourceWorkspaceSha256"],
                "rowCounts": {"nativeEnglish": STAFF_ROWS, "gsTeam": STAFF_ROWS},
            }
        else:
            base, seed = verify_workspace_base(args.base_workspace, args.seed_sql, args.expected_base_sha256, args.expected_seed_sha256)
            receipt = prepare_ai(base, seed, args.output)
            receipt["sourceBaseWorkspace"] = str(base)
            receipt["sourceBaseWorkspaceSha256"] = sha256_file(base)
            receipt["sourceSeedSql"] = str(seed)
            receipt["sourceSeedSqlSha256"] = sha256_file(seed)
            receipt_path = args.receipt.resolve()
            receipt_path.parent.mkdir(parents=True, exist_ok=True)
            if receipt_path.exists():
                raise ValueError(f"Refusing to overwrite preparation receipt {receipt_path}")
            receipt_path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            response = {
                "schema": receipt["schema"], "status": receipt["status"], "kind": receipt["kind"],
                "workspace": receipt["workspace"], "workspaceSha256": receipt["workspaceSha256"],
                "receipt": str(receipt_path), "receiptSha256": sha256_file(receipt_path),
                "rowCounts": ({"nativeEnglish": STAFF_ROWS, "gsTeam": STAFF_ROWS}
                              if receipt["kind"] == "staff-directory" else {"candidates": AI_ROWS, "initialSelectionBoundary": AI_SELECTED_ROWS}),
            }
        print(json.dumps(response, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"P9-14 workspace operation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
