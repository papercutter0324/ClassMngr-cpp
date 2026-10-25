import hashlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from bake_resources import bake_resources
from generate_resource_catalog import CatalogError, generate_resource_catalog


class ResourceCatalogTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.sources = {
            "resources/assets/icons/sample.svg": b"<svg>icon</svg>\n",
            "resources/assets/translations/ClassMngr_fr_FR.ts": b"<TS>translation</TS>\n",
            "resources/assets/documents/documents.json": b'{"documents": []}\n',
            "resources/assets/documents/Guides/guide.pdf": b"%PDF-1.7\nguide\n",
            "resources/assets/splash/splash.png": b"legacy splash\n",
            "resources/windows/template.rc.in": b"build template\n",
            "src/features/calendar/ui/qml/EventCalendar.qml": b"import QtQuick\nItem {}\n",
        }
        for relative_path, contents in self.sources.items():
            destination = self.root / relative_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(contents)
        self.inventory_path = self.root / "inventory.json"
        self.inventory = self._inventory()
        self._save_inventory()
        self.baked_root = self.root / "build" / "ClassMngrNext" / "Resources"
        self.manifest_path = self.baked_root / "resource-manifest.json"
        self.output_path = self.baked_root.parent / "resource-catalog.json"
        self._bake()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _record(self, logical_id: str, source_path: str, kind: str, disposition: str) -> dict:
        contents = self.sources[source_path]
        return {
            "logical_id": logical_id,
            "source_path": source_path,
            "resource_kind": kind,
            "category": "documents" if "documents/" in source_path else "icons",
            "owner_feature": "Catalog test owner",
            "package_disposition": disposition,
            "source_file": {
                "bytes": len(contents),
                "sha256": hashlib.sha256(contents).hexdigest(),
            },
            "startup_necessity": "not_required_until_request",
            "loading_tier": "feature_on_demand",
            "loading_method": "Load when the owning feature requests this resource.",
            "expected_lifetime": "Owning feature lifetime.",
            "release_policy": "Release when the owning feature closes.",
            "streamability": {"status": "no", "method_ref": "stream_no"},
        }

    def _inventory(self) -> dict:
        return {
            "schema_version": 1,
            "source_snapshot": {
                "tracked_source_tree_ids": {
                    "resources": "a" * 40,
                    "calendar_qml": "b" * 40,
                }
            },
            "document_catalog_metadata": [
                self._record(
                    "document.catalog-metadata",
                    "resources/assets/documents/documents.json",
                    "document_catalog_metadata",
                    "runtime_packaged",
                )
            ],
            "document_bodies": [
                self._record(
                    "document.body:Guides/guide.pdf",
                    "resources/assets/documents/Guides/guide.pdf",
                    "document_body",
                    "runtime_packaged",
                )
            ],
            "resources": [
                self._record(
                    "resource:assets/icons/sample.svg",
                    "resources/assets/icons/sample.svg",
                    "icon",
                    "runtime_packaged",
                ),
                self._record(
                    "resource:assets/translations/ClassMngr_fr_FR.ts",
                    "resources/assets/translations/ClassMngr_fr_FR.ts",
                    "translation_source_for_generated_qm",
                    "runtime_packaged_generated",
                ),
            ],
            "excluded_resources": [
                self._record(
                    "resource:assets/splash/splash.png",
                    "resources/assets/splash/splash.png",
                    "legacy_splash_asset",
                    "excluded_v2",
                )
            ],
            "build_inputs": [
                self._record(
                    "resource:windows/template.rc.in",
                    "resources/windows/template.rc.in",
                    "windows_packaging_template",
                    "build_only_excluded",
                )
            ],
            "calendar_qml_sources": [
                self._record(
                    "calendar.qml:EventCalendar.qml",
                    "src/features/calendar/ui/qml/EventCalendar.qml",
                    "calendar_qml_module_source",
                    "runtime_packaged_generated",
                )
            ],
        }

    def _save_inventory(self) -> None:
        self.inventory_path.write_text(
            json.dumps(self.inventory, indent=2) + "\n", encoding="utf-8"
        )

    def _bake(self) -> dict:
        return bake_resources(self.inventory_path, self.root, self.baked_root)

    def _fresh_bake(self) -> dict:
        shutil.rmtree(self.baked_root, ignore_errors=True)
        return self._bake()

    def _catalog(self, output_path: Path | None = None) -> dict:
        return generate_resource_catalog(
            self.inventory_path,
            self.manifest_path,
            self.baked_root,
            output_path or self.output_path,
        )

    def _load_manifest(self) -> dict:
        return json.loads(self.manifest_path.read_text(encoding="utf-8"))

    def _save_manifest(self, value: dict) -> None:
        self.manifest_path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

    def test_catalog_has_stable_paths_types_and_document_metadata_separation(self) -> None:
        catalog = self._catalog()

        self.assertEqual(catalog["format"], "ClassMngrNext.ResourceCatalog")
        self.assertEqual(catalog["package_path_base"], "ClassMngrNext::Resources")
        self.assertEqual(len(catalog["records"]), 5)
        by_id = {record["logical_id"]: record for record in catalog["records"]}
        self.assertEqual(
            set(by_id),
            {
                "document.catalog-metadata",
                "document.body:Guides/guide.pdf",
                "resource:assets/icons/sample.svg",
                "resource:assets/translations/ClassMngr_fr_FR.ts",
                "calendar.qml:EventCalendar.qml",
            },
        )

        metadata = by_id["document.catalog-metadata"]
        body = by_id["document.body:Guides/guide.pdf"]
        self.assertEqual(metadata["package_relative_path"], "assets/documents/documents.json")
        self.assertEqual(body["package_relative_path"], "assets/documents/Guides/guide.pdf")
        self.assertNotEqual(metadata["logical_id"], body["logical_id"])
        self.assertEqual(body["expected_type"], "pdf")
        self.assertEqual(body["startup_necessity"], "not_required_until_request")
        self.assertEqual(body["loading"]["tier"], "feature_on_demand")

        direct = by_id["resource:assets/icons/sample.svg"]
        self.assertEqual(direct["expected_type"], "svg")
        self.assertEqual(direct["provenance"]["baked"]["bytes"], len(self.sources["resources/assets/icons/sample.svg"]))
        self.assertEqual(
            direct["provenance"]["baked"]["sha256"],
            hashlib.sha256(self.sources["resources/assets/icons/sample.svg"]).hexdigest(),
        )

        translation = by_id["resource:assets/translations/ClassMngr_fr_FR.ts"]
        self.assertEqual(
            translation["package_relative_path"],
            "assets/translations/ClassMngr_fr_FR.qm",
        )
        self.assertEqual(translation["generation"]["status"], "expected_generated_output")
        self.assertNotIn("baked", translation["provenance"])
        self.assertNotIn("bytes", translation["generation"])
        self.assertNotIn("sha256", translation["generation"])

        calendar = by_id["calendar.qml:EventCalendar.qml"]
        self.assertEqual(
            calendar["package_relative_path"],
            "qt/qml/ClassMngr/Calendar/EventCalendar.qml",
        )
        self.assertEqual(calendar["runtime_url"], "qrc:/qt/qml/ClassMngr/Calendar/EventCalendar.qml")
        self.assertEqual(calendar["expected_type"], "qml_module_component")
        self.assertNotIn("baked", calendar["provenance"])

    def test_catalog_bytes_are_deterministic(self) -> None:
        first = self._catalog()
        first_bytes = self.output_path.read_bytes()
        second_path = self.root / "second-catalog.json"
        second = self._catalog(second_path)

        self.assertEqual(first, second)
        self.assertEqual(first_bytes, second_path.read_bytes())

    def test_unknown_kind_duplicate_ids_and_unsafe_inventory_paths_are_rejected(self) -> None:
        self.inventory["resources"][0]["resource_kind"] = "unknown_kind"
        self._save_inventory()
        with self.assertRaisesRegex(CatalogError, "unknown resource_kind"):
            self._catalog()

        self.inventory = self._inventory()
        self.inventory["resources"][0]["logical_id"] = "document.catalog-metadata"
        self._save_inventory()
        with self.assertRaisesRegex(CatalogError, "duplicate logical ID"):
            self._catalog()

        self.inventory = self._inventory()
        self.inventory["resources"][0]["source_path"] = "../escape.svg"
        self._save_inventory()
        with self.assertRaisesRegex(CatalogError, "parent segment"):
            self._catalog()

    def test_duplicate_inventory_source_paths_are_rejected(self) -> None:
        duplicate = dict(self.inventory["resources"][0])
        duplicate["logical_id"] = "resource:assets/icons/other.svg"
        self.inventory["resources"].append(duplicate)
        self._save_inventory()

        with self.assertRaisesRegex(CatalogError, "duplicate source path"):
            self._catalog()

    def test_malformed_or_mismatched_manifest_is_rejected(self) -> None:
        self.manifest_path.write_text("not json", encoding="utf-8")
        with self.assertRaisesRegex(CatalogError, "valid UTF-8 JSON"):
            self._catalog()

        self.manifest_path.write_text('{"format":"one","format":"two"}', encoding="utf-8")
        with self.assertRaisesRegex(CatalogError, "repeats key"):
            self._catalog()

        self._fresh_bake()
        manifest = self._load_manifest()
        manifest["direct_resources"][0]["output_sha256"] = "0" * 64
        self._save_manifest(manifest)
        with self.assertRaisesRegex(CatalogError, "differ from the supplied inventory"):
            self._catalog()

    def test_unsafe_manifest_path_and_duplicate_manifest_id_are_rejected(self) -> None:
        manifest = self._load_manifest()
        manifest["direct_resources"][0]["relative_path"] = "assets/../escape.svg"
        self._save_manifest(manifest)
        with self.assertRaisesRegex(CatalogError, "parent segment"):
            self._catalog()

        self._fresh_bake()
        manifest = self._load_manifest()
        manifest["direct_resources"].append(dict(manifest["direct_resources"][0]))
        self._save_manifest(manifest)
        with self.assertRaisesRegex(CatalogError, "repeats logical ID"):
            self._catalog()

    def test_missing_or_modified_direct_baked_file_is_rejected(self) -> None:
        (self.baked_root / "assets/icons/sample.svg").unlink()
        with self.assertRaisesRegex(CatalogError, "missing baked direct resource"):
            self._catalog()

        self._fresh_bake()
        (self.baked_root / "assets/icons/sample.svg").write_bytes(b"changed")
        with self.assertRaisesRegex(CatalogError, "differs from its manifest"):
            self._catalog()

    def test_generated_output_name_collisions_are_rejected(self) -> None:
        path = "resources/assets/translations/locale/ClassMngr_fr_FR.ts"
        contents = b"<TS>second translation source</TS>\n"
        self.sources[path] = contents
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(contents)
        self.inventory["resources"].append(
            self._record(
                "resource:assets/translations/locale/ClassMngr_fr_FR.ts",
                path,
                "translation_source_for_generated_qm",
                "runtime_packaged_generated",
            )
        )
        self._save_inventory()
        self._fresh_bake()

        with self.assertRaisesRegex(CatalogError, "duplicate or portable-colliding catalog path"):
            self._catalog()

    def test_catalog_output_cannot_overwrite_an_input_or_baked_resource(self) -> None:
        with self.assertRaisesRegex(CatalogError, "cannot replace an input"):
            self._catalog(self.manifest_path)

        direct_path = self.baked_root / "assets/icons/sample.svg"
        with self.assertRaisesRegex(CatalogError, "cannot replace an input"):
            self._catalog(direct_path)


if __name__ == "__main__":
    unittest.main()
