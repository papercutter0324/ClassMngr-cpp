#!/usr/bin/env python3
"""Generate the deterministic Phase 4 runtime resource catalog.

Catalog paths are POSIX paths relative to ``ClassMngrNext::Resources``. Direct
files retain the baker's ``assets/...`` paths. Generated translations use
``assets/translations/<locale>.qm``. Calendar entries use the existing Qt QML
resource namespace, ``qt/qml/ClassMngr/Calendar/<name>.qml``; their runtime URL
is ``qrc:/`` plus that path. Generated outputs have no claimed baked size or
hash until the Qt build produces them.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import stat
import sys
import tempfile
import unicodedata
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INVENTORY = REPOSITORY_ROOT / "docs/qt-rewrite/phase-4-resource-inventory.json"
MANIFEST_FORMAT = "ClassMngrNext.ResourceBake"
MANIFEST_SCHEMA_VERSION = 1
CATALOG_FORMAT = "ClassMngrNext.ResourceCatalog"
CATALOG_SCHEMA_VERSION = 1
MANIFEST_NAME = "resource-manifest.json"
CATALOG_NAME = "resource-catalog.json"

SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
TREE_ID_PATTERN = re.compile(r"^[0-9a-f]{40}$")
DRIVE_PREFIX_PATTERN = re.compile(r"^[A-Za-z]:")

COLLECTION_RULES: dict[str, dict[str, Any]] = {
    "document_catalog_metadata": {
        "dispositions": {"runtime_packaged"},
        "kinds": {"document_catalog_metadata"},
        "source_prefix": "resources/assets/",
    },
    "document_bodies": {
        "dispositions": {"runtime_packaged"},
        "kinds": {"document_body"},
        "source_prefix": "resources/assets/documents/",
    },
    "resources": {
        "dispositions": {"runtime_packaged", "runtime_packaged_generated"},
        "kinds": {
            "campus_data",
            "campus_map_image",
            "icon",
            "optional_decorative_font",
            "optional_dynamic_image",
            "required_ui_font",
            "roster_workflow_file",
            "speaking_report_artwork",
            "speaking_report_template_metadata",
            "theme_stylesheet",
            "translation_source_for_generated_qm",
        },
        "source_prefix": "resources/assets/",
    },
    "excluded_resources": {
        "dispositions": {"excluded_v2"},
        "kinds": {"legacy_splash_asset", "office_lock_file"},
        "source_prefix": "resources/assets/",
    },
    "build_inputs": {
        "dispositions": {"build_only_excluded"},
        "kinds": {"speaking_report_art_source", "windows_packaging_template"},
        "source_prefix": "resources/",
    },
    "calendar_qml_sources": {
        "dispositions": {"runtime_packaged_generated"},
        "kinds": {"calendar_qml_module_source"},
        "source_prefix": "src/features/calendar/ui/qml/",
    },
}

KIND_SUFFIXES: dict[str, set[str]] = {
    "document_catalog_metadata": {".json"},
    "document_body": {".pdf", ".pptx", ".docx", ".odp"},
    "campus_data": {".json"},
    "campus_map_image": {".png"},
    "icon": {".svg", ".png", ".icns", ".ico"},
    "optional_decorative_font": {".ttf"},
    "optional_dynamic_image": {".png"},
    "required_ui_font": {".ttf", ".ttc"},
    "roster_workflow_file": {".odt", ".ods", ".xlsx"},
    "speaking_report_artwork": {".png"},
    "speaking_report_template_metadata": {".json"},
    "theme_stylesheet": {".qss"},
    "translation_source_for_generated_qm": {".ts"},
    "legacy_splash_asset": {".png"},
    "office_lock_file": {".pptx"},
    "speaking_report_art_source": {".json", ".md", ".png", ".svg"},
    "windows_packaging_template": {".in"},
    "calendar_qml_module_source": {".qml"},
}

DIRECT_EXPECTED_TYPES = {
    ".json": "json",
    ".pdf": "pdf",
    ".pptx": "pptx",
    ".docx": "docx",
    ".odp": "odp",
    ".odt": "odt",
    ".ods": "ods",
    ".xlsx": "xlsx",
    ".svg": "svg",
    ".png": "png",
    ".icns": "icns",
    ".ico": "ico",
    ".ttf": "ttf",
    ".ttc": "ttc",
    ".qss": "stylesheet",
}


class CatalogError(ValueError):
    """Raised when inventory, bake manifest, or baked files are invalid."""


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode(
        "utf-8"
    )


def _checked_relative_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise CatalogError(f"{label} must be a non-empty relative POSIX path")
    if (
        "\\" in value
        or "\x00" in value
        or value.startswith("/")
        or DRIVE_PREFIX_PATTERN.match(value)
    ):
        raise CatalogError(f"{label} is not a canonical relative POSIX path: {value!r}")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise CatalogError(f"{label} contains an empty, current, or parent segment: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value:
        raise CatalogError(f"{label} is not canonical: {value!r}")
    return value


def _portable_path_key(path: str) -> str:
    return unicodedata.normalize("NFC", path).casefold()


def _is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _read_regular_file(path: Path, label: str) -> bytes:
    try:
        info = path.lstat()
    except OSError as error:
        raise CatalogError(f"cannot inspect {label} {path}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CatalogError(f"{label} must be a regular, non-symlink file: {path}")
    try:
        with path.open("rb") as stream:
            opened = os.fstat(stream.fileno())
            if not stat.S_ISREG(opened.st_mode):
                raise CatalogError(f"{label} changed file type while opening: {path}")
            return stream.read()
    except OSError as error:
        raise CatalogError(f"cannot read {label} {path}: {error}") from error


def _unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise CatalogError(f"JSON object repeats key {key!r}")
        result[key] = value
    return result


def _load_json(path: Path, label: str) -> tuple[Any, bytes]:
    raw = _read_regular_file(path, label)
    try:
        return json.loads(raw, object_pairs_hook=_unique_json_object), raw
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CatalogError(f"{label} is not valid UTF-8 JSON: {error}") from error


def _valid_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value):
        raise CatalogError(f"{label} must be a lowercase SHA-256 digest")
    return value


def _valid_size(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise CatalogError(f"{label} must be a non-negative integer")
    return value


def _check_inventory(inventory: Any) -> tuple[list[dict[str, Any]], dict[str, str]]:
    version = inventory.get("schema_version") if isinstance(inventory, dict) else None
    if (
        not isinstance(inventory, dict)
        or isinstance(version, bool)
        or not isinstance(version, int)
        or version != 1
    ):
        raise CatalogError("inventory must be an object with schema_version 1")

    snapshot = inventory.get("source_snapshot")
    tree_ids = snapshot.get("tracked_source_tree_ids") if isinstance(snapshot, dict) else None
    if not isinstance(tree_ids, dict) or set(tree_ids) != {"resources", "calendar_qml"}:
        raise CatalogError("inventory source_snapshot must identify resources and calendar_qml trees")
    checked_tree_ids: dict[str, str] = {}
    for name, value in tree_ids.items():
        if not isinstance(value, str) or not TREE_ID_PATTERN.fullmatch(value):
            raise CatalogError(f"inventory has an invalid {name} Git tree ID")
        checked_tree_ids[name] = value

    records: list[dict[str, Any]] = []
    ids: set[str] = set()
    source_paths: set[str] = set()
    for collection, rules in COLLECTION_RULES.items():
        entries = inventory.get(collection)
        if not isinstance(entries, list):
            raise CatalogError(f"inventory field {collection!r} must be an array")
        for original in entries:
            if not isinstance(original, dict):
                raise CatalogError(f"{collection} entries must be objects")
            logical_id = original.get("logical_id")
            if (
                not isinstance(logical_id, str)
                or not logical_id
                or logical_id != logical_id.strip()
            ):
                raise CatalogError(f"{collection} entry is missing a canonical logical_id")
            source_path = _checked_relative_path(
                original.get("source_path"), f"{collection}.source_path"
            )
            if not source_path.startswith(rules["source_prefix"]):
                raise CatalogError(f"{logical_id!r} source is outside its canonical tree: {source_path}")
            resource_kind = original.get("resource_kind")
            if not isinstance(resource_kind, str) or resource_kind not in rules["kinds"]:
                raise CatalogError(
                    f"{logical_id!r} has unknown resource_kind {resource_kind!r} for {collection}"
                )
            disposition = original.get("package_disposition")
            if not isinstance(disposition, str) or disposition not in rules["dispositions"]:
                raise CatalogError(
                    f"{logical_id!r} has invalid package_disposition {disposition!r}"
                )
            suffix = PurePosixPath(source_path).suffix.lower()
            if suffix not in KIND_SUFFIXES[resource_kind]:
                raise CatalogError(
                    f"{logical_id!r} has unexpected source extension {suffix!r} for {resource_kind}"
                )
            if logical_id in ids:
                raise CatalogError(f"duplicate logical ID: {logical_id}")
            if source_path in source_paths:
                raise CatalogError(f"duplicate source path: {source_path}")
            ids.add(logical_id)
            source_paths.add(source_path)

            source_file = original.get("source_file")
            if not isinstance(source_file, dict):
                raise CatalogError(f"{logical_id!r} is missing source_file metadata")
            source_bytes = _valid_size(source_file.get("bytes"), f"{logical_id} source bytes")
            source_sha256 = _valid_sha256(
                source_file.get("sha256"), f"{logical_id} source SHA-256"
            )

            category = original.get("category")
            owner = original.get("owner_feature")
            tier = original.get("loading_tier")
            method = original.get("loading_method")
            lifetime = original.get("expected_lifetime")
            release_policy = original.get("release_policy")
            startup = original.get("startup_necessity")
            streamability = original.get("streamability")
            if not isinstance(category, str) or not category:
                raise CatalogError(f"{logical_id!r} is missing its resource category")
            if not isinstance(owner, str) or not owner:
                raise CatalogError(f"{logical_id!r} is missing its resource owner")
            for label, value in (
                ("loading_tier", tier),
                ("loading_method", method),
                ("expected_lifetime", lifetime),
                ("release_policy", release_policy),
                ("startup_necessity", startup),
            ):
                if not isinstance(value, str) or not value:
                    raise CatalogError(f"{logical_id!r} is missing {label}")
            if (
                not isinstance(streamability, dict)
                or not isinstance(streamability.get("status"), str)
                or not isinstance(streamability.get("method_ref"), str)
            ):
                raise CatalogError(f"{logical_id!r} has invalid streamability metadata")

            # The baker's canonical logical IDs bind resources to their source paths.
            if collection == "document_catalog_metadata":
                if logical_id != "document.catalog-metadata" or source_path != "resources/assets/documents/documents.json":
                    raise CatalogError("document catalog metadata has a non-canonical ID or path")
            elif collection == "document_bodies":
                expected_id = "document.body:" + source_path[len("resources/assets/documents/") :]
                if logical_id != expected_id:
                    raise CatalogError(f"document body {logical_id!r} does not match its source path")
            elif collection == "resources":
                expected_id = "resource:" + source_path[len("resources/") :]
                if logical_id != expected_id:
                    raise CatalogError(f"resource {logical_id!r} does not match its source path")
            elif collection == "calendar_qml_sources":
                expected_id = "calendar.qml:" + PurePosixPath(source_path).name
                if logical_id != expected_id:
                    raise CatalogError(f"Calendar QML ID {logical_id!r} does not match its source path")

            records.append(
                {
                    "collection": collection,
                    "logical_id": logical_id,
                    "source_path": source_path,
                    "source_bytes": source_bytes,
                    "source_sha256": source_sha256,
                    "resource_kind": resource_kind,
                    "disposition": disposition,
                    "category": category,
                    "owner": owner,
                    "loading_tier": tier,
                    "loading_method": method,
                    "lifetime": lifetime,
                    "release_policy": release_policy,
                    "startup_necessity": startup,
                    "streamability": {
                        "status": streamability["status"],
                        "method_ref": streamability["method_ref"],
                    },
                }
            )

    return sorted(records, key=lambda entry: (entry["source_path"], entry["logical_id"])), checked_tree_ids


def _expected_manifest(inventory_records: list[dict[str, Any]]) -> dict[str, Any]:
    dispositions = Counter(record["disposition"] for record in inventory_records)
    direct: list[dict[str, Any]] = []
    generated: list[dict[str, Any]] = []
    for record in inventory_records:
        common = {
            "logical_id": record["logical_id"],
            "resource_kind": record["resource_kind"],
            "source_relative_path": record["source_path"],
            "source_bytes": record["source_bytes"],
            "source_sha256": record["source_sha256"],
        }
        if record["disposition"] == "runtime_packaged":
            if not record["source_path"].startswith("resources/"):
                raise CatalogError(f"direct resource is outside resources/: {record['source_path']}")
            relative_path = _checked_relative_path(
                record["source_path"][len("resources/") :],
                f"{record['logical_id']} package path",
            )
            direct.append(
                {
                    **common,
                    "relative_path": relative_path,
                    "output_bytes": record["source_bytes"],
                    "output_sha256": record["source_sha256"],
                }
            )
        elif record["disposition"] == "runtime_packaged_generated":
            generated.append(
                {
                    **common,
                    "runtime_relative_path": None,
                    "generation_status": "generated_by_qt_build_not_baked",
                    "output_bytes": None,
                    "output_sha256": None,
                }
            )
    return {
        "direct_resources": direct,
        "generated_runtime_resources": generated,
        "disposition_counts": dict(sorted(dispositions.items())),
    }


def _validate_manifest(
    manifest: Any,
    inventory_raw: bytes,
    inventory_records: list[dict[str, Any]],
    tree_ids: dict[str, str],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    if not isinstance(manifest, dict):
        raise CatalogError("bake manifest must be a JSON object")
    if set(manifest) != {
        "format",
        "schema_version",
        "provenance",
        "layout",
        "disposition_counts",
        "direct_resources",
        "generated_runtime_resources",
    }:
        raise CatalogError("bake manifest has an unexpected schema shape")
    manifest_version = manifest.get("schema_version")
    if (
        manifest.get("format") != MANIFEST_FORMAT
        or isinstance(manifest_version, bool)
        or not isinstance(manifest_version, int)
        or manifest_version != MANIFEST_SCHEMA_VERSION
    ):
        raise CatalogError("bake manifest format/schema version is unsupported")
    provenance = manifest.get("provenance")
    if not isinstance(provenance, dict) or set(provenance) != {
        "inventory_schema_version",
        "inventory_sha256",
        "tracked_source_tree_ids",
        "verified_source_content_sha256",
        "verified_source_count",
    }:
        raise CatalogError("bake manifest provenance is malformed")
    inventory_schema_version = provenance.get("inventory_schema_version")
    if (
        isinstance(inventory_schema_version, bool)
        or not isinstance(inventory_schema_version, int)
        or inventory_schema_version != 1
    ):
        raise CatalogError("bake manifest references an unsupported inventory schema")
    expected_inventory_sha256 = hashlib.sha256(inventory_raw).hexdigest()
    if provenance.get("inventory_sha256") != expected_inventory_sha256:
        raise CatalogError("bake manifest does not match the supplied inventory")
    if provenance.get("tracked_source_tree_ids") != tree_ids:
        raise CatalogError("bake manifest source tree IDs do not match the inventory")
    try:
        verified_source_count = _valid_size(
            provenance.get("verified_source_count"), "bake manifest verified source count"
        )
    except CatalogError as error:
        raise CatalogError("bake manifest verified source count is malformed") from error
    if verified_source_count != len(inventory_records):
        raise CatalogError("bake manifest verified source count does not match the inventory")
    verified_hash = _valid_sha256(
        provenance.get("verified_source_content_sha256"),
        "bake manifest verified source content SHA-256",
    )
    content_records = [
        {
            "source_path": record["source_path"],
            "bytes": record["source_bytes"],
            "sha256": record["source_sha256"],
        }
        for record in inventory_records
    ]
    if verified_hash != hashlib.sha256(_json_bytes(content_records)).hexdigest():
        raise CatalogError("bake manifest source-content provenance does not match the inventory")
    if manifest.get("layout") != {
        "direct_resource_root": "assets",
        "manifest_filename": MANIFEST_NAME,
    }:
        raise CatalogError("bake manifest layout is unsupported")

    expected = _expected_manifest(inventory_records)
    for field in ("direct_resources", "generated_runtime_resources"):
        entries = manifest.get(field)
        if not isinstance(entries, list):
            raise CatalogError(f"bake manifest field {field!r} must be an array")
        seen_ids: set[str] = set()
        seen_paths: set[str] = set()
        portable_paths: set[str] = set()
        for entry in entries:
            if not isinstance(entry, dict):
                raise CatalogError(f"bake manifest {field} entries must be objects")
            logical_id = entry.get("logical_id")
            if not isinstance(logical_id, str) or not logical_id:
                raise CatalogError(f"bake manifest {field} entry has no logical ID")
            if logical_id in seen_ids:
                raise CatalogError(f"bake manifest repeats logical ID: {logical_id}")
            seen_ids.add(logical_id)
            source_path = _checked_relative_path(
                entry.get("source_relative_path"), f"{logical_id} manifest source path"
            )
            _valid_size(entry.get("source_bytes"), f"{logical_id} manifest source bytes")
            _valid_sha256(entry.get("source_sha256"), f"{logical_id} manifest source SHA-256")
            if source_path in seen_paths:
                raise CatalogError(f"bake manifest repeats source path: {source_path}")
            seen_paths.add(source_path)
            if field == "direct_resources":
                package_path = _checked_relative_path(
                    entry.get("relative_path"), f"{logical_id} manifest package path"
                )
                collision_key = _portable_path_key(package_path)
                if collision_key in portable_paths:
                    raise CatalogError(f"bake manifest repeats package path: {package_path}")
                portable_paths.add(package_path)
                _valid_size(entry.get("output_bytes"), f"{logical_id} output bytes")
                _valid_sha256(entry.get("output_sha256"), f"{logical_id} output SHA-256")
            else:
                if entry.get("runtime_relative_path") is not None:
                    raise CatalogError(f"generated resource {logical_id!r} has a baked path")
                if entry.get("generation_status") != "generated_by_qt_build_not_baked":
                    raise CatalogError(f"generated resource {logical_id!r} has an invalid generation status")
                if entry.get("output_bytes") is not None or entry.get("output_sha256") is not None:
                    raise CatalogError(f"generated resource {logical_id!r} claims baked output metadata")
    if manifest.get("direct_resources") != expected["direct_resources"]:
        raise CatalogError("bake manifest direct resources differ from the supplied inventory")
    if manifest.get("generated_runtime_resources") != expected["generated_runtime_resources"]:
        raise CatalogError("bake manifest generated resources differ from the supplied inventory")
    disposition_counts = manifest.get("disposition_counts")
    if not isinstance(disposition_counts, dict):
        raise CatalogError("bake manifest disposition counts must be an object")
    for disposition, count in disposition_counts.items():
        _valid_size(count, f"bake manifest {disposition} disposition count")
    if manifest.get("disposition_counts") != expected["disposition_counts"]:
        raise CatalogError("bake manifest disposition counts differ from the supplied inventory")

    return expected["direct_resources"], expected["generated_runtime_resources"]


def _safe_baked_file(root: Path, relative_path: str) -> Path:
    checked = _checked_relative_path(relative_path, "baked resource path")
    current = root
    parts = checked.split("/")
    for index, part in enumerate(parts):
        current = current / part
        try:
            info = current.lstat()
        except OSError as error:
            raise CatalogError(f"missing baked direct resource {relative_path}: {error}") from error
        if stat.S_ISLNK(info.st_mode):
            raise CatalogError(f"baked resource path traverses a symlink: {relative_path}")
        if index < len(parts) - 1 and not stat.S_ISDIR(info.st_mode):
            raise CatalogError(f"baked resource parent is not a directory: {relative_path}")
        if index == len(parts) - 1 and not stat.S_ISREG(info.st_mode):
            raise CatalogError(f"baked direct resource is not a regular file: {relative_path}")
    try:
        if not _is_relative_to(current.resolve(strict=True), root.resolve(strict=True)):
            raise CatalogError(f"baked resource escapes the requested root: {relative_path}")
    except OSError as error:
        raise CatalogError(f"cannot resolve baked direct resource {relative_path}: {error}") from error
    return current


def _verify_direct_files(root: Path, direct_entries: list[dict[str, Any]]) -> None:
    if root.is_symlink():
        raise CatalogError(f"baked resource root must not be a symlink: {root}")
    try:
        root_info = root.lstat()
    except OSError as error:
        raise CatalogError(f"cannot inspect baked resource root {root}: {error}") from error
    if not stat.S_ISDIR(root_info.st_mode):
        raise CatalogError(f"baked resource root must be a directory: {root}")
    for entry in direct_entries:
        path = _safe_baked_file(root, entry["relative_path"])
        digest = hashlib.sha256()
        size = 0
        try:
            with path.open("rb") as stream:
                opened = os.fstat(stream.fileno())
                if not stat.S_ISREG(opened.st_mode):
                    raise CatalogError(f"baked direct resource changed file type: {entry['relative_path']}")
                while chunk := stream.read(1024 * 1024):
                    size += len(chunk)
                    digest.update(chunk)
        except OSError as error:
            raise CatalogError(f"cannot read baked direct resource {entry['relative_path']}: {error}") from error
        if size != entry["output_bytes"] or digest.hexdigest() != entry["output_sha256"]:
            raise CatalogError(f"baked direct resource differs from its manifest: {entry['relative_path']}")


def _expected_type(record: dict[str, Any]) -> str:
    if record["resource_kind"] == "translation_source_for_generated_qm":
        return "qm"
    if record["resource_kind"] == "calendar_qml_module_source":
        return "qml_module_component"
    suffix = PurePosixPath(record["source_path"]).suffix.lower()
    try:
        return DIRECT_EXPECTED_TYPES[suffix]
    except KeyError as error:
        raise CatalogError(f"no catalog expected type is defined for {record['source_path']}") from error


def _catalog_record(
    record: dict[str, Any],
    direct_by_id: dict[str, dict[str, Any]],
    generated_by_id: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    logical_id = record["logical_id"]
    catalog_record: dict[str, Any] = {
        "logical_id": logical_id,
        "package_relative_path": "",
        "location": "",
        "category": record["category"],
        "resource_kind": record["resource_kind"],
        "expected_type": _expected_type(record),
        "owner": record["owner"],
        "startup_necessity": record["startup_necessity"],
        "loading": {
            "tier": record["loading_tier"],
            "policy": record["loading_method"],
            "lifetime": record["lifetime"],
            "release_policy": record["release_policy"],
        },
        "streamability": record["streamability"],
        "provenance": {
            "source": {
                "path": record["source_path"],
                "bytes": record["source_bytes"],
                "sha256": record["source_sha256"],
            }
        },
    }
    if record["disposition"] == "runtime_packaged":
        entry = direct_by_id[logical_id]
        catalog_record["package_relative_path"] = entry["relative_path"]
        catalog_record["location"] = "resource_file"
        catalog_record["provenance"]["baked"] = {
            "bytes": entry["output_bytes"],
            "sha256": entry["output_sha256"],
        }
    elif record["resource_kind"] == "translation_source_for_generated_qm":
        entry = generated_by_id[logical_id]
        source = PurePosixPath(entry["source_relative_path"])
        output_path = f"assets/translations/{source.stem}.qm"
        catalog_record["package_relative_path"] = _checked_relative_path(
            output_path, f"{logical_id} generated output path"
        )
        catalog_record["location"] = "resource_file"
        catalog_record["generation"] = {
            "status": "expected_generated_output",
            "generator": "qt_linguist",
        }
    elif record["resource_kind"] == "calendar_qml_module_source":
        entry = generated_by_id[logical_id]
        name = PurePosixPath(entry["source_relative_path"]).name
        output_path = f"qt/qml/ClassMngr/Calendar/{name}"
        catalog_record["package_relative_path"] = _checked_relative_path(
            output_path, f"{logical_id} generated output path"
        )
        catalog_record["location"] = "qt_resource"
        catalog_record["runtime_url"] = "qrc:/" + catalog_record["package_relative_path"]
        catalog_record["generation"] = {
            "status": "expected_generated_output",
            "generator": "qt_qml_module",
        }
    else:
        raise CatalogError(f"unsupported runtime generated resource kind: {record['resource_kind']}")
    return catalog_record


def _build_catalog(
    inventory_records: list[dict[str, Any]],
    direct_entries: list[dict[str, Any]],
    generated_entries: list[dict[str, Any]],
) -> dict[str, Any]:
    direct_by_id = {entry["logical_id"]: entry for entry in direct_entries}
    generated_by_id = {entry["logical_id"]: entry for entry in generated_entries}
    runtime_records = [
        record
        for record in inventory_records
        if record["disposition"] in {"runtime_packaged", "runtime_packaged_generated"}
    ]
    catalog_records = [
        _catalog_record(record, direct_by_id, generated_by_id) for record in runtime_records
    ]
    catalog_records.sort(
        key=lambda record: (record["package_relative_path"], record["logical_id"])
    )

    ids: set[str] = set()
    paths: set[str] = set()
    portable_paths: set[str] = set()
    for record in catalog_records:
        logical_id = record["logical_id"]
        package_path = _checked_relative_path(
            record["package_relative_path"], f"{logical_id} package-relative path"
        )
        collision_key = _portable_path_key(package_path)
        if logical_id in ids:
            raise CatalogError(f"duplicate catalog logical ID: {logical_id}")
        if package_path in paths or collision_key in portable_paths:
            raise CatalogError(f"duplicate or portable-colliding catalog path: {package_path}")
        ids.add(logical_id)
        paths.add(package_path)
        portable_paths.add(collision_key)

    expected_runtime_ids = {record["logical_id"] for record in runtime_records}
    if set(direct_by_id) | set(generated_by_id) != expected_runtime_ids:
        raise CatalogError("bake manifest runtime records do not cover the inventory")
    return {
        "format": CATALOG_FORMAT,
        "schema_version": CATALOG_SCHEMA_VERSION,
        "package_path_base": "ClassMngrNext::Resources",
        "path_format": "relative-posix",
        "records": catalog_records,
    }


def _write_catalog(path: Path, value: dict[str, Any]) -> None:
    if path.is_symlink():
        raise CatalogError(f"catalog output must not be a symlink: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not path.is_file():
        raise CatalogError(f"catalog output must be a regular file: {path}")
    raw = _json_bytes(value)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as temporary:
            temporary_name = temporary.name
            temporary.write(raw)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.chmod(temporary_name, 0o644)
        os.replace(temporary_name, path)
    except OSError as error:
        raise CatalogError(f"cannot write resource catalog {path}: {error}") from error
    finally:
        if temporary_name is not None:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass


def generate_resource_catalog(
    inventory_path: Path,
    bake_manifest_path: Path,
    baked_root: Path,
    output_path: Path,
) -> dict[str, Any]:
    """Validate the F1464/F1465 inputs, verify direct outputs, and write the catalog."""
    inventory, inventory_raw = _load_json(inventory_path, "inventory")
    inventory_records, tree_ids = _check_inventory(inventory)
    manifest = _load_json(bake_manifest_path, "bake manifest")[0]
    direct_entries, generated_entries = _validate_manifest(
        manifest,
        inventory_raw,
        inventory_records,
        tree_ids,
    )
    _verify_direct_files(baked_root, direct_entries)
    protected_paths = {
        inventory_path.resolve(strict=False),
        bake_manifest_path.resolve(strict=False),
    }
    protected_paths.update(
        _safe_baked_file(baked_root, entry["relative_path"]).resolve(strict=True)
        for entry in direct_entries
    )
    if output_path.resolve(strict=False) in protected_paths:
        raise CatalogError(f"catalog output cannot replace an input or baked resource: {output_path}")
    catalog = _build_catalog(inventory_records, direct_entries, generated_entries)
    _write_catalog(output_path, catalog)
    return catalog


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--bake-manifest", type=Path, required=True)
    parser.add_argument(
        "--baked-root",
        type=Path,
        help="root containing the manifest's direct relative_path outputs (defaults to manifest parent)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="catalog path (defaults beside the baked Resources directory)",
    )
    args = parser.parse_args(argv)
    baked_root = args.baked_root if args.baked_root is not None else args.bake_manifest.parent
    output = args.output if args.output is not None else baked_root.parent / CATALOG_NAME
    try:
        catalog = generate_resource_catalog(
            args.inventory,
            args.bake_manifest,
            baked_root,
            output,
        )
    except (CatalogError, OSError) as error:
        print(f"resource catalog generation failed: {error}", file=sys.stderr)
        return 1
    generated_count = sum("generation" in record for record in catalog["records"])
    print(
        f"Generated {len(catalog['records'])} runtime resource records "
        f"({len(catalog['records']) - generated_count} direct, {generated_count} generated) "
        f"at {output}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
