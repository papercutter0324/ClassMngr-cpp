#!/usr/bin/env python3
"""Validate the P9-19 packaged Classes navigation and editor lifecycle evidence."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import struct
import sys
from pathlib import Path
from typing import Any

SCHEMA = "classmngr-phase9-p9-19-packaged-classes-lifecycle-v1"
TASK_ID = "P9_19_PACKAGED_NEXT_CLASSES_NAVIGATION"
BUILD_SCHEMA = "classmngr-phase9-p9-19-exact-release-build-v1"
FIXTURE_SCHEMA = "classmngr-phase9-p9-19-large-startup-fixture-v1"
SWEEP_SCHEMA = "classmngr-phase9-p9-19-class-selection-sweep-v1"
BASE_SHA256 = "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9"
SEED_CANONICAL_SHA256 = "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3"
WORKING_SET_TARGET = 268_435_456
WORKING_SET_CEILING = 471_859_200
OPERATION_PEAK_ADVISORY = 536_870_912
CLASS_LIST_AUTOMATION_ID_SUFFIX = "classesNavigatorClassList"
CLASS_NAMES = [f"Heavy Fixture Class {index:03d}" for index in range(1, 97)]
PRODUCT_ROOTS = [
    "CMakeLists.txt", "CMakePresets.json", "cmake",
    "docs/qt-rewrite/phase-4-resource-inventory.json", "licenses", "resources",
    "scripts/phase4", "src", "tests", "third_party",
]
HARNESS_PATHS = [
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/p9_19_build_current_source_release.ps1",
    "scripts/phase9/prepare_p9_19_large_startup_fixture.py",
    "scripts/phase9/run_packaged_next_p9_19_classes_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_p9_19_classes_lifecycle.py",
    "scripts/phase9/test_prepare_p9_19_large_startup_fixture.py",
    "scripts/phase9/test_validate_packaged_next_p9_19_classes_lifecycle.py",
]
P918_VALIDATOR_PATH = Path(__file__).with_name("validate_packaged_next_p9_18_campus_map.py")
P918_SPEC = importlib.util.spec_from_file_location("p918_formulas_for_p919", P918_VALIDATOR_PATH)
assert P918_SPEC is not None and P918_SPEC.loader is not None
p918 = importlib.util.module_from_spec(P918_SPEC)
P918_SPEC.loader.exec_module(p918)


class ValidationError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_fingerprint(files: list[dict[str, Any]]) -> str:
    ordered = sorted(files, key=lambda row: str(row["path"]).lower())
    canonical = "".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}\n"
        for row in ordered
    )
    return sha256_bytes(canonical.encode("utf-8"))


def canonical_inventory(root: Path) -> dict[str, Any]:
    root = root.resolve(strict=True)
    files: list[dict[str, Any]] = []
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() in {".pyc", ".pyo"}:
            continue
        if any(part in {"__pycache__", ".pytest_cache", ".mypy_cache"} for part in path.parts):
            continue
        resolved = path.resolve(strict=True)
        require(root == resolved.parent or root in resolved.parents, f"Inventory path escapes root: {path}")
        files.append({"path": resolved.relative_to(root).as_posix(), "sizeBytes": resolved.stat().st_size, "sha256": sha256_file(resolved)})
    files.sort(key=lambda row: str(row["path"]).lower())
    return {"root": str(root), "sha256": canonical_fingerprint(files), "fileCount": len(files), "files": files}


def source_inventory(repository: Path, roots: list[str]) -> dict[str, Any]:
    repository = repository.resolve(strict=True)
    files: list[dict[str, Any]] = []
    for relative in roots:
        target = repository / relative
        require(target.exists(), f"Source inventory root is missing: {relative}")
        paths = [target] if target.is_file() else [path for path in target.rglob("*") if path.is_file()]
        for path in paths:
            if path.suffix.lower() in {".pyc", ".pyo"} or any(part in {"__pycache__", ".pytest_cache", ".mypy_cache"} for part in path.parts):
                continue
            resolved = path.resolve(strict=True)
            require(repository == resolved.parent or repository in resolved.parents, f"Source path escapes repository: {path}")
            files.append({"path": resolved.relative_to(repository).as_posix(), "sizeBytes": resolved.stat().st_size, "sha256": sha256_file(resolved)})
    files.sort(key=lambda row: str(row["path"]).lower())
    return {"repositoryRoot": str(repository), "includedRoots": roots, "sha256": canonical_fingerprint(files), "fileCount": len(files), "files": files}


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"Cannot read JSON evidence {path}: {error}") from error
    require(isinstance(value, dict), f"JSON evidence is not an object: {path}")
    return value


def resolve_evidence_path(evidence_root: Path, relative: str) -> Path:
    path = (evidence_root / relative).resolve(strict=True)
    root = evidence_root.resolve(strict=True)
    require(path == root or root in path.parents, f"Evidence path escapes run root: {relative}")
    return path


def verify_file_record(evidence_root: Path, relative: str, expected_sha: str, label: str, expected_size: int | None = None) -> Path:
    path = resolve_evidence_path(evidence_root, relative)
    require(sha256_file(path) == expected_sha, f"{label} SHA-256 differs from its manifest record.")
    if expected_size is not None:
        require(path.stat().st_size == expected_size, f"{label} size differs from its manifest record.")
    return path


def verify_absolute_evidence_file(evidence_root: Path, value: str, expected_sha: str, label: str) -> Path:
    path = Path(value).resolve(strict=True)
    root = evidence_root.resolve(strict=True)
    require(path == root or root in path.parents, f"{label} path is outside the run evidence directory.")
    require(sha256_file(path) == expected_sha, f"{label} SHA-256 differs from its manifest record.")
    return path


def read_png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR", f"Screenshot is not a valid PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    require(width > 0 and height > 0, f"Screenshot dimensions are invalid: {path}")
    return width, height


def expected_window_ids() -> list[str]:
    result = ["cold-baseline", "warmup-classes", "warmup-return"]
    result.extend(f"plateau-baseline-{index:02d}" for index in range(1, 6))
    for cycle in range(1, 11):
        result.extend((f"cycle-{cycle:02d}-classes", f"cycle-{cycle:02d}-return"))
    return result


def expected_transition_pairs() -> list[tuple[str, str]]:
    result = [("my_workspace", "classes"), ("classes", "my_workspace")]
    for _ in range(10):
        result.extend((("my_workspace", "classes"), ("classes", "my_workspace")))
    return result


def validate_manifest_identity(manifest: dict[str, Any]) -> None:
    require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID, "P9-19 manifest schema/task identity differs.")
    require(manifest.get("status") == "route-completed-validation-pending", "Route did not complete before validation or a gate already failed.")
    require(manifest.get("failure") is None, "Route runner recorded a failure.")


def validate_fixture(manifest: dict[str, Any], evidence_root: Path, repository: Path) -> dict[str, Any]:
    fixture = manifest.get("fixture") or {}
    base = Path(str(fixture.get("baseWorkspacePath", ""))).resolve(strict=True)
    require(sha256_file(base) == BASE_SHA256 and fixture.get("baseWorkspaceSha256Expected") == BASE_SHA256 and fixture.get("baseWorkspaceSha256Observed") == BASE_SHA256 and fixture.get("baseWorkspacePinVerified") is True, "P9-19 route did not retain the exact master-blank base pin.")
    seed = repository / "tests/fixtures/workspaces/large_startup.sql"
    raw = seed.read_bytes()
    canonical = raw.replace(b"\r\n", b"\n")
    require(b"\r" not in canonical, "Pinned SQL contains a bare carriage return.")
    raw_sha, canonical_sha = sha256_bytes(raw), sha256_bytes(canonical)
    require(canonical_sha == SEED_CANONICAL_SHA256 and fixture.get("seedSqlRawSha256") == raw_sha and fixture.get("seedSqlCanonicalLfSha256Expected") == SEED_CANONICAL_SHA256 and fixture.get("seedSqlCanonicalLfSha256Observed") == SEED_CANONICAL_SHA256, "P9-19 SQL raw/canonical identity does not match its strict pin.")
    preparation = fixture.get("preparationReceipt") or {}
    receipt_path = verify_file_record(evidence_root, str(preparation.get("relativePath", "")), str(preparation.get("sha256", "")), "P9-19 preparation receipt")
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == FIXTURE_SCHEMA and receipt.get("status") == "passed" and receipt.get("baseWorkspaceSha256") == BASE_SHA256 and receipt.get("seedSqlRawSha256") == raw_sha and receipt.get("seedSqlCanonicalLfSha256") == SEED_CANONICAL_SHA256 and receipt.get("canonicalSeedSqlSha256") == SEED_CANONICAL_SHA256, "P9-19 preparation receipt differs from pinned input identities.")
    require(receipt.get("seedSqlLineEndingNormalization") in {"CRLF-to-LF", "none"}, "P9-19 SQL normalization record is malformed.")
    canonical_seed = verify_absolute_evidence_file(evidence_root, str(receipt.get("canonicalSeedSql", "")), SEED_CANONICAL_SHA256, "Canonical fixture SQL")
    require(canonical_seed.read_bytes() == canonical, "Canonical SQL evidence bytes differ from the raw pinned seed normalization.")
    require(receipt.get("identity") == {"classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768}, "P9-19 preparation identity is not 96 classes, 96 details, and 768 schedules.")
    require(preparation.get("workspaceSha256") == receipt.get("workspaceSha256") and preparation.get("seedSqlCanonicalLfSha256") == SEED_CANONICAL_SHA256, "P9-19 manifest fixture summary differs from its preparation receipt.")
    snapshots: dict[str, Any] = {}
    for phase in ("before", "after"):
        record = fixture.get(f"{phase}Snapshot") or {}
        path = verify_file_record(evidence_root, str(record.get("relativePath", "")), str(record.get("sha256", "")), f"P9-19 SQLite {phase} snapshot")
        snapshot = load_json(path)
        require(snapshot.get("schema") == FIXTURE_SCHEMA and snapshot.get("phase") == phase and snapshot.get("readOnlyConnection") is True and snapshot.get("integrityCheck") == "ok" and snapshot.get("foreignKeyErrors") == [], f"P9-19 SQLite {phase} snapshot did not pass integrity and read-only inspection.")
        require(snapshot.get("identity") == receipt.get("identity") and snapshot.get("baseWorkspaceSha256") == BASE_SHA256 and snapshot.get("seedSqlCanonicalLfSha256") == SEED_CANONICAL_SHA256, f"P9-19 SQLite {phase} snapshot differs from the pinned fixture.")
        snapshots[phase] = snapshot
    require(snapshots["before"].get("workspaceSha256") == receipt.get("workspaceSha256"), "Pre-route SQLite bytes differ from the freshly prepared workspace.")
    require(snapshots["after"].get("workspaceSha256") == snapshots["before"].get("workspaceSha256") and snapshots["after"].get("workspaceTablesSha256") == snapshots["before"].get("workspaceTablesSha256") and snapshots["after"].get("schemaObjectsSha256") == snapshots["before"].get("schemaObjectsSha256"), "Navigation-only route changed SQLite bytes, schema, or rows.")
    return {"baseWorkspaceSha256": BASE_SHA256, "seedSqlRawSha256": raw_sha, "seedSqlCanonicalLfSha256": canonical_sha, "fixtureIdentity": receipt["identity"], "workspaceSha256": snapshots["before"]["workspaceSha256"], "beforeReadOnlyIntegrity": snapshots["before"]["integrityCheck"], "afterReadOnlyIntegrity": snapshots["after"]["integrityCheck"]}


def current_harness_inventory(repository: Path) -> dict[str, Any]:
    files = []
    for relative in HARNESS_PATHS:
        path = repository / relative
        require(path.is_file(), f"Required P9-19 harness source is missing: {relative}")
        files.append({"path": relative, "sizeBytes": path.stat().st_size, "sha256": sha256_file(path)})
    return {"fileCount": len(files), "sha256": canonical_fingerprint(files), "files": files}


def validate_build_source_snapshot(receipt: dict[str, Any], repository: Path, build_evidence_root: Path) -> dict[str, Any]:
    build_evidence_root = build_evidence_root.resolve(strict=True)
    roots = list(receipt.get("sourceIncludedRoots") or [])
    snapshot_root = Path(str(receipt.get("buildSourceRoot", ""))).resolve(strict=True)
    require(snapshot_root == build_evidence_root or build_evidence_root in snapshot_root.parents, "Canonical build-source snapshot is outside its exact-source build evidence.")
    inventory_path = Path(str(receipt.get("buildSourceInventoryPath", ""))).resolve(strict=True)
    require(inventory_path == build_evidence_root or build_evidence_root in inventory_path.parents, "Canonical build-source inventory is outside its exact-source build evidence.")
    require(sha256_file(inventory_path) == receipt.get("buildSourceInventoryFileSha256"), "Canonical build-source inventory file SHA-256 differs from the build receipt.")
    recorded = load_json(inventory_path)
    actual = source_inventory(snapshot_root, roots)
    require(actual.get("sha256") == receipt.get("buildSourceSha256") and actual.get("fileCount") == receipt.get("buildSourceFileCount"), "Canonical build-source snapshot differs from its receipt fingerprint.")
    require(recorded.get("sha256") == actual.get("sha256") and recorded.get("fileCount") == actual.get("fileCount") and recorded.get("files") == actual.get("files"), "Canonical build-source inventory does not reproduce snapshot files.")

    current = source_inventory(repository, roots)
    source_inventory_path = Path(str(receipt.get("sourceInventoryPath", ""))).resolve(strict=True)
    require(source_inventory_path == build_evidence_root or build_evidence_root in source_inventory_path.parents, "Original source inventory is outside its exact-source build evidence.")
    require(sha256_file(source_inventory_path) == receipt.get("sourceInventoryFileSha256"), "Original source inventory file SHA-256 differs from the build receipt.")
    recorded_current = load_json(source_inventory_path)
    require(current.get("sha256") == receipt.get("sourceSha256") and current.get("fileCount") == receipt.get("sourceFileCount"), "Current source no longer matches the exact-source build receipt.")
    require(recorded_current.get("sha256") == current.get("sha256") and recorded_current.get("fileCount") == current.get("fileCount") and recorded_current.get("files") == current.get("files"), "Recorded source inventory does not reproduce current source files.")

    resource_inventory_path = repository / "docs/qt-rewrite/phase-4-resource-inventory.json"
    resource_inventory = load_json(resource_inventory_path)
    pinned_resources: dict[str, dict[str, Any]] = {}
    source_collections = ("document_catalog_metadata", "document_bodies", "resources", "excluded_resources", "build_inputs", "calendar_qml_sources")
    for collection in source_collections:
        for row in resource_inventory.get(collection, []):
            if "source_path" not in row or not isinstance(row.get("source_file"), dict):
                continue
            relative = str(row["source_path"])
            require(relative not in pinned_resources, f"Pinned resource inventory repeats source path {relative}.")
            pinned_resources[relative] = row["source_file"]
    current_rows = {str(row["path"]): row for row in current["files"]}
    snapshot_rows = {str(row["path"]): row for row in actual["files"]}
    normalization = receipt.get("buildSourceNormalization") or {}
    require(Path(str(normalization.get("root", ""))).resolve(strict=True) == snapshot_root, "Build-source normalization root differs from its captured snapshot.")
    transformations = normalization.get("normalizedResources") or []
    require(normalization.get("normalizedResourceCount") == len(transformations), "Build-source normalization count differs from its records.")
    transformation_by_path: dict[str, dict[str, Any]] = {}
    for row in transformations:
        relative = str(row.get("path", ""))
        require(relative and relative not in transformation_by_path, f"Build-source normalization path is missing or duplicated: {relative}")
        transformation_by_path[relative] = row

    expected_transformations: set[str] = set()
    for relative, row in current_rows.items():
        source_path = repository / relative
        snapshot_path = snapshot_root / relative
        require(snapshot_path.is_file(), f"Canonical build-source snapshot omitted {relative}.")
        raw = source_path.read_bytes()
        built = snapshot_path.read_bytes()
        if relative not in pinned_resources:
            require(built == raw, f"Build-source snapshot changed a non-resource source file: {relative}")
            continue
        pinned = pinned_resources[relative]
        require(isinstance(pinned, dict), f"Pinned resource source identity is malformed: {relative}")
        canonical = raw.replace(b"\r\n", b"\n")
        raw_matches = len(raw) == int(pinned["bytes"]) and sha256_bytes(raw) == str(pinned["sha256"])
        canonical_matches = len(canonical) == int(pinned["bytes"]) and sha256_bytes(canonical) == str(pinned["sha256"])
        if raw_matches:
            require(relative not in transformation_by_path and built == raw, f"Unnecessary or unrecorded resource normalization: {relative}")
            continue
        require(canonical_matches, f"Resource source differs from pinned inventory beyond CRLF-to-LF expansion: {relative}")
        expected_transformations.add(relative)
        transformation = transformation_by_path.get(relative)
        require(transformation is not None, f"Pinned resource normalization evidence is missing: {relative}")
        require(transformation.get("normalization") == "CRLF-to-LF only; exact pinned source bytes and SHA-256 verified", f"Pinned resource normalization method differs: {relative}")
        require(transformation.get("originalRawBytes") == len(raw) and transformation.get("originalRawSha256") == sha256_bytes(raw), f"Raw resource identity differs from its normalization receipt: {relative}")
        require(transformation.get("buildSourceBytes") == len(canonical) and transformation.get("buildSourceSha256") == sha256_bytes(canonical) and built == canonical, f"Canonical resource identity differs from its normalization receipt: {relative}")
    require(set(transformation_by_path) == expected_transformations, "Build-source normalization records do not match exactly the CRLF-expanded pinned resources.")
    require(set(snapshot_rows) == set(current_rows), "Canonical build-source file set differs from current product source.")
    return {"buildSourceSha256": actual["sha256"], "buildSourceFileCount": actual["fileCount"], "normalizedResourceCount": len(expected_transformations)}


def validate_build_and_package(manifest: dict[str, Any], evidence_root: Path, repository: Path) -> dict[str, Any]:
    provenance = manifest.get("packageProvenance") or {}
    receipt_path = Path(str(provenance.get("path", ""))).resolve(strict=True)
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == BUILD_SCHEMA and receipt.get("taskId") == TASK_ID and receipt.get("status") == "succeeded", "Exact-source build receipt schema/status differs.")
    require(sha256_file(receipt_path) == provenance.get("sha256"), "Build receipt SHA-256 differs from the route manifest.")
    require(receipt.get("configuration") == "Release" and receipt.get("platform") == "x64" and receipt.get("generator") == "Visual Studio 17 2022" and receipt.get("generatorPlatform") == "x64" and receipt.get("generatorToolset") == "v143" and receipt.get("windowsSdkVersion") == "10.0.26100.0" and (receipt.get("compiler") or {}).get("fileVersion") == "19.44.35230.0" and (receipt.get("compiler") or {}).get("toolsetVersion") == "14.44.35207", "Exact build did not prove the pinned VS2022/v143 Release toolchain.")
    require(receipt.get("build", {}).get("exitCode") == 0 and receipt.get("focusedTestBuild", {}).get("exitCode") == 0 and receipt.get("focusedTests", {}).get("exitCode") == 0, "Exact-source Release or focused C++ tests failed.")
    helper_path = repository / "scripts/phase9/p9_19_build_current_source_release.ps1"
    require(sha256_file(helper_path) == receipt.get("buildHelperSha256"), "Build helper changed after its exact-source receipt.")
    require(receipt.get("releaseExecutableSha256") == sha256_file(Path(str(receipt.get("releaseExecutablePath", ""))).resolve(strict=True)) and receipt.get("releaseExecutableMachine") == 0x8664, "Release executable bytes or x64 architecture differ from the build receipt.")
    current_source = source_inventory(repository, list(receipt.get("sourceIncludedRoots") or []))
    require(current_source.get("sha256") == receipt.get("sourceSha256") and current_source.get("fileCount") == receipt.get("sourceFileCount"), "Current product/test source differs from the exact-source build.")
    build_source = validate_build_source_snapshot(receipt, repository, receipt_path.parent)
    harness = current_harness_inventory(repository)
    require(harness.get("sha256") == receipt.get("harnessSourceSha256") and harness.get("fileCount") == receipt.get("harnessSourceFileCount"), "Current P9-19 harness differs from the exact-source build.")
    harness_path = Path(str(receipt.get("harnessInventoryPath", ""))).resolve(strict=True)
    require(sha256_file(harness_path) == receipt.get("harnessInventoryFileSha256"), "Recorded build harness inventory file changed.")
    recorded_harness = load_json(harness_path)
    require(recorded_harness.get("sha256") == harness.get("sha256") and recorded_harness.get("files") == harness.get("files"), "Build harness inventory does not reproduce current P9-19 harness sources.")
    generated = receipt.get("generatedResources") or {}
    qt_config = Path(str(receipt.get("qt6Directory", ""))).resolve(strict=True) / "Qt6Config.cmake"
    require(sha256_file(qt_config) == receipt.get("qt6ConfigSha256"), "Exact-source build Qt6 configuration differs from its recorded identity.")
    assets = canonical_inventory(Path(str(generated.get("assetsRoot", ""))))
    translations = canonical_inventory(Path(str(generated.get("translationsRoot", ""))))
    require(assets.get("sha256") == generated.get("assetsSha256") and translations.get("sha256") == generated.get("translationsSha256") and sha256_file(Path(str(generated.get("catalogPath", "")))) == generated.get("catalogSha256"), "Generated Release resources differ from their exact-source receipt.")
    package = manifest.get("package") or {}
    package_inventory_path = verify_file_record(evidence_root, str(package.get("packageInventoryPath", "")), str(package.get("packageInventoryFileSha256", "")), "P9-19 package inventory")
    package_inventory = load_json(package_inventory_path)
    require(package_inventory.get("sha256") == package.get("packageSha256") and package_inventory.get("fileCount") == package.get("packageFileCount"), "P9-19 package inventory differs from package manifest.")
    package_root = Path(str(package.get("packageRoot", ""))).resolve(strict=True)
    actual_package = canonical_inventory(package_root)
    require(actual_package.get("sha256") == package_inventory.get("sha256") and actual_package.get("files") == package_inventory.get("files"), "Staged P9-19 package files differ from their inventory.")
    app = Path(str(package.get("applicationPath", ""))).resolve(strict=True)
    require(sha256_file(app) == package.get("applicationSha256") == receipt.get("releaseExecutableSha256"), "Staged application bytes differ from the exact-source Release executable.")
    required_files = ["Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "platforms/qwindows.dll", "sqldrivers/qsqlite.dll", "resources/resource-catalog.json"]
    require(all((package_root / relative).is_file() for relative in required_files), "Staged package is missing required Qt/runtime resources.")
    deploy_log = verify_file_record(evidence_root, str(package.get("deployLogPath", "")), str(package.get("deployLogSha256", "")), "windeployqt log")
    require(package.get("deployExitCode") == 0 and package.get("architecture") == "x64", "P9-19 staged package or windeployqt gate failed.")
    return {"buildReceiptSha256": sha256_file(receipt_path), "releaseExecutableSha256": receipt["releaseExecutableSha256"], "sourceSha256": receipt["sourceSha256"], "buildSourceSha256": build_source["buildSourceSha256"], "normalizedResourceCount": build_source["normalizedResourceCount"], "harnessSha256": harness["sha256"], "packageSha256": package["packageSha256"], "packageFileCount": package["packageFileCount"], "windeployqtLog": str(deploy_log)}


def validate_sweep_readbacks(sweep: dict[str, Any], evidence_root: Path, pid: int, hwnd: int, expected_cycle: int | None) -> dict[str, Any]:
    require(sweep.get("classCount") == 96 and sweep.get("selectionActionCount") == 96 and sweep.get("allSelectionReadbacksConfirmed") is True, "P9-19 selection sweep does not claim 96 confirmed row selections.")
    details = sweep.get("appCheckpointDetails") or {}
    require(details.get("schema") == SWEEP_SCHEMA and details.get("class_count") == 96 and details.get("visible_class_count") == 96 and details.get("selected_unique_class_count") == 96 and details.get("selection_observation_count") == 96, "App-side P9-19 checkpoint did not observe all 96 unique classes.")
    require(details.get("selected_class_editor_count_minimum") == 1 and details.get("selected_class_editor_count_maximum") == 1 and details.get("non_roster_widget_count_maximum", 250) < 250 and details.get("class_id_child_widget_count_maximum") == 0 and details.get("selected_class_editor_stable_within_sweep") is True and details.get("selected_class_editor_stable_across_sweeps") is True and details.get("selected_class_editor_instance_generation") == 1, "App-side SelectedClassEditor/widget-count contract failed.")
    path = verify_file_record(evidence_root, str(sweep.get("selectionReadbackPath", "")), str(sweep.get("selectionReadbackSha256", "")), "P9-19 per-selection readbacks", int(sweep.get("selectionReadbackBytes", -1)))
    manifest = load_json(evidence_root / "manifest.json")
    checkpoint_events = [
        row for row in ((manifest.get("route") or {}).get("lifecycleEvents") or [])
        if row.get("arrivalIndex") == sweep.get("appCheckpointArrivalIndex")
    ]
    require(len(checkpoint_events) == 1, "P9-19 app checkpoint arrival index is absent or duplicated in app lifecycle output.")
    checkpoint_line = str(checkpoint_events[0].get("rawLine", ""))
    prefix = "ClassMngrNext page lifecycle checkpoint="
    require(checkpoint_line.startswith(prefix), "P9-19 sweep checkpoint is not bound to its app-origin lifecycle output line.")
    try:
        checkpoint_record = json.loads(checkpoint_line[len(prefix):])
    except json.JSONDecodeError as error:
        raise ValidationError(f"P9-19 app checkpoint lifecycle line is malformed: {error}") from error
    require(checkpoint_record.get("event") == "page-action-checkpoint" and checkpoint_record.get("page_key") == "classes" and checkpoint_record.get("checkpoint") == "p9-19-class-selection-sweep" and checkpoint_record.get("outcome") == "completed" and checkpoint_record.get("details") == details, "P9-19 app-origin lifecycle checkpoint differs from its sweep summary.")
    try:
        rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    except json.JSONDecodeError as error:
        raise ValidationError(f"P9-19 selection readback JSONL is malformed: {error}") from error
    require(len(rows) == 96, "P9-19 per-selection readback file does not have exactly 96 rows.")
    observed_names = [str(row.get("selectedListItemName", "")) for row in rows]
    require(len(set(observed_names)) == 96 and set(observed_names) == set(CLASS_NAMES), "P9-19 UIA selection readbacks do not cover the exact unique 96 fixture classes.")
    observed_ids: set[int] = set()
    for row in rows:
        target = row.get("target") or {}
        name = str(row.get("selectedListItemName", ""))
        expected_id = int(name.rsplit(" ", 1)[-1])
        require(row.get("actionKind") == "select-fixture-class" and row.get("classId") == expected_id and row.get("className") == name and row.get("selectionItemPatternIsSelected") is True, f"P9-19 UIA selection readback for {name} differs from the pinned fixture row.")
        observed_ids.add(expected_id)
        require(row.get("sweepId") == sweep.get("sweepId") and row.get("cycleNumber") == expected_cycle, f"P9-19 UIA action for {name} is bound to a different sweep/cycle.")
        require(target.get("controlType") == "ControlType.ListItem" and target.get("name") == name and target.get("processId") == pid and target.get("ownerWindowHandle") == hwnd and target.get("ownerWindowProcessId") == pid, f"P9-19 UIA selection for {name} is not bound to the exact measured PID/HWND list row.")
    require(observed_ids == set(range(1, 97)), "P9-19 selected fixture class ids do not cover exactly 1 through 96.")
    require(sweep.get("listItemNameHash") == sha256_bytes("\n".join(observed_names).encode("utf-8")), "P9-19 live Classes list row-order hash differs from selection readbacks.")
    return {"sweepId": sweep.get("sweepId"), "readbackCount": len(rows), "appCheckpointArrivalIndex": sweep.get("appCheckpointArrivalIndex"), "selectedClassEditorGeneration": details.get("selected_class_editor_instance_generation")}


def validate_lifecycle_and_route(manifest: dict[str, Any], evidence_root: Path, pid: int, hwnd: int) -> dict[str, Any]:
    route = manifest.get("route") or {}
    transitions = route.get("transitions") or []
    expected_pairs = expected_transition_pairs()
    require(len(transitions) == len(expected_pairs), "P9-19 route does not contain exactly one warm-up plus ten measured return cycles.")
    for index, (transition, pair) in enumerate(zip(transitions, expected_pairs), start=1):
        require((transition.get("fromPageId"), transition.get("toPageId")) == pair and transition.get("targetAccessibleName") == ("Classes" if pair[1] == "classes" else "My Workspace"), f"P9-19 transition {index} has the wrong source/target route state.")
        require(transition.get("processId") == pid and transition.get("mainWindowHandle") == hwnd, f"P9-19 transition {index} is not tied to the measured PID/HWND.")
        event = transition.get("routeEvent") or {}
        require(event.get("targetPageId") == pair[1] and event.get("processId") == pid, f"P9-19 transition {index} lacks its matching app-origin route event.")
        life = transition.get("lifecycleEvents") or []
        expected_states = {(pair[1], "activated"), (pair[0], "suspended")}
        observed = {(row.get("pageKey"), row.get("state")) for row in life if row.get("event") == "page-lifecycle"}
        require(expected_states.issubset(observed), f"P9-19 transition {index} lacks exact activated/suspended page lifecycle events.")
    lifecycle = route.get("lifecycleEvents") or []
    page_events = [row for row in lifecycle if row.get("event") == "page-lifecycle"]
    for page, expected in (("my_workspace", {"created": 1, "activated": 12, "suspended": 11}), ("classes", {"created": 1, "activated": 11, "suspended": 11})):
        counts = {state: sum(1 for row in page_events if row.get("pageKey") == page and row.get("state") == state) for state in expected}
        require(counts == expected, f"P9-19 app-origin {page} lifecycle is incomplete or shows page-instance churn: {counts}.")
    require(manifest.get("route", {}).get("warmupCount") == 1 and manifest.get("route", {}).get("baselineWindowsPerState") == 5 and manifest.get("route", {}).get("measuredCycleCount") == 10, "P9-19 route summary does not retain the fixed warm-up/baseline/ten-cycle contract.")
    sweeps = route.get("selectionSweeps") or []
    require(len(sweeps) == 11, "P9-19 does not contain one warm-up plus ten full 96-class selection sweeps.")
    sweep_summaries = []
    for index, sweep in enumerate(sweeps):
        phase = "warmup" if index == 0 else "measured-cycle"
        cycle = None if index == 0 else index
        expected_sweep_id = "class-selections-warmup" if index == 0 else f"class-selections-cycle-{index:02d}"
        require(sweep.get("phase") == phase and sweep.get("cycleNumber") == cycle and sweep.get("sweepId") == expected_sweep_id, "P9-19 class sweep ordering/identity differs from its route cycle.")
        sweep_summaries.append(validate_sweep_readbacks(sweep, evidence_root, pid, hwnd, cycle))
    expected_screenshots = len(expected_window_ids())
    screenshots = route.get("screenshots") or []
    screenshot_by_path = {str(row.get("relativePath")): row for row in screenshots}
    require(len(screenshots) == expected_screenshots and len(screenshot_by_path) == expected_screenshots, "P9-19 does not have one unique settled-state screenshot per required memory window.")
    screenshot_results = []
    for record in screenshots:
        require(record.get("processId") == pid and record.get("windowHandle") == hwnd and record.get("windowProcessId") == pid, "P9-19 screenshot is not bound to measured PID/HWND.")
        path = verify_file_record(evidence_root, str(record.get("relativePath", "")), str(record.get("sha256", "")), "P9-19 settled-state screenshot", int(record.get("sizeBytes", -1)))
        width, height = read_png_dimensions(path)
        require((width, height) == (record.get("width"), record.get("height")), "P9-19 screenshot dimensions differ from the recorded capture bounds.")
        screenshot_results.append({"path": record["relativePath"], "sha256": record["sha256"], "width": width, "height": height})
    route_manifest_record = manifest.get("routeManifest") or {}
    route_manifest_path = verify_file_record(evidence_root, str(route_manifest_record.get("path", "")), str(route_manifest_record.get("sha256", "")), "P9-19 route manifest")
    route_manifest = load_json(route_manifest_path)
    require(route_manifest.get("selectionSweeps") == sweeps and route_manifest.get("transitions") == transitions and route_manifest.get("screenshots") == screenshots, "Standalone route manifest differs from the root lifecycle records.")
    return {"transitionCount": len(transitions), "lifecyclePageEventCount": len(page_events), "selectionSweeps": sweep_summaries, "screenshotCount": len(screenshot_results), "routeManifestSha256": sha256_file(route_manifest_path)}


def validate_memory(manifest: dict[str, Any], evidence_root: Path, pid: int, hwnd: int) -> dict[str, Any]:
    memory = manifest.get("memory") or {}
    windows = memory.get("windows") or []
    expected = expected_window_ids()
    require([row.get("windowId") for row in windows] == expected, "P9-19 settled memory windows are missing, duplicated, or out of order.")
    by_id = {str(row.get("windowId")): row for row in windows}
    require(len(by_id) == 28, "P9-19 memory window ids are not unique.")
    target_misses: list[str] = []
    ceiling_misses: list[dict[str, Any]] = []
    settled_values: list[int] = []
    screenshots = {str(row.get("relativePath")): row for row in (manifest.get("route") or {}).get("screenshots", [])}
    for name in expected:
        window = by_id[name]
        expected_state = "classes" if name.endswith("-classes") else "my_workspace"
        expected_phase = "cold-baseline" if name == "cold-baseline" else "warmup" if name.startswith("warmup-") else "no-action-baseline" if name.startswith("plateau-baseline-") else "measured-cycle-classes" if name.endswith("-classes") else "measured-cycle-return"
        require(window.get("matchedState") == expected_state and window.get("state") == expected_state and window.get("processId") == pid and window.get("mainWindowHandle") == hwnd, f"P9-19 memory window {name} does not bind the required page/PID/HWND.")
        require(window.get("phase") == expected_phase, f"P9-19 memory window {name} has the wrong route phase.")
        require(window.get("sampleIntervalMilliseconds") == 100 and window.get("sampleCount") == 10 and window.get("samplingCompleted") is True and window.get("settleDelaySeconds") == 2 and window.get("stateConfirmedBeforeSampling") is True, f"P9-19 memory window {name} does not preserve the two-second settle and ten 100 ms samples.")
        confirmation = window.get("stateConfirmation") or {}
        require(confirmation.get("state") == expected_state and confirmation.get("processId") == pid and confirmation.get("mainWindowHandle") == hwnd, f"P9-19 window {name} state confirmation differs from the measured process or page.")
        confirmation_time = dt.datetime.fromisoformat(str(confirmation.get("confirmedAtUtc", "")).replace("Z", "+00:00"))
        sampling_time = dt.datetime.fromisoformat(str(window.get("samplingStartedAtUtc", "")).replace("Z", "+00:00"))
        require((sampling_time - confirmation_time).total_seconds() >= 1.9, f"P9-19 memory window {name} began before its required two-second settle interval.")
        screenshot = confirmation.get("screenshot") or {}
        record = screenshots.get(str(screenshot.get("relativePath", "")))
        require(record is not None and record.get("sha256") == screenshot.get("sha256") and record.get("processId") == pid and record.get("windowHandle") == hwnd, f"P9-19 memory window {name} lacks a matching PID/HWND screenshot.")
        samples = window.get("samples") or []
        require(len(samples) == 10 and window.get("samplingErrorCount") == 0, f"P9-19 memory window {name} has missing samples or sampling errors.")
        times: list[str] = []
        target_indexes: list[int] = []
        ceiling_indexes: list[int] = []
        for index, sample in enumerate(samples):
            require(sample.get("sampleIndex") == index and sample.get("processId") == pid and sample.get("processState") == "running" and sample.get("metricErrors") == [], f"P9-19 memory sample {name}/{index} is not valid for the measured process.")
            for key in ("workingSetBytes", "privateBytes", "peakWorkingSetBytes", "handles", "threadCount"):
                require(type(sample.get(key)) is int and sample[key] >= 0, f"P9-19 memory sample {name}/{index} lacks {key}.")
            value = int(sample["workingSetBytes"]); settled_values.append(value); times.append(str(sample.get("capturedAtUtc", "")))
            if value >= WORKING_SET_TARGET: target_indexes.append(index)
            if value > WORKING_SET_CEILING:
                ceiling_indexes.append(index); ceiling_misses.append({"windowId": name, "sampleIndex": index, "workingSetBytes": value})
        require(times == sorted(times) and len(set(times)) == 10, f"P9-19 sample times are not strictly increasing in {name}.")
        require(window.get("workingSetTargetBytes") == WORKING_SET_TARGET and window.get("workingSetTargetExclusive") is True and window.get("workingSetCeilingBytes") == WORKING_SET_CEILING and window.get("workingSetCeilingExclusive") is False, f"P9-19 window {name} changed the memory thresholds.")
        require(window.get("workingSetTargetExceededSampleIndexes") == target_indexes and window.get("workingSetTargetMet") is (not target_indexes), f"P9-19 target miss report differs from samples in {name}.")
        require(window.get("workingSetCeilingExceededSampleIndexes") == ceiling_indexes and window.get("workingSetCeilingPassed") is (not ceiling_indexes), f"P9-19 ceiling report differs from samples in {name}.")
        if target_indexes: target_misses.append(name)
    require(not ceiling_misses, f"Settled Working Set exceeded the inclusive P9-19 450 MiB ceiling: {ceiling_misses}.")
    operation = memory.get("processMetrics") or {}
    require(operation.get("path") == "process-metrics.jsonl" and type(operation.get("operationPeakWorkingSetBytes")) is int and operation.get("operationPeakWorkingSetBytes") >= 0, "P9-19 separate 50 ms operation peak is missing.")
    metrics_path = verify_file_record(evidence_root, str(operation.get("path")), str(operation.get("sha256")), "P9-19 50 ms process metrics")
    metric_rows = [json.loads(line) for line in metrics_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(len(metric_rows) == operation.get("sampleCount") and metric_rows and all(row.get("pid") == pid for row in metric_rows), "P9-19 process-metrics JSONL does not bind every 50 ms sample to the app PID.")
    require(operation.get("operationPeakThresholdBytesExclusiveAdvisory") == OPERATION_PEAK_ADVISORY and memory.get("operationPeakWorkingSetBytes") == operation.get("operationPeakWorkingSetBytes"), "P9-19 operation advisory threshold/value differs from the fixed contract.")

    formulas = memory.get("formulaInputs") or {}
    cold = [int(row["workingSetBytes"]) for row in by_id["cold-baseline"]["samples"]]
    first_return = [int(row["workingSetBytes"]) for row in by_id["warmup-return"]["samples"]]
    baseline_names = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    return_names = [f"cycle-{index:02d}-return" for index in range(1, 11)]
    baseline_samples = [[int(row["workingSetBytes"]) for row in by_id[name]["samples"]] for name in baseline_names]
    return_samples = [[int(row["workingSetBytes"]) for row in by_id[name]["samples"]] for name in return_names]
    require(formulas.get("coldBaselineWindowId") == "cold-baseline" and formulas.get("coldBaselineWorkingSetSamples") == cold and formulas.get("firstReturnWindowId") == "warmup-return" and formulas.get("firstReturnWorkingSetSamples") == first_return, "P9-19 manifest cold-cleanup raw formula inputs differ from their windows.")
    require(formulas.get("plateauBaselineWindowIds") == baseline_names and formulas.get("plateauBaselineWorkingSetSamples") == baseline_samples and formulas.get("measuredReturnWindowIds") == return_names and formulas.get("measuredReturnWorkingSetSamples") == return_samples, "P9-19 manifest plateau-cleanup raw formula inputs differ from their windows.")
    cleanup = p918.compute_cleanup_analysis(cold, first_return, baseline_samples, return_samples)
    memory["analysis"] = cleanup
    return {"windowCount": len(windows), "samplesPerWindow": 10, "strictTargetBytes": WORKING_SET_TARGET, "inclusiveCeilingBytes": WORKING_SET_CEILING, "maximumSettledWorkingSetBytes": max(settled_values), "windowsWithTargetMisses": target_misses, "settledCeilingExceedances": ceiling_misses, "operationPeakWorkingSetBytes": operation["operationPeakWorkingSetBytes"], "cleanupAnalysis": cleanup, "existingPhase14CleanupStatus": "P9-17 plateau and P9-18 cold cleanup remain Deferred/unaccepted for Phase 14"}


def validate_runtime_and_exit(manifest: dict[str, Any], evidence_root: Path, repository: Path) -> dict[str, Any]:
    process = manifest.get("process") or {}
    pid, hwnd = process.get("processId"), process.get("mainWindowHandle")
    require(type(pid) is int and pid > 0 and type(hwnd) is int and hwnd > 0, "P9-19 process PID/HWND identity is missing.")
    require(process.get("architecture") == "x64" and Path(str(process.get("applicationPath", ""))).is_file() and process.get("applicationSha256") == sha256_file(Path(str(process.get("applicationPath"))).resolve(strict=True)), "Measured P9-19 process image differs from the staged x64 Release package.")
    process_exit = manifest.get("processExit") or {}
    require(process_exit.get("observed") is True and process_exit.get("normalExit") is True and process_exit.get("exitCode") == 0 and process_exit.get("forcedTermination") is False, "P9-19 process did not exit normally with code 0.")
    cleanup = (manifest.get("runtimeAppData") or {}).get("cleanup") or {}
    require(cleanup.get("processExited") is True and cleanup.get("forcedTermination") is False, "P9-19 runtime cleanup did not observe normal process exit.")
    trace_relative = cleanup.get("shutdownTracePath")
    require(trace_relative == "runtime/diagnostics/application-shutdown.jsonl", "P9-19 application shutdown trace is missing.")
    trace_path = resolve_evidence_path(evidence_root, str(trace_relative))
    require(trace_path.stat().st_size > 0, "P9-19 shutdown trace is empty.")
    app_rows = []
    for line in trace_path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            try: app_rows.append(json.loads(line))
            except json.JSONDecodeError as error: raise ValidationError(f"P9-19 shutdown trace contains malformed JSON: {error}") from error
    require(app_rows, "P9-19 app-origin shutdown trace has no structured lifecycle record.")
    runtime = manifest.get("runtimeAppData") or {}
    before, after = runtime.get("before") or {}, runtime.get("after") or {}
    require(set(before) == {"settings", "local-app-data", "app-data", "temp"} and set(after) == {"settings", "local-app-data", "app-data", "temp"}, "P9-19 app-data before/after inventory does not cover all isolated runtime roots.")
    for phase, records in (("before", before), ("after", after)):
        for name, record in records.items():
            path = verify_file_record(evidence_root, str(record.get("relativePath", "")), str(record.get("inventoryFileSha256", "")), f"P9-19 {phase} {name} inventory")
            inventory = load_json(path)
            require(inventory.get("sha256") == record.get("sha256") and inventory.get("fileCount") == record.get("fileCount") and canonical_fingerprint(inventory.get("files") or []) == inventory.get("sha256"), f"P9-19 {phase} {name} inventory cannot be independently reproduced.")
            root = Path(str(record.get("root", ""))).resolve(strict=True)
            actual = canonical_inventory(root)
            require(actual.get("sha256") == inventory.get("sha256") and actual.get("files") == inventory.get("files"), f"P9-19 {phase} {name} runtime files changed after inventory capture.")
    require(after["temp"].get("fileCount") == 0 and runtime.get("tempFilesAfterExit") == [], "P9-19 isolated temp files remain after process exit.")
    sidecars = list(runtime.get("sqliteSidecarsAfterExit") or []) + list(cleanup.get("workspaceSidecarFilesAfterExit") or [])
    require(not sidecars, f"P9-19 SQLite sidecars remain after process exit: {sidecars}.")
    return {"processId": pid, "mainWindowHandle": hwnd, "normalExitCode": process_exit.get("exitCode"), "shutdownTraceRecordCount": len(app_rows), "runtimeRootsInventoriedBeforeAndAfter": list(before), "tempFilesAfterExit": 0, "sqliteSidecarsAfterExit": 0}


def validate(manifest_path: Path, repository: Path) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    evidence_root = manifest_path.parent.resolve(strict=True)
    manifest = load_json(manifest_path)
    report: dict[str, Any] = {"schema": "classmngr-phase9-p9-19-validation-report-v1", "taskId": TASK_ID, "manifestPath": str(manifest_path), "validationStartedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z"), "gates": {}}

    def gate(name: str, operation) -> Any:
        try:
            details = operation()
            report["gates"][name] = {"passed": True, "details": details}
            return details
        except (ValidationError, OSError, ValueError, TypeError, KeyError, IndexError, json.JSONDecodeError) as error:
            report["gates"][name] = {"passed": False, "failureReasons": [str(error)]}
            return None

    gate("manifest", lambda: (validate_manifest_identity(manifest), {"schema": manifest.get("schema"), "status": manifest.get("status"), "failure": manifest.get("failure")})[1])
    fixture = gate("pinnedFixtureAndReadOnlySQLite", lambda: validate_fixture(manifest, evidence_root, repository))
    package = gate("exactSourceBuildAndPackage", lambda: validate_build_and_package(manifest, evidence_root, repository))
    runtime = gate("normalExitAndRuntimeCleanup", lambda: validate_runtime_and_exit(manifest, evidence_root, repository))
    if runtime:
        route = gate("uiaRouteLifecycleAndEditorCheckpoints", lambda: validate_lifecycle_and_route(manifest, evidence_root, int(runtime["processId"]), int(runtime["mainWindowHandle"])))
        memory = gate("settledMemoryAndCleanupFormulas", lambda: validate_memory(manifest, evidence_root, int(runtime["processId"]), int(runtime["mainWindowHandle"])))
    else:
        route = gate("uiaRouteLifecycleAndEditorCheckpoints", lambda: (_ for _ in ()).throw(ValidationError("P9-19 process identity/normal exit gate failed.")))
        memory = gate("settledMemoryAndCleanupFormulas", lambda: (_ for _ in ()).throw(ValidationError("P9-19 process identity/normal exit gate failed.")))
    report["passed"] = all(bool(item.get("passed")) for item in report["gates"].values())
    report["status"] = "passed" if report["passed"] else "failed"
    report["validationCompletedAtUtc"] = dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
    report["summary"] = {"pinnedFixture": fixture, "exactSourceBuildAndPackage": package, "runtimeAndExit": runtime, "routeAndCheckpoints": route, "memoryAndCleanup": memory, "phase14DeferredGates": ["P9-17 plateau cleanup", "P9-18 cold cleanup"]}
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--report-output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = validate(args.manifest, args.repository.resolve(strict=True))
    except (OSError, ValidationError, ValueError, TypeError, KeyError, IndexError, json.JSONDecodeError) as error:
        report = {"schema": "classmngr-phase9-p9-19-validation-report-v1", "taskId": TASK_ID, "status": "failed", "passed": False, "gates": {"manifest": {"passed": False, "failureReasons": [str(error)]}}}
    args.report_output.parent.mkdir(parents=True, exist_ok=True)
    args.report_output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"status": report.get("status"), "passed": report.get("passed"), "report": str(args.report_output.resolve()), "gates": report.get("gates")}, ensure_ascii=False))
    return 0 if report.get("passed") is True else 1


if __name__ == "__main__":
    raise SystemExit(main())
