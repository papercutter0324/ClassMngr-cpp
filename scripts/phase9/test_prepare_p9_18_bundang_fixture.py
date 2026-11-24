#!/usr/bin/env python3
"""Focused tests for the read-only P9-18 Bundang fixture receipt."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("prepare_p9_18_bundang_fixture.py")
SPEC = importlib.util.spec_from_file_location("p9_18_fixture", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)

REPOSITORY = SCRIPT.parents[2]
ARTIFACTS = REPOSITORY / "artifacts" / "phase9"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class BundangFixtureTests(unittest.TestCase):
    def make_package(self, root: Path) -> None:
        resources = root / "resources"
        records = []
        for relative in fixture.EXPECTED.values():
            source = REPOSITORY / "resources" / relative
            package = resources / relative
            package.parent.mkdir(parents=True, exist_ok=True)
            package.write_bytes(source.read_bytes())
            digest = sha256(source)
            records.append(
                {
                    "logical_id": f"resource:{relative}",
                    "expected_type": "png",
                    "package_relative_path": relative,
                    "provenance": {
                        "source": {"path": f"resources/{relative}", "sha256": digest},
                        "baked": {"sha256": digest},
                    },
                }
            )
        (resources / "resource-catalog.json").write_text(
            json.dumps({"records": records}), encoding="utf-8"
        )

    def test_receipt_binds_exact_ordered_images_and_catalog_provenance(self) -> None:
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="p9-18-fixture-test-", dir=ARTIFACTS) as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            self.make_package(package)
            output = root / "fixture.json"

            receipt = fixture.prepare(REPOSITORY, package, output)

            self.assertEqual(receipt["schema"], fixture.SCHEMA)
            self.assertEqual(receipt["status"], "passed")
            self.assertEqual(receipt["campusId"], "bundang")
            self.assertEqual(
                receipt["mapResourceIds"],
                [f"resource:{relative}" for relative in fixture.EXPECTED.values()],
            )
            self.assertEqual([image["role"] for image in receipt["images"]], ["map", "building"])
            self.assertEqual(
                [image["sha256"] for image in receipt["images"]],
                [sha256(REPOSITORY / "resources" / relative) for relative in fixture.EXPECTED.values()],
            )
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), receipt)

    def test_modified_package_image_is_rejected(self) -> None:
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="p9-18-fixture-test-", dir=ARTIFACTS) as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            self.make_package(package)
            image = package / "resources" / next(iter(fixture.EXPECTED.values()))
            image.write_bytes(image.read_bytes() + b"tampered")

            with self.assertRaisesRegex(ValueError, "differs from the current source asset"):
                fixture.prepare(REPOSITORY, package, root / "fixture.json")

    def test_existing_receipt_is_never_overwritten(self) -> None:
        ARTIFACTS.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="p9-18-fixture-test-", dir=ARTIFACTS) as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            self.make_package(package)
            output = root / "fixture.json"
            output.write_text("preserve", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "Refusing to overwrite"):
                fixture.prepare(REPOSITORY, package, output)
            self.assertEqual(output.read_text(encoding="utf-8"), "preserve")


if __name__ == "__main__":
    unittest.main()
