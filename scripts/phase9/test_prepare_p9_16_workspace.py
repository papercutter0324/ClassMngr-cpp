#!/usr/bin/env python3
"""Focused tests for P9-16 pinned fixture preparation and read-only inspection."""

from __future__ import annotations

import tempfile
import unittest
import importlib.util
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("prepare_p9_16_workspace.py")
SPEC = importlib.util.spec_from_file_location("p916_fixture", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)


REPOSITORY = Path(__file__).resolve().parents[2]
BASE = (
    REPOSITORY
    / "artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1"
    / "workspace-snapshots/master-blank.tps"
)
SEED = REPOSITORY / "tests/fixtures/workspaces/large_startup.sql"


class PrepareP916WorkspaceTests(unittest.TestCase):
    def test_prepare_tiny_profile_from_pinned_current_schema_blank_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "tiny-profile.tps"
            receipt_path = root / "receipt.json"
            receipt = fixture.prepare(BASE, None, workspace, "tiny")
            receipt_path.write_text(
                __import__("json").dumps(receipt, ensure_ascii=False), encoding="utf-8"
            )

            self.assertEqual(receipt["profileSize"], "tiny")
            self.assertIsNone(receipt["seedSqlSha256"])
            self.assertEqual(
                receipt["identity"],
                {"classCount": 0, "classInfoCount": 0, "scheduleEntryCount": 0},
            )
            before = fixture.snapshot("before", workspace, receipt_path)
            after = fixture.snapshot("after", workspace, receipt_path)
            self.assertEqual(before["integrityCheck"], "ok")
            self.assertEqual(after["integrityCheck"], "ok")
            self.assertEqual(before["schemaObjects"], after["schemaObjects"])
            self.assertEqual(before["workspaceTables"], after["workspaceTables"])
            self.assertEqual(before["workspaceTableCount"], 17)

    def test_tiny_profile_rejects_seed_sql(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "derived directly from the pinned"):
                fixture.prepare(BASE, SEED, Path(directory) / "tiny-profile.tps", "tiny")

    def test_prepare_then_before_after_snapshots_preserve_all_rows_and_schema(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "fixture.tps"
            receipt_path = root / "receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace)
            receipt_path.write_text(
                __import__("json").dumps(receipt, ensure_ascii=False), encoding="utf-8"
            )

            self.assertEqual(
                receipt["identity"],
                {"classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768},
            )
            before = fixture.snapshot("before", workspace, receipt_path)
            after = fixture.snapshot("after", workspace, receipt_path)

            self.assertEqual(before["integrityCheck"], "ok")
            self.assertEqual(after["integrityCheck"], "ok")
            self.assertEqual(before["workspaceTables"], after["workspaceTables"])
            self.assertEqual(before["schemaObjects"], after["schemaObjects"])
            self.assertEqual(
                receipt["identity"],
                {"classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768},
            )

    def test_prepare_refuses_to_overwrite_existing_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory) / "fixture.tps"
            workspace.write_bytes(b"preserve this file")
            with self.assertRaisesRegex(ValueError, "Refusing to overwrite"):
                fixture.prepare(BASE, SEED, workspace)
            self.assertEqual(workspace.read_bytes(), b"preserve this file")

    def test_snapshot_fails_closed_when_schema_or_rows_change(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / "fixture.tps"
            receipt_path = root / "receipt.json"
            receipt = fixture.prepare(BASE, SEED, workspace)
            receipt_path.write_text(
                __import__("json").dumps(receipt, ensure_ascii=False), encoding="utf-8"
            )
            connection = __import__("sqlite3").connect(workspace)
            try:
                connection.execute("UPDATE classes SET name=? WHERE id=1", ("Changed fixture",))
                connection.commit()
            finally:
                connection.close()
            with self.assertRaisesRegex(ValueError, "pinned 96-class fixture|prepared baseline"):
                fixture.snapshot("after", workspace, receipt_path)


if __name__ == "__main__":
    unittest.main()
