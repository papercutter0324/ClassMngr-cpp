import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from generate_resource_inventory import build_inventory


class ResourceInventoryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def _write(self, path: str, data: bytes) -> str:
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        return path

    def test_catalog_metadata_is_separate_from_document_bodies(self) -> None:
        metadata = self._write(
            "resources/assets/documents/documents.json",
            b'{"documents": []}',
        )
        body = self._write(
            "resources/assets/documents/Guides/guide.pdf",
            b"%PDF-1.7\nbody",
        )

        inventory = build_inventory(
            self.root,
            [metadata, body],
            source_tree_ids={"resources": "test-tree"},
        )

        self.assertEqual(len(inventory["document_catalog_metadata"]), 1)
        self.assertEqual(len(inventory["document_bodies"]), 1)
        self.assertEqual(
            inventory["document_catalog_metadata"][0]["logical_id"],
            "document.catalog-metadata",
        )
        document_body = inventory["document_bodies"][0]
        self.assertEqual(document_body["logical_id"], "document.body:Guides/guide.pdf")
        self.assertEqual(document_body["startup_necessity"], "not_required_until_document_request")
        self.assertEqual(document_body["streamability"]["status"], "candidate_file_backed")

    def test_excluded_inputs_keep_exact_source_hash_and_reason(self) -> None:
        source = self._write(
            "resources/assets/templates/speaking-eval/sources/README.md",
            b"source inputs are excluded",
        )
        splash = self._write(
            "resources/assets/splash/splash.png",
            b"splash-bytes",
        )
        lock = self._write(
            "resources/assets/documents/Speaking Evaluations/~$template.pptx",
            b"lock-bytes",
        )

        inventory = build_inventory(
            self.root,
            [source, splash, lock],
            source_tree_ids={"resources": "test-tree"},
        )
        excluded = {
            record["source_path"]: record
            for record in inventory["excluded_resources"] + inventory["build_inputs"]
        }

        self.assertEqual(excluded[source]["package_disposition"], "build_only_excluded")
        self.assertEqual(excluded[splash]["package_disposition"], "excluded_v2")
        self.assertEqual(excluded[lock]["package_disposition"], "excluded_v2")
        self.assertEqual(
            excluded[source]["source_file"]["sha256"],
            hashlib.sha256(b"source inputs are excluded").hexdigest(),
        )
        self.assertIn("sources/ subtree", excluded[source]["disposition_reason"])
        self.assertEqual(excluded[splash]["installed_size"]["bytes"], None)
        self.assertIn("legacy documents RCC glob includes", excluded[lock]["disposition_reason"])
        self.assertIn("165-byte", excluded[lock]["disposition_reason"])

    def test_dyb_image_maps_to_typed_catalog_without_claiming_a_feature_caller(self) -> None:
        image = self._write("resources/assets/images/dyb.png", b"image-bytes")
        inventory = build_inventory(self.root, [image])

        record = inventory["resources"][0]
        self.assertEqual(record["package_disposition"], "runtime_packaged")
        self.assertIn("ResourcePaths::DynamicImages", record["owner_feature"])
        self.assertIn("no production feature caller found", record["owner_feature"])
        self.assertEqual(
            record["loading_tier"],
            "feature_or_operation_on_demand_if_claimed",
        )
        self.assertIn("ClassMngrNext::Resources", record["v2_migration_destination"])
        self.assertEqual(
            record["startup_necessity"],
            "not_required_no_legacy_feature_callsite",
        )

    def test_calendar_qml_is_listed_as_a_generated_runtime_resource(self) -> None:
        qml = self._write(
            "src/features/calendar/ui/qml/EventCalendar.qml",
            b"import QtQuick\nItem {}\n",
        )

        inventory = build_inventory(
            self.root,
            [],
            [qml],
            source_tree_ids={"calendar_qml": "test-qml-tree"},
        )

        self.assertEqual(len(inventory["calendar_qml_sources"]), 1)
        record = inventory["calendar_qml_sources"][0]
        self.assertEqual(record["package_disposition"], "runtime_packaged_generated")
        self.assertEqual(record["installed_size"]["bytes"], None)
        self.assertEqual(record["source_file"]["bytes"], len(b"import QtQuick\nItem {}\n"))


if __name__ == "__main__":
    unittest.main()
