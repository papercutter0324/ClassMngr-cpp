#!/usr/bin/env python3
"""Fail-closed validation for the packaged P9-11 Class Transfer routes."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import sys
from datetime import date
from pathlib import Path
from typing import Any


MANIFEST_SCHEMA = "classmngr-phase9-packaged-next-class-transfer-v1"
TASK_ID = "P9_11_NEXT_CLASS_TRANSFER_PACKAGED_LIFECYCLE"
WORKING_SET_LIMIT = 262_144_000
OPERATION_PEAK_LIMIT = 536_870_912
EXPECTED_ROUTE_IDS = (
    "route-1-next-export",
    "route-2-next-apply",
    "route-3-next-cancel",
)
ALLOWED_SCRIPT_ONLY_BUILD_DELTA = {
    "scripts/phase9/prepare_class_transfer_workspace.py",
    "scripts/phase9/run_packaged_next_class_transfer.ps1",
    "scripts/phase9/test_prepare_class_transfer_workspace.py",
    "scripts/phase9/validate_packaged_next_class_transfer.py",
    "scripts/phase9/test_validate_packaged_next_class_transfer.py",
}
EXPECTED_ROLES = {"route-1-next-export": "export", "route-2-next-apply": "apply", "route-3-next-cancel": "cancel"}
EXPECTED_TABLE_COUNTS = {
    "classes": 96,
    "teachers": 24,
    "class_info": 96,
    "class_times": 768,
    "class_intensive_times": 24,
    "roster_columns": 288,
    "roster_data": 7200,
    "speaking_evaluations": 20,
    "speaking_eval_data": 600,
    "calendar_events": 180,
}
SOURCE_TEACHER_PROFILE_COLUMNS = (
    "teacher_kr",
    "teacher_en",
    "preferred_romanization",
    "preferred_name",
    "birthday",
)
SOURCE_TEACHER_KOREAN_BASE = "선생님"
SOURCE_TEACHER_ENGLISH_BASE = "Teacher"
SOURCE_CLASS_DETAIL_COLUMNS = ("class_level", "reading_book", "essay_book")
SOURCE_LEVELS_BY_GRADE = {
    "E4": ("Theseus", "Perseus", "Odysseus", "Hercules"),
    "E5": ("Artemis", "Hermes", "Apollo", "Zeus", "Athena"),
    "M1": ("Elephantus", "Galaxia", "Solis", "Major", "Song's"),
    "M2": ("Ursa", "Leo", "Tigris", "Major", "Song's"),
}
SOURCE_CLASS_DETAIL_RULES = {
    "catalogPath": "src/domain/config/class_details_catalog.h",
    "levelMapping": "parse Level NN; select levelsForGrade(grade)[(NN-1) modulo level count]",
    "readingBook": "blank string, first allowed catalog value for every supported grade-level pair",
    "essayBook": "blank string for E4/E5; N/A for M1/M2",
}


class ValidationFailure(RuntimeError):
    pass


def _canonical(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _file_sha256(path: Path) -> str:
    return _sha256(path.read_bytes())


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationFailure(message)


def _read_json(path: Path) -> Any:
    _require(path.is_file(), f"required evidence file is missing: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationFailure(f"could not parse JSON evidence {path}: {error}") from error


def _resolve_evidence_path(root: Path, value: str, label: str) -> Path:
    path = Path(value)
    resolved = path.resolve(strict=True) if path.is_absolute() else (root / path).resolve(strict=True)
    try:
        resolved.relative_to(root.resolve(strict=True))
    except ValueError as error:
        raise ValidationFailure(f"{label} escaped the run evidence directory: {resolved}") from error
    return resolved


def inventory_fingerprint(rows: list[dict[str, Any]]) -> str:
    ordered = sorted(rows, key=lambda row: str(row["path"]).casefold())
    text = "\n".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}"
        for row in ordered
    )
    return _sha256((text + "\n").encode("utf-8"))


def validate_script_only_source_delta(
    build_rows: list[dict[str, Any]],
    current_rows: list[dict[str, Any]],
    expected_changes: list[dict[str, Any]],
    *,
    build_source_sha256: str,
    current_source_sha256: str,
    product_source_sha256: str,
    allowed_paths: list[str],
) -> list[dict[str, Any]]:
    """Prove a packaged binary differs from current measurement only by known harness files."""
    _require(inventory_fingerprint(build_rows) == build_source_sha256, "derived build-source rows do not match the original Release fingerprint")
    _require(inventory_fingerprint(current_rows) == current_source_sha256, "derived current-source rows do not match the run fingerprint")
    _require(set(allowed_paths) == ALLOWED_SCRIPT_ONLY_BUILD_DELTA, "derived receipt has an unexpected script-only allowlist")
    build_map = {str(row["path"]): row for row in build_rows}
    current_map = {str(row["path"]): row for row in current_rows}
    _require(len(build_map) == len(build_rows) and len(current_map) == len(current_rows), "source inventories contain duplicate paths")
    _require(set(build_map) == set(current_map), "script-only derivation contains an added or removed source path")
    changes: list[dict[str, Any]] = []
    build_product: list[dict[str, Any]] = []
    current_product: list[dict[str, Any]] = []
    for path in sorted(build_map, key=str.casefold):
        before = build_map[path]
        after = current_map[path]
        changed = int(before["sizeBytes"]) != int(after["sizeBytes"]) or str(before["sha256"]).lower() != str(after["sha256"]).lower()
        if changed:
            _require(path in ALLOWED_SCRIPT_ONLY_BUILD_DELTA, f"product/compiler source changed after Release build: {path}")
            changes.append(
                {
                    "path": path,
                    "buildSizeBytes": int(before["sizeBytes"]),
                    "buildSha256": str(before["sha256"]).lower(),
                    "currentSizeBytes": int(after["sizeBytes"]),
                    "currentSha256": str(after["sha256"]).lower(),
                }
            )
        else:
            build_product.append(before)
            current_product.append(after)
    _require(bool(changes), "source fingerprint differs without an allowed harness-file change")
    _require(inventory_fingerprint(build_product) == inventory_fingerprint(current_product), "product/compiler source inventory differs after filtering harness scripts")
    _require(inventory_fingerprint(build_product) == product_source_sha256, "derived product-source fingerprint differs")
    _require(changes == expected_changes, "derived receipt source-change rows differ from recomputed hashes")
    return changes


def _inventory_files_on_disk(directory: Path) -> list[str]:
    return sorted(
        path.relative_to(directory).as_posix()
        for path in directory.rglob("*")
        if path.is_file()
        and not any(part in {"__pycache__", ".pytest_cache", ".mypy_cache"} for part in path.parts)
        and path.suffix.lower() not in {".pyc", ".pyo"}
    )


def _validate_derived_build_reuse(
    manifest: dict[str, Any],
    root: Path,
    current_rows: list[dict[str, Any]],
    build_receipt: dict[str, Any],
) -> dict[str, Any]:
    """Recheck script-only build derivation, old packaged inputs, and current copied package."""
    build_link = manifest.get("releaseBuildReceipt") or {}
    derived_path = _resolve_evidence_path(root, str(build_link.get("derivedReceiptPath", "")), "derived Release receipt")
    _require(_file_sha256(derived_path) == str(build_link.get("derivedReceiptSha256", "")), "derived Release receipt SHA-256 mismatch")
    derived = _read_json(derived_path)
    _require(derived.get("schema") == "classmngr-phase9-class-transfer-script-only-derived-build-receipt-v1" and derived.get("status") == "verified-script-only-derivation", "derived Release receipt schema/status is invalid")
    _require(derived.get("taskId") == TASK_ID, "derived Release receipt belongs to another task")

    source_proof = derived.get("source") or {}
    build_inventory_path = _resolve_evidence_path(root, str(source_proof.get("buildInventoryCopy", "")), "original build-source inventory copy")
    _require(_file_sha256(build_inventory_path) == source_proof.get("buildInventoryCopySha256"), "copied original build-source inventory hash mismatch")
    build_inventory = _read_json(build_inventory_path)
    build_rows = build_inventory.get("files")
    _require(isinstance(build_rows, list) and build_rows, "derived receipt lacks original build-source rows")
    _require(source_proof.get("buildSourceSha256") == build_receipt.get("sourceSha256"), "derived build source fingerprint differs from original receipt")
    _require(source_proof.get("currentSourceSha256") == (manifest.get("source") or {}).get("sha256"), "derived current-source fingerprint differs from run source")
    changed = validate_script_only_source_delta(
        build_rows,
        current_rows,
        source_proof.get("changedFiles", []),
        build_source_sha256=str(source_proof.get("buildSourceSha256", "")),
        current_source_sha256=str(source_proof.get("currentSourceSha256", "")),
        product_source_sha256=str(source_proof.get("productSourceSha256", "")),
        allowed_paths=source_proof.get("allowedPaths", []),
    )
    _require(build_link.get("sourceDeltaPaths") == [row["path"] for row in changed], "manifest source-delta paths differ from derived receipt")
    _require(build_link.get("buildSourceSha256") == build_receipt.get("sourceSha256"), "manifest build-source fingerprint differs from receipt")
    _require(build_link.get("currentSourceSha256") == (manifest.get("source") or {}).get("sha256"), "manifest current-source fingerprint differs from measured source")

    build_proof = derived.get("build") or {}
    receipt_copy = _resolve_evidence_path(root, str(build_proof.get("receiptCopy", "")), "copied original Release receipt")
    _require(_file_sha256(receipt_copy) == build_proof.get("receiptCopySha256") == build_link.get("sha256"), "copied original Release receipt hash mismatch")
    _require(receipt_copy.read_bytes() == Path(str(build_link.get("path", ""))).read_bytes(), "copied original Release receipt differs from the referenced receipt")
    _require(build_proof.get("receiptPath") == build_link.get("path") and build_proof.get("receiptSha256") == build_link.get("sha256"), "derived receipt points at a different original Release receipt")
    _require(build_receipt.get("status") == "succeeded" and build_receipt.get("taskId") == TASK_ID, "original build receipt is not successful P9-11 evidence")
    _require(build_proof.get("buildDirectory") == build_receipt.get("buildDirectory") and build_proof.get("generator") == build_receipt.get("generator"), "derived Release build directory/generator differs from original receipt")
    _require(build_proof.get("configuration") == "Release" and build_proof.get("configuration") == build_receipt.get("configuration"), "derived Release configuration is not Release")
    _require(build_proof.get("platform") == "x64" and build_proof.get("platform") == build_receipt.get("platform"), "derived Release platform is not x64")
    _require(build_proof.get("targets") == build_receipt.get("targets"), "derived Release target list differs from original receipt")
    _require(build_proof.get("configureExitCode") == 0 and build_receipt.get("configure", {}).get("exitCode") == 0, "original Release configure did not succeed")
    _require(build_proof.get("buildExitCode") == 0 and build_receipt.get("build", {}).get("exitCode") == 0, "original Release build did not succeed")
    cache_path = Path(str(build_proof.get("cachePath", ""))).resolve(strict=True)
    _require(_file_sha256(cache_path) == build_proof.get("cacheSha256") == build_receipt.get("cacheSha256After"), "standard Release CMake cache differs from original receipt")
    build_output = derived.get("executable") or {}
    _require(build_output.get("recordedBuildOutputPath") == build_receipt.get("releaseExecutablePath"), "derived executable source path differs from original receipt")
    _require(build_output.get("recordedBuildOutputStillMatches") is False, "derived receipt did not disclose the changed standard build-tree executable")
    _require(build_output.get("recordedBuildOutputHashAtDerivation") != build_receipt.get("releaseExecutableSha256"), "standard build-tree executable unexpectedly matches the preserved package premise")

    prior = derived.get("priorPackage") or {}
    prior_manifest_copy = _resolve_evidence_path(root, str(prior.get("runManifestCopy", "")), "prior package run manifest copy")
    _require(_file_sha256(prior_manifest_copy) == prior.get("runManifestCopySha256") == prior.get("runManifestSha256"), "prior package run manifest copy hash mismatch")
    prior_manifest = _read_json(prior_manifest_copy)
    prior_source_copy = _resolve_evidence_path(root, str(prior.get("runSourceInventoryCopy", "")), "prior package source inventory copy")
    _require(_file_sha256(prior_source_copy) == prior.get("runSourceInventoryCopySha256"), "prior package source inventory copy hash mismatch")
    prior_source = _read_json(prior_source_copy)
    _require(inventory_fingerprint(prior_source.get("files", [])) == build_receipt.get("sourceSha256") == prior_manifest.get("source", {}).get("sha256"), "prior packaged run source inventory differs from the original build source")
    _require(prior_manifest.get("releaseBuildReceipt", {}).get("sha256") == build_link.get("sha256"), "prior package manifest does not link to the original Release receipt")

    package_inventory_copy = _resolve_evidence_path(root, str(prior.get("inventoryCopy", "")), "prior package inventory copy")
    _require(_file_sha256(package_inventory_copy) == prior.get("inventoryCopySha256"), "prior package inventory copy hash mismatch")
    prior_inventory = _read_json(package_inventory_copy)
    prior_rows = prior_inventory.get("files")
    _require(isinstance(prior_rows, list) and prior_rows, "prior package inventory is empty")
    _require(inventory_fingerprint(prior_rows) == prior.get("inventorySha256") == prior_inventory.get("sha256") == prior_manifest.get("package", {}).get("sha256"), "prior package inventories and manifest disagree")
    _require(len(prior_rows) == prior.get("fileCount") == prior_inventory.get("fileCount") == prior_manifest.get("package", {}).get("fileCount"), "prior package inventory file count differs")
    prior_root = Path(str(prior.get("packageRoot", ""))).resolve(strict=True)
    _require(str(prior_inventory.get("root", "")).casefold() == str(prior_root).casefold(), "prior package inventory root differs from derivation")
    expected_prior_paths: set[str] = set()
    prior_rows_by_path: dict[str, dict[str, Any]] = {}
    for row in prior_rows:
        relative = str(row["path"])
        file_path = (prior_root / Path(*Path(relative).parts)).resolve(strict=True)
        try:
            file_path.relative_to(prior_root)
        except ValueError as error:
            raise ValidationFailure(f"prior package inventory path escaped its root: {relative}") from error
        _require(file_path.is_file() and file_path.stat().st_size == int(row["sizeBytes"]) and _file_sha256(file_path) == str(row["sha256"]).lower(), f"preserved package file differs from inventory: {relative}")
        expected_prior_paths.add(relative)
        prior_rows_by_path[relative] = row

    recorded_export = prior.get("recordedNextExport") or {}
    export_path = Path(str(recorded_export.get("path", ""))).resolve(strict=True)
    _require(str(export_path.parent).casefold() == str(prior_root).casefold() and export_path.name == "next-exported-classes.json", "prior package has an unexpected extra file path")
    _require(export_path.is_file() and export_path.stat().st_size == int(recorded_export.get("sizeBytes", -1)) and _file_sha256(export_path) == recorded_export.get("sha256"), "prior package extra export file differs from its Next operation hash")
    prior_route = [route for route in prior_manifest.get("routes", []) if route.get("routeId") == "route-1-next-export"]
    _require(len(prior_route) == 1, "prior package manifest lacks Route 1 provenance")
    prior_operation = prior_route[0].get("operation", {})
    _require(prior_operation.get("outcome") == "completed" and prior_operation.get("packageSha256") == recorded_export.get("sha256") and int(prior_operation.get("packageSizeBytes", -1)) == int(recorded_export.get("sizeBytes", -2)), "prior extra package file differs from completed Next export telemetry")
    _require(prior_operation.get("metrics", {}).get("exports_completed") == 1 and prior_operation.get("metrics", {}).get("export_selected_class_count") == 48, "prior package provenance lacks one 48-class Next export")
    prior_actual_paths = set(_inventory_files_on_disk(prior_root))
    _require(prior_actual_paths == expected_prior_paths | {"next-exported-classes.json"}, "prior package has unexpected or missing files beyond its inventory and recorded Next export")
    preserved_executable = Path(str(build_output.get("preservedPackagePath", ""))).resolve(strict=True)
    _require(preserved_executable == (prior_root / "ClassMngrNext.exe").resolve(strict=True), "preserved executable is not from the inventoried prior package")
    _require(_file_sha256(preserved_executable) == build_receipt.get("releaseExecutableSha256") == prior_manifest.get("application", {}).get("sha256"), "preserved package executable differs from original Release receipt")
    _require(prior_manifest.get("package", {}).get("applicationSha256") == build_receipt.get("releaseExecutableSha256"), "prior package executable manifest hash differs")

    current_package = manifest.get("package") or {}
    current_package_inventory_path = _resolve_evidence_path(root, str(manifest.get("files", {}).get("packageInventory", "")), "current package inventory")
    current_package_inventory = _read_json(current_package_inventory_path)
    _require(current_package.get("reusedPriorPackage") is True and current_package.get("sha256") == prior.get("inventorySha256") and current_package.get("fileCount") == prior.get("fileCount"), "current package is not the exact preserved package inventory")
    _require(current_package_inventory.get("sha256") == prior.get("inventorySha256") and current_package_inventory.get("fileCount") == prior.get("fileCount"), "current package inventory differs from the preserved package")
    _require(current_package.get("reusedPackageSource", {}).get("inventorySha256") == prior.get("inventorySha256"), "current package reuse link differs from the derived receipt")

    resources = derived.get("resources") or {}
    resource_root = Path(str(resources.get("stagedRoot", ""))).resolve(strict=True)
    resource_rows = build_receipt.get("generatedResources", {}).get("stagedResourceInputs", {}).get("files", [])
    actual_resource_rows: list[dict[str, Any]] = []
    for path in _inventory_files_on_disk(resource_root):
        file_path = resource_root / Path(*Path(path).parts)
        actual_resource_rows.append({"path": path, "sizeBytes": file_path.stat().st_size, "sha256": _file_sha256(file_path)})
    _require(inventory_fingerprint(actual_resource_rows) == resources.get("inventorySha256") == build_receipt.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256"), "staged resource tree differs from original Release receipt")
    _require(len(actual_resource_rows) == resources.get("fileCount") == len(resource_rows), "staged resource count differs from Release receipt")
    _require(inventory_fingerprint(actual_resource_rows) == inventory_fingerprint(resource_rows), "staged resource rows differ from Release receipt")
    for row in resource_rows:
        if str(row["path"]).startswith("resources/"):
            _require(str(row["path"]) in prior_rows_by_path, f"preserved package lacks staged resource {row['path']}")
            _require(prior_rows_by_path[str(row["path"])] == row, f"preserved package resource differs from staged resource {row['path']}")

    workspace_input = derived.get("workspaceInput") or {}
    seed_path = Path(str(workspace_input.get("seedSqlPath", ""))).resolve(strict=True)
    _require(_file_sha256(seed_path) == workspace_input.get("seedSqlSha256") == manifest.get("masterWorkspace", {}).get("seedSqlSha256"), "derived workspace seed differs from current route input")
    return derived


def _row_dicts(snapshot: dict[str, Any], table: str) -> tuple[list[str], list[dict[str, Any]]]:
    table_value = snapshot.get("tables", {}).get(table)
    _require(isinstance(table_value, dict), f"workspace snapshot lacks table {table}")
    names = [str(column[1]) for column in table_value.get("schema", [])]
    rows = table_value.get("rows", [])
    _require(all(len(row) == len(names) for row in rows), f"{table} row width differs from its schema")
    return names, [dict(zip(names, row, strict=True)) for row in rows]


def _row_projection(rows: list[dict[str, Any]], columns: tuple[str, ...]) -> collections.Counter[tuple[Any, ...]]:
    return collections.Counter(tuple(row.get(column) for column in columns) for row in rows)


def _same_tables(left: dict[str, Any], right: dict[str, Any]) -> bool:
    if set(left.get("tables", {})) != set(right.get("tables", {})):
        return False
    for table in left["tables"]:
        a = left["tables"][table]
        b = right["tables"][table]
        if a.get("schema") != b.get("schema") or a.get("rows") != b.get("rows"):
            return False
    return True


def _expected_source_teacher_profile(teacher_id: int) -> dict[str, str]:
    _require(1 <= teacher_id <= 24, f"source teacher ID {teacher_id} is outside the approved range 1..24")
    suffix = chr(ord("A") + teacher_id - 1)
    korean = f"{SOURCE_TEACHER_KOREAN_BASE}({suffix})"
    english = f"{SOURCE_TEACHER_ENGLISH_BASE} {suffix}"
    romanization = f"{SOURCE_TEACHER_ENGLISH_BASE}-{suffix}"
    birthday = f"01-{teacher_id:02d}"
    _require(bool(re.fullmatch(r"^[\s\uAC00-\uD7A3]+(?:\s*\([A-Za-z]\))?\s*$", korean)), "synthetic Korean source teacher name is outside the accepted name pattern")
    _require(len(SOURCE_TEACHER_KOREAN_BASE) == 3, "synthetic Korean source teacher base must have three Hangul syllables")
    _require(
        all(len(value) <= 20 and re.fullmatch(r"[A-Za-z .-]+", value) for value in (english, romanization)),
        "synthetic English source teacher name is outside the accepted 20-character pattern",
    )
    _require(english == english.strip() and romanization == romanization.strip(), "synthetic source teacher names are not normalized")
    _require(bool(re.fullmatch(r"\d{2}-\d{2}", birthday)), "synthetic source teacher birthday does not use MM-dd")
    try:
        date(2000, int(birthday[:2]), int(birthday[3:]))
    except ValueError as error:
        raise ValidationFailure(f"synthetic source teacher birthday is not valid under TeacherValidator: {birthday}") from error
    return {
        "teacher_kr": korean,
        "teacher_en": english,
        "preferred_romanization": romanization,
        "preferred_name": english,
        "birthday": birthday,
    }


def _expected_source_teacher_profile_transformation(
    source: dict[str, Any], cancel: dict[str, Any]
) -> dict[str, Any]:
    source_columns, source_rows = _row_dicts(source, "teachers")
    cancel_columns, cancel_rows = _row_dicts(cancel, "teachers")
    _require(source_columns == cancel_columns, "source teacher profile preparation changed the teachers schema")
    before_by_id = {int(row["id"]): row for row in cancel_rows}
    after_by_id = {int(row["id"]): row for row in source_rows}
    _require(len(before_by_id) == len(cancel_rows) and len(after_by_id) == len(source_rows), "teacher table contains duplicate IDs")
    _require(set(before_by_id) == set(after_by_id) == set(range(1, 25)), "source teacher profile preparation changed the teacher ID set")
    unchanged_columns = [column for column in cancel_columns if column not in SOURCE_TEACHER_PROFILE_COLUMNS]
    row_evidence: list[dict[str, Any]] = []
    before_full_rows: list[dict[str, Any]] = []
    after_full_rows: list[dict[str, Any]] = []
    before_profile_rows: list[dict[str, Any]] = []
    after_profile_rows: list[dict[str, Any]] = []
    unchanged_projection_rows: list[dict[str, Any]] = []
    for teacher_id in range(1, 25):
        before = before_by_id[teacher_id]
        expected_after = {**before, **_expected_source_teacher_profile(teacher_id)}
        after = after_by_id[teacher_id]
        _require(after == expected_after, f"source teacher {teacher_id} has a field change outside the approved profile normalization")
        before_profile = {column: before[column] for column in SOURCE_TEACHER_PROFILE_COLUMNS}
        after_profile = {column: after[column] for column in SOURCE_TEACHER_PROFILE_COLUMNS}
        unchanged_values = {column: before[column] for column in unchanged_columns}
        before_full_rows.append(before)
        after_full_rows.append(after)
        before_profile_rows.append({"teacherId": teacher_id, "values": before_profile})
        after_profile_rows.append({"teacherId": teacher_id, "values": after_profile})
        unchanged_projection_rows.append({"teacherId": teacher_id, "values": unchanged_values})
        row_evidence.append(
            {
                "teacherId": teacher_id,
                "beforeProfile": before_profile,
                "afterProfile": after_profile,
                "beforeRowSha256": _sha256(_canonical(before)),
                "afterRowSha256": _sha256(_canonical(after)),
                "unchangedColumnsSha256": _sha256(_canonical(unchanged_values)),
            }
        )
    return {
        "table": "teachers",
        "modifiedColumns": list(SOURCE_TEACHER_PROFILE_COLUMNS),
        "rowCount": len(row_evidence),
        "koreanNameRule": "선생님(A)..선생님(X); three Hangul syllables with the supported A-X suffix",
        "englishNameRule": "Teacher A..Teacher X; ASCII letters and spaces, maximum 20 characters",
        "preferredRomanizationRule": "Teacher-A..Teacher-X; ASCII letters and hyphens, maximum 20 characters",
        "preferredNameRule": "equals teacher_en and is therefore an allowed preferred-name choice",
        "birthdayRule": "synthetic MM-dd values 01-01..01-24; valid under TeacherValidator MM-dd parsing",
        "beforeFullRowsSha256": _sha256(_canonical(before_full_rows)),
        "afterFullRowsSha256": _sha256(_canonical(after_full_rows)),
        "beforeProfileRowsSha256": _sha256(_canonical(before_profile_rows)),
        "afterProfileRowsSha256": _sha256(_canonical(after_profile_rows)),
        "unchangedColumnsSha256": _sha256(_canonical(unchanged_projection_rows)),
        "unchangedColumnsPreserved": True,
        "rows": row_evidence,
    }


def _expected_source_class_details(class_id: int, grade: str, original_level: str) -> dict[str, str]:
    _require(1 <= class_id <= 48, f"source class ID {class_id} is outside the approved range 1..48")
    levels = SOURCE_LEVELS_BY_GRADE.get(grade)
    _require(levels is not None, f"source class {class_id} has unsupported catalog grade {grade!r}")
    match = re.fullmatch(r"Level (?P<number>\d{2})", original_level)
    _require(match is not None, f"source class {class_id} level is not the seeded Level NN value")
    level_number = int(match.group("number"))
    _require(level_number >= 1, f"source class {class_id} has invalid seeded level number {level_number}")
    return {
        "class_level": levels[(level_number - 1) % len(levels)],
        "reading_book": "",
        "essay_book": "" if grade in {"E4", "E5"} else "N/A",
    }


def _expected_source_class_detail_transformation(
    source: dict[str, Any], cancel: dict[str, Any]
) -> dict[str, Any]:
    source_columns, source_rows = _row_dicts(source, "class_info")
    cancel_columns, cancel_rows = _row_dicts(cancel, "class_info")
    _require(source_columns == cancel_columns, "source class detail preparation changed the class_info schema")
    before_by_id = {int(row["class_id"]): row for row in cancel_rows}
    after_by_id = {int(row["class_id"]): row for row in source_rows}
    _require(len(before_by_id) == len(cancel_rows) and len(after_by_id) == len(source_rows), "class_info contains duplicate class IDs")
    _require(set(before_by_id) == set(after_by_id) == set(range(1, 97)), "source class detail preparation changed class IDs")
    unchanged_columns = [column for column in cancel_columns if column not in SOURCE_CLASS_DETAIL_COLUMNS]
    row_evidence: list[dict[str, Any]] = []
    before_full_rows: list[dict[str, Any]] = []
    after_full_rows: list[dict[str, Any]] = []
    before_profile_rows: list[dict[str, Any]] = []
    after_profile_rows: list[dict[str, Any]] = []
    unchanged_projection_rows: list[dict[str, Any]] = []
    untouched_rows: list[dict[str, Any]] = []
    relations: list[dict[str, Any]] = []
    for class_id in range(1, 97):
        before = before_by_id[class_id]
        after = after_by_id[class_id]
        if class_id <= 48:
            expected_after = {**before, **_expected_source_class_details(class_id, str(before["class_grade"]), str(before["class_level"]))}
            _require(after == expected_after, f"source class {class_id} changed outside the approved class-detail fields")
            before_profile = {column: before[column] for column in SOURCE_CLASS_DETAIL_COLUMNS}
            after_profile = {column: after[column] for column in SOURCE_CLASS_DETAIL_COLUMNS}
            unchanged_values = {column: before[column] for column in unchanged_columns}
            before_profile_rows.append({"classId": class_id, "values": before_profile})
            after_profile_rows.append({"classId": class_id, "values": after_profile})
            unchanged_projection_rows.append({"classId": class_id, "values": unchanged_values})
            row_evidence.append({
                "classId": class_id,
                "beforeProfile": before_profile,
                "afterProfile": after_profile,
                "beforeRowSha256": _sha256(_canonical(before)),
                "afterRowSha256": _sha256(_canonical(after)),
                "unchangedColumnsSha256": _sha256(_canonical(unchanged_values)),
            })
        else:
            _require(after == before, f"source class {class_id} outside the selected range was modified")
            untouched_rows.append(before)
        _require(after["teacher_id"] == before["teacher_id"], f"source class {class_id} teacher relation changed")
        relations.append({"classId": class_id, "teacherId": before["teacher_id"]})
        before_full_rows.append(before)
        after_full_rows.append(after)
    return {
        "table": "class_info",
        "selectedClassIds": [1, 48],
        "modifiedColumns": list(SOURCE_CLASS_DETAIL_COLUMNS),
        "rowCount": len(row_evidence),
        "rules": SOURCE_CLASS_DETAIL_RULES,
        "catalogLevelOrderByGrade": {grade: list(levels) for grade, levels in SOURCE_LEVELS_BY_GRADE.items()},
        "beforeFullRowsSha256": _sha256(_canonical(before_full_rows)),
        "afterFullRowsSha256": _sha256(_canonical(after_full_rows)),
        "beforeProfileRowsSha256": _sha256(_canonical(before_profile_rows)),
        "afterProfileRowsSha256": _sha256(_canonical(after_profile_rows)),
        "unchangedColumnsSha256": _sha256(_canonical(unchanged_projection_rows)),
        "untouchedClassRowsSha256": _sha256(_canonical(untouched_rows)),
        "classTeacherRelationsSha256": _sha256(_canonical(relations)),
        "classTeacherRelationsPreserved": True,
        "untouchedClassesPreserved": True,
        "rows": row_evidence,
    }


def validate_source_preparation(
    source: dict[str, Any],
    cancel: dict[str, Any],
    teacher_profile_transformation: dict[str, Any],
    class_detail_transformation: dict[str, Any],
) -> dict[str, Any]:
    """Require the source workspace transform to remove only the approved rows."""
    _require(set(source.get("tables", {})) == set(cancel.get("tables", {})), "source and cancel prepared schemas differ")
    source_classes = source["tables"]["classes"]["rows"]
    cancel_classes = cancel["tables"]["classes"]["rows"]
    _require(source_classes == cancel_classes, "source schedule prep changed class rows")
    for table in source["tables"]:
        if table in {"class_times", "teachers", "class_info"}:
            continue
        _require(
            source["tables"][table]["schema"] == cancel["tables"][table]["schema"]
            and source["tables"][table]["rows"] == cancel["tables"][table]["rows"],
            f"source schedule prep changed unrelated table {table}",
        )
    names, source_rows = _row_dicts(source, "class_times")
    _, cancel_rows = _row_dicts(cancel, "class_times")
    _require(names == [str(c[1]) for c in cancel["tables"]["class_times"]["schema"]], "class_times schema changed")
    before = collections.Counter(tuple(row[name] for name in names) for row in cancel_rows)
    after = collections.Counter(tuple(row[name] for name in names) for row in source_rows)
    removed = before - after
    added = after - before
    _require(not added, "source schedule prep added or invented regular schedule rows")
    expected_removed = collections.Counter(
        tuple(row[name] for name in names)
        for row in cancel_rows
        if 1 <= int(row["class_id"]) <= 48 and int(row["class_id"]) not in {1, 9}
    )
    _require(removed == expected_removed, "source schedule prep did not remove exactly the other selected regular schedule rows")
    _require(sum(removed.values()) == 368, f"source schedule prep removed {sum(removed.values())} regular rows, expected 368")
    expected_teacher_transform = _expected_source_teacher_profile_transformation(source, cancel)
    _require(teacher_profile_transformation == expected_teacher_transform, "source teacher profile transformation rows or hashes differ from the approved deterministic normalization")
    expected_class_detail_transform = _expected_source_class_detail_transformation(source, cancel)
    _require(class_detail_transformation == expected_class_detail_transform, "source class detail transformation rows or hashes differ from the approved deterministic normalization")
    return {
        "removedRegularRows": sum(removed.values()),
        "retainedSelectedRegularRows": sum(1 for row in source_rows if 1 <= int(row["class_id"]) <= 48),
        "retainedSelectedIntensiveRows": source["tables"]["class_intensive_times"]["rowCount"],
        "normalizedTeacherRows": int(expected_teacher_transform["rowCount"]),
        "normalizedTeacherColumns": expected_teacher_transform["modifiedColumns"],
        "teacherUnchangedColumnsSha256": expected_teacher_transform["unchangedColumnsSha256"],
        "normalizedClassDetailRows": int(expected_class_detail_transform["rowCount"]),
        "normalizedClassDetailColumns": expected_class_detail_transform["modifiedColumns"],
        "classDetailUnchangedColumnsSha256": expected_class_detail_transform["unchangedColumnsSha256"],
        "classTeacherRelationsSha256": expected_class_detail_transform["classTeacherRelationsSha256"],
        "allOtherTablesIdentical": True,
    }


def _expected_apply_preparation_row(table: str, columns: list[str], row: list[Any]) -> tuple[Any, ...]:
    projected: list[Any] = []
    for column, value in zip(columns, row, strict=True):
        if value is None:
            projected.append(value)
        elif column == "class_id":
            projected.append(int(value) + 10000)
        elif column == "teacher_id":
            projected.append(int(value) + 20000)
        elif column == "evaluation_id":
            projected.append(int(value) + 30000)
        elif column == "id" and table == "classes":
            projected.append(int(value) + 10000)
        elif column == "id" and table == "teachers":
            projected.append(int(value) + 20000)
        elif column == "id" and table == "speaking_evaluations":
            projected.append(int(value) + 30000)
        elif column == "name" and table == "classes":
            projected.append("Destination Copy " + str(value))
        elif column in {"teacher_kr", "teacher_en", "preferred_name", "preferred_romanization"} and table == "teachers":
            projected.append("Destination " + str(value))
        elif column == "class_grade" and table == "class_info":
            projected.append("Destination " + str(value))
        elif column == "class_level" and table == "class_info":
            projected.append("Destination " + str(value))
        else:
            projected.append(value)
    return tuple(projected)


def _count_nonempty_evaluation_cells(source_rows: list[list[Any]]) -> int:
    """Count populated package values, excluding the fixed-schema blank padding."""
    return sum(
        1
        for row in source_rows
        for value in row[:11]
        if value is not None and value != ""
    )


def _expected_classes_navigator_default_rows() -> collections.Counter[tuple[Any, ...]]:
    return collections.Counter(
        {
            ("classes_navigation_visibility_scope", "active_schedule"): 1,
            ("classes_navigation_day_filter_reset_policy", "on_application_close"): 1,
            ("classes_navigation_class_selection_reset_policy", "on_application_close"): 1,
        }
    )


def _expected_import_teacher_profile(
    teacher: dict[str, Any], teacher_columns: list[str]
) -> dict[str, Any]:
    """Project the approved synthetic teacher through current product normalization.

    ClassTransferPackageValidator normalizes imported teachers with
    TeacherValidator::normalized, which applies StudentNameUtils::normalizeEnglishName
    to preferredRomanization. That policy capitalizes after spaces and periods,
    and lowercases the token following a hyphen. The accepted fixture is limited
    to the validated Teacher A..X / Teacher-A..X profile, so this projection
    models that exact product behavior without accepting other profile changes.
    """
    profile = {name: teacher.get(name) for name in teacher_columns if name != "id"}
    english = profile.get("teacher_en")
    romanization = profile.get("preferred_romanization")
    match = re.fullmatch(r"Teacher ([A-X])", english if isinstance(english, str) else "")
    _require(match is not None, "Next package teacher English name is outside the approved Teacher A..X profile")
    suffix = match.group(1)
    _require(
        romanization == f"{SOURCE_TEACHER_ENGLISH_BASE}-{suffix}",
        "Next package teacher romanization differs from its approved Teacher-A..X profile",
    )
    profile["preferred_romanization"] = f"{SOURCE_TEACHER_ENGLISH_BASE}-{suffix.lower()}"
    return profile


def validate_apply_preparation(apply: dict[str, Any], cancel: dict[str, Any]) -> dict[str, Any]:
    """Prove the apply clone differs only by approved namespacing/schedule clearing."""
    _require(set(apply.get("tables", {})) == set(cancel.get("tables", {})), "apply and cancel prepared schemas differ")
    checked_tables = 0
    for table in cancel["tables"]:
        cancel_value = cancel["tables"][table]
        apply_value = apply["tables"][table]
        _require(cancel_value.get("schema") == apply_value.get("schema"), f"apply preparation changed schema for {table}")
        columns = [str(column[1]) for column in cancel_value.get("schema", [])]
        if table in {"class_times", "class_intensive_times"}:
            _require(not apply_value.get("rows"), f"apply preparation did not clear {table}")
            continue
        expected = collections.Counter(
            _expected_apply_preparation_row(table, columns, row)
            for row in cancel_value.get("rows", [])
        )
        actual = collections.Counter(tuple(row) for row in apply_value.get("rows", []))
        _require(actual == expected, f"apply preparation changed unapproved rows or references in {table}")
        checked_tables += 1
    return {"nonScheduleTablesCompared": checked_tables, "scheduleTablesCleared": ["class_times", "class_intensive_times"], "preservedRowsAndRelations": True}


def _expected_import_rows(before: dict[str, Any], package: dict[str, Any]) -> dict[str, collections.Counter[tuple[Any, ...]]]:
    """Project the exported package into its exact expected destination rows."""
    package_classes = package.get("classes")
    package_teachers = package.get("teachers")
    _require(isinstance(package_classes, list) and isinstance(package_teachers, list), "Next export package lacks classes/teachers arrays")
    _require(len(package_classes) == 48 and len(package_teachers) == 24, "Next package does not contain exactly 48 classes and 24 teachers")

    def rows(table: str) -> list[dict[str, Any]]:
        return _row_dicts(before, table)[1]

    next_class_id = max(int(row["id"]) for row in rows("classes")) + 1
    next_teacher_id = max(int(row["id"]) for row in rows("teachers")) + 1
    next_evaluation_id = max(int(row["id"]) for row in rows("speaking_evaluations")) + 1
    teacher_ids: dict[str, int] = {}
    for offset, teacher in enumerate(package_teachers):
        key = str(teacher.get("key", ""))
        _require(key and key not in teacher_ids, "Next package has a missing or duplicate teacher key")
        teacher_ids[key] = next_teacher_id + offset
    class_ids: dict[str, int] = {}
    for offset, transfer_class in enumerate(package_classes):
        key = str(transfer_class.get("key", ""))
        _require(key and key not in class_ids, "Next package has a missing or duplicate class key")
        class_ids[key] = next_class_id + offset

    expected: dict[str, collections.Counter[tuple[Any, ...]]] = {
        "classes": collections.Counter(),
        "app_settings": _expected_classes_navigator_default_rows(),
        "teachers": collections.Counter(),
        "class_info": collections.Counter(),
        "class_times": collections.Counter(),
        "class_intensive_times": collections.Counter(),
        "roster_columns": collections.Counter(),
        "roster_data": collections.Counter(),
        "speaking_evaluations": collections.Counter(),
        "speaking_eval_data": collections.Counter(),
    }
    teacher_columns = _row_dicts(before, "teachers")[0]
    for teacher in package_teachers:
        profile = _expected_import_teacher_profile(teacher, teacher_columns)
        profile["id"] = teacher_ids[str(teacher["key"])]
        expected["teachers"].update([tuple(profile.get(column) for column in teacher_columns)])

    roster_cell_total = 0
    evaluation_value_total = 0
    regular_total = 0
    intensive_total = 0
    evaluation_total = 0
    for transfer_class in package_classes:
        class_key = str(transfer_class["key"])
        class_id = class_ids[class_key]
        class_name = transfer_class.get("name")
        expected["classes"].update([(class_id, class_name)])
        teacher_ref = transfer_class.get("teacher_ref")
        teacher_id = teacher_ids.get(str(teacher_ref)) if teacher_ref else None
        info = transfer_class.get("info") or {}
        expected["class_info"].update([(
            class_id,
            teacher_id,
            info.get("class_grade", ""),
            info.get("class_level", ""),
            info.get("reading_book", ""),
            info.get("essay_book", ""),
            info.get("class_color", ""),
            info.get("font_color", ""),
            info.get("notes", ""),
            info.get("time_filler_activities", ""),
        )])
        for json_name, table in (("regular_times", "class_times"), ("intensive_times", "class_intensive_times")):
            times = info.get(json_name) or []
            for time in times:
                expected[table].update([(class_id, time.get("day"), time.get("start_time"), time.get("end_time"))])
            if table == "class_times":
                regular_total += len(times)
            else:
                intensive_total += len(times)

        roster = transfer_class.get("roster") or {}
        columns = roster.get("columns") or []
        widths = roster.get("column_widths") or []
        for position, name in enumerate(columns):
            width = widths[position] if position < len(widths) else 0
            expected["roster_columns"].update([(class_id, name, position, width)])
        for row_index, row in enumerate(roster.get("rows") or []):
            for column_index in range(len(columns)):
                value = row[column_index] if column_index < len(row) else ""
                if value is None:
                    value = ""
                if value != "":
                    roster_cell_total += 1
                    expected["roster_data"].update([(class_id, row_index, column_index, value)])

        evaluations = transfer_class.get("speaking_evaluations") or []
        for evaluation in evaluations:
            evaluation_id = next_evaluation_id + evaluation_total
            evaluation_total += 1
            expected["speaking_evaluations"].update([(evaluation_id, class_id, evaluation.get("name", ""))])
            source_rows = evaluation.get("rows") or []
            _require(len(source_rows) <= 100, "package evaluation exceeds the current 100-row schema limit")
            evaluation_value_total += _count_nonempty_evaluation_cells(source_rows)
            for row_index in range(100):
                source_row = source_rows[row_index] if row_index < len(source_rows) else []
                values: list[Any] = []
                for column_index in range(11):
                    value = source_row[column_index] if column_index < len(source_row) else ""
                    if value is None:
                        value = ""
                    values.append(value)
                expected["speaking_eval_data"].update([(evaluation_id, row_index, *values)])

    _require(regular_total == 16, f"Next package has {regular_total} regular schedule rows, expected 16")
    _require(intensive_total == 24, f"Next package has {intensive_total} intensive schedule rows, expected 24")
    _require(roster_cell_total == 3600, f"Next package has {roster_cell_total} nonempty roster cells, expected 3,600")
    _require(evaluation_value_total == 6600, f"Next package has {evaluation_value_total} evaluation cells, expected 6,600")
    _require(evaluation_total == 20, f"Next package has {evaluation_total} evaluations, expected 20")
    return expected


def validate_apply_delta(before: dict[str, Any], after: dict[str, Any], package: dict[str, Any]) -> dict[str, Any]:
    expected = _expected_import_rows(before, package)
    actual_counts: dict[str, int] = {}
    for table in before.get("tables", {}):
        before_names, before_rows = _row_dicts(before, table)
        after_names, after_rows = _row_dicts(after, table)
        _require(before_names == after_names, f"apply changed schema for {table}")
        before_counter = collections.Counter(tuple(row[column] for column in before_names) for row in before_rows)
        after_counter = collections.Counter(tuple(row[column] for column in after_names) for row in after_rows)
        _require(not (before_counter - after_counter), f"apply removed or modified existing rows in {table}")
        added_counter = after_counter - before_counter
        actual_counts[table] = sum(added_counter.values())
        if table == "class_times" or table == "class_intensive_times":
            projected = _row_projection(after_rows, ("class_id", "day", "start_time", "end_time")) - _row_projection(before_rows, ("class_id", "day", "start_time", "end_time"))
        elif table == "roster_columns":
            projected = _row_projection(after_rows, ("class_id", "name", "position", "width")) - _row_projection(before_rows, ("class_id", "name", "position", "width"))
        else:
            projected = added_counter
        expected_counter = expected.get(table, collections.Counter())
        _require(projected == expected_counter, f"apply persisted fields or relationships differ from the exact Next package in {table}")
    _require(actual_counts.get("classes") == 48, "apply did not add exactly 48 classes")
    _require(actual_counts.get("teachers") == 24, "apply did not add exactly 24 teachers")
    _require(actual_counts.get("class_info") == 48, "apply did not add exactly 48 class-info relations")
    _require(actual_counts.get("class_times") == 16, "apply did not add exactly 16 regular schedules")
    _require(actual_counts.get("class_intensive_times") == 24, "apply did not add exactly 24 intensive schedules")
    _require(actual_counts.get("roster_data") == 3600, "apply did not add exactly 3,600 roster cells")
    _require(actual_counts.get("speaking_evaluations") == 20, "apply did not add exactly 20 speaking evaluations")
    _require(actual_counts.get("speaking_eval_data") == 2000, "apply did not add the expected 2,000 fixed-schema evaluation rows")
    return {
        "tableRowDeltas": actual_counts,
        "classesAdded": 48,
        "teachersAdded": 24,
        "regularSchedulesAdded": 16,
        "intensiveSchedulesAdded": 24,
        "rosterCellsAdded": 3600,
        "evaluationCellsFromPackage": 6600,
        "evaluationStorageRowsAdded": 2000,
        "classesNavigatorDefaultSettingsAdded": 3,
        "productNormalizedPackageFields": {
            "teachers": ["preferred_romanization"],
            "rule": "TeacherValidator::normalized via StudentNameUtils::normalizeEnglishName; the validated Teacher-A..X tokens persist as Teacher-a..x",
        },
        "allPackageFieldsAndRelationsMatchAfterProductNormalization": True,
    }


def _validate_inventory(manifest: dict[str, Any], root: Path) -> None:
    source = manifest.get("source") or {}
    source_path = _resolve_evidence_path(root, str(source.get("inventoryPath", "")), "source inventory")
    source_document = _read_json(source_path)
    rows = source_document.get("files")
    _require(isinstance(rows, list) and rows, "source inventory has no measurement rows")
    _require(inventory_fingerprint(rows) == str(source.get("sha256", "")), "source inventory rows do not match manifest source fingerprint")
    _require(source_document.get("sha256") == source.get("sha256"), "source inventory top-level hash differs from manifest")
    _require(source_document.get("fileCount") == len(rows), "source inventory file count differs from rows")
    _require(source_document.get("root") == manifest.get("repositoryRoot"), "source inventory root differs from manifest")
    for row in rows:
        path = Path(str(row["path"]))
        full = Path(str(source_document["root"])) / Path(*path.parts)
        _require(full.is_file(), f"measured source input is missing: {full}")
        _require(full.stat().st_size == int(row["sizeBytes"]) and _file_sha256(full) == str(row["sha256"]).lower(), f"measured source input changed: {full}")

    build_link = manifest.get("releaseBuildReceipt") or {}
    receipt_path = Path(str(build_link.get("path", ""))).resolve(strict=True)
    _require(_file_sha256(receipt_path) == str(build_link.get("sha256", "")), "Release receipt SHA-256 does not match manifest")
    receipt = _read_json(receipt_path)
    _require(receipt.get("status") == "succeeded" and receipt.get("taskId") == TASK_ID, "Release build receipt is not a successful P9-11 receipt")
    _require(receipt.get("releaseExecutableSha256") == build_link.get("releaseExecutableSha256"), "Release executable hash differs across receipt link")
    _require(receipt.get("configuration") == "Release" and receipt.get("platform") == "x64", "build receipt is not Release x64")
    _require(build_link.get("buildSourceSha256") == receipt.get("sourceSha256"), "manifest build-source fingerprint differs from original receipt")
    _require(build_link.get("currentSourceSha256") == source.get("sha256"), "manifest current-source fingerprint differs from measured source")
    if receipt.get("sourceSha256") == source.get("sha256"):
        _require(not build_link.get("derivedReceiptPath"), "identical source run unexpectedly links a derived receipt")
        executable = Path(str(receipt.get("releaseExecutablePath", ""))).resolve(strict=True)
        _require(_file_sha256(executable) == str(receipt.get("releaseExecutableSha256", "")), "Release executable does not match build receipt")
    else:
        _validate_derived_build_reuse(manifest, root, rows, receipt)

    package = manifest.get("package") or {}
    package_path = _resolve_evidence_path(root, str(package.get("directory", "")), "packaged application")
    package_inventory_path = _resolve_evidence_path(root, str(manifest.get("files", {}).get("packageInventory", "")), "package inventory")
    package_inventory = _read_json(package_inventory_path)
    package_rows = package_inventory.get("files")
    _require(isinstance(package_rows, list) and package_rows, "package inventory has no files")
    _require(inventory_fingerprint(package_rows) == package.get("sha256"), "package file inventory fingerprint differs from manifest")
    _require(package_inventory.get("sha256") == package.get("sha256") and package_inventory.get("fileCount") == len(package_rows), "package inventory metadata differs from manifest")
    _require(str(package_inventory.get("root", "")).casefold() == str(package_path).casefold(), "package inventory root differs from the current package directory")
    for row in package_rows:
        path = _resolve_evidence_path(package_path, str(row["path"]), "package file")
        _require(path.stat().st_size == int(row["sizeBytes"]) and _file_sha256(path) == str(row["sha256"]).lower(), f"packaged file changed: {path}")
    app_path = _resolve_evidence_path(package_path, str(package.get("applicationRelativePath", "")), "packaged executable")
    _require(_file_sha256(app_path) == str(package.get("applicationSha256", "")), "packaged executable hash differs from manifest")
    _require(_file_sha256(app_path) == str(receipt.get("releaseExecutableSha256", "")), "packaged executable differs from the original Release build hash")
    _require(package.get("candidateSourceSha256") == source.get("sha256"), "package is not bound to the measured current source")
    exported = manifest.get("exportedPackage") or {}
    exported_path = _resolve_evidence_path(root, str(exported.get("path", "")), "Next-exported package")
    _require(exported_path.parent == package_path and exported_path.name == "next-exported-classes.json", "Next export output is outside the packaged app directory")
    expected_package_paths = {str(row["path"]) for row in package_rows}
    expected_package_paths.add(exported_path.relative_to(package_path).as_posix())
    _require(set(_inventory_files_on_disk(package_path)) == expected_package_paths, "packaged application directory contains unexpected or missing files")


def _validate_preparation(manifest: dict[str, Any], root: Path) -> dict[str, dict[str, Any]]:
    entries = manifest.get("workspacePreparation")
    _require(isinstance(entries, list) and len(entries) == 3, "workspace preparation must contain exactly three role receipts")
    by_role: dict[str, dict[str, Any]] = {}
    for entry in entries:
        role = str(entry.get("role", ""))
        _require(role in {"source", "apply_destination", "cancel_destination"} and role not in by_role, f"unexpected or duplicate preparation role {role}")
        workspace = _resolve_evidence_path(root, str(entry.get("path", "")), f"{role} prepared workspace")
        _require(_file_sha256(workspace) == entry.get("sha256"), f"{role} prepared workspace hash mismatch")
        receipt_path = _resolve_evidence_path(root, str(entry.get("receiptPath", "")), f"{role} preparation receipt")
        _require(_file_sha256(receipt_path) == entry.get("receiptSha256"), f"{role} preparation receipt hash mismatch")
        receipt = _read_json(receipt_path)
        _require(receipt.get("role") == role and receipt.get("workspaceSha256") == entry.get("sha256"), f"{role} preparation receipt identity mismatch")
        transformation_hash = _sha256(_canonical(receipt.get("transformations")))
        _require(transformation_hash == receipt.get("transformationSha256") == entry.get("transformationSha256"), f"{role} transformation hash mismatch")
        snapshot_path = _resolve_evidence_path(root, str(entry.get("preparedSnapshotPath", "")), f"{role} prepared snapshot")
        _require(_file_sha256(snapshot_path) == entry.get("preparedSnapshotSha256"), f"{role} prepared snapshot file hash mismatch")
        snapshot = _read_json(snapshot_path)
        _require(snapshot.get("tableContentSha256") == entry.get("preparedTableContentSha256"), f"{role} prepared table content hash mismatch")
        _require(snapshot.get("integrityCheck") == "ok" and not snapshot.get("foreignKeyErrors"), f"{role} prepared workspace integrity failed")
        _require(snapshot.get("workspaceFileSha256") == entry.get("sha256"), f"{role} snapshot refers to different workspace bytes")
        _require(receipt.get("finalCounts") == entry.get("finalCounts"), f"{role} final counts differ from preparation receipt")
        for table, expected_count in EXPECTED_TABLE_COUNTS.items():
            if role == "source" and table == "class_times":
                expected_count = 400
            if role == "apply_destination" and table in {"class_times", "class_intensive_times"}:
                expected_count = 0
            _require(snapshot["tables"][table]["rowCount"] == expected_count, f"{role} {table} count is not {expected_count}")
        by_role[role] = {"entry": entry, "receipt": receipt, "snapshot": snapshot}
    source = by_role["source"]
    apply = by_role["apply_destination"]
    cancel = by_role["cancel_destination"]
    master = manifest.get("masterWorkspace") or {}
    master_path = _resolve_evidence_path(root, str(master.get("path", "")), "master blank workspace")
    _require(_file_sha256(master_path) == master.get("sha256"), "master blank workspace hash mismatch")
    seed_path = Path(str(master.get("seedSqlPath", ""))).resolve(strict=True)
    _require(_file_sha256(seed_path) == master.get("seedSqlSha256"), "large-startup seed SQL hash mismatch")
    _require(source["entry"]["blankWorkspaceSha256"] == apply["entry"]["blankWorkspaceSha256"] == cancel["entry"]["blankWorkspaceSha256"] == master.get("sha256"), "prepared workspaces do not share one master blank seed")
    source_transform = source["receipt"]["transformations"].get("sourceScheduleTransformation")
    _require(source_transform == {
        "selectedClassIds": [1, 48],
        "retainedRegularClassIds": [1, 9],
        "removedRegularRows": 368,
        "retainedSelectedRegularRows": 16,
        "retainedSelectedIntensiveRows": 24,
        "regularIncomingConflictCount": 0,
        "intensiveIncomingConflictCount": 0,
        "retainedScheduleValuesInvented": False,
    }, "source workspace schedule transformation differs from the approved plan")
    teacher_profile_transform = source["receipt"]["transformations"].get("sourceTeacherProfileTransformation")
    _require(isinstance(teacher_profile_transform, dict), "source workspace teacher profile transformation is missing")
    class_detail_transform = source["receipt"]["transformations"].get("sourceClassDetailTransformation")
    _require(isinstance(class_detail_transform, dict), "source workspace class detail transformation is missing")
    validate_source_preparation(
        source["snapshot"],
        cancel["snapshot"],
        teacher_profile_transform,
        class_detail_transform,
    )
    apply_transform = apply["receipt"]["transformations"]
    identity = apply_transform
    schedule_transform = identity.get("destinationScheduleTransformation") or {}
    _require(schedule_transform.get("clearedTables") == ["class_times", "class_intensive_times"], "apply clone did not clear both schedule tables")
    _require(schedule_transform.get("removedRows") == {"class_times": 768, "class_intensive_times": 24}, "apply clone schedule row-removal counts differ")
    _require(identity.get("classIdOffset") == 10000 and identity.get("teacherIdOffset") == 20000 and identity.get("evaluationIdOffset") == 30000, "apply clone identity offsets differ")
    _require(cancel["receipt"]["transformations"].get("role") == "cancel_destination" and cancel["receipt"]["transformations"].get("destinationScheduleTransformation") is None, "cancel clone was transformed")
    _require(_same_tables(cancel["snapshot"], by_role["cancel_destination"]["snapshot"]), "cancel clone is not the untouched seeded workspace")
    apply["preparationValidation"] = validate_apply_preparation(apply["snapshot"], cancel["snapshot"])
    return by_role


def _validate_diagnostics(route: dict[str, Any], root: Path) -> list[dict[str, Any]]:
    diagnostic_path = _resolve_evidence_path(root, str(route.get("diagnosticPath", "")), f"{route.get('routeId')} class transfer diagnostics")
    _require(_file_sha256(diagnostic_path) == route.get("diagnosticSha256"), f"{route.get('routeId')} diagnostic hash mismatch")
    try:
        records = [json.loads(line) for line in diagnostic_path.read_text(encoding="utf-8").splitlines() if line]
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationFailure(f"could not read class transfer diagnostics: {error}") from error
    pid = int(route["processIdentity"]["pid"])
    _require(records and all(record.get("process_id") == pid for record in records), f"{route.get('routeId')} diagnostics are not bound to its process")
    _require(all(record.get("schema") == "classmngr-next-class-transfer-checkpoint-v1" and record.get("event") == "class-transfer-checkpoint" for record in records), f"{route.get('routeId')} contains an unexpected diagnostic schema")
    sequences = [int(record["sequence"]) for record in records]
    _require(sequences == sorted(sequences) and len(sequences) == len(set(sequences)), f"{route.get('routeId')} diagnostic sequence is not strictly increasing")
    _require(all(record.get("page_key") == "class_transfer" for record in records), f"{route.get('routeId')} diagnostics contain a different page key")
    return records


def validate_lifecycle_checkpoints(route: dict[str, Any], diagnostics: list[dict[str, Any]]) -> None:
    """Require one explicit Calendar cycle, plus the app's automatic apply page switch."""
    route_id = str(route.get("routeId", ""))
    lifecycle = route.get("lifecycle") or {}
    _require(lifecycle.get("suspendCount") == 1 and lifecycle.get("reentryCount") == 1, f"{route_id} did not perform exactly one page suspend/re-entry")
    activated = [record for record in diagnostics if record.get("checkpoint") == "activated"]
    suspended = [record for record in diagnostics if record.get("checkpoint") == "suspended"]
    apply_route = route_id == "route-2-next-apply"
    expected_activations, expected_suspensions = (3, 3) if apply_route else (2, 2)
    _require(len(activated) == expected_activations and len(suspended) == expected_suspensions, f"{route_id} diagnostic trace does not contain the expected app transition plus one explicit Calendar suspend/re-entry")
    tokens = [str(record.get("metrics", {}).get("page_instance_token", "")) for record in activated + suspended]
    _require(len(set(tokens)) == 1 and tokens[0], f"{route_id} suspend/re-entry did not retain the measured page instance")
    if apply_route:
        apply_suspended = lifecycle.get("applyAutomaticSuspension") or {}
        pre_calendar = lifecycle.get("preCalendarReentry") or {}
        _require(
            int(activated[0].get("sequence", 0)) < int(suspended[0].get("sequence", 0)) < int(activated[1].get("sequence", 0)) < int(suspended[1].get("sequence", 0)) < int(activated[2].get("sequence", 0)) < int(suspended[2].get("sequence", 0)),
            f"{route_id} automatic Classes transition and explicit Calendar lifecycle checkpoints are misordered",
        )
        _require(apply_suspended.get("captured_at_utc") == suspended[0].get("captured_at_utc") and int((apply_suspended.get("metrics") or {}).get("imports_applied", 0)) == 1, f"{route_id} lacks the measured apply-triggered Class Transfer suspension")
        _require(pre_calendar.get("captured_at_utc") == activated[1].get("captured_at_utc") and int((pre_calendar.get("metrics") or {}).get("imports_applied", 0)) == 1, f"{route_id} lacks the same-instance Class Transfer re-entry before its Calendar check")
        _require("navigation event=page-route outcome=completed cause=selection from=class_transfer to=classes" in str(lifecycle.get("applyToClassesRouteLogLine", "")), f"{route_id} lacks the apply-triggered Classes route log line")
        _require("navigation event=page-route outcome=completed cause=selection from=classes to=class_transfer" in str(lifecycle.get("classesToClassTransferRouteLogLine", "")), f"{route_id} lacks the measured Class Transfer restore route log line")
    else:
        _require(int(activated[0].get("sequence", 0)) < int(suspended[0].get("sequence", 0)) < int(activated[1].get("sequence", 0)) < int(suspended[1].get("sequence", 0)), f"{route_id} page lifecycle checkpoints are misordered")
    shutdown_suspension = lifecycle.get("shutdownSuspension") or {}
    _require(
        shutdown_suspension.get("captured_at_utc") == suspended[-1].get("captured_at_utc")
        and int(shutdown_suspension.get("sequence", 0)) == int(suspended[-1].get("sequence", -1))
        and int(suspended[-1].get("sequence", 0)) > int(activated[-1].get("sequence", 0))
        and diagnostics[-1] == suspended[-1],
        f"{route_id} lacks the terminal page suspension recorded during natural shutdown",
    )


def _validate_route_identity(route: dict[str, Any], manifest: dict[str, Any], root: Path) -> list[dict[str, Any]]:
    route_id = str(route.get("routeId", ""))
    pid = int(route.get("processIdentity", {}).get("pid", 0))
    process_identity = route.get("processIdentity") or {}
    _require(pid > 0 and int(process_identity.get("mainWindowHandle", 0)) > 0, f"{route_id} lacks a correlated PID/HWND")
    _require(int(process_identity.get("mainWindowProcessId", 0)) == pid, f"{route_id} main window HWND belongs to another process")
    _require(process_identity.get("architecture") == "x64" and process_identity.get("sha256") == manifest.get("application", {}).get("sha256"), f"{route_id} process image differs from packaged x64 app")
    validate_process_exit(route)
    diag = _validate_diagnostics(route, root)
    validate_lifecycle_checkpoints(route, diag)
    metrics_path = _resolve_evidence_path(root, str(manifest.get("files", {}).get("processMetrics", "")), "process metrics")
    try:
        samples = [json.loads(line) for line in metrics_path.read_text(encoding="utf-8").splitlines() if line and json.loads(line).get("routeId") == route_id]
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationFailure(f"could not read process memory samples: {error}") from error
    required_points = {"startup-ready-checkpoint", "startup-ready-external", "startup-settled", "post-cleanup"}
    required_points.update(sample["point"] for sample in samples if str(sample.get("point", "")).startswith("post-cleanup-idle-tail"))
    points = {str(sample.get("point")) for sample in samples}
    _require(required_points <= points, f"{route_id} lacks required startup/post-cleanup working-set samples")
    for sample in samples:
        _require(int(sample.get("pid", 0)) == pid and sample.get("sha256") == process_identity.get("sha256"), f"{route_id} memory sample is bound to another process image")
        if sample.get("point") in required_points:
            _require(int(sample.get("currentWorkingSetBytes", 0)) < WORKING_SET_LIMIT, f"{route_id} strict working-set gate failed at {sample.get('point')}")
    route["memory"]["validatedMaximumOperationPeakBytes"] = max((int(sample.get("peakWorkingSetBytes", 0)) for sample in samples), default=0)
    route["memory"]["operationPeakAdvisoryExceeded"] = route["memory"]["validatedMaximumOperationPeakBytes"] >= OPERATION_PEAK_LIMIT
    return diag


def validate_process_exit(route: dict[str, Any]) -> None:
    """Require explicit evidence that the process exited naturally with code 0."""
    route_id = str(route.get("routeId", "unknown-route"))
    process_exit = route.get("processExit")
    _require(isinstance(process_exit, dict), f"{route_id} lacks process-exit evidence")
    _require(
        route.get("normalExit") is True
        and route.get("processExitCode") == 0
        and process_exit.get("exitCode") == 0
        and process_exit.get("waitCompleted") is True
        and process_exit.get("forcedTerminationForCleanup") is False,
        f"{route_id} did not exit naturally with code 0",
    )


def _validate_uia_action_binding(
    action: dict[str, Any],
    *,
    route_id: str,
    action_kind: str,
    expected_name: str,
    pid: int,
    hwnd: int,
) -> None:
    _require(action.get("actionKind") == action_kind, f"{route_id} UIA action kind mismatch: {action_kind}")
    target = action.get("target") or {}
    _require(target.get("name") == expected_name, f"{route_id} UIA target name mismatch for {action_kind}")
    _require(target.get("processId") == pid, f"{route_id} UIA element belongs to another process for {action_kind}")
    _require(target.get("offscreen") is False and target.get("enabled") is True, f"{route_id} UIA target is hidden or disabled for {action_kind}")
    _require(int(action.get("windowHandle", 0)) == hwnd and int(action.get("windowProcessId", 0)) == pid, f"{route_id} UIA window is not bound to its measured PID for {action_kind}")
    _require(int(action.get("mainWindowHandle", 0)) == hwnd and int(action.get("mainWindowProcessId", 0)) == pid, f"{route_id} UIA action main HWND/PID correlation failed for {action_kind}")
    _require(action.get("method") == "uia-patterns-plus-pid-bound-window-input" or action.get("method") == "uia-invoke-pattern", f"{route_id} UIA action used an unexpected method for {action_kind}")
    activation = action.get("nativeActivation")
    if action.get("method") == "uia-patterns-plus-pid-bound-window-input":
        _require(isinstance(activation, dict) and activation.get("expectedProcessId") == pid and activation.get("windowProcessId") == pid, f"{route_id} native UIA input lacks PID-bound window evidence for {action_kind}")


def _validate_uia_control_action(
    action: dict[str, Any],
    *,
    route_id: str,
    action_kind: str,
    automation_id_suffix: str,
    pid: int,
    main_hwnd: int,
    action_hwnd: int | None = None,
) -> None:
    _require(action.get("actionKind") == action_kind, f"{route_id} UIA control action kind mismatch: {action_kind}")
    target = action.get("target") or {}
    _require(str(target.get("automationId", "")).endswith(automation_id_suffix), f"{route_id} UIA control ID mismatch for {action_kind}")
    _require(target.get("processId") == pid and target.get("offscreen") is False and target.get("enabled") is True, f"{route_id} UIA control identity/visibility mismatch for {action_kind}")
    expected_hwnd = main_hwnd if action_hwnd is None else action_hwnd
    _require(int(action.get("windowHandle", 0)) == expected_hwnd and int(action.get("windowProcessId", 0)) == pid, f"{route_id} UIA action window PID/HWND mismatch for {action_kind}")
    _require(int(action.get("mainWindowHandle", 0)) == main_hwnd and int(action.get("mainWindowProcessId", 0)) == pid, f"{route_id} UIA control action main PID/HWND mismatch for {action_kind}")
    _require(action.get("method") == "uia-invoke-pattern", f"{route_id} UIA control action did not use InvokePattern for {action_kind}")


def validate_review_table_uia(route: dict[str, Any], pid: int) -> None:
    """Require both preview tables to be visible, live UIA controls in the measured process."""
    route_id = str(route.get("routeId", "unknown-route"))
    uia = route.get("uia") or {}
    for table_key, object_name in (("classReviewTable", "classTransferClassReviewTable"), ("teacherReviewTable", "classTransferTeacherReviewTable")):
        table = uia.get(table_key) or {}
        control = table.get("control") or {}
        _require(control.get("automationId", "").endswith(object_name) and control.get("controlType") == "ControlType.Table" and control.get("processId") == pid and control.get("offscreen") is False, f"{route_id} {object_name} is not a visible PID-bound UIA table")
        _require(int(control.get("rectangle", {}).get("width", 0)) > 0 and int(control.get("rectangle", {}).get("height", 0)) > 0 and int(table.get("descendantCount", 0)) > 0, f"{route_id} {object_name} lacks visible row evidence")


def validate_post_apply_page_uia(route: dict[str, Any], pid: int) -> dict[str, Any]:
    """Prove the app selected the imported class in its visible Classes page."""
    route_id = str(route.get("routeId", "unknown-route"))
    page = ((route.get("uia") or {}).get("postApplyPage") or {})
    root = page.get("root") or {}
    title = page.get("title") or {}
    _require(
        str(root.get("automationId", "")).endswith("classesNavigatorPage")
        and root.get("controlType") == "ControlType.Group"
        and root.get("processId") == pid
        and root.get("offscreen") is False
        and int(root.get("rectangle", {}).get("width", 0)) > 0
        and int(root.get("rectangle", {}).get("height", 0)) > 0,
        f"{route_id} post-apply Classes page is not a visible PID-bound UIA group",
    )
    _require(
        str(title.get("automationId", "")).endswith("classesNavigatorTitle")
        and title.get("controlType") == "ControlType.Text"
        and title.get("processId") == pid
        and title.get("offscreen") is False
        and title.get("name") == "Classes",
        f"{route_id} post-apply Classes page title is not visible or PID-bound",
    )
    details = page.get("selectedClassDetails") or {}
    required = {
        "classGrade": "classEditorClassGrade",
        "classLevel": "classEditorClassLevel",
        "readingBook": "classEditorReadingBook",
        "essayBook": "classEditorEssayBook",
    }
    for key, automation_id in required.items():
        control = details.get(key) or {}
        _require(
            str(control.get("automationId", "")).endswith(automation_id)
            and control.get("controlType") == "ControlType.ComboBox"
            and control.get("processId") == pid
            and control.get("offscreen") is False
            and control.get("enabled") is True
            and "value" in control
            and control.get("value") is not None,
            f"{route_id} post-apply selected-class {key} is not a visible PID-bound control with a value",
        )
    return details


def validate_post_apply_selection_matches_package(
    route: dict[str, Any], package: dict[str, Any]
) -> None:
    """Match the UI-selected class details to the exact first exported row."""
    route_id = str(route.get("routeId", "unknown-route"))
    pid = int(route.get("processIdentity", {}).get("pid", 0))
    details = validate_post_apply_page_uia(route, pid)
    classes = package.get("classes")
    _require(isinstance(classes, list) and classes, f"{route_id} exported package has no first class row")
    first_class_info = classes[0].get("info") or {}
    for ui_key, package_key in (
        ("classGrade", "class_grade"),
        ("classLevel", "class_level"),
        ("readingBook", "reading_book"),
        ("essayBook", "essay_book"),
    ):
        _require(
            details[ui_key].get("value") == first_class_info.get(package_key, ""),
            f"{route_id} selected-class UIA {ui_key} differs from the first exported class",
        )


def validate_route_uia(route: dict[str, Any]) -> None:
    route_id = str(route.get("routeId", ""))
    pid = int(route.get("processIdentity", {}).get("pid", 0))
    hwnd = int(route.get("processIdentity", {}).get("mainWindowHandle", 0))
    uia = route.get("uia") or {}
    initial = uia.get("initialPage") or {}
    _require(initial.get("automationId", "").endswith("nextClassTransferPage") and initial.get("controlType") == "ControlType.Group" and initial.get("processId") == pid and initial.get("offscreen") is False, f"{route_id} initial page is not an authentic PID-bound Class Transfer UIA element")
    _validate_uia_action_binding(
        uia.get("navigationClassTransfer") or {},
        route_id=route_id,
        action_kind="navigate-to-class-transfer",
        expected_name="Class Transfer",
        pid=pid,
        hwnd=hwnd,
    )
    _validate_uia_action_binding(
        uia.get("navigateToCalendar") or {},
        route_id=route_id,
        action_kind="suspend-class-transfer-by-calendar-navigation",
        expected_name="Calendar",
        pid=pid,
        hwnd=hwnd,
    )
    _validate_uia_action_binding(
        uia.get("navigateBackToClassTransfer") or {},
        route_id=route_id,
        action_kind="reenter-class-transfer",
        expected_name="Class Transfer",
        pid=pid,
        hwnd=hwnd,
    )
    calendar = uia.get("calendarPage") or {}
    indicator_kind = uia.get("calendarPageIndicatorKind")
    expected_calendar_suffix = {"root": "nextCalendarPage", "title": "calendarPageTitle", "status": "calendarPageStatus"}.get(str(indicator_kind))
    expected_calendar_type = "ControlType.Group" if indicator_kind == "root" else "ControlType.Text"
    _require(expected_calendar_suffix is not None and str(calendar.get("automationId", "")).endswith(expected_calendar_suffix) and calendar.get("processId") == pid and calendar.get("controlType") == expected_calendar_type and calendar.get("offscreen") is False, f"{route_id} Calendar page UIA identity mismatch")
    _require("navigation event=page-route outcome=completed cause=selection from=class_transfer to=calendar" in str((route.get("lifecycle") or {}).get("calendarRouteLogLine", "")), f"{route_id} lacks the completed Calendar page-route log line")
    operation = route.get("operation") or {}
    file_picker = operation.get("filePicker") or {}
    expected_picker_path = operation.get("outputPath") if route_id == "route-1-next-export" else operation.get("packagePath")
    expected_picker_action = "file-picker-save" if route_id == "route-1-next-export" else "file-picker-open"
    _require(file_picker.get("path") == expected_picker_path and int(file_picker.get("windowHandle", 0)) > 0 and file_picker.get("windowProcessId") == pid, f"{route_id} QFileDialog is not bound to the expected path and process")
    value_control = file_picker.get("valueControl") or {}
    _require(value_control.get("processId") == pid and value_control.get("controlType") == "ControlType.Edit" and value_control.get("value") == expected_picker_path, f"{route_id} QFileDialog path field did not retain the exact path")
    picker_button = file_picker.get("button") or {}
    _require(picker_button.get("processId") == pid and picker_button.get("controlType") == "ControlType.Button" and picker_button.get("enabled") is True, f"{route_id} QFileDialog action button identity mismatch")
    _validate_uia_control_action(file_picker.get("action") or {}, route_id=route_id, action_kind=expected_picker_action, automation_id_suffix=str(picker_button.get("automationId", "")).split(".")[-1], pid=pid, main_hwnd=hwnd, action_hwnd=int(file_picker.get("windowHandle", 0)))
    if route_id == "route-1-next-export":
        export_button = uia.get("exportButtonBefore") or {}
        _require(export_button.get("automationId", "").endswith("classTransferExport") and export_button.get("processId") == pid and export_button.get("enabled") is True, f"{route_id} Export button is not an enabled PID-bound UIA control")
        _validate_uia_control_action(uia.get("exportButtonAction") or {}, route_id=route_id, action_kind="open-next-class-transfer-export-picker", automation_id_suffix="classTransferExport", pid=pid, main_hwnd=hwnd)
    else:
        import_button = uia.get("importButtonBefore") or {}
        _require(import_button.get("automationId", "").endswith("classTransferImport") and import_button.get("processId") == pid and import_button.get("enabled") is True, f"{route_id} Import button is not an enabled PID-bound UIA control")
        _validate_uia_control_action(uia.get("importButtonAction") or {}, route_id=route_id, action_kind="open-next-class-transfer-import-picker", automation_id_suffix="classTransferImport", pid=pid, main_hwnd=hwnd)
        teacher_tab_action = uia.get("teacherReviewTabAction") or {}
        teacher_tab = teacher_tab_action.get("target") or {}
        _require(teacher_tab.get("name") == "Teachers" and teacher_tab.get("controlType") == "ControlType.TabItem", f"{route_id} did not expose the Teachers review tab through UIA")
        _validate_uia_control_action(teacher_tab_action, route_id=route_id, action_kind="select-teacher-review-tab", automation_id_suffix="qt_tabwidget_tabbar", pid=pid, main_hwnd=hwnd)
        _require(teacher_tab.get("offscreen") is False and teacher_tab.get("enabled") is True, f"{route_id} Teachers review tab is hidden or disabled")
        validate_review_table_uia(route, pid)
        if route_id == "route-2-next-apply":
            _validate_uia_control_action(uia.get("applyAction") or {}, route_id=route_id, action_kind="apply-next-class-transfer-package", automation_id_suffix="classTransferApply", pid=pid, main_hwnd=hwnd)
            _validate_uia_action_binding(
                uia.get("navigateBackToClassTransferAfterApply") or {},
                route_id=route_id,
                action_kind="restore-class-transfer-before-calendar-lifecycle",
                expected_name="Class Transfer",
                pid=pid,
                hwnd=hwnd,
            )
            validate_post_apply_page_uia(route, pid)
        else:
            _validate_uia_control_action(uia.get("cancelAction") or {}, route_id=route_id, action_kind="cancel-next-class-transfer-review", automation_id_suffix="classTransferCancelReview", pid=pid, main_hwnd=hwnd)


def validate_export_uia_selection(route: dict[str, Any], expected_names: list[str]) -> None:
    route_id = str(route.get("routeId", ""))
    pid = int(route.get("processIdentity", {}).get("pid", 0))
    uia = route.get("uia") or {}
    rows = uia.get("exportSelection")
    _require(isinstance(rows, list) and len(rows) == 48, f"{route_id} UIA export selection does not contain 48 rows")
    _require(len(expected_names) == 48 and len(set(expected_names)) == 48, "source UIA selection expectations are not 48 unique names")
    observed_names: list[str] = []
    for index, row in enumerate(rows):
        _require(row.get("index") == index and row.get("expectedClassName") == expected_names[index], f"{route_id} UIA export selection index/name mismatch at row {index}")
        identity = row.get("identity") or {}
        _require(identity.get("name") == expected_names[index] and identity.get("processId") == pid and identity.get("controlType") == "ControlType.ListItem", f"{route_id} UIA selected row is not the expected PID-bound ListItem at row {index}")
        _require(identity.get("offscreen") is False and row.get("method") == "uia-listitem-pid-bound-window-input" and row.get("toggleStateAfter") == "On", f"{route_id} UIA selection was not visible and toggled On at row {index}")
        _require(str(identity.get("automationId", "")).endswith("classTransferExportClassList"), f"{route_id} UIA selected row is not under the export list at row {index}")
        activation = row.get("nativeActivation") or {}
        rectangle = identity.get("rectangle") or {}
        _require(activation.get("expectedProcessId") == pid and activation.get("windowProcessId") == pid, f"{route_id} UIA selected row input is not bound to its PID at row {index}")
        _require(activation.get("leftButtonDownPosted") is True and activation.get("leftButtonUpPosted") is True, f"{route_id} UIA selected row lacks a completed click message at row {index}")
        click_x = int(activation.get("screenX", -1))
        click_y = int(activation.get("screenY", -1))
        left = int(rectangle.get("x", -1))
        top = int(rectangle.get("y", -1))
        width = int(rectangle.get("width", 0))
        height = int(rectangle.get("height", 0))
        _require(left <= click_x <= left + min(width, 32) and top <= click_y <= top + height, f"{route_id} UIA click missed the class checkbox region at row {index}")
        observed_names.append(str(identity.get("name", "")))
    _require(observed_names == expected_names, f"{route_id} UIA selected rows are not in source class ID order")
    scrolls = uia.get("exportListScroll")
    _require(isinstance(scrolls, list) and scrolls, f"{route_id} UIA export list did not record the required scroll progression")
    for scroll in scrolls:
        target_list = scroll.get("targetList") or {}
        result = scroll.get("messageResult") or {}
        _require(scroll.get("actionKind") == "scroll-export-class-list" and scroll.get("method") == "uia-list-bound-pid-verified-window-wheel", f"{route_id} UIA scroll action metadata mismatch")
        _require(target_list.get("processId") == pid and target_list.get("controlType") == "ControlType.List" and str(target_list.get("automationId", "")).endswith("classTransferExportClassList"), f"{route_id} UIA scroll target is not the measured export list")
        _require(result.get("expectedProcessId") == pid and result.get("windowProcessId") == pid and result.get("wheelPosted") is True and result.get("wheelDelta") == -120, f"{route_id} UIA scroll input lacks PID-bound window evidence")


def validate_evidence(evidence_dir: Path) -> dict[str, Any]:
    root = evidence_dir.resolve(strict=True)
    manifest = _read_json(root / "manifest.json")
    _require(manifest.get("schema") == MANIFEST_SCHEMA and manifest.get("taskId") == TASK_ID, "manifest schema/task ID mismatch")
    _require(manifest.get("status") == "evidence-collected", f"manifest is not evidence-collected: {manifest.get('status')}")
    _validate_inventory(manifest, root)
    preparation = _validate_preparation(manifest, root)
    routes = manifest.get("routes")
    _require(isinstance(routes, list) and [route.get("routeId") for route in routes] == list(EXPECTED_ROUTE_IDS), "exactly three routes in export/apply/cancel order are required")
    all_pids: set[int] = set()
    diagnostics_by_route: dict[str, list[dict[str, Any]]] = {}
    for route in routes:
        route_id = str(route["routeId"])
        _require(route.get("role") == EXPECTED_ROLES[route_id], f"{route_id} route role mismatch")
        _require(int(route["processIdentity"]["pid"]) not in all_pids, "P9-11 routes reused a process PID")
        all_pids.add(int(route["processIdentity"]["pid"]))
        diagnostics_by_route[route_id] = _validate_route_identity(route, manifest, root)
        validate_route_uia(route)
        if route_id == "route-1-next-export":
            source_rows = _row_dicts(preparation["source"]["snapshot"], "classes")[1]
            expected_names = [str(row["name"]) for row in sorted(source_rows, key=lambda row: int(row["id"])) if 1 <= int(row["id"]) <= 48]
            validate_export_uia_selection(route, expected_names)
        workspace = route.get("workspace") or {}
        route_workspace = _resolve_evidence_path(root, str(workspace.get("path", "")), f"{route_id} runtime workspace")
        _require(workspace.get("sha256AtLaunch") == workspace.get("preparedSha256"), f"{route_id} launch workspace differs from its prepared copy")
        before_path = _resolve_evidence_path(root, str(workspace.get("beforeSnapshot", "")), f"{route_id} before snapshot")
        after_path = _resolve_evidence_path(root, str(workspace.get("afterSnapshot", "")), f"{route_id} after snapshot")
        before = _read_json(before_path)
        after = _read_json(after_path)
        _require(before.get("integrityCheck") == "ok" and after.get("integrityCheck") == "ok" and not before.get("foreignKeyErrors") and not after.get("foreignKeyErrors"), f"{route_id} before/after SQLite integrity failed")
        _require(before.get("tableContentSha256") == workspace.get("beforeTableContentSha256") and after.get("tableContentSha256") == workspace.get("afterTableContentSha256"), f"{route_id} workspace table hash link mismatch")
        expected_role = {"route-1-next-export": "source", "route-2-next-apply": "apply_destination", "route-3-next-cancel": "cancel_destination"}[route_id]
        prepared_entry = preparation[expected_role]["entry"]
        _require(workspace.get("preparedSha256") == prepared_entry.get("sha256"), f"{route_id} runtime workspace is not its prepared role copy")
        if route_id in {"route-1-next-export", "route-3-next-cancel"}:
            _require(workspace.get("rowLevelContentsUnchanged") is True and _same_tables(before, after), f"{route_id} full before/after workspace equality failed")
        route["workspace"]["validatedTableCount"] = int(after.get("tableCount", 0))

    exported = manifest.get("exportedPackage") or {}
    package_path = _resolve_evidence_path(root, str(exported.get("path", "")), "Next-exported package")
    package = _read_json(package_path)
    package_hash = _file_sha256(package_path)
    _require(package_hash == exported.get("sha256"), "Next-exported package SHA-256 mismatch")
    _require(package_hash == routes[0].get("operation", {}).get("packageSha256"), "Route 1 operation did not create manifest package bytes")
    _require(package_path == _resolve_evidence_path(root, str(routes[0].get("operation", {}).get("outputPath", "")), "Route 1 export output"), "Route 1 exported a different package path")
    _require(routes[0].get("operation", {}).get("metrics", {}).get("export_selected_class_count") == 48, "Route 1 did not export exactly 48 selected classes")
    _require(routes[0].get("operation", {}).get("metrics", {}).get("exports_completed") == 1, "Route 1 export completion counter is not one")
    _require(not routes[0].get("operation", {}).get("packageRetained"), "Route 1 retained package data after export")
    _require(len(package.get("classes", [])) == 48 and len(package.get("teachers", [])) == 24, "Next-exported package shape is not 48 classes / 24 teachers")
    validate_post_apply_selection_matches_package(routes[1], package)
    prep_source = preparation["source"]["snapshot"]
    source_classes = _row_dicts(prep_source, "classes")[1]
    selected_names = {str(row["name"]) for row in source_classes if 1 <= int(row["id"]) <= 48}
    package_names = {str(row.get("name", "")) for row in package["classes"]}
    _require(len(package_names) == 48 and package_names == selected_names, "Next-exported class names/order set does not represent source classes 1–48")
    source_route_before = _read_json(_resolve_evidence_path(root, str(routes[0]["workspace"]["beforeSnapshot"]), "Route 1 baseline"))
    source_route_after = _read_json(_resolve_evidence_path(root, str(routes[0]["workspace"]["afterSnapshot"]), "Route 1 final"))
    _require(_same_tables(source_route_before, source_route_after), "Next UI export changed source workspace rows")

    apply_route = routes[1]
    _require(apply_route.get("operation", {}).get("packageSha256") == package_hash, "Apply route did not consume the exact Next-exported package SHA")
    review = apply_route.get("operation", {}).get("reviewMetrics") or {}
    _require(review.get("matching_class_candidate_count") == 0 and review.get("matching_teacher_candidate_count") == 0, "Apply preview contains identity matches")
    _require(review.get("apply_enabled") is True, "Apply preview did not enable Apply")
    _require(review.get("class_review_row_count") == 48 and review.get("teacher_review_row_count") == 24, "Apply preview review-table rows differ")
    terminal = apply_route.get("operation", {}).get("terminalMetrics") or {}
    _require(terminal.get("imports_applied") == 1 and not terminal.get("package_retained") and not terminal.get("preview_retained"), "Apply terminal counters do not prove one operation and state release")
    apply_workspace = _read_json(_resolve_evidence_path(root, str(apply_route["workspace"]["beforeSnapshot"]), "Apply baseline"))
    apply_after = _read_json(_resolve_evidence_path(root, str(apply_route["workspace"]["afterSnapshot"]), "Apply final"))
    apply_result = validate_apply_delta(apply_workspace, apply_after, package)
    apply_route["workspace"]["expectedDeltaVerified"] = True
    apply_route["workspace"]["validatedDelta"] = apply_result

    cancel_route = routes[2]
    _require(cancel_route.get("operation", {}).get("packageSha256") == package_hash, "Cancel route did not consume the exact Next-exported package SHA")
    cancel_review = cancel_route.get("operation", {}).get("reviewMetrics") or {}
    _require(cancel_review.get("package_class_count") == 48 and cancel_review.get("package_teacher_count") == 24, "Cancel route did not review the exact package")
    cancel_terminal = cancel_route.get("operation", {}).get("terminalMetrics") or {}
    _require(cancel_terminal.get("reviews_cancelled") == 1 and not cancel_terminal.get("package_retained") and not cancel_terminal.get("preview_retained") and not cancel_terminal.get("apply_enabled"), "Cancel terminal metrics do not prove explicit cancellation and state release")

    return {
        "schema": "classmngr-phase9-packaged-next-class-transfer-validation-v1",
        "taskId": TASK_ID,
        "status": "passed",
        "evidenceDirectory": str(root),
        "sourceSha256": manifest["source"]["sha256"],
        "releaseExecutableSha256": manifest["application"]["sha256"],
        "packageSha256": package_hash,
        "packageBytes": int(exported.get("sizeBytes", 0)),
        "routeCount": 3,
        "routes": [route["routeId"] for route in routes],
        "sourceTransformation": validate_source_preparation(
            preparation["source"]["snapshot"],
            preparation["cancel_destination"]["snapshot"],
            preparation["source"]["receipt"]["transformations"]["sourceTeacherProfileTransformation"],
            preparation["source"]["receipt"]["transformations"]["sourceClassDetailTransformation"],
        ),
        "applyPreparation": preparation["apply_destination"].get("preparationValidation"),
        "applyDelta": apply_result,
        "naturalExitCodes": [route["processExitCode"] for route in routes],
        "strictWorkingSetGateBytesExclusive": WORKING_SET_LIMIT,
        "operationPeakAdvisoryBytesExclusive": OPERATION_PEAK_LIMIT,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", required=True, type=Path)
    options = parser.parse_args()
    try:
        result = validate_evidence(options.evidence_dir)
    except (ValidationFailure, OSError, ValueError, KeyError, TypeError) as error:
        print(json.dumps({"schema": "classmngr-phase9-packaged-next-class-transfer-validation-v1", "taskId": TASK_ID, "status": "failed", "error": str(error)}, ensure_ascii=False, sort_keys=True))
        return 1
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
