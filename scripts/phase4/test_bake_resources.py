import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from bake_resources import BakeError, bake_resources


class ResourceBakerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        self.sources = {
            "resources/assets/icons/sample.svg": b"<svg>icon</svg>\n",
            "resources/assets/translations/ClassMngr_fr_FR.ts": b"<TS>translation</TS>\n",
            "resources/assets/documents/documents.json": b'{"documents": []}\n',
            "resources/assets/documents/Guides/guide.pdf": b"%PDF-1.7\nguide\n",
            "resources/assets/splash/splash.png": b"legacy splash\n",
            "resources/assets/images/dyb.png": b"dynamic image\n",
            "resources/assets/files/rosters/Class Register.odt": b"roster odt\n",
            "resources/assets/files/rosters/Roster Template 1 - By Day.ods": b"roster ods\n",
            "resources/assets/files/rosters/Roster Template 1 - By Day.xlsx": b"roster xlsx\n",
            "resources/windows/template.rc.in": b"build template\n",
            "src/features/calendar/ui/qml/EventCalendar.qml": b"import QtQuick\nItem {}\n",
        }
        for relative, data in self.sources.items():
            self._write(relative, data)
        self.inventory_path = self.root / "inventory.json"
        self.output = self.root / "build" / "ClassMngrNext" / "Resources"
        self.inventory = self._inventory()
        self._save_inventory()

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write(self, relative: str, data: bytes) -> Path:
        destination = self.root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        return destination

    def _record(self, logical_id: str, source_path: str, kind: str, disposition: str) -> dict:
        data = self.sources[source_path]
        return {
            "logical_id": logical_id,
            "source_path": source_path,
            "resource_kind": kind,
            "package_disposition": disposition,
            "source_file": {
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            },
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
                ),
                self._record(
                    "resource:assets/images/dyb.png",
                    "resources/assets/images/dyb.png",
                    "optional_dynamic_image",
                    "excluded_v2",
                ),
                self._record(
                    "resource:assets/files/rosters/Class Register.odt",
                    "resources/assets/files/rosters/Class Register.odt",
                    "roster_workflow_file",
                    "excluded_v2",
                ),
                self._record(
                    "resource:assets/files/rosters/Roster Template 1 - By Day.ods",
                    "resources/assets/files/rosters/Roster Template 1 - By Day.ods",
                    "roster_workflow_file",
                    "excluded_v2",
                ),
                self._record(
                    "resource:assets/files/rosters/Roster Template 1 - By Day.xlsx",
                    "resources/assets/files/rosters/Roster Template 1 - By Day.xlsx",
                    "roster_workflow_file",
                    "excluded_v2",
                ),
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
        self.inventory_path.write_text(json.dumps(self.inventory, indent=2) + "\n", encoding="utf-8")

    def _bake(self) -> dict:
        return bake_resources(self.inventory_path, self.root, self.output)

    def _output_snapshot(self) -> dict[str, bytes]:
        return {
            path.relative_to(self.output).as_posix(): path.read_bytes()
            for path in sorted(self.output.rglob("*"))
            if path.is_file()
        }

    def test_success_copies_direct_resources_and_separates_generated_inputs(self) -> None:
        manifest = self._bake()

        self.assertEqual(
            {path for path in self._output_snapshot()},
            {
                "assets/documents/documents.json",
                "assets/documents/Guides/guide.pdf",
                "assets/icons/sample.svg",
                "resource-manifest.json",
            },
        )
        self.assertEqual(
            (self.output / "assets/icons/sample.svg").read_bytes(),
            self.sources["resources/assets/icons/sample.svg"],
        )
        self.assertFalse((self.output / "assets/translations/ClassMngr_fr_FR.ts").exists())
        self.assertFalse((self.output / "assets/splash/splash.png").exists())
        self.assertFalse((self.output / "assets/images/dyb.png").exists())
        self.assertFalse((self.output / "assets/files/rosters").exists())
        self.assertFalse((self.output / "windows/template.rc.in").exists())

        self.assertEqual(manifest["provenance"]["tracked_source_tree_ids"]["resources"], "a" * 40)
        self.assertEqual(len(manifest["direct_resources"]), 3)
        self.assertEqual(len(manifest["generated_runtime_resources"]), 2)
        generated = {item["logical_id"]: item for item in manifest["generated_runtime_resources"]}
        translation = generated["resource:assets/translations/ClassMngr_fr_FR.ts"]
        self.assertEqual(translation["source_bytes"], len(self.sources["resources/assets/translations/ClassMngr_fr_FR.ts"]))
        self.assertIsNone(translation["runtime_relative_path"])
        self.assertEqual(translation["generation_status"], "generated_by_qt_build_not_baked")
        self.assertIsNone(translation["output_bytes"])
        self.assertIsNone(translation["output_sha256"])
        self.assertEqual(
            manifest["disposition_counts"],
            {
                "build_only_excluded": 1,
                "excluded_v2": 5,
                "runtime_packaged": 3,
                "runtime_packaged_generated": 2,
            },
        )

    def test_repeat_bake_is_byte_deterministic_and_replaces_only_owned_output(self) -> None:
        self._bake()
        first = self._output_snapshot()
        self._bake()
        self.assertEqual(first, self._output_snapshot())

    def test_edited_manifest_cannot_authorize_replacing_a_modified_output(self) -> None:
        self._bake()
        modified_output = self.output / "assets/icons/sample.svg"
        changed_bytes = b"user-modified output\n"
        modified_output.write_bytes(changed_bytes)

        manifest_path = self.output / "resource-manifest.json"
        edited_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        edited_entry = next(
            item
            for item in edited_manifest["direct_resources"]
            if item["relative_path"] == "assets/icons/sample.svg"
        )
        edited_entry["output_bytes"] = len(changed_bytes)
        edited_entry["output_sha256"] = hashlib.sha256(changed_bytes).hexdigest()
        manifest_path.write_text(json.dumps(edited_manifest, indent=2) + "\n", encoding="utf-8")

        with self.assertRaisesRegex(BakeError, "manifest differs from the current verified inventory"):
            self._bake()
        self.assertEqual(modified_output.read_bytes(), changed_bytes)
        self.assertEqual(json.loads(manifest_path.read_text(encoding="utf-8")), edited_manifest)

    def test_malformed_inventory_is_rejected(self) -> None:
        self.inventory["resources"][0]["source_file"]["sha256"] = "not-a-hash"
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "invalid source SHA-256"):
            self._bake()
        self.assertFalse(self.output.exists())

        self.inventory = self._inventory()
        self.inventory["schema_version"] = True
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "schema_version 1"):
            self._bake()

    def test_unknown_resource_kind_is_rejected(self) -> None:
        self.inventory["resources"][0]["resource_kind"] = "unrecognized_kind"
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "unknown resource_kind"):
            self._bake()

    def test_path_traversal_is_rejected(self) -> None:
        record = self.inventory["resources"][0]
        record["source_path"] = "resources/assets/../../outside.svg"
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "parent path segment"):
            self._bake()

    def test_duplicate_logical_ids_are_rejected(self) -> None:
        duplicate = self._record(
            "resource:assets/icons/sample.svg",
            "resources/assets/icons/sample.svg",
            "icon",
            "runtime_packaged",
        )
        self.inventory["resources"].append(duplicate)
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "duplicate logical ID"):
            self._bake()

    def test_duplicate_source_path_is_rejected(self) -> None:
        duplicate = self._record(
            "excluded:sample-icon",
            "resources/assets/icons/sample.svg",
            "legacy_splash_asset",
            "excluded_v2",
        )
        self.inventory["excluded_resources"].append(duplicate)
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "duplicate source path"):
            self._bake()

    def test_case_insensitive_destination_collision_is_rejected(self) -> None:
        original_data = self.sources["resources/assets/icons/sample.svg"]
        self.inventory["resources"].append(
            {
                "logical_id": "resource:assets/icons/SAMPLE.svg",
                "source_path": "resources/assets/icons/SAMPLE.svg",
                "resource_kind": "icon",
                "package_disposition": "runtime_packaged",
                "source_file": {
                    "bytes": len(original_data),
                    "sha256": hashlib.sha256(original_data).hexdigest(),
                },
            }
        )
        self._save_inventory()
        with self.assertRaisesRegex(BakeError, "duplicate canonical destination"):
            self._bake()

    def test_missing_or_changed_inventory_sources_are_rejected(self) -> None:
        (self.root / "resources/assets/splash/splash.png").unlink()
        with self.assertRaisesRegex(BakeError, "missing source input"):
            self._bake()

        self.sources["resources/assets/splash/splash.png"] = b"changed splash\n"
        self._write("resources/assets/splash/splash.png", b"changed splash\n")
        with self.assertRaisesRegex(BakeError, "source changed from inventory"):
            self._bake()

    def test_symlink_escape_is_rejected(self) -> None:
        source = self.root / "resources/assets/icons/sample.svg"
        source.unlink()
        outside = self.root.parent / (self.root.name + "-outside.svg")
        outside.write_bytes(self.sources["resources/assets/icons/sample.svg"])
        try:
            source.symlink_to(outside)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"symlink creation is unavailable: {error}")
        try:
            with self.assertRaisesRegex(BakeError, "symlink"):
                self._bake()
        finally:
            outside.unlink(missing_ok=True)

    def test_non_regular_source_is_rejected(self) -> None:
        source = self.root / "resources/assets/icons/sample.svg"
        source.unlink()
        source.mkdir()
        with self.assertRaisesRegex(BakeError, "not a regular file"):
            self._bake()

    def test_output_that_would_overwrite_source_input_is_rejected(self) -> None:
        output = self.root / "resources"
        with self.assertRaisesRegex(BakeError, "overwrite a source input"):
            bake_resources(self.inventory_path, self.root, output)
        self.assertEqual(
            (self.root / "resources/assets/icons/sample.svg").read_bytes(),
            self.sources["resources/assets/icons/sample.svg"],
        )

    def test_nonempty_unowned_output_is_preserved_and_rejected(self) -> None:
        self.output.mkdir(parents=True)
        keep = self.output / "keep.txt"
        keep.write_text("user data", encoding="utf-8")
        with self.assertRaisesRegex(BakeError, "unowned output directory"):
            self._bake()
        self.assertEqual(keep.read_text(encoding="utf-8"), "user data")


if __name__ == "__main__":
    unittest.main()
