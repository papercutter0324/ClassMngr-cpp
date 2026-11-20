#!/usr/bin/env python3
"""Prepare the isolated P9-13 Next speaking-evaluation workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-speaking-evaluation-workspace-preparation-v2"
SEEDED_EVALUATION_NAME = "Heavy Term 01"
UI_EVALUATION_NAME = "Winter"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_sha256(value: Any) -> str:
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def table_rows_hash(connection: sqlite3.Connection, table: str, order_by: str) -> str:
    quoted_table = table.replace('"', '""')
    quoted_order = order_by.replace('"', '""')
    rows = connection.execute(f'SELECT * FROM "{quoted_table}" ORDER BY {quoted_order}').fetchall()
    names = [str(item[1]) for item in connection.execute(f'PRAGMA table_info("{quoted_table}")')]
    return canonical_sha256({"columns": names, "rows": rows})


def prepare(base_workspace: Path, seed_sql: Path, output: Path, expected_base_sha256: str, expected_seed_sha256: str) -> dict[str, Any]:
    base_workspace = base_workspace.resolve(strict=True)
    seed_sql = seed_sql.resolve(strict=True)
    output = output.resolve()
    if output.exists():
        raise ValueError(f"Refusing to overwrite workspace {output}")
    if sha256_file(base_workspace) != expected_base_sha256.lower():
        raise ValueError("Blank workspace bytes differ from the accepted P9-11 master workspace.")
    if sha256_file(seed_sql) != expected_seed_sha256.lower():
        raise ValueError("large_startup.sql bytes differ from the accepted P9-11 seed identity.")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base_workspace.read_bytes())
    try:
        connection = sqlite3.connect(str(output))
        try:
            connection.execute("PRAGMA foreign_keys = ON")
            connection.execute("PRAGMA journal_mode = DELETE")
            connection.executescript(seed_sql.read_text(encoding="utf-8"))

            evaluation_columns = [
                str(item[1])
                for item in connection.execute('PRAGMA table_info("speaking_evaluations")')
            ]
            if evaluation_columns != ["id", "class_id", "evaluation_name"]:
                raise ValueError("speaking_evaluations schema differs from the expected fixture table.")
            evaluation_rows_before = [
                list(row)
                for row in connection.execute(
                    "SELECT * FROM speaking_evaluations ORDER BY id"
                )
            ]
            selected_seed_rows = [
                row for row in evaluation_rows_before if row[0] == 1 and row[1] == 1
            ]
            if len(selected_seed_rows) != 1 or selected_seed_rows[0][2] != SEEDED_EVALUATION_NAME:
                raise ValueError("Selected seed row does not contain Heavy Term 01 for class 1.")
            evaluation_table_sha256_before = canonical_sha256(
                {"columns": evaluation_columns, "rows": evaluation_rows_before}
            )
            connection.commit()
            workspace_sha256_before_mapping = sha256_file(output)

            update = connection.execute(
                "UPDATE speaking_evaluations SET evaluation_name = ? "
                "WHERE id = ? AND class_id = ? AND evaluation_name = ?",
                (UI_EVALUATION_NAME, 1, 1, SEEDED_EVALUATION_NAME),
            )
            if update.rowcount != 1:
                raise ValueError("UI-name mapping did not update exactly the selected evaluation row.")
            evaluation_rows_after = [
                list(row)
                for row in connection.execute(
                    "SELECT * FROM speaking_evaluations ORDER BY id"
                )
            ]
            expected_evaluation_rows_after = [list(row) for row in evaluation_rows_before]
            selected_position = next(
                index
                for index, row in enumerate(expected_evaluation_rows_after)
                if row[0] == 1 and row[1] == 1
            )
            expected_evaluation_rows_after[selected_position][2] = UI_EVALUATION_NAME
            if evaluation_rows_after != expected_evaluation_rows_after:
                raise ValueError("UI-name mapping changed evaluation fields outside the selected name cell.")
            connection.commit()
            evaluation_table_sha256_after = table_rows_hash(
                connection, "speaking_evaluations", "id"
            )
            evaluation_name_mapping = {
                "table": "speaking_evaluations",
                "column": "evaluation_name",
                "evaluationId": 1,
                "classId": 1,
                "beforeValue": SEEDED_EVALUATION_NAME,
                "afterValue": UI_EVALUATION_NAME,
                "updateRowCount": int(update.rowcount),
                "onlySelectedEvaluationNameCellChanged": True,
                "seededTableSha256BeforeMapping": evaluation_table_sha256_before,
                "preparedTableSha256AfterMapping": evaluation_table_sha256_after,
                "workspaceSha256BeforeMapping": workspace_sha256_before_mapping,
            }

            integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
            foreign_key_errors = [list(row) for row in connection.execute("PRAGMA foreign_key_check")]
            counts = {
                table: int(connection.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0])
                for table in (
                    "classes",
                    "roster_columns",
                    "roster_data",
                    "speaking_evaluations",
                    "speaking_eval_data",
                )
            }
            evaluations = [
                {
                    "id": int(row[0]),
                    "classId": int(row[1]),
                    "evaluationName": str(row[2]),
                    "studentRows": int(row[3]),
                }
                for row in connection.execute(
                    "SELECT e.id, e.class_id, e.evaluation_name, COUNT(d.row_index) "
                    "FROM speaking_evaluations e LEFT JOIN speaking_eval_data d "
                    "ON d.evaluation_id = e.id GROUP BY e.id ORDER BY e.id"
                )
            ]
            selected = next((row for row in evaluations if row["id"] == 1), None)
            if (
                integrity != "ok"
                or foreign_key_errors
                or counts != {
                    "classes": 96,
                    "roster_columns": 288,
                    "roster_data": 7200,
                    "speaking_evaluations": 20,
                    "speaking_eval_data": 600,
                }
                or len(evaluations) != 20
                or any(row["studentRows"] != 30 for row in evaluations)
                or selected is None
                or selected["classId"] != 1
                or selected["evaluationName"] != UI_EVALUATION_NAME
                or selected["studentRows"] != 30
            ):
                raise ValueError(
                    "Seeded workspace counts or selected evaluation rows do not match "
                    "the 96-class / 20-evaluation / 600-row fixture contract."
                )
            selected_rows = [
                [str(cell or "") for cell in row]
                for row in connection.execute(
                    "SELECT col_0, col_1, col_2 FROM speaking_eval_data WHERE evaluation_id = 1 "
                    "ORDER BY row_index"
                )
            ]
            expected_source_labels = [f"Student {index:03d}" for index in range(1, 31)]
            expected_report_names = [
                [f"Student {index:03d}", "Heavy Eval 01", f"Score {index:02d}"]
                for index in range(1, 31)
            ]
            if selected_rows != expected_report_names:
                raise ValueError("Selected speaking-evaluation report rows differ from the expected fixture.")
            table_hashes = {
                "speaking_evaluations": table_rows_hash(connection, "speaking_evaluations", "id"),
                "speaking_eval_data": table_rows_hash(connection, "speaking_eval_data", "evaluation_id, row_index"),
            }
        finally:
            connection.close()
    except Exception:
        output.unlink(missing_ok=True)
        for suffix in ("-journal", "-wal", "-shm"):
            Path(str(output) + suffix).unlink(missing_ok=True)
        raise

    for suffix in ("-journal", "-wal", "-shm"):
        Path(str(output) + suffix).unlink(missing_ok=True)
    evaluation_name_mapping["workspaceSha256AfterMapping"] = sha256_file(output)
    return {
        "schema": SCHEMA,
        "status": "passed",
        "sourceBlankWorkspace": str(base_workspace),
        "sourceBlankWorkspaceSha256": sha256_file(base_workspace),
        "sourceSeedSql": str(seed_sql),
        "sourceSeedSqlSha256": sha256_file(seed_sql),
        "workspace": str(output),
        "workspaceBytes": output.stat().st_size,
        "workspaceSha256": sha256_file(output),
        "integrityCheck": integrity,
        "foreignKeyErrors": foreign_key_errors,
        "counts": counts,
        "evaluations": evaluations,
        "evaluationNameUiMapping": evaluation_name_mapping,
        "selectedEvaluation": {
            "evaluationId": selected["id"],
            "classId": selected["classId"],
            "evaluationName": selected["evaluationName"],
            "studentCount": selected["studentRows"],
            "sourceRowLabels": [row[0] for row in selected_rows],
            "reportNames": [f"{row[1]} ({row[2]})" for row in selected_rows],
            "reportEnglishName": "Heavy Eval 01",
            "reportKoreanNames": [row[2] for row in selected_rows],
        },
        "speakingTableSha256": table_hashes,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-workspace", type=Path, required=True)
    parser.add_argument("--seed-sql", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--expected-base-sha256", required=True)
    parser.add_argument("--expected-seed-sha256", required=True)
    options = parser.parse_args()
    try:
        receipt = prepare(
            options.base_workspace,
            options.seed_sql,
            options.output,
            options.expected_base_sha256,
            options.expected_seed_sha256,
        )
        options.receipt.parent.mkdir(parents=True, exist_ok=True)
        options.receipt.write_text(
            json.dumps(receipt, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(json.dumps(receipt, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(f"workspace preparation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
