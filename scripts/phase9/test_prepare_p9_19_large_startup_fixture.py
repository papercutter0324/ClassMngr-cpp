"""Focused tests for the P9-19 pinned large-startup fixture preparation."""

from __future__ import annotations

import importlib.util
import sqlite3
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

MODULE_PATH = Path(__file__).with_name("prepare_p9_19_large_startup_fixture.py")
SPEC = importlib.util.spec_from_file_location("p919_fixture", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)

REPOSITORY = Path(__file__).resolve().parents[2]
SEED = REPOSITORY / "tests/fixtures/workspaces/large_startup.sql"


class PrepareP919LargeStartupFixtureTests(unittest.TestCase):
    @staticmethod
    def create_synthetic_schema_base(path: Path) -> None:
        statements = """
        CREATE TABLE app_settings (key TEXT PRIMARY KEY, value TEXT);
        CREATE TABLE roster_columns (id INTEGER PRIMARY KEY AUTOINCREMENT, class_id INTEGER, name TEXT, position INTEGER, width INTEGER);
        CREATE TABLE roster_data (class_id INTEGER, row_index INTEGER, col_index INTEGER, value TEXT, PRIMARY KEY (class_id, row_index, col_index));
        CREATE TABLE speaking_evaluations (id INTEGER PRIMARY KEY AUTOINCREMENT, class_id INTEGER, evaluation_name TEXT, UNIQUE(class_id, evaluation_name));
        CREATE TABLE speaking_eval_data (evaluation_id INTEGER, row_index INTEGER, col_0 TEXT, col_1 TEXT, col_2 TEXT, col_3 TEXT, col_4 TEXT, col_5 TEXT, col_6 TEXT, col_7 TEXT, col_8 TEXT, col_9 TEXT, col_10 TEXT, PRIMARY KEY (evaluation_id, row_index));
        CREATE TABLE campuses (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, building_name TEXT, address TEXT, phone_number TEXT, office_number TEXT, transit_steps TEXT, arrival_info TEXT, image_path TEXT, office_wifi TEXT, office_wifi_password TEXT, printer_name TEXT, printer_steps TEXT, photocopier_code TEXT, housing_locations TEXT);
        CREATE TABLE teachers (id INTEGER PRIMARY KEY AUTOINCREMENT, teacher_kr TEXT, teacher_en TEXT, preferred_romanization TEXT, preferred_name TEXT, room_number TEXT, birthday TEXT, phone_number TEXT, wifi_name TEXT, wifi_password TEXT, internet_type TEXT DEFAULT 'WiFi', zoom_id TEXT, zoom_password TEXT, projection_type TEXT DEFAULT 'HDMI', notes TEXT);
        CREATE TABLE native_english_teachers (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, position TEXT, phone_number TEXT, birthday TEXT, nationality TEXT, email TEXT);
        CREATE TABLE gs_team (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, korean_name TEXT, position TEXT, phone_number TEXT, birthday TEXT);
        CREATE TABLE classes (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT);
        CREATE TABLE class_info (class_id INTEGER PRIMARY KEY, teacher_id INTEGER, class_grade TEXT, class_level TEXT, reading_book TEXT, essay_book TEXT, class_color TEXT DEFAULT '#FFFFFF', font_color TEXT DEFAULT '#000000', notes TEXT, time_filler_activities TEXT);
        CREATE TABLE testing_classes (class_id INTEGER PRIMARY KEY, room TEXT NOT NULL);
        CREATE TABLE class_times (id INTEGER PRIMARY KEY AUTOINCREMENT, class_id INTEGER, day TEXT, start_time TEXT, end_time TEXT);
        CREATE TABLE class_intensive_times (id INTEGER PRIMARY KEY AUTOINCREMENT, class_id INTEGER, day TEXT, start_time TEXT, end_time TEXT, UNIQUE(day, start_time));
        CREATE TABLE intensive_slot_states (id INTEGER PRIMARY KEY AUTOINCREMENT, day TEXT, start_time TEXT, state TEXT, UNIQUE(day, start_time));
        CREATE TABLE schedule_testing_blocks (id INTEGER PRIMARY KEY AUTOINCREMENT, day TEXT NOT NULL, start_time TEXT NOT NULL, room TEXT NOT NULL DEFAULT '', class_id INTEGER, UNIQUE(day, start_time));
        CREATE TABLE calendar_events (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL, event_type TEXT DEFAULT 'Other', time_status TEXT DEFAULT 'Timed', repeat_series_id TEXT, all_day INTEGER DEFAULT 0, start_date TEXT, start_time TEXT, end_date TEXT, end_time TEXT);
        """
        connection = sqlite3.connect(path)
        try:
            connection.executescript(statements)
            connection.commit()
        finally:
            connection.close()

    def test_canonical_lf_identity_accepts_windows_checkout_bytes_and_records_both_hashes(self) -> None:
        canonical, fingerprints = fixture.canonical_seed_bytes(SEED)
        self.assertEqual(fingerprints["rawSha256"], fixture.sha256_file(SEED))
        self.assertEqual(fingerprints["canonicalLfSha256"], fixture.SEED_CANONICAL_SHA256)
        self.assertEqual(fingerprints["canonicalLfBytes"], len(canonical))
        self.assertNotIn(b"\r", canonical)
        self.assertEqual(
            fingerprints["lineEndingNormalization"],
            "CRLF-to-LF" if SEED.read_bytes() != canonical else "none",
        )

    def test_prepare_creates_fresh_96_class_workspace_with_pinned_canonical_seed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "synthetic-empty-base.tps"
            self.create_synthetic_schema_base(base)
            workspace = root / "p9-19-workspace.tps"
            receipt_path = root / "fixture-receipt.json"
            p916_receipt_path = root / "p9-16-preparation-receipt.json"
            canonical_seed_path = root / "large_startup.canonical-lf.sql"
            local_base_pin = fixture.sha256_file(base)

            with (
                patch.object(fixture, "BASE_SHA256", local_base_pin),
                patch.object(fixture.p916, "BASE_SHA256", local_base_pin),
            ):
                receipt = fixture.prepare(
                    base,
                    SEED,
                    workspace,
                    receipt_path,
                    p916_receipt_path,
                    canonical_seed_path,
                )
                before = fixture.snapshot("before", workspace, receipt_path)
                after = fixture.snapshot("after", workspace, receipt_path)

            self.assertEqual(receipt["schema"], fixture.SCHEMA)
            self.assertEqual(receipt["baseWorkspaceSha256"], local_base_pin)
            self.assertEqual(receipt["seedSqlRawSha256"], fixture.sha256_file(SEED))
            self.assertEqual(
                receipt["seedSqlCanonicalLfSha256"], fixture.SEED_CANONICAL_SHA256
            )
            self.assertEqual(
                receipt["canonicalSeedSqlSha256"], fixture.SEED_CANONICAL_SHA256
            )
            self.assertEqual(
                receipt["identity"],
                {"classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768},
            )
            self.assertEqual(before["integrityCheck"], "ok")
            self.assertEqual(after["integrityCheck"], "ok")
            self.assertEqual(receipt["appSettingsPreseed"], fixture.APP_SETTINGS_PRESEED)
            expected_settings = sorted(receipt["priorAppSettingsRows"] + [[row["key"], row["value"]] for row in fixture.APP_SETTINGS_PRESEED])
            self.assertEqual(before["workspaceTables"]["app_settings"]["rows"], expected_settings)
            self.assertEqual(before["appSettingsPreseed"], fixture.APP_SETTINGS_PRESEED)
            self.assertEqual(before["workspaceTables"], after["workspaceTables"])
            self.assertEqual(before["schemaObjects"], after["schemaObjects"])
            self.assertFalse(Path(str(workspace) + "-wal").exists())
            self.assertFalse(Path(str(workspace) + "-shm").exists())
            self.assertFalse(Path(str(workspace) + "-journal").exists())

    def test_canonical_seed_rejects_changed_sql_and_bare_carriage_returns(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            changed = root / "changed.sql"
            changed.write_bytes(
                SEED.read_bytes().replace(b"Heavy Fixture Class", b"Altered Fixture Class", 1)
            )
            with self.assertRaisesRegex(ValueError, "strict pinned fingerprint"):
                fixture.canonical_seed_bytes(changed)

            bare_carriage = root / "bare-carriage.sql"
            bare_carriage.write_bytes(SEED.read_bytes().replace(b"\n", b"\r", 1))
            with self.assertRaisesRegex(ValueError, "bare carriage return"):
                fixture.canonical_seed_bytes(bare_carriage)

    def test_prepare_keeps_the_exact_master_blank_pin_strict(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "wrong-base.tps"
            base.write_bytes(b"not the pinned master blank workspace")
            with self.assertRaisesRegex(ValueError, "strict pinned fingerprint"):
                fixture.prepare(
                    base,
                    SEED,
                    root / "workspace.tps",
                    root / "fixture-receipt.json",
                    root / "p9-16-receipt.json",
                    root / "large_startup.canonical-lf.sql",
                )

    def test_prepare_refuses_to_overwrite_any_fixture_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "synthetic-empty-base.tps"
            self.create_synthetic_schema_base(base)
            local_base_pin = fixture.sha256_file(base)
            workspace = root / "workspace.tps"
            workspace.write_bytes(b"preserve")
            with (
                patch.object(fixture, "BASE_SHA256", local_base_pin),
                patch.object(fixture.p916, "BASE_SHA256", local_base_pin),
            ):
                with self.assertRaisesRegex(ValueError, "Refusing to overwrite"):
                    fixture.prepare(
                        base,
                        SEED,
                        workspace,
                        root / "fixture-receipt.json",
                        root / "p9-16-receipt.json",
                        root / "large_startup.canonical-lf.sql",
                    )
            self.assertEqual(workspace.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
