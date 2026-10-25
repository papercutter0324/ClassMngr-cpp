import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from audit_resource_stage import StageAuditError, audit_resource_stage


class ResourceStageAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.direct_bytes = b"canonical asset\n"
        self.catalog = {
            "format": "ClassMngrNext.ResourceCatalog",
            "schema_version": 1,
            "package_path_base": "ClassMngrNext::Resources",
            "records": [
                {
                    "logical_id": "resource:assets/icons/sample.svg",
                    "package_relative_path": "assets/icons/sample.svg",
                    "location": "resource_file",
                    "expected_type": "svg",
                    "provenance": {
                        "baked": {
                            "bytes": len(self.direct_bytes),
                            "sha256": hashlib.sha256(self.direct_bytes).hexdigest(),
                        }
                    },
                }
            ]
            + [
                {
                    "logical_id": f"resource:assets/translations/{locale}.ts",
                    "package_relative_path": f"assets/translations/{locale}.qm",
                    "location": "resource_file",
                    "expected_type": "qm",
                    "generation": {"status": "expected_generated_output"},
                }
                for locale in ("en_AU", "en_CA", "en_GB", "en_US", "ko_KR")
            ]
            + [
                {
                    "logical_id": f"calendar.qml:{name}",
                    "package_relative_path": f"qt/qml/ClassMngr/Calendar/{name}",
                    "location": "qt_resource",
                    "runtime_url": f"qrc:/qt/qml/ClassMngr/Calendar/{name}",
                    "expected_type": "qml_module_component",
                }
                for name in ("EventCalendar.qml", "MonthGridDelegate.qml")
            ],
        }
        (self.root / "assets/icons").mkdir(parents=True)
        (self.root / "assets/icons/sample.svg").write_bytes(self.direct_bytes)
        translations = self.root / "assets/translations"
        translations.mkdir(parents=True)
        for locale in ("en_AU", "en_CA", "en_GB", "en_US", "ko_KR"):
            (translations / f"{locale}.qm").write_bytes(f"qm {locale}".encode())
        self._write_catalog()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write_catalog(self) -> None:
        (self.root / "resource-catalog.json").write_text(
            json.dumps(self.catalog, sort_keys=True), encoding="utf-8"
        )

    def test_valid_stage_matches_catalog_hashes_and_qml_keys(self) -> None:
        result = audit_resource_stage(self.root)

        self.assertEqual(result["staged_files"], 7)
        self.assertEqual(result["hash_checked"], 1)
        self.assertEqual(result["generated_qm"], 5)
        self.assertEqual(result["qml_keys"], 2)

    def test_changed_baked_hash_is_rejected(self) -> None:
        (self.root / "assets/icons/sample.svg").write_bytes(b"changed\n")

        with self.assertRaisesRegex(StageAuditError, "bytes|SHA-256"):
            audit_resource_stage(self.root)

    def test_provenance_manifest_is_rejected_as_an_extra_installed_file(self) -> None:
        (self.root / "resource-manifest.json").write_text("{}\n", encoding="utf-8")

        with self.assertRaisesRegex(StageAuditError, "unexpected=.*resource-manifest.json"):
            audit_resource_stage(self.root)


if __name__ == "__main__":
    unittest.main()
