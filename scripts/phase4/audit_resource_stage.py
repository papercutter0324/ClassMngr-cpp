#!/usr/bin/env python3
"""Audit a staged ClassMngrNext resource component against its catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import stat
import sys
from pathlib import Path, PurePosixPath
from typing import Any


class StageAuditError(ValueError):
    """Raised when the staged component differs from its catalog contract."""


def _read_catalog(path: Path) -> dict[str, Any]:
    try:
        info = path.lstat()
    except OSError as error:
        raise StageAuditError(f"cannot inspect resource catalog {path}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise StageAuditError(f"resource catalog must be a regular, non-symlink file: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise StageAuditError(f"cannot read valid resource catalog {path}: {error}") from error
    if (
        not isinstance(value, dict)
        or value.get("format") != "ClassMngrNext.ResourceCatalog"
        or value.get("schema_version") != 1
        or value.get("package_path_base") != "ClassMngrNext::Resources"
        or not isinstance(value.get("records"), list)
    ):
        raise StageAuditError("catalog has an unsupported format or schema")
    return value


def _package_path(value: Any, logical_id: str) -> str:
    if not isinstance(value, str) or not value:
        raise StageAuditError(f"{logical_id} has no package-relative path")
    if "\\" in value or value.startswith("/") or "\x00" in value:
        raise StageAuditError(f"{logical_id} has a non-canonical package path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or path.as_posix() != value or any(
        part in ("", ".", "..") for part in value.split("/")
    ):
        raise StageAuditError(f"{logical_id} has an unsafe package path: {value!r}")
    return value


def _read_payload_file(root: Path, relative_path: str) -> bytes:
    current = root
    for part in PurePosixPath(relative_path).parts:
        current = current / part
        try:
            info = current.lstat()
        except OSError as error:
            raise StageAuditError(f"missing staged resource {relative_path}: {error}") from error
        if stat.S_ISLNK(info.st_mode):
            raise StageAuditError(f"staged resource path traverses a symlink: {relative_path}")
    if not stat.S_ISREG(info.st_mode):
        raise StageAuditError(f"staged resource is not a regular file: {relative_path}")
    try:
        return current.read_bytes()
    except OSError as error:
        raise StageAuditError(f"cannot read staged resource {relative_path}: {error}") from error


def _resource_files(root: Path) -> set[str]:
    found: set[str] = set()
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        try:
            info = path.lstat()
        except OSError as error:
            raise StageAuditError(f"cannot inspect staged path {relative}: {error}") from error
        if stat.S_ISLNK(info.st_mode):
            raise StageAuditError(f"staged resource root contains a symlink: {relative}")
        if stat.S_ISREG(info.st_mode):
            found.add(relative)
        elif not stat.S_ISDIR(info.st_mode):
            raise StageAuditError(f"staged resource root contains a non-file entry: {relative}")
    return found


def audit_resource_stage(root: Path) -> dict[str, int]:
    """Check exact staged file layout, baked hashes, generated QMs, and QML keys."""
    try:
        root = root.resolve(strict=True)
    except OSError as error:
        raise StageAuditError(f"cannot resolve staged resource root {root}: {error}") from error
    if not root.is_dir():
        raise StageAuditError(f"staged resource root is not a directory: {root}")

    catalog_relative_path = "resource-catalog.json"
    catalog = _read_catalog(root / catalog_relative_path)
    expected_files = {catalog_relative_path}
    hash_checked = 0
    generated_qm_count = 0
    qml_keys: set[str] = set()
    logical_ids: set[str] = set()
    package_paths: set[str] = set()

    for record in catalog["records"]:
        if not isinstance(record, dict):
            raise StageAuditError("catalog records must be objects")
        logical_id = record.get("logical_id")
        if not isinstance(logical_id, str) or not logical_id:
            raise StageAuditError("catalog record has no logical ID")
        if logical_id in logical_ids:
            raise StageAuditError(f"catalog repeats logical ID {logical_id!r}")
        logical_ids.add(logical_id)
        relative_path = _package_path(record.get("package_relative_path"), logical_id)
        if relative_path in package_paths:
            raise StageAuditError(f"catalog repeats package path {relative_path!r}")
        package_paths.add(relative_path)

        location = record.get("location")
        if location == "qt_resource":
            runtime_url = record.get("runtime_url")
            expected_url = "qrc:/" + relative_path
            if runtime_url != expected_url:
                raise StageAuditError(
                    f"{logical_id} QML runtime URL must be {expected_url!r}, got {runtime_url!r}"
                )
            if not relative_path.startswith("qt/qml/ClassMngr/Calendar/"):
                raise StageAuditError(f"{logical_id} has an unexpected Calendar QML key")
            qml_keys.add(expected_url)
            continue

        if location != "resource_file":
            raise StageAuditError(f"{logical_id} has unsupported catalog location {location!r}")
        expected_files.add(relative_path)
        contents = _read_payload_file(root, relative_path)
        provenance = record.get("provenance")
        baked = provenance.get("baked") if isinstance(provenance, dict) else None
        if baked is not None:
            if not isinstance(baked, dict):
                raise StageAuditError(f"{logical_id} baked provenance must be an object")
            expected_bytes = baked.get("bytes")
            expected_sha256 = baked.get("sha256")
            if isinstance(expected_bytes, bool) or not isinstance(expected_bytes, int):
                raise StageAuditError(f"{logical_id} has invalid baked byte metadata")
            if not isinstance(expected_sha256, str) or len(expected_sha256) != 64:
                raise StageAuditError(f"{logical_id} has invalid baked SHA-256 metadata")
            if len(contents) != expected_bytes:
                raise StageAuditError(
                    f"{relative_path} has {len(contents)} bytes; expected {expected_bytes}"
                )
            actual_sha256 = hashlib.sha256(contents).hexdigest()
            if actual_sha256 != expected_sha256:
                raise StageAuditError(
                    f"{relative_path} SHA-256 is {actual_sha256}; expected {expected_sha256}"
                )
            hash_checked += 1
        elif record.get("expected_type") == "qm":
            if not relative_path.endswith(".qm") or not contents:
                raise StageAuditError(f"generated translation output is missing or empty: {relative_path}")
            generated_qm_count += 1
        else:
            raise StageAuditError(
                f"{logical_id} has no baked hash and is not a generated QM output"
            )

    if len(qml_keys) != 2 or qml_keys != {
        "qrc:/qt/qml/ClassMngr/Calendar/EventCalendar.qml",
        "qrc:/qt/qml/ClassMngr/Calendar/MonthGridDelegate.qml",
    }:
        raise StageAuditError(f"catalog does not contain the two Calendar QML keys: {sorted(qml_keys)}")
    if generated_qm_count != 5:
        raise StageAuditError(f"expected five generated QM files, found {generated_qm_count}")

    actual_files = _resource_files(root)
    if actual_files != expected_files:
        missing = sorted(expected_files - actual_files)
        unexpected = sorted(actual_files - expected_files)
        raise StageAuditError(
            f"staged file layout differs from catalog; missing={missing}, unexpected={unexpected}"
        )
    if any(PurePosixPath(path).name == "resource-manifest.json" for path in actual_files):
        raise StageAuditError("provenance-only resource-manifest.json must not be installed")

    return {
        "staged_files": len(actual_files),
        "hash_checked": hash_checked,
        "generated_qm": generated_qm_count,
        "qml_keys": len(qml_keys),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        required=True,
        help="staged platform resource root containing resource-catalog.json",
    )
    args = parser.parse_args(argv)
    try:
        result = audit_resource_stage(args.root)
    except (StageAuditError, OSError) as error:
        print(f"resource stage audit failed: {error}", file=sys.stderr)
        return 1
    print(
        "Audited staged ClassMngrNext resources: "
        f"{result['staged_files']} files, {result['hash_checked']} SHA-256 checks, "
        f"{result['generated_qm']} generated QM files, {result['qml_keys']} QML catalog keys."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
