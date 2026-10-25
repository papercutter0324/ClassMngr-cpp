#!/usr/bin/env python3
"""Bake the pinned Phase 4 direct runtime resources into a canonical tree.

The baker is intentionally offline and does not mount resources at runtime.
Direct runtime files are written below ``assets/`` and a deterministic
``resource-manifest.json`` records their provenance. Runtime inputs that need
Qt generation (translations and Calendar QML) are validated and recorded in a
separate manifest section, but are never represented as copied outputs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import stat
import sys
import tempfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INVENTORY = REPOSITORY_ROOT / "docs/qt-rewrite/phase-4-resource-inventory.json"
MANIFEST_NAME = "resource-manifest.json"
MANIFEST_FORMAT = "ClassMngrNext.ResourceBake"
MANIFEST_SCHEMA_VERSION = 1

COLLECTION_DISPOSITIONS = {
    "document_catalog_metadata": {"runtime_packaged"},
    "document_bodies": {"runtime_packaged"},
    "resources": {"runtime_packaged", "runtime_packaged_generated"},
    "excluded_resources": {"excluded_v2"},
    "build_inputs": {"build_only_excluded"},
    "calendar_qml_sources": {"runtime_packaged_generated"},
}
COLLECTION_RESOURCE_KINDS = {
    "document_catalog_metadata": {"document_catalog_metadata"},
    "document_bodies": {"document_body"},
    "resources": {
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
    "excluded_resources": {"legacy_splash_asset", "office_lock_file"},
    "build_inputs": {"speaking_report_art_source", "windows_packaging_template"},
    "calendar_qml_sources": {"calendar_qml_module_source"},
}
TREE_ID_NAMES = {"resources", "calendar_qml"}
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
TREE_ID_PATTERN = re.compile(r"^[0-9a-f]{40}$")
DRIVE_PREFIX_PATTERN = re.compile(r"^[A-Za-z]:")


class BakeError(ValueError):
    """Raised when the inventory, inputs, or requested output are unsafe."""


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode(
        "utf-8"
    )


def _checked_relative_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise BakeError(f"{label} must be a non-empty relative POSIX path")
    if "\\" in value or value.startswith("/") or DRIVE_PREFIX_PATTERN.match(value):
        raise BakeError(f"{label} is not a canonical relative POSIX path: {value!r}")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise BakeError(f"{label} contains an empty, current, or parent path segment: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value:
        raise BakeError(f"{label} is not canonical: {value!r}")
    return value


def _source_relative_path(record: dict[str, Any], collection: str) -> str:
    path = _checked_relative_path(record.get("source_path"), f"{collection}.source_path")
    if collection == "calendar_qml_sources":
        prefix = "src/features/calendar/ui/qml/"
        if not path.startswith(prefix) or not path.endswith(".qml"):
            raise BakeError(f"Calendar QML source has an unexpected canonical path: {path}")
    elif collection == "build_inputs":
        if not path.startswith("resources/"):
            raise BakeError(f"Build input is outside the inventoried resources tree: {path}")
    else:
        if not path.startswith("resources/assets/"):
            raise BakeError(f"Resource source is outside resources/assets/: {path}")
    return path


def _check_record(record: Any, collection: str) -> dict[str, Any]:
    if not isinstance(record, dict):
        raise BakeError(f"{collection} entries must be objects")
    logical_id = record.get("logical_id")
    if (
        not isinstance(logical_id, str)
        or not logical_id.strip()
        or logical_id != logical_id.strip()
    ):
        raise BakeError(f"{collection} entry is missing a logical_id")
    resource_kind = record.get("resource_kind")
    if (
        not isinstance(resource_kind, str)
        or resource_kind not in COLLECTION_RESOURCE_KINDS[collection]
    ):
        raise BakeError(f"{logical_id!r} has unknown resource_kind {resource_kind!r} for {collection}")
    disposition = record.get("package_disposition")
    if disposition not in COLLECTION_DISPOSITIONS[collection]:
        expected = ", ".join(sorted(COLLECTION_DISPOSITIONS[collection]))
        raise BakeError(
            f"{collection} entry {logical_id!r} has invalid package_disposition "
            f"{disposition!r}; expected {expected}"
        )
    source_path = _source_relative_path(record, collection)
    source_file = record.get("source_file")
    if not isinstance(source_file, dict):
        raise BakeError(f"{logical_id!r} is missing source_file metadata")
    size = source_file.get("bytes")
    digest = source_file.get("sha256")
    if isinstance(size, bool) or not isinstance(size, int) or size < 0:
        raise BakeError(f"{logical_id!r} has an invalid source byte count")
    if not isinstance(digest, str) or not SHA256_PATTERN.fullmatch(digest):
        raise BakeError(f"{logical_id!r} has an invalid source SHA-256")

    if collection == "document_catalog_metadata":
        if logical_id != "document.catalog-metadata" or source_path != "resources/assets/documents/documents.json":
            raise BakeError("document catalog metadata does not use its canonical identity and path")
    elif collection == "document_bodies":
        prefix = "resources/assets/documents/"
        if not source_path.startswith(prefix) or logical_id != "document.body:" + source_path[len(prefix) :]:
            raise BakeError(f"document body {logical_id!r} does not match its canonical source path")
    elif collection == "resources":
        if not logical_id.startswith("resource:"):
            raise BakeError(f"resource entry has a non-canonical logical ID: {logical_id!r}")
        if logical_id[len("resource:") :] != source_path[len("resources/") :]:
            raise BakeError(f"resource ID {logical_id!r} does not match its canonical source path")
    if collection == "resources":
        if disposition == "runtime_packaged_generated":
            if (
                resource_kind != "translation_source_for_generated_qm"
                or not source_path.endswith(".ts")
                or not source_path.startswith("resources/assets/translations/")
            ):
                raise BakeError(f"generated resource has an unexpected canonical file type: {source_path}")
        elif resource_kind == "translation_source_for_generated_qm" or source_path.endswith(".ts"):
            raise BakeError(f"translation source must be recorded as generated runtime input: {source_path}")
    elif collection == "calendar_qml_sources":
        if not logical_id.startswith("calendar.qml:"):
            raise BakeError(f"Calendar QML entry has a non-canonical logical ID: {logical_id!r}")
        if logical_id[len("calendar.qml:") :] != PurePosixPath(source_path).name:
            raise BakeError(f"Calendar QML ID {logical_id!r} does not match its source filename")
    if collection == "calendar_qml_sources" and resource_kind != "calendar_qml_module_source":
        raise BakeError(f"Calendar QML entry has an unexpected file type: {source_path}")

    return {
        "collection": collection,
        "logical_id": logical_id,
        "source_path": source_path,
        "source_bytes": size,
        "source_sha256": digest,
        "disposition": disposition,
        "resource_kind": resource_kind,
        "record": record,
    }


def _load_inventory(inventory_path: Path) -> tuple[dict[str, Any], bytes, Path]:
    if inventory_path.is_symlink():
        raise BakeError(f"inventory must not be a symlink: {inventory_path}")
    try:
        inventory_file = inventory_path.resolve(strict=True)
    except OSError as error:
        raise BakeError(f"cannot resolve inventory {inventory_path}: {error}") from error
    try:
        info = inventory_file.lstat()
    except OSError as error:
        raise BakeError(f"cannot inspect inventory {inventory_file}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise BakeError(f"inventory must be a regular file: {inventory_file}")
    try:
        raw = inventory_file.read_bytes()
        inventory = json.loads(raw)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BakeError(f"cannot read valid JSON inventory {inventory_file}: {error}") from error
    schema_version = inventory.get("schema_version") if isinstance(inventory, dict) else None
    if (
        not isinstance(inventory, dict)
        or isinstance(schema_version, bool)
        or not isinstance(schema_version, int)
        or schema_version != 1
    ):
        raise BakeError("inventory must be an object with schema_version 1")
    snapshot = inventory.get("source_snapshot")
    tree_ids = snapshot.get("tracked_source_tree_ids") if isinstance(snapshot, dict) else None
    if not isinstance(tree_ids, dict) or set(tree_ids) != TREE_ID_NAMES:
        raise BakeError("inventory source_snapshot must identify resources and calendar_qml trees")
    for name, tree_id in tree_ids.items():
        if not isinstance(tree_id, str) or not TREE_ID_PATTERN.fullmatch(tree_id):
            raise BakeError(f"inventory has an invalid {name} Git tree ID")
    for collection in COLLECTION_DISPOSITIONS:
        records = inventory.get(collection)
        if not isinstance(records, list):
            raise BakeError(f"inventory field {collection!r} must be an array")
    return inventory, raw, inventory_file


def _inventory_records(inventory: dict[str, Any]) -> list[dict[str, Any]]:
    checked: list[dict[str, Any]] = []
    ids: set[str] = set()
    source_paths: set[str] = set()
    destinations: set[str] = set()
    portable_destinations: set[str] = set()
    for collection in COLLECTION_DISPOSITIONS:
        for original in inventory[collection]:
            record = _check_record(original, collection)
            logical_id = record["logical_id"]
            source_path = record["source_path"]
            if logical_id in ids:
                raise BakeError(f"duplicate logical ID: {logical_id}")
            if source_path in source_paths:
                raise BakeError(f"duplicate source path: {source_path}")
            ids.add(logical_id)
            source_paths.add(source_path)

            if record["disposition"] == "runtime_packaged":
                relative_path = source_path[len("resources/") :]
                _checked_relative_path(relative_path, f"{logical_id} destination")
                collision_key = _portable_path_key(relative_path)
                if relative_path in destinations or collision_key in portable_destinations:
                    raise BakeError(f"duplicate canonical destination: {relative_path}")
                destinations.add(relative_path)
                portable_destinations.add(collision_key)
                record["relative_path"] = relative_path
            elif record["disposition"] == "runtime_packaged_generated":
                record["relative_path"] = None
            else:
                record["relative_path"] = None
            checked.append(record)
    return sorted(checked, key=lambda entry: (entry["source_path"], entry["logical_id"]))


def _portable_path_key(path: str) -> str:
    # Case-folding and NFC normalization catch collisions on common
    # case-insensitive and Unicode-normalizing filesystems before packaging.
    import unicodedata

    return unicodedata.normalize("NFC", path).casefold()


def _read_source(source_root: Path, relative_path: str) -> bytes:
    parts = relative_path.split("/")
    current = source_root
    for index, part in enumerate(parts):
        current = current / part
        try:
            info = current.lstat()
        except OSError as error:
            raise BakeError(f"missing source input {relative_path}: {error}") from error
        if stat.S_ISLNK(info.st_mode):
            raise BakeError(f"source input traverses a symlink: {relative_path}")
        if index < len(parts) - 1 and not stat.S_ISDIR(info.st_mode):
            raise BakeError(f"source input parent is not a directory: {relative_path}")
    try:
        resolved = current.resolve(strict=True)
    except OSError as error:
        raise BakeError(f"cannot resolve source input {relative_path}: {error}") from error
    if not _is_relative_to(resolved, source_root):
        raise BakeError(f"source input escapes source root: {relative_path}")
    try:
        info = current.lstat()
        if not stat.S_ISREG(info.st_mode):
            raise BakeError(f"source input is not a regular file: {relative_path}")
        with current.open("rb") as stream:
            opened_info = os.fstat(stream.fileno())
            if not stat.S_ISREG(opened_info.st_mode):
                raise BakeError(f"source input changed file type while opening: {relative_path}")
            return stream.read()
    except OSError as error:
        raise BakeError(f"cannot read source input {relative_path}: {error}") from error


def _is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _verify_sources(source_root: Path, records: list[dict[str, Any]]) -> str:
    content_records = []
    for record in records:
        data = _read_source(source_root, record["source_path"])
        size = len(data)
        digest = hashlib.sha256(data).hexdigest()
        if size != record["source_bytes"] or digest != record["source_sha256"]:
            raise BakeError(
                f"source changed from inventory for {record['logical_id']!r} "
                f"({record['source_path']})"
            )
        content_records.append(
            {
                "source_path": record["source_path"],
                "bytes": size,
                "sha256": digest,
            }
        )
    aggregate = hashlib.sha256(_json_bytes(content_records)).hexdigest()
    return aggregate


def _canonical_output(output_dir: Path) -> Path:
    target = output_dir.resolve(strict=False)
    if target == Path(target.anchor):
        raise BakeError("refusing to use a filesystem root as the output directory")
    if target.exists() and not target.is_dir():
        raise BakeError(f"output path is not a directory: {target}")
    return target


def _check_output_collisions(
    output_root: Path,
    source_root: Path,
    records: list[dict[str, Any]],
    inventory_file: Path,
) -> None:
    target_paths = [
        output_root / record["relative_path"]
        for record in records
        if record["disposition"] == "runtime_packaged"
    ]
    target_paths.append(output_root / MANIFEST_NAME)
    protected = {inventory_file}
    for record in records:
        protected.add((source_root / Path(*PurePosixPath(record["source_path"]).parts)).resolve(strict=False))
    for path in target_paths:
        canonical = path.resolve(strict=False)
        if canonical in protected:
            raise BakeError(f"output would overwrite a source input: {canonical}")


def _expected_manifest(
    inventory: dict[str, Any],
    inventory_raw: bytes,
    records: list[dict[str, Any]],
    source_content_sha256: str,
) -> dict[str, Any]:
    direct_entries = []
    generated_entries = []
    dispositions = Counter(record["disposition"] for record in records)
    for record in records:
        common = {
            "logical_id": record["logical_id"],
            "resource_kind": record["resource_kind"],
            "source_relative_path": record["source_path"],
            "source_bytes": record["source_bytes"],
            "source_sha256": record["source_sha256"],
        }
        if record["disposition"] == "runtime_packaged":
            direct_entries.append(
                {
                    **common,
                    "relative_path": record["relative_path"],
                    "output_bytes": record["source_bytes"],
                    "output_sha256": record["source_sha256"],
                }
            )
        elif record["disposition"] == "runtime_packaged_generated":
            generated_entries.append(
                {
                    **common,
                    "runtime_relative_path": None,
                    "generation_status": "generated_by_qt_build_not_baked",
                    "output_bytes": None,
                    "output_sha256": None,
                }
            )
    return {
        "format": MANIFEST_FORMAT,
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "provenance": {
            "inventory_schema_version": inventory["schema_version"],
            "inventory_sha256": hashlib.sha256(inventory_raw).hexdigest(),
            "tracked_source_tree_ids": inventory["source_snapshot"]["tracked_source_tree_ids"],
            "verified_source_content_sha256": source_content_sha256,
            "verified_source_count": len(records),
        },
        "layout": {
            "direct_resource_root": "assets",
            "manifest_filename": MANIFEST_NAME,
        },
        "disposition_counts": dict(sorted(dispositions.items())),
        "direct_resources": direct_entries,
        "generated_runtime_resources": generated_entries,
    }


def _actual_output_tree(output_root: Path) -> tuple[set[str], set[str]]:
    files: set[str] = set()
    directories: set[str] = set()
    for current, dirs, names in os.walk(output_root, followlinks=False):
        current_path = Path(current)
        relative_dir = current_path.relative_to(output_root)
        for name in list(dirs):
            path = current_path / name
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
                raise BakeError(f"existing output contains a symlink or special directory: {path}")
            rel = (relative_dir / name).as_posix()
            directories.add(rel)
        for name in names:
            path = current_path / name
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
                raise BakeError(f"existing output contains a symlink or special file: {path}")
            files.add((relative_dir / name).as_posix())
    return files, directories


def _check_reusable_output(output_root: Path, expected_manifest: dict[str, Any]) -> None:
    if not output_root.exists():
        return
    if not output_root.is_dir():
        raise BakeError(f"existing output is not a directory: {output_root}")
    files, directories = _actual_output_tree(output_root)
    if not files and not directories:
        return
    if MANIFEST_NAME not in files:
        raise BakeError(f"refusing to replace non-empty, unowned output directory: {output_root}")
    try:
        manifest_bytes = (output_root / MANIFEST_NAME).read_bytes()
    except OSError as error:
        raise BakeError(f"cannot validate existing output manifest: {error}") from error
    if manifest_bytes != _json_bytes(expected_manifest):
        raise BakeError(
            "refusing to replace output whose manifest differs from the current "
            f"verified inventory: {output_root}"
        )
    expected_files = {MANIFEST_NAME}
    expected_directories: set[str] = set()
    output_hashes: dict[str, tuple[int, str]] = {}
    for entry in expected_manifest["direct_resources"]:
        if not isinstance(entry, dict):
            raise BakeError("existing output manifest contains a malformed direct resource")
        relative = _checked_relative_path(entry.get("relative_path"), "existing manifest relative_path")
        if relative in expected_files:
            raise BakeError(f"existing output manifest repeats destination: {relative}")
        expected_files.add(relative)
        size = entry.get("output_bytes")
        digest = entry.get("output_sha256")
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise BakeError(f"existing output manifest has invalid byte count: {relative}")
        if not isinstance(digest, str) or not SHA256_PATTERN.fullmatch(digest):
            raise BakeError(f"existing output manifest has invalid SHA-256: {relative}")
        output_hashes[relative] = (size, digest)
        parent = PurePosixPath(relative).parent
        while parent.as_posix() != ".":
            expected_directories.add(parent.as_posix())
            parent = parent.parent
    if files != expected_files or directories != expected_directories:
        raise BakeError(f"existing output contains untracked or missing files: {output_root}")
    for relative, (size, digest) in output_hashes.items():
        data = (output_root / Path(*PurePosixPath(relative).parts)).read_bytes()
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise BakeError(f"existing baked file was modified: {relative}")


def _write_stage(stage: Path, manifest: dict[str, Any], source_root: Path, records: list[dict[str, Any]]) -> None:
    direct_records = [record for record in records if record["disposition"] == "runtime_packaged"]
    for record in direct_records:
        relative = record["relative_path"]
        destination = stage.joinpath(*PurePosixPath(relative).parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        current = stage
        for component in PurePosixPath(relative).parts[:-1]:
            current = current / component
            os.chmod(current, 0o755)
        hasher = hashlib.sha256()
        written = 0
        try:
            with (source_root / Path(*PurePosixPath(record["source_path"]).parts)).open("rb") as source:
                with destination.open("xb") as output:
                    while True:
                        chunk = source.read(1024 * 1024)
                        if not chunk:
                            break
                        if output.write(chunk) != len(chunk):
                            raise BakeError(f"short write while baking {record['source_path']}")
                        hasher.update(chunk)
                        written += len(chunk)
                    output.flush()
                    os.fsync(output.fileno())
        except OSError as error:
            raise BakeError(f"cannot copy {record['source_path']} to staged output: {error}") from error
        if written != record["source_bytes"] or hasher.hexdigest() != record["source_sha256"]:
            raise BakeError(f"source changed while baking {record['source_path']}")
        try:
            output_info = destination.lstat()
            if not stat.S_ISREG(output_info.st_mode):
                raise BakeError(f"staged output is not a regular file: {relative}")
            output_hasher = hashlib.sha256()
            output_bytes = 0
            with destination.open("rb") as staged_file:
                while True:
                    chunk = staged_file.read(1024 * 1024)
                    if not chunk:
                        break
                    output_hasher.update(chunk)
                    output_bytes += len(chunk)
        except OSError as error:
            raise BakeError(f"cannot verify staged output {relative}: {error}") from error
        if output_bytes != record["source_bytes"] or output_hasher.hexdigest() != record["source_sha256"]:
            raise BakeError(f"staged output failed byte/hash validation: {relative}")
        os.chmod(destination, 0o644)

    manifest_path = stage / MANIFEST_NAME
    manifest_path.write_bytes(_json_bytes(manifest))
    os.chmod(manifest_path, 0o644)
    os.chmod(stage, 0o755)


def _replace_output(stage: Path, output_root: Path, expected_manifest: dict[str, Any]) -> None:
    if not output_root.exists():
        os.replace(stage, output_root)
        return
    _check_reusable_output(output_root, expected_manifest)
    if not any(output_root.iterdir()):
        output_root.rmdir()
        os.replace(stage, output_root)
        return

    backup = output_root.with_name(f".{output_root.name}.previous-{os.getpid()}")
    if backup.exists():
        raise BakeError(f"refusing to replace an existing backup path: {backup}")
    os.replace(output_root, backup)
    try:
        os.replace(stage, output_root)
    except OSError:
        os.replace(backup, output_root)
        raise
    shutil.rmtree(backup)


def bake_resources(inventory_path: Path, source_root: Path, output_dir: Path) -> dict[str, Any]:
    """Validate and atomically create a direct-resource tree and manifest."""
    try:
        root = source_root.resolve(strict=True)
    except OSError as error:
        raise BakeError(f"cannot resolve source root {source_root}: {error}") from error
    if not root.is_dir():
        raise BakeError(f"source root is not a directory: {root}")
    inventory, inventory_raw, inventory_file = _load_inventory(inventory_path)
    records = _inventory_records(inventory)
    source_content_sha256 = _verify_sources(root, records)
    output_root = _canonical_output(output_dir)
    _check_output_collisions(output_root, root, records, inventory_file)
    manifest = _expected_manifest(inventory, inventory_raw, records, source_content_sha256)
    _check_reusable_output(output_root, manifest)

    output_root.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=f".{output_root.name}.bake-", dir=output_root.parent))
    try:
        _write_stage(stage, manifest, root, records)
        _replace_output(stage, output_root, manifest)
    except Exception:
        if stage.exists():
            shutil.rmtree(stage)
        raise
    return manifest


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output_dir",
        type=Path,
        help="explicit directory to replace/create as the canonical baked resource tree",
    )
    parser.add_argument(
        "--inventory",
        type=Path,
        default=DEFAULT_INVENTORY,
        help=f"inventory JSON (default: {DEFAULT_INVENTORY})",
    )
    parser.add_argument(
        "--source-root",
        type=Path,
        default=REPOSITORY_ROOT,
        help=f"root against which inventory source_path values resolve (default: {REPOSITORY_ROOT})",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = _argument_parser().parse_args(argv)
    try:
        manifest = bake_resources(args.inventory, args.source_root, args.output_dir)
    except (BakeError, OSError) as error:
        print(f"resource bake failed: {error}", file=sys.stderr)
        return 2
    print(
        f"Baked {len(manifest['direct_resources'])} direct resources; "
        f"recorded {len(manifest['generated_runtime_resources'])} generated runtime inputs."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
