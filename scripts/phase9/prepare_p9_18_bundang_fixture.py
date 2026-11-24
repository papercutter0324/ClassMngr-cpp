#!/usr/bin/env python3
"""Pin the read-only Bundang map inputs for the P9-18 packaged route."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any


SCHEMA = "classmngr-phase9-p9-18-bundang-map-fixture-v1"
EXPECTED = {
    "map": "assets/campuses/bundang/bundang_map.png",
    "building": "assets/campuses/bundang/bundang_building.png",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"Bundang image is not a valid PNG with an IHDR header: {path}")
    width, height = struct.unpack(">II", header[16:24])
    if width <= 0 or height <= 0:
        raise ValueError(f"Bundang image has invalid dimensions: {path}")
    return width, height


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value


def prepare(repository: Path, package_root: Path, output: Path) -> dict[str, Any]:
    repository = repository.resolve(strict=True)
    package_root = package_root.resolve(strict=True)
    output = output.resolve()
    campus_json = repository / "resources/assets/campuses/bundang.json"
    campus = load_json(campus_json)
    actual_images = campus.get("map", {}).get("images")
    expected_ids = [f":/{EXPECTED['map']}", f":/{EXPECTED['building']}"]
    if campus.get("id") != "bundang" or actual_images != expected_ids:
        raise ValueError(
            "Bundang campus record does not identify the exact ordered map and building images."
        )

    catalog_path = package_root / "resources/resource-catalog.json"
    catalog = load_json(catalog_path)
    records = catalog.get("records")
    if not isinstance(records, list):
        raise ValueError("Packaged resource catalog has no records array.")
    by_id = {str(record.get("logical_id")): record for record in records if isinstance(record, dict)}

    images: list[dict[str, Any]] = []
    for role, relative in EXPECTED.items():
        source_path = repository / "resources" / relative
        package_path = package_root / "resources" / relative
        if not source_path.is_file() or not package_path.is_file():
            raise ValueError(f"Bundang {role} image is missing from source or staged package.")
        source_hash = sha256_file(source_path)
        package_hash = sha256_file(package_path)
        if source_hash != package_hash:
            raise ValueError(f"Packaged Bundang {role} image differs from the current source asset.")
        width, height = read_png_dimensions(source_path)
        logical_id = f"resource:{relative}"
        record = by_id.get(logical_id)
        if record is None:
            raise ValueError(f"Packaged catalog is missing the Bundang {role} resource: {logical_id}")
        if record.get("expected_type") not in {"image", "png"} or record.get("package_relative_path") != relative:
            raise ValueError(f"Catalog identity/type/path is invalid for the Bundang {role} image.")
        provenance = record.get("provenance", {})
        source = provenance.get("source", {}) if isinstance(provenance, dict) else {}
        baked = provenance.get("baked", {}) if isinstance(provenance, dict) else {}
        if (
            source.get("path") != f"resources/{relative}"
            or source.get("sha256") != source_hash
            or baked.get("sha256") != package_hash
        ):
            raise ValueError(f"Packaged catalog source hash does not bind the Bundang {role} image.")
        images.append(
            {
                "role": role,
                "resourceId": logical_id,
                "sourcePath": f"resources/{relative}",
                "packagePath": f"resources/{relative}",
                "sha256": source_hash,
                "sizeBytes": source_path.stat().st_size,
                "widthPixels": width,
                "heightPixels": height,
                "catalogRecord": record,
            }
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError(f"Refusing to overwrite P9-18 fixture receipt: {output}")
    receipt = {
        "schema": SCHEMA,
        "status": "passed",
        "taskId": "P9_18_PACKAGED_NEXT_CAMPUS_MAP_OPEN_CLOSE",
        "kind": "read-only-bundang-campus-map",
        "campusId": "bundang",
        "campusJsonPath": "resources/assets/campuses/bundang.json",
        "campusJsonSha256": sha256_file(campus_json),
        "mapResourceIds": [image["resourceId"] for image in images],
        "images": images,
        "packageRoot": str(package_root),
        "preparedAtUtc": dt.datetime.now(dt.timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z"),
    }
    output.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = prepare(args.repository, args.package_root, args.output)
        print(
            json.dumps(
                {
                    "schema": receipt["schema"],
                    "status": receipt["status"],
                    "campusId": receipt["campusId"],
                    "mapResourceIds": receipt["mapResourceIds"],
                    "imageHashes": [image["sha256"] for image in receipt["images"]],
                    "receipt": str(args.output.resolve()),
                },
                ensure_ascii=False,
            )
        )
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"P9-18 Bundang fixture preparation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
