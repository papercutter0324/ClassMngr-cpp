"""Focused tests for P9-17 harness, snapshot, and Working Set validation."""

from __future__ import annotations

import importlib.util
import hashlib
import json
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest import mock

REPOSITORY = Path(__file__).resolve().parents[2]
PREPARER_PATH = Path(__file__).with_name("prepare_p9_17_large_roster.py")
VALIDATOR_PATH = Path(__file__).with_name("validate_packaged_next_p9_17_large_roster.py")
RUNNER_PATH = Path(__file__).with_name("run_packaged_next_p9_17_large_roster.ps1")


def load_module(path: Path, module_name: str):
    spec = importlib.util.spec_from_file_location(module_name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fixture = load_module(PREPARER_PATH, "p917_fixture_for_validator")
validator = load_module(VALIDATOR_PATH, "p917_validator")
BASE = (
    REPOSITORY
    / "artifacts/phase9/packaged-next-class-transfer/20261119T155840703Z-892ae1169ba945f5be16ef08fa0a3fb1/workspace-snapshots/master-blank.tps"
)
SEED = REPOSITORY / "tests/fixtures/workspaces/large_startup.sql"
P917_BUILD_RECEIPT = (
    REPOSITORY
    / "artifacts/phase9/p9-17-exact-release-builds/20261121T152057132Z-158877903bd543dcb1083ee9437c57fc/release-build-receipt.json"
)


def _write_json_artifact(path: Path, value: dict[str, object]) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
    return path


def _artifact_record(evidence_root: Path, path: Path, **extra: object) -> dict[str, object]:
    return {
        "relativePath": path.relative_to(evidence_root).as_posix(),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        **extra,
    }


def _refresh_arm_evidence_inventory(manifest_path: Path) -> None:
    evidence_root = manifest_path.parent
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    evidence_files = [
        {
            "path": path.relative_to(evidence_root).as_posix(),
            "sizeBytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
        for path in evidence_root.rglob("*")
        if path.is_file() and path.name not in {"manifest.json", "evidence-files.json"}
    ]
    evidence_files.sort(key=lambda row: validator.ordinal_lower_path_sort_key(str(row["path"])))
    fingerprint = validator.canonical_inventory_fingerprint(evidence_files)
    evidence_path = evidence_root / "evidence-files.json"
    _write_json_artifact(evidence_path, {
        "schema": "classmngr-phase9-p9-17-evidence-files-v1",
        "root": str(evidence_root.resolve()),
        "fileCount": len(evidence_files),
        "sha256": fingerprint,
        "files": evidence_files,
    })
    manifest["evidenceInventory"] = _artifact_record(
        evidence_root, evidence_path, treeSha256=fingerprint, fileCount=len(evidence_files)
    )
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")


def _write_source_inventory(
    evidence_root: Path,
    relative_path: str,
    repository_root: Path,
    included_roots: list[str],
    files: list[dict[str, object]],
) -> dict[str, object]:
    fingerprint = validator.canonical_inventory_fingerprint(files)
    inventory = {
        "repositoryRoot": str(repository_root.resolve()),
        "includedRoots": included_roots,
        "fileCount": len(files),
        "sha256": fingerprint,
        "files": files,
    }
    path = _write_json_artifact(evidence_root / relative_path, inventory)
    return _artifact_record(
        evidence_root,
        path,
        treeSha256=fingerprint,
        fileCount=len(files),
    )


def _synthetic_execution_context(observed_at: str) -> dict[str, object]:
    return {
        "identity": "Synthetic\\DiagnosticUser",
        "userSid": "S-1-5-21-123456789-123456789-123456789-1001",
        "isElevated": False,
        "isAdministratorRole": False,
        "integrityLevelRid": 8192,
        "integrityLevel": "Medium",
        "userInteractive": True,
        "processSessionId": 1,
        "activeConsoleSessionId": 1,
        "observedAtUtc": observed_at,
    }


def _synthetic_x64_executable() -> bytes:
    data = bytearray(0x80)
    data[0:2] = b"MZ"
    data[0x3C:0x40] = (0x40).to_bytes(4, "little")
    data[0x40:0x44] = b"PE\0\0"
    data[0x44:0x46] = (0x8664).to_bytes(2, "little")
    return bytes(data)


def _ensure_synthetic_release_binding(root: Path) -> tuple[Path, Path, list[dict[str, object]], list[str]]:
    repository_root = root / "synthetic-repository"
    repository_root.mkdir(parents=True, exist_ok=True)
    for relative_path in ("src/diagnostic_product.cpp", *validator.P917_HARNESS_SOURCES):
        source_path = repository_root / relative_path
        source_path.parent.mkdir(parents=True, exist_ok=True)
        if not source_path.exists():
            source_path.write_text(f"synthetic source for {relative_path}\n", encoding="utf-8")

    included_roots = ["src", *validator.P917_HARNESS_SOURCES]
    source_files = validator._scan_source_inventory(repository_root, included_roots)
    exact_build_root = repository_root / "artifacts/phase9/p9-17-exact-release-builds/synthetic-v3"
    build_source_path = exact_build_root / "source-files.json"
    if not build_source_path.exists():
        _write_json_artifact(build_source_path, {
            "repositoryRoot": str(repository_root.resolve()),
            "includedRoots": included_roots,
            "fileCount": len(source_files),
            "sha256": validator.canonical_inventory_fingerprint(source_files),
            "files": source_files,
        })

    build_directory = repository_root / "build/windows-x64-release"
    build_directory.mkdir(parents=True, exist_ok=True)
    cache_path = build_directory / "CMakeCache.txt"
    cache_path.write_text(
        "\n".join((
            f"CMAKE_GENERATOR:INTERNAL={validator.P917_RELEASE_GENERATOR}",
            f"CMAKE_GENERATOR_PLATFORM:STRING={validator.P917_RELEASE_PLATFORM}",
            f"CMAKE_GENERATOR_TOOLSET:STRING={validator.P917_RELEASE_TOOLSET}",
            f"CMAKE_GENERATOR_INSTANCE:INTERNAL={validator.P917_RELEASE_GENERATOR_INSTANCE}",
            f"CMAKE_HOME_DIRECTORY:INTERNAL={repository_root.resolve()}",
            "CLASSMNGR_NEXT_RELEASE_PACKAGE:BOOL=ON",
            "",
        )),
        encoding="utf-8",
    )
    generated_root = build_directory / "generated/ClassMngrNext"
    assets_root = generated_root / "Resources/assets"
    translations_root = generated_root / "assets/translations"
    assets_root.mkdir(parents=True, exist_ok=True)
    translations_root.mkdir(parents=True, exist_ok=True)
    for index in range(192):
        (assets_root / f"synthetic-asset-{index:03d}.bin").write_bytes(f"synthetic baked asset {index}\n".encode("utf-8"))
    (translations_root / "ClassMngrNext_en.qm").write_bytes(b"synthetic translation\n")
    catalog_path = generated_root / "resource-catalog.json"
    catalog_path.write_text("{\"synthetic\":true}\n", encoding="utf-8")
    assets_sha, assets_files = validator._directory_inventory(assets_root)
    translations_sha, translations_files = validator._directory_inventory(translations_root)

    release_executable = build_directory / "next-package-build/Release/ClassMngr.exe"
    release_executable.parent.mkdir(parents=True, exist_ok=True)
    if not release_executable.exists():
        release_executable.write_bytes(_synthetic_x64_executable())
    compiler_path = Path(validator.P917_RELEASE_COMPILER_PATH)
    if not compiler_path.is_file():
        raise RuntimeError(f"Required VS2022 compiler is unavailable for the v3 receipt fixture: {compiler_path}")
    compiler_identity = {
        "path": str(compiler_path.resolve()),
        "fileVersion": validator.P917_RELEASE_COMPILER_VERSION,
        "productVersion": validator.P917_RELEASE_COMPILER_VERSION,
        "sha256": hashlib.sha256(compiler_path.read_bytes()).hexdigest(),
        "toolsetVersion": validator.P917_RELEASE_TOOLSET_VERSION,
        "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
        "windowsSdkIncludeRoot": validator.P917_RELEASE_WINDOWS_SDK_INCLUDE_ROOT,
    }
    build_log_path = exact_build_root / "logs/current-source-release-build.log"
    build_log_path.parent.mkdir(parents=True, exist_ok=True)
    build_log_path.write_text(f"Compiler invoked: {compiler_path}\n", encoding="utf-8")
    build_log_sha = hashlib.sha256(build_log_path.read_bytes()).hexdigest()
    cache_sha = hashlib.sha256(cache_path.read_bytes()).hexdigest()
    build_context = {
        "start": _synthetic_execution_context("2026-11-24T00:00:00+00:00"),
        "completion": _synthetic_execution_context("2026-11-24T00:00:01+00:00"),
        "sameIdentityAndConsoleSession": True,
    }
    generated_project_toolchain = {
        "projectFileCount": 1,
        "platformToolset": validator.P917_RELEASE_TOOLSET,
        "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
    }
    preflight_path = exact_build_root / "preflight.json"
    _write_json_artifact(preflight_path, {
        "schema": "classmngr-phase9-p9-17-standard-token-build-preflight-v2",
        "executionContext": build_context["start"],
        "resourceRoot": str(assets_root.resolve()),
        "expectedBakedAssetFileCount": 192,
        "resourceOutputsGeneratedByBuild": True,
        "sourceInventoryPath": str(build_source_path.resolve()),
        "sourceInventoryFileSha256": hashlib.sha256(build_source_path.read_bytes()).hexdigest(),
        "sourceSha256": validator.canonical_inventory_fingerprint(source_files),
        "sourceFileCount": len(source_files),
        "cachePath": str(cache_path.resolve()),
        "cacheSha256": cache_sha,
        "generator": validator.P917_RELEASE_GENERATOR,
        "generatorPlatform": validator.P917_RELEASE_PLATFORM,
        "generatorToolset": validator.P917_RELEASE_TOOLSET,
        "generatorInstance": validator.P917_RELEASE_GENERATOR_INSTANCE,
        "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
        "generatedProjectToolchain": generated_project_toolchain,
        "compiler": compiler_identity,
        "packageOption": "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON",
        "status": "passed",
    })
    receipt_path = exact_build_root / "release-build-receipt.json"
    if not receipt_path.exists():
        build_inventory_hash = hashlib.sha256(build_source_path.read_bytes()).hexdigest()
        release_hash = hashlib.sha256(release_executable.read_bytes()).hexdigest()
        _write_json_artifact(receipt_path, {
            "schema": validator.P917_BUILD_RECEIPT_SCHEMA,
            "status": "succeeded",
            "repositoryRoot": str(repository_root.resolve()),
            "buildDirectory": str(build_directory.resolve()),
            "configuration": "Release",
            "platform": "x64",
            "generator": validator.P917_RELEASE_GENERATOR,
            "generatorPlatform": validator.P917_RELEASE_PLATFORM,
            "generatorToolset": validator.P917_RELEASE_TOOLSET,
            "generatorInstance": validator.P917_RELEASE_GENERATOR_INSTANCE,
            "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
            "generatedProjectToolchain": generated_project_toolchain,
            "compiler": compiler_identity,
            "packageOption": "CLASSMNGR_NEXT_RELEASE_PACKAGE=ON",
            "incrementalBuild": True,
            "executionContext": build_context,
            "cachePath": str(cache_path.resolve()),
            "cacheSha256": cache_sha,
            "build": {
                "exitCode": 0,
                "logPath": str(build_log_path.resolve()),
                "logSha256": build_log_sha,
                "actualCompilerPath": str(compiler_path.resolve()),
                "compilerObservedInLog": True,
            },
            "targets": ["ClassMngrNext", "ClassMngrNextResourcePackage"],
            "sourceInventoryPath": str(build_source_path.resolve()),
            "sourceInventoryFileSha256": build_inventory_hash,
            "sourceFileCount": len(source_files),
            "sourceSha256": validator.canonical_inventory_fingerprint(source_files),
            "releaseExecutablePath": str(release_executable.resolve()),
            "releaseExecutableSha256": release_hash,
            "preflightPath": str(preflight_path.resolve()),
            "preflightSha256": hashlib.sha256(preflight_path.read_bytes()).hexdigest(),
            "buildHelperPath": str((repository_root / validator.P917_BUILD_HELPER_RELATIVE).resolve()),
            "buildHelperSha256": hashlib.sha256((repository_root / validator.P917_BUILD_HELPER_RELATIVE).read_bytes()).hexdigest(),
            "generatedResources": {
                "bakedAssetsRoot": str(assets_root.resolve()),
                "bakedAssetsSha256": assets_sha,
                "bakedAssetsFileCount": len(assets_files),
                "translationsRoot": str(translations_root.resolve()),
                "translationsSha256": translations_sha,
                "translationsFileCount": len(translations_files),
                "catalogPath": str(catalog_path.resolve()),
                "catalogSha256": hashlib.sha256(catalog_path.read_bytes()).hexdigest(),
            },
        })
    return repository_root, receipt_path, source_files, included_roots


def _make_payload_arm_provenance(root: Path, arm_root: Path) -> dict[str, object]:
    repository_root, receipt_path, source_files, included_roots = _ensure_synthetic_release_binding(root)
    release_receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    package_root = arm_root / "package/bin"
    resource_root = package_root / "resources"
    resource_root.mkdir(parents=True, exist_ok=True)
    application_path = package_root / "ClassMngrNext.exe"
    shutil.copyfile(Path(release_receipt["releaseExecutablePath"]), application_path)
    generated = release_receipt["generatedResources"]
    assets_root = Path(generated["bakedAssetsRoot"])
    translations_root = Path(generated["translationsRoot"])
    for source in assets_root.rglob("*"):
        if source.is_file():
            destination = resource_root / "assets" / source.relative_to(assets_root)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    for source in translations_root.rglob("*"):
        if source.is_file():
            destination = resource_root / "assets/translations" / source.relative_to(translations_root)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    shutil.copyfile(generated["catalogPath"], resource_root / "resource-catalog.json")

    current_source = _write_source_inventory(
        arm_root, "source-files.json", repository_root, included_roots, source_files
    )
    product_files = [row for row in source_files if row["path"] == "src/diagnostic_product.cpp"]
    product_source = _write_source_inventory(
        arm_root, "product-source-files.json", repository_root, included_roots, product_files
    )
    harness_files = [row for row in source_files if row["path"] in set(validator.P917_HARNESS_SOURCES)]
    harness_source = _write_source_inventory(
        arm_root, "harness-source-files.json", repository_root,
        list(validator.P917_HARNESS_SOURCES), harness_files,
    )

    package_sha, package_files = validator._directory_inventory(package_root)
    package_inventory_path = _write_json_artifact(arm_root / "package-files.json", {
        "root": str(package_root.resolve()),
        "fileCount": len(package_files),
        "sha256": package_sha,
        "files": package_files,
    })
    package_inventory = _artifact_record(
        arm_root, package_inventory_path, treeSha256=package_sha, fileCount=len(package_files)
    )
    resource_sha, resource_files = validator._directory_inventory(resource_root)
    resource_inventory_path = _write_json_artifact(arm_root / "resource-files.json", {
        "root": str(resource_root.resolve()),
        "fileCount": len(resource_files),
        "sha256": resource_sha,
        "files": resource_files,
    })
    resource_inventory = _artifact_record(
        arm_root, resource_inventory_path, treeSha256=resource_sha, fileCount=len(resource_files)
    )
    receipt_hash = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
    source_inventory_path = Path(json.loads(receipt_path.read_text(encoding="utf-8"))["sourceInventoryPath"])
    source_inventory_hash = hashlib.sha256(source_inventory_path.read_bytes()).hexdigest()
    application_hash = hashlib.sha256(application_path.read_bytes()).hexdigest()
    execution_context = {
        "build": release_receipt["executionContext"],
        "observer": _synthetic_execution_context("2026-11-24T00:00:02+00:00"),
    }
    compiler = release_receipt["compiler"]
    build = release_receipt["build"]
    provenance: dict[str, object] = {
        "buildMode": "exact-current-source-release",
        "exactCurrentSourceBuild": True,
        "configuration": "Release",
        "platform": "x64",
        "architecture": "x64",
        "buildDirectory": release_receipt["buildDirectory"],
        "cmakeHomeDirectory": str(repository_root.resolve()),
        "buildExitCode": 0,
        "incrementalBuild": True,
        "cachePath": release_receipt["cachePath"],
        "cacheSha256": release_receipt["cacheSha256"],
        "generator": release_receipt["generator"],
        "generatorPlatform": release_receipt["generatorPlatform"],
        "generatorToolset": release_receipt["generatorToolset"],
        "generatorInstance": release_receipt["generatorInstance"],
        "windowsSdkVersion": release_receipt["windowsSdkVersion"],
        "compiler": compiler,
        "buildLogPath": build["logPath"],
        "buildLogSha256": build["logSha256"],
        "releaseExecutablePath": release_receipt["releaseExecutablePath"],
        "executionContext": execution_context,
        "packageRoot": str(package_root.resolve()),
        "packageSha256": package_sha,
        "packageInventory": package_inventory,
        "applicationPath": str(application_path.resolve()),
        "applicationSha256": application_hash,
        "releaseExecutableSha256": application_hash,
        "productSourceSha256": product_source["treeSha256"],
        "currentSourceSha256": current_source["treeSha256"],
        "currentSourceInventory": current_source,
        "productSourceInventory": product_source,
        "harnessSourceInventory": harness_source,
        "releaseBuildReceiptPath": str(receipt_path.resolve()),
        "releaseBuildReceiptSha256": receipt_hash,
        "acceptedReleaseReceiptPath": str(receipt_path.resolve()),
        "acceptedReleaseReceiptSha256": receipt_hash,
        "buildReceiptSourceInventoryPath": str(source_inventory_path.resolve()),
        "buildReceiptSourceInventoryFileSha256": source_inventory_hash,
        "resourceRoot": str(resource_root.resolve()),
        "resourceInventorySha256": resource_sha,
        "resourceInventory": resource_inventory,
    }
    process_execution_context = {
        "sharedSessionVerified": True,
        "measuredProcessSessionId": 1,
        "buildProcessSessionId": 1,
        "observerProcessSessionId": 1,
        "activeConsoleSessionId": 1,
        "buildUserSid": execution_context["build"]["completion"]["userSid"],
        "observerUserSid": execution_context["observer"]["userSid"],
        "buildIdentity": execution_context["build"]["completion"]["identity"],
        "observerIdentity": execution_context["observer"]["identity"],
    }

    evidence_files = [
        {
            "path": path.relative_to(arm_root).as_posix(),
            "sizeBytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
        for path in arm_root.rglob("*")
        if path.is_file() and path.name not in {"manifest.json", "evidence-files.json"}
    ]
    evidence_files.sort(key=lambda row: validator.ordinal_lower_path_sort_key(str(row["path"])))
    evidence_hash = validator.canonical_inventory_fingerprint(evidence_files)
    evidence_inventory_path = _write_json_artifact(arm_root / "evidence-files.json", {
        "schema": "classmngr-phase9-p9-17-evidence-files-v1",
        "root": str(arm_root.resolve()),
        "fileCount": len(evidence_files),
        "sha256": evidence_hash,
        "files": evidence_files,
    })
    return {
        "packageProvenance": provenance,
        "evidenceInventory": _artifact_record(
            arm_root, evidence_inventory_path, treeSha256=evidence_hash, fileCount=len(evidence_files)
        ),
        "repositoryRoot": str(repository_root.resolve()),
        "processImagePath": str(application_path.resolve()),
        "processExecutionContext": process_execution_context,
    }


def make_payload_diagnostic_arm(root: Path, mode: str, process_id: int) -> Path:
    arm_root = root / mode
    workspace_root = arm_root / "workspace"
    workspace_root.mkdir(parents=True)
    workspace = workspace_root / "large-roster.tps"
    receipt_path = workspace_root / "preparation-receipt.json"
    receipt = fixture.prepare(BASE, SEED, workspace, mode)
    receipt_path.write_text(json.dumps(receipt, ensure_ascii=False), encoding="utf-8")
    before = fixture.snapshot("before", workspace, receipt_path)
    before_path = workspace_root / "before-snapshot.json"
    before_path.write_text(json.dumps(before, ensure_ascii=False), encoding="utf-8")

    connection = sqlite3.connect(workspace)
    try:
        for target in receipt["expectedEditTargets"]:
            connection.execute(
                "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                (target["afterValue"], target["classId"], target["row"], target["storedColumn"]),
            )
        connection.commit()
    finally:
        connection.close()
    after = fixture.snapshot("after", workspace, receipt_path)
    after_path = workspace_root / "after-snapshot.json"
    after_path.write_text(json.dumps(after, ensure_ascii=False), encoding="utf-8")

    def readbacks(value_key: str) -> list[dict[str, object]]:
        return [
            {
                "name": target["name"], "row": target["row"],
                "modelColumn": target["modelColumn"], "storedColumn": target["storedColumn"],
                "expected": target[value_key], "actual": target[value_key], "matched": True,
            }
            for target in receipt["expectedEditTargets"]
        ]

    before_readbacks = readbacks("beforeValue")
    after_readbacks = readbacks("afterValue")
    cycle_records = [
        {
            "cycleNumber": cycle,
            "parity": "after-to-before" if cycle % 2 else "before-to-after",
            "initialReadbacks": before_readbacks,
            "editedReadbacks": after_readbacks,
            "savedReadbacks": after_readbacks,
            "gridRows": 100, "gridColumns": 156, "verifiedHeaderCount": 156,
        }
        for cycle in range(1, 11)
    ]
    actions = [
        {
            "actionKind": f"edit-target-{target['name']}",
            "method": "ValuePattern.SetValue",
            "row": target["row"], "modelColumn": target["modelColumn"],
            "storedColumn": target["storedColumn"],
            "expectedBeforeValue": target["beforeValue"],
            "valueAfter": target["afterValue"],
            "valuePatternSetValueSucceeded": True,
            "immediateSetValueReadback": target["afterValue"],
            "immediateSetValueReadbackSource": "GridPattern.GetItem + ValuePattern.SetValue followed by exact UIA readback",
            "target": {"name": target["name"], "controlType": "ControlType.DataItem", "automationId": ""},
        }
        for target in receipt["expectedEditTargets"]
    ]
    baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    cycle_ids = [
        f"cycle-{cycle:02d}-{stage}"
        for cycle in range(1, 11)
        for stage in ("roster-loaded", "post-save", "return")
    ] + ["cycle-final-reentry"]
    windows = []
    for sequence, window_id in enumerate(baseline_ids + cycle_ids):
        windows.append({
            "windowId": window_id,
            "state": "fixture-state",
            "phase": (
                "baseline" if window_id.startswith("plateau-")
                else "final-reentry" if window_id == "cycle-final-reentry"
                else "cycle"
            ),
            "matchedState": "classes-details-selected-class",
            "sampleCount": 10,
            "sampleIntervalMilliseconds": 100,
            "samplingCompleted": True,
            "samplingErrorCount": 0,
            "samplingErrors": [],
            "samples": [
                {
                    "sampleIndex": index,
                    "workingSetBytes": 100_000_000 + sequence * 100_000 + index,
                    "privateBytes": 200_000_000 + sequence * 200_000 + index,
                }
                for index in range(10)
            ],
        })

    binding = _make_payload_arm_provenance(root, arm_root)
    package_provenance = binding["packageProvenance"]
    manifest = {
        "schema": validator.SCHEMA,
        "taskId": "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
        "repositoryRoot": binding["repositoryRoot"],
        "status": "diagnostic-only",
        "diagnosticOnly": True,
        "accepted": False,
        "evidenceInventory": binding["evidenceInventory"],
        "payloadDiagnostic": {
            "schema": "classmngr-phase9-p9-17-payload-diagnostic-v1",
            "diagnosticOnly": True,
            "accepted": False,
            "payloadMode": mode,
            "customCellRecordCount": 15000,
            "customCellValueCount": 15000,
            "blankCustomCellValueCount": 0,
            "compactSentinelValue": validator.PAYLOAD_SPARSE_SENTINEL if mode == "sparse" else None,
            "compactSentinelValueCount": validator.PAYLOAD_SPARSE_SENTINEL_COUNT if mode == "sparse" else 0,
            "preservedProbeCellCount": 3,
            "preparationReceiptSha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
            "beforeSnapshotSha256": hashlib.sha256(before_path.read_bytes()).hexdigest(),
        },
        "packageProvenance": package_provenance,
        "process": {
            "processId": process_id,
            "sessionId": 1,
            "processImageSha256": package_provenance["applicationSha256"],
            "processImagePath": binding["processImagePath"],
            "executionContext": binding["processExecutionContext"],
        },
        "processExit": {"observed": True, "normalExit": True, "exitCode": 0, "forcedTermination": False},
        "fixture": {
            "payloadMode": mode,
            "payloadDiagnostic": receipt["payloadDiagnostic"],
            "fixturePreparerSha256": receipt["preparerSha256"],
            "preparationReceipt": {
                "relativePath": "workspace/preparation-receipt.json",
                "sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
                "workspaceSha256": receipt["workspaceSha256"],
                "identity": receipt["identity"],
            },
            "beforeSnapshot": {
                "relativePath": "workspace/before-snapshot.json",
                "sha256": hashlib.sha256(before_path.read_bytes()).hexdigest(),
                "phase": "before",
            },
            "afterSnapshot": {
                "relativePath": "workspace/after-snapshot.json",
                "sha256": hashlib.sha256(after_path.read_bytes()).hexdigest(),
                "phase": "after",
            },
        },
        "route": {
            "observerMode": "full-observer",
            "observerDiagnosticOnly": False,
            "primingCycleCount": 1,
            "baselineWindowsPerState": 5,
            "measuredCycleCount": 10,
            "memoryWindowSampleCount": 10,
            "memoryWindowIntervalMilliseconds": 100,
            "modelCounts": {"rosterRows": 100, "rosterColumns": 156, "verifiedHeaderCount": 156},
            "actions": actions,
            "cycleRecords": cycle_records,
            "readBackChecks": {
                "beforeEdit": before_readbacks,
                "afterEdit": after_readbacks,
                "afterSave": after_readbacks,
                "afterReentry": after_readbacks,
                "afterCycles": after_readbacks,
            },
        },
        "memory": {
            "workingSetTargetBytes": validator.WORKING_SET_TARGET,
            "workingSetTargetExclusive": True,
            "workingSetCeilingBytes": validator.WORKING_SET_CEILING,
            "workingSetCeilingExclusive": False,
            "operationPeakAdvisoryLimitBytes": validator.OPERATION_PEAK_ADVISORY_LIMIT,
            "plateauBaselineWindowIds": baseline_ids,
            "plateauCycleWindowIds": cycle_ids,
            "windows": windows,
        },
    }
    manifest_path = arm_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
    return manifest_path


def make_record_count_diagnostic_arm(root: Path, mode: str, process_id: int) -> Path:
    arm_root = root / mode
    workspace_root = arm_root / "workspace"
    workspace_root.mkdir(parents=True)
    workspace = workspace_root / "large-roster.tps"
    receipt_path = workspace_root / "preparation-receipt.json"
    receipt = fixture.prepare(BASE, SEED, workspace, record_count_mode=mode)
    receipt_path.write_text(json.dumps(receipt, ensure_ascii=False), encoding="utf-8")
    before = fixture.snapshot("before", workspace, receipt_path)
    before_path = workspace_root / "before-snapshot.json"
    before_path.write_text(json.dumps(before, ensure_ascii=False), encoding="utf-8")
    connection = sqlite3.connect(workspace)
    try:
        for target in receipt["expectedEditTargets"]:
            connection.execute(
                "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                (target["afterValue"], target["classId"], target["row"], target["storedColumn"]),
            )
        connection.commit()
    finally:
        connection.close()
    after = fixture.snapshot("after", workspace, receipt_path)
    after_path = workspace_root / "after-snapshot.json"
    after_path.write_text(json.dumps(after, ensure_ascii=False), encoding="utf-8")

    def readbacks(value_key: str) -> list[dict[str, object]]:
        return [
            {
                "name": target["name"], "row": target["row"],
                "modelColumn": target["modelColumn"], "storedColumn": target["storedColumn"],
                "expected": target[value_key], "actual": target[value_key], "matched": True,
            }
            for target in receipt["expectedEditTargets"]
        ]

    before_readbacks = readbacks("beforeValue")
    after_readbacks = readbacks("afterValue")
    cycle_records = [
        {
            "cycleNumber": cycle,
            "parity": "after-to-before" if cycle % 2 else "before-to-after",
            "initialReadbacks": before_readbacks,
            "editedReadbacks": after_readbacks,
            "savedReadbacks": after_readbacks,
            "gridRows": 100, "gridColumns": 156, "verifiedHeaderCount": 156,
        }
        for cycle in range(1, 11)
    ]
    actions = [
        {
            "actionKind": f"edit-target-{target['name']}", "method": "ValuePattern.SetValue",
            "row": target["row"], "modelColumn": target["modelColumn"],
            "storedColumn": target["storedColumn"],
            "expectedBeforeValue": target["beforeValue"], "valueAfter": target["afterValue"],
            "valuePatternSetValueSucceeded": True,
            "immediateSetValueReadback": target["afterValue"],
            "immediateSetValueReadbackSource": "GridPattern.GetItem + ValuePattern.SetValue followed by exact UIA readback",
            "target": {"name": target["name"], "controlType": "ControlType.DataItem", "automationId": ""},
        }
        for target in receipt["expectedEditTargets"]
    ]
    baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    cycle_ids = [
        f"cycle-{cycle:02d}-{stage}"
        for cycle in range(1, 11)
        for stage in ("roster-loaded", "post-save", "return")
    ] + ["cycle-final-reentry"]
    windows = []
    for sequence, window_id in enumerate(baseline_ids + cycle_ids):
        windows.append({
            "windowId": window_id,
            "state": "fixture-state",
            "phase": "baseline" if window_id.startswith("plateau-") else "final-reentry" if window_id == "cycle-final-reentry" else "cycle",
            "matchedState": "classes-details-selected-class",
            "sampleCount": 10,
            "sampleIntervalMilliseconds": 100,
            "samplingCompleted": True,
            "samplingErrorCount": 0,
            "samplingErrors": [],
            "samples": [
                {
                    "sampleIndex": index,
                    "workingSetBytes": 100_000_000 + sequence * 100_000 + index,
                    "privateBytes": 200_000_000 + sequence * 200_000 + index,
                }
                for index in range(10)
            ],
        })
    binding = _make_payload_arm_provenance(root, arm_root)
    package = binding["packageProvenance"]
    record = receipt["recordCountDiagnostic"]
    manifest = {
        "schema": validator.SCHEMA,
        "taskId": "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
        "repositoryRoot": binding["repositoryRoot"],
        "status": "diagnostic-only", "diagnosticOnly": True, "accepted": False,
        "evidenceInventory": binding["evidenceInventory"],
        "recordCountDiagnostic": {
            "schema": validator.RECORD_COUNT_DIAGNOSTIC_SCHEMA,
            "diagnosticOnly": True, "accepted": False,
            "recordCountMode": mode,
            "customCellRecordCount": record["customCellRecordCount"],
            "customCellValueCount": record["customCellValueCount"],
            "blankCustomCellValueCount": record["blankCustomCellValueCount"],
            "removedCustomCellRecordCount": record["removedCustomCellRecordCount"],
            "preservedProbeCellCount": 3,
            "preparationReceiptSha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
            "beforeSnapshotSha256": hashlib.sha256(before_path.read_bytes()).hexdigest(),
        },
        "packageProvenance": package,
        "process": {
            "processId": process_id, "sessionId": 1,
            "processImageSha256": package["applicationSha256"],
            "processImagePath": binding["processImagePath"],
            "executionContext": binding["processExecutionContext"],
        },
        "processExit": {"observed": True, "normalExit": True, "exitCode": 0, "forcedTermination": False},
        "fixture": {
            "recordCountMode": mode,
            "recordCountDiagnostic": record,
            "fixturePreparerSha256": receipt["preparerSha256"],
            "preparationReceipt": {
                "relativePath": "workspace/preparation-receipt.json",
                "sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
                "workspaceSha256": receipt["workspaceSha256"], "identity": receipt["identity"],
            },
            "beforeSnapshot": {
                "relativePath": "workspace/before-snapshot.json",
                "sha256": hashlib.sha256(before_path.read_bytes()).hexdigest(), "phase": "before",
            },
            "afterSnapshot": {
                "relativePath": "workspace/after-snapshot.json",
                "sha256": hashlib.sha256(after_path.read_bytes()).hexdigest(), "phase": "after",
            },
        },
        "route": {
            "observerMode": "full-observer", "observerDiagnosticOnly": False,
            "primingCycleCount": 1, "baselineWindowsPerState": 5, "measuredCycleCount": 10,
            "memoryWindowSampleCount": 10, "memoryWindowIntervalMilliseconds": 100,
            "modelCounts": {"rosterRows": 100, "rosterColumns": 156, "verifiedHeaderCount": 156},
            "actions": actions, "cycleRecords": cycle_records,
            "readBackChecks": {
                "beforeEdit": before_readbacks, "afterEdit": after_readbacks,
                "afterSave": after_readbacks, "afterReentry": after_readbacks,
                "afterCycles": after_readbacks,
            },
        },
        "memory": {
            "workingSetTargetBytes": validator.WORKING_SET_TARGET,
            "workingSetTargetExclusive": True,
            "workingSetCeilingBytes": validator.WORKING_SET_CEILING,
            "workingSetCeilingExclusive": False,
            "operationPeakAdvisoryLimitBytes": validator.OPERATION_PEAK_ADVISORY_LIMIT,
            "plateauBaselineWindowIds": baseline_ids,
            "plateauCycleWindowIds": cycle_ids,
            "windows": windows,
        },
    }
    manifest_path = arm_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
    _refresh_arm_evidence_inventory(manifest_path)
    return manifest_path


def make_normal_cleanup_gate_manifest(root: Path) -> Path:
    repository_root = root / "repository"
    package_root = root / "package"
    evidence_root = root / "evidence"
    repository_root.mkdir()
    package_root.mkdir()
    evidence_root.mkdir()
    app_path = package_root / "ClassMngrNext.exe"
    app_path.write_bytes(_synthetic_x64_executable())
    app_hash = hashlib.sha256(app_path.read_bytes()).hexdigest()
    expected_name = "E4 Level 02 â€¢ M/F 5:00"
    targets = [
        {"name": "start", "classId": 1, "row": 0, "modelColumn": 6, "storedColumn": 6, "beforeValue": "before-start", "afterValue": "after-start"},
        {"name": "middle", "classId": 1, "row": 49, "modelColumn": 77, "storedColumn": 77, "beforeValue": "before-middle", "afterValue": "after-middle"},
        {"name": "end", "classId": 1, "row": 99, "modelColumn": 155, "storedColumn": 155, "beforeValue": "before-end", "afterValue": "after-end"},
    ]
    identity = {
        "targetClassId": 1,
        "targetClassName": "Heavy Fixture Class 001",
        "targetClassListAccessibleName": expected_name,
        "targetRosterRowCount": 100,
        "targetDisplayedRosterColumnCount": 156,
    }

    def artifact(relative_path: str, value: dict[str, object]) -> dict[str, object]:
        path = _write_json_artifact(evidence_root / relative_path, value)
        return _artifact_record(evidence_root, path)

    receipt = {
        "schema": validator.FIXTURE_SCHEMA,
        "status": "passed",
        "baseWorkspaceSha256": validator.BASE_SHA256,
        "seedSqlSha256": validator.SEED_SHA256,
        "preparerSha256": "synthetic-preparer-hash",
        "workspaceSha256": "synthetic-workspace-hash",
        "expectedEditTargets": targets,
        "identity": identity,
    }
    receipt_record = artifact("workspace/preparation.json", receipt)
    before_record = artifact("workspace/before.json", {
        "workspaceSha256": receipt["workspaceSha256"], "identity": identity, "workspaceTableCount": 17,
    })
    after_record = artifact("workspace/after.json", {"identity": identity})
    fixture = {
        "workspacePath": str((evidence_root / "workspace/large-roster.tps").resolve()),
        "fixturePreparerSha256": receipt["preparerSha256"],
        "preparationReceipt": {**receipt_record, "workspaceSha256": receipt["workspaceSha256"]},
        "beforeSnapshot": before_record,
        "afterSnapshot": after_record,
    }

    screenshots = []
    for index in range(3):
        path = evidence_root / f"screenshots/{index}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}", encoding="utf-8")
        screenshots.append({
            "relativePath": path.relative_to(evidence_root).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "processId": 4567,
            "windowHandle": 1,
        })
    uia_snapshots = []
    for index in range(3):
        path = evidence_root / f"uia/base-{index}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{}", encoding="utf-8")
        uia_snapshots.append({
            "relativePath": path.relative_to(evidence_root).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "processId": 4567,
            "windowHandle": 1,
        })
    actions = [
        {"actionKind": kind}
        for kind in (
            "open-teacher-profile", "navigate-classes", "select-target-class", "open-roster-tab",
            "edit-target-start", "edit-target-middle", "edit-target-end", "leave-roster-section",
            "navigate-my_workspace", "navigate-classes", "reenter-roster-tab",
        )
    ]
    cycle_records = []
    memory_windows = []
    for cycle in range(1, 11):
        base = datetime(2026, 11, 24, 1, 0, tzinfo=timezone.utc) + timedelta(minutes=cycle)
        loaded_completed = base + timedelta(seconds=1)
        issued = base + timedelta(seconds=2)
        observed = base + timedelta(seconds=3)
        post_started = base + timedelta(seconds=4)
        save_action_index = len(actions)
        actions.append({
            "actionKind": "save-roster", "cycleNumber": cycle,
            "measurementRole": "measured-cycle-save",
            "issuedAtUtc": issued.isoformat(), "savedStateObservedAtUtc": observed.isoformat(),
            "completedAtUtc": observed.isoformat(),
        })
        loaded_id = f"loaded-{cycle:02d}"
        post_save_id = f"post-save-{cycle:02d}"
        memory_windows.extend((
            {"windowId": loaded_id, "completedAtUtc": loaded_completed.isoformat()},
            {"windowId": post_save_id, "startedAtUtc": post_started.isoformat()},
        ))
        initial = [
            {"name": target["name"], "actual": target["afterValue"] if cycle % 2 else target["beforeValue"], "matched": True}
            for target in targets
        ]
        edited = [
            {"name": target["name"], "actual": target["beforeValue"] if cycle % 2 else target["afterValue"], "matched": True}
            for target in targets
        ]
        relative_paths = []
        for stage in ("roster-loaded-contract", "edited-contract", "post-save-contract"):
            relative = f"uia/cycle-{cycle:02d}-{stage}.json"
            path = evidence_root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("{}", encoding="utf-8")
            uia_snapshots.append({
                "relativePath": relative, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "processId": 4567, "windowHandle": 1,
            })
            relative_paths.append(relative)
        cycle_records.append({
            "cycleNumber": cycle,
            "targetClassId": 1,
            "targetClassName": identity["targetClassName"],
            "selectedClassAccessibleName": expected_name,
            "classSelectionVerified": True,
            "parity": "after-to-before" if cycle % 2 else "before-to-after",
            "initialReadbacks": initial,
            "editedReadbacks": edited,
            "savedReadbacks": edited,
            "loadedContractRelativePath": relative_paths[0],
            "editedContractRelativePath": relative_paths[1],
            "savedContractRelativePath": relative_paths[2],
            "saveActionIndex": save_action_index,
            "rosterLoadedWindowId": loaded_id,
            "postSaveWindowId": post_save_id,
        })
    final_contract_path = evidence_root / "uia/final-cycle.json"
    final_contract_path.parent.mkdir(parents=True, exist_ok=True)
    final_contract_path.write_text("{}", encoding="utf-8")
    uia_snapshots.append({
        "relativePath": "uia/final-cycle.json",
        "sha256": hashlib.sha256(final_contract_path.read_bytes()).hexdigest(),
        "processId": 4567, "windowHandle": 1,
    })
    after_readbacks = [
        {"name": target["name"], "actual": target["afterValue"], "matched": True}
        for target in targets
    ]
    route = {
        "workspaceOpen": {"selectedPath": fixture["workspacePath"], "dialogWindowHandle": 2},
        "actions": actions,
        "routeEvents": [
            {"targetPageId": "my_workspace"}, {"targetPageId": "classes"}, {"targetPageId": "classes"},
        ],
        "targetClassSelection": {"selected": True, "classId": 1, "className": identity["targetClassName"]},
        "reentryTargetClassSearch": {"targetAccessibleName": expected_name, "matched": True, "pages": [{"matchingAccessibleNameCount": 1}]},
        "reentryTargetClassSelection": {
            "classId": 1, "className": identity["targetClassName"], "listItemName": expected_name,
            "expectedAccessibleName": expected_name, "selected": True,
        },
        "lifecycleEvents": [
            {"pageKey": "classes", "state": "activated"},
            {"pageKey": "classes", "state": "suspended"},
            {"pageKey": "classes", "state": "activated"},
        ],
        "modelCounts": {"rosterRows": 100, "rosterColumns": 156, "verifiedHeaderCount": 156, "preSuspensionRootModelRows": 100},
        "screenshots": screenshots,
        "uiaSnapshots": uia_snapshots,
        "postPrimingClearCheckpoint": {},
        "cycleRecords": cycle_records,
        "readBackChecks": {phase: after_readbacks for phase in ("afterEdit", "afterSave", "afterReentry")},
        "finalCycleReadback": after_readbacks,
        "finalCycleRosterContractRelativePath": "uia/final-cycle.json",
    }
    manifest = {
        "schema": validator.SCHEMA,
        "taskId": "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
        "status": "completed", "diagnosticOnly": False, "repositoryRoot": str(repository_root.resolve()),
        "packageProvenance": {
            "configuration": "Release", "platform": "x64", "architecture": "x64",
            "cmakeHomeDirectory": str(repository_root.resolve()),
            "buildMode": "exact-current-source-release", "buildExitCode": 0,
            "exactCurrentSourceBuild": True,
            "currentSourceInventory": {"relativePath": "source.json"},
            "productSourceInventory": {"relativePath": "product.json", "treeSha256": "product"},
            "harnessSourceInventory": {"relativePath": "harness.json", "treeSha256": "harness"},
            "packageRoot": str(package_root.resolve()),
            "packageInventory": {"relativePath": "package-files.json"},
            "applicationPath": str(app_path.resolve()), "applicationSha256": app_hash,
            "machineType": 0x8664,
        },
        "fixture": fixture,
        "process": {
            "processId": 4567, "mainWindowHandle": 1, "windowProcessId": 4567,
            "sessionId": 1, "processImagePath": str(app_path.resolve()),
            "processImageSha256": app_hash, "dialogWindowHandles": [2],
        },
        "processExit": {"observed": True, "normalExit": True, "exitCode": 0, "closeMethod": "WindowPattern.Close", "forcedTermination": False},
        "route": route,
        "memory": {"windows": memory_windows},
        "evidenceInventory": None,
    }
    manifest_path = evidence_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
    return manifest_path


class ValidateP917LargeRosterTests(unittest.TestCase):
    def test_inventory_fingerprint_uses_matching_ordinal_order_across_python_and_powershell(self) -> None:
        paths = [
            "package/bin/ClassMngrNext.exe",
            "package-files.json",
        ]
        rows = [
            {"path": paths[0], "sizeBytes": 17, "sha256": "a" * 64},
            {"path": paths[1], "sizeBytes": 23, "sha256": "b" * 64},
        ]
        ordered_paths = [
            row["path"]
            for row in sorted(rows, key=lambda row: validator.ordinal_lower_path_sort_key(row["path"]))
        ]
        self.assertEqual(ordered_paths, ["package-files.json", "package/bin/ClassMngrNext.exe"])
        expected_fingerprint = validator.canonical_inventory_fingerprint(rows)

        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("function Sort-P917OrdinalInventoryRows", runner_source)
        self.assertIn("$rows = @(Sort-P917OrdinalInventoryRows -Rows $files.ToArray())", runner_source)
        self.assertIn("[System.StringComparer]::Ordinal.Compare", runner_source)
        if sys.platform != "win32":
            self.skipTest("The P9-17 runner inventory helper is PowerShell for local Windows.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for evaluating the runner's inventory sort helper.")

        command = r"""
$path = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
$functionAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Sort-P917OrdinalInventoryRows' }, $true)
if ($null -eq $functionAst) { throw 'Ordinal inventory sort helper is missing.' }
Invoke-Expression $functionAst.Extent.Text
$rows = [object[]]@(
    [pscustomobject]@{ path = 'package/bin/ClassMngrNext.exe'; sizeBytes = 17; sha256 = ('a' * 64) }
    [pscustomobject]@{ path = 'package-files.json'; sizeBytes = 23; sha256 = ('b' * 64) }
)
$sortedRows = @(Sort-P917OrdinalInventoryRows -Rows $rows)
$canonical = @($sortedRows | ForEach-Object { "{0}`t{1}`t{2}" -f $_.path, $_.sizeBytes, $_.sha256 }) -join "`n"
$sha = [System.Security.Cryptography.SHA256]::Create()
$hash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical + "`n"))).Replace('-', '').ToLowerInvariant()
ConvertTo-Json -InputObject @{ paths = @($sortedRows | ForEach-Object { $_.path }); sha256 = $hash } -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env=environment,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        actual = json.loads(result.stdout.strip().splitlines()[-1])
        self.assertEqual(actual["paths"], ordered_paths)
        self.assertEqual(actual["sha256"], expected_fingerprint)

    def test_runner_owned_source_check_uses_the_declared_variable(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")

        self.assertIn("$OwnedSources = @(", runner_source)
        self.assertIn("foreach ($path in $HarnessSources)", runner_source)
        self.assertIn("($OwnedSources | Sort-Object)", runner_source)
        self.assertIn("scripts/phase9/p9_17_build_current_source_release.ps1", runner_source)
        self.assertIn(validator.P917_BUILD_HELPER_RELATIVE, validator.P917_OWNED_SOURCES)
        owned_start = runner_source.index("$OwnedSources = @(")
        owned_end = runner_source.index("\n)", owned_start)
        runner_owned = re.findall(r"'([^']+)'", runner_source[owned_start:owned_end])
        self.assertEqual(tuple(runner_owned), validator.P917_OWNED_SOURCES)
        self.assertNotIn("$P917OwnedSources", runner_source)

    def test_package_staging_log_calls_resolve_to_the_utf8_file_writer(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("function Write-P917File([string]$Path, [object[]]$Lines)", runner_source)
        self.assertEqual(runner_source.count("Write-P917File"), 5)
        package_command = runner_source.split("function Invoke-P917PackageCommand(", 1)[1].split(
            "\nfunction ", 1
        )[0]
        self.assertIn("Write-P917File $LogPath", package_command)
        if sys.platform != "win32":
            self.skipTest("The P9-17 runner uses Windows PowerShell.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for writer validation.")

        with tempfile.TemporaryDirectory() as temporary:
            command = r"""
$runnerPath = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
$writerAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Write-P917File' }, $true)
if ($null -eq $writerAst) { throw 'P9-17 text-file writer definition is missing.' }
$writerCalls = @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Write-P917File' }, $true))
if ($writerCalls.Count -ne 4) { throw "Expected four bound text-file writer calls; found $($writerCalls.Count)." }
Invoke-Expression $writerAst.Extent.Text
$path = Join-Path $env:P917_OUTPUT_ROOT 'package-command.log'
Write-P917File $path @('exitCode: 0', 'stdoutPath: stdout.txt', 'stderrPath: stderr.txt')
$bytes = [IO.File]::ReadAllBytes($path)
$text = [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
$expected = ('exitCode: 0' + [Environment]::NewLine + 'stdoutPath: stdout.txt' + [Environment]::NewLine + 'stderrPath: stderr.txt' + [Environment]::NewLine)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
ConvertTo-Json -InputObject @{ callCount = $writerCalls.Count; contentMatches = ($text -ceq $expected); utf8Bom = $hasBom; outputExists = (Test-Path -LiteralPath $path -PathType Leaf) } -Compress
"""
            environment = os.environ.copy()
            environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
            environment["P917_OUTPUT_ROOT"] = temporary
            result = subprocess.run(
                [powershell, "-NoLogo", "-NoProfile", "-Command", command],
                capture_output=True,
                text=True,
                env=environment,
                timeout=20,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            json.loads(result.stdout.strip().splitlines()[-1]),
            {"callCount": 4, "contentMatches": True, "utf8Bom": False, "outputExists": True},
        )

    def test_heap_snapshots_bind_to_three_post_window_pre_action_checkpoints(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        worker_path = RUNNER_PATH.with_name("p9_17_heap_snapshot_elevated_worker.ps1")
        worker_source = worker_path.read_text(encoding="utf-8")
        measured = runner_source.split("function Invoke-P917MeasuredRosterCycles(", 1)[1].split(
            "\nfunction Invoke-P917Route(", 1
        )[0]

        baseline_sample = measured.index("$window = Get-P917MemoryWindow $Context $Manifest $windowId")
        baseline_snapshot = measured.index("Add-P917HeapSnapshot $HeapDiagnostic 'post-primer-plateau-baseline'")
        first_cycle_action = measured.index('"cycle-$cycleTag-open-roster"')
        self.assertLess(baseline_sample, baseline_snapshot)
        self.assertLess(baseline_snapshot, first_cycle_action)
        self.assertIn("nextUiActionKind = 'cycle-01-open-roster'", measured)

        cycle_return_sample = measured.index("$returnWindow = Get-P917MemoryWindow $Context $Manifest $returnWindowId")
        cycle_return_snapshot = measured.index('Add-P917HeapSnapshot $HeapDiagnostic "cycle-$cycleTag-return"')
        cycle_record = measured.index("$Manifest.route.cycleRecords.Add", cycle_return_snapshot)
        final_reentry = measured.index("'cycle-final-reenter-roster' $rosterTab")
        self.assertLess(cycle_return_sample, cycle_return_snapshot)
        self.assertLess(cycle_return_snapshot, cycle_record)
        self.assertLess(cycle_record, final_reentry)
        self.assertIn("'cycle-02-open-roster'", measured)
        self.assertIn("'cycle-final-reenter-roster'", measured)
        self.assertIn("$cycleNumber -in @(1, 10)", measured)
        self.assertIn("'plateau-baseline-05'", worker_source)
        self.assertIn("'cycle-01-return'", worker_source)
        self.assertIn("'cycle-10-return'", worker_source)
        self.assertIn("Heap snapshot checkpoints were requested out of the required plateau, cycle-1, cycle-10 order.", worker_source)
        self.assertIn("Measured application PID image or session changed before the requested heap snapshot.", worker_source)
        self.assertNotIn("Add-P917HeapSnapshot $HeapDiagnostic 'matched-baseline'", runner_source)
        self.assertNotIn("Add-P917HeapSnapshot $HeapDiagnostic 'post-roster-clear'", runner_source)
        self.assertNotIn("Add-P917HeapSnapshot $HeapDiagnostic 're-entry'", runner_source)

    def test_heap_snapshot_wpr_call_path_p917_commands_resolve(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        common_path = RUNNER_PATH.with_name("p9_14_packaged_lifecycle_common.ps1")
        common_source = common_path.read_text(encoding="utf-8")
        function_pattern = re.compile(r"(?m)^function\s+([A-Za-z][A-Za-z0-9-]*)\b")

        def function_bodies(source: str) -> dict[str, str]:
            matches = list(function_pattern.finditer(source))
            return {
                match.group(1): source[match.start() : matches[index + 1].start() if index + 1 < len(matches) else len(source)]
                for index, match in enumerate(matches)
            }

        runner_functions = function_bodies(runner_source)
        imported_functions = function_bodies(common_source)
        wpr_functions = (
            "New-P917HeapSnapshotDiagnostic",
            "Assert-P917HeapSnapshotApplicationBinding",
            "New-P917HeapSnapshotBrokerBootstrap",
            "Stop-P917HeapSnapshotLaunchBroker",
            "Start-P917HeapSnapshotTrace",
            "Invoke-P917HeapWorkerRequest",
            "Add-P917HeapSnapshot",
            "Complete-P917HeapSnapshotTrace",
            "Stop-P917HeapSnapshotTrace",
        )
        missing_definitions = set(wpr_functions) - runner_functions.keys()
        self.assertFalse(missing_definitions, f"WPR call-path definitions are missing: {sorted(missing_definitions)}")

        invoked_helper_pattern = re.compile(r"\b(?:Assert|Get|Grant|Set|Start|Stop|Invoke)-P917[A-Za-z0-9_-]*\b")
        unresolved: dict[str, list[str]] = {}
        observed_calls: set[str] = set()
        available_functions = runner_functions.keys() | imported_functions.keys()
        for function_name in wpr_functions:
            calls = set(invoked_helper_pattern.findall(runner_functions[function_name]))
            observed_calls.update(calls)
            missing_calls = sorted(calls - available_functions)
            if missing_calls:
                unresolved[function_name] = missing_calls
        self.assertFalse(unresolved, f"WPR call-path invokes undefined P9-17 helpers: {unresolved}")
        for helper_name in (
            "Assert-P917ExecutionContextRecord",
            "Assert-P917HeapSnapshotApplicationBinding",
            "Grant-P917HeapAdministratorDirectoryAccess",
        ):
            self.assertIn(helper_name, observed_calls)

        start_trace = runner_functions["Start-P917HeapSnapshotTrace"]
        self.assertIn("$runnerContext = Get-P917CurrentExecutionContext", start_trace)
        self.assertIn(
            "Assert-P917HeapSnapshotApplicationBinding -RunnerContext $runnerContext -ApplicationContext $ApplicationContext -ProcessEvidence $ProcessEvidence",
            start_trace,
        )
        self.assertNotIn("Get-P917ExecutionContext", start_trace)
        self.assertNotIn("Assert-P917ExecutionContextRecord $ApplicationExecutionContext", start_trace)
        application_binding = runner_functions["Assert-P917HeapSnapshotApplicationBinding"]
        self.assertNotIn("$ApplicationContext.executionContext.identity", application_binding)
        lifecycle = runner_functions["Invoke-P917PackagedLifecycle"]
        self.assertIn("Start-P917HeapSnapshotTrace $heapDiagnostic $context $manifest.process", lifecycle)
        for process_field in (
            "processId = [Int64]$context.process.Id",
            "processImagePath = [string]$context.processImagePath",
            "processImageSha256 = [string]$context.processImageSha256",
            "sessionId = [Int64]$context.processSessionId",
            "executionContext = $context.executionContext",
        ):
            self.assertIn(process_field, lifecycle)
        current_context = runner_functions["Get-P917CurrentExecutionContext"]
        for field in (
            "identity",
            "userSid",
            "isElevated",
            "isAdministratorRole",
            "integrityLevelRid",
            "integrityLevel",
            "userInteractive",
            "processSessionId",
            "activeConsoleSessionId",
        ):
            self.assertRegex(current_context, rf"(?m)^\s+{field}\s*=")

    def test_heap_snapshot_application_binding_accepts_distinct_contexts_and_rejects_mismatches(self) -> None:
        if sys.platform != "win32":
            self.skipTest("The P9-17 heap snapshot harness uses Windows PowerShell.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for app-binding validation.")
        command = r"""
$runnerPath = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
$functionAsts = @{}
foreach ($functionAst in @($ast.FindAll({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true))) {
    $functionAsts[$functionAst.Name] = $functionAst
}
foreach ($functionName in @('Test-P917JsonInteger', 'Assert-P917ExecutionContextRecord', 'Assert-P917HeapSnapshotApplicationBinding')) {
    if (-not $functionAsts.ContainsKey($functionName)) { throw "Required P9-17 binding helper is missing: $functionName" }
    Invoke-Expression $functionAsts[$functionName].Extent.Text
}
$runner = [ordered]@{
    identity = 'Synthetic\Runner'
    userSid = 'S-1-5-21-123456789-123456789-123456789-1001'
    isElevated = $false
    isAdministratorRole = $false
    integrityLevelRid = [Int64]8192
    integrityLevel = 'Medium'
    userInteractive = $true
    processSessionId = [Int64]1
    activeConsoleSessionId = [Int64]1
}
$appSession = [ordered]@{
    buildIdentity = $runner.identity
    buildUserSid = $runner.userSid
    observerIdentity = $runner.identity
    observerUserSid = $runner.userSid
    buildProcessSessionId = [Int64]1
    observerProcessSessionId = [Int64]1
    measuredProcessSessionId = [Int64]1
    activeConsoleSessionId = [Int64]1
    sharedSessionVerified = $true
}
$liveProcess = [pscustomobject]@{ Id = [Int32]4712; SessionId = [Int32]1; HasExited = $false }
$liveProcess | Add-Member -MemberType ScriptMethod -Name Refresh -Value {}
$application = [ordered]@{
    process = $liveProcess
    processImagePath = 'C:\P917Test\ClassMngr.exe'
    processImageSha256 = ('a' * 64)
    processSessionId = [Int64]1
    executionContext = $appSession
}
$evidence = [ordered]@{
    processId = [Int64]4712
    processImagePath = 'C:\P917Test\ClassMngr.exe'
    processImageSha256 = ('a' * 64)
    sessionId = [Int64]1
    executionContext = $appSession
}
Assert-P917ExecutionContextRecord $runner 'synthetic runner'
$validDistinctSchemas = -not $appSession.Contains('identity') -and $runner.Contains('identity')
if ($validDistinctSchemas) { Assert-P917HeapSnapshotApplicationBinding $runner $application $evidence }
function Test-P917ExpectedBindingRejection([object]$RunnerContext, [object]$ApplicationContext, [object]$ProcessEvidence, [string]$Pattern) {
    try {
        Assert-P917HeapSnapshotApplicationBinding $RunnerContext $ApplicationContext $ProcessEvidence
        return $false
    } catch {
        return ([string]$_.Exception.Message -match $Pattern)
    }
}
$badPid = [ordered]@{
    processId = [Int64]4713; processImagePath = $evidence.processImagePath
    processImageSha256 = $evidence.processImageSha256; sessionId = $evidence.sessionId
    executionContext = $evidence.executionContext
}
$badSession = [ordered]@{
    processId = $evidence.processId; processImagePath = $evidence.processImagePath
    processImageSha256 = $evidence.processImageSha256; sessionId = [Int64]2
    executionContext = $evidence.executionContext
}
$badImageHash = [ordered]@{
    processId = $evidence.processId; processImagePath = $evidence.processImagePath
    processImageSha256 = ('b' * 64); sessionId = $evidence.sessionId
    executionContext = $evidence.executionContext
}
$badImagePath = [ordered]@{
    processId = $evidence.processId; processImagePath = 'C:\P917Test\Other.exe'
    processImageSha256 = $evidence.processImageSha256; sessionId = $evidence.sessionId
    executionContext = $evidence.executionContext
}
ConvertTo-Json -InputObject @{
    validDistinctSchemas = $validDistinctSchemas
    pidMismatchRejected = (Test-P917ExpectedBindingRejection $runner $application $badPid 'PID')
    sessionMismatchRejected = (Test-P917ExpectedBindingRejection $runner $application $badSession 'session')
    imageHashMismatchRejected = (Test-P917ExpectedBindingRejection $runner $application $badImageHash 'image hash')
    imagePathMismatchRejected = (Test-P917ExpectedBindingRejection $runner $application $badImagePath 'image path')
} -Compress
"""
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env={**os.environ, "P917_RUNNER_PATH": str(RUNNER_PATH.resolve())},
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            json.loads(result.stdout.strip().splitlines()[-1]),
            {
                "validDistinctSchemas": True,
                "pidMismatchRejected": True,
                "sessionMismatchRejected": True,
                "imageHashMismatchRejected": True,
                "imagePathMismatchRejected": True,
            },
        )

    def test_heap_snapshot_worker_is_a_bounded_diagnostic_only_wpr_process(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        worker_path = RUNNER_PATH.with_name("p9_17_heap_snapshot_elevated_worker.ps1")
        worker_source = worker_path.read_text(encoding="utf-8")
        start_trace = runner_source.split("function Start-P917HeapSnapshotTrace(", 1)[1].split(
            "\nfunction ", 1
        )[0]
        self.assertIn("scripts/phase9/p9_17_heap_snapshot_elevated_worker.ps1", validator.P917_OWNED_SOURCES)
        self.assertIn("workerElevationMechanism = 'One user-approved UAC RunAs launch", runner_source)
        self.assertIn("Start-Process -FilePath $workerPowerShell -ArgumentList $brokerArguments -WorkingDirectory $RepositoryRoot -PassThru -WindowStyle Hidden", start_trace)
        self.assertEqual(start_trace.count("-Verb RunAs"), 0)
        self.assertEqual(runner_source.count("-Verb RunAs"), 1)
        self.assertIn("function New-P917HeapSnapshotBrokerBootstrap", runner_source)
        self.assertIn("$elevatedWorker = Start-Process -FilePath ([string]$brokerConfig.workerPowerShell) -ArgumentList ([string]$brokerConfig.workerArguments) -WorkingDirectory ([string]$brokerConfig.workingDirectory) -Verb RunAs -PassThru -WindowStyle Hidden", runner_source)
        self.assertNotIn("$startInfo", start_trace)
        self.assertNotIn("ProcessStartInfo", start_trace)
        self.assertNotIn("RedirectStandardOutput", start_trace)
        self.assertNotIn("RedirectStandardError", start_trace)
        self.assertNotIn("CreateNoWindow", start_trace)
        self.assertNotIn("UseShellExecute = $false", start_trace)
        self.assertIn("workerLaunchTimeoutSeconds = 180", runner_source)
        self.assertIn("workerBrokerStopTimeoutSeconds = 5", runner_source)
        self.assertIn("Stop-P917HeapSnapshotLaunchBroker $brokerProcess", start_trace)
        self.assertIn("Stop-Process -Id ([int]$BrokerProcess.Id) -Force", runner_source)
        self.assertIn("$BrokerProcess.WaitForExit($TimeoutMilliseconds)", runner_source)
        timeout_branch = start_trace.index("if ($null -eq $launchStatus)")
        timeout_stop = start_trace.index("Stop-P917HeapSnapshotLaunchBroker $brokerProcess", timeout_branch)
        timeout_failure = start_trace.index("did not return a status within", timeout_branch)
        authorization_write = start_trace.index("Write-P917HeapJsonAtomic ([string]$Diagnostic.workerAuthorizationPath) $authorization")
        self.assertLess(timeout_branch, timeout_stop)
        self.assertLess(timeout_stop, timeout_failure)
        self.assertLess(timeout_failure, authorization_write)
        self.assertIn("workerStartupTimeoutSeconds = 240", runner_source)
        self.assertIn("Assert-P917HeapSnapshotApplicationBinding -RunnerContext $runnerContext", runner_source)
        self.assertIn("runnerUserSid = [string]$runnerContext.userSid", runner_source)
        self.assertIn("processSessionId = $processSessionId", runner_source)
        self.assertIn("[string]$ready.executionContext.userSid -cne [string]$runnerContext.userSid", runner_source)
        self.assertIn("launchStatus.brokerProcessId", start_trace)
        self.assertIn("launchStatus.brokerSessionId", start_trace)
        self.assertIn("launchStatus.brokerUserSid", start_trace)
        self.assertIn("launchStatus.runnerProcessId", start_trace)
        self.assertIn("workerLaunchBrokerImageSha256", start_trace)
        self.assertIn("[Int64]$elevatedWorker.SessionId -ne $processSessionId", start_trace)
        self.assertIn("[string]$ready.workerImageSha256 -cne [string]$Diagnostic.workerExecutableSha256", start_trace)
        self.assertIn("workerCommandResponseTimeoutSeconds = 60", runner_source)
        self.assertIn("workerStopResponseTimeoutSeconds = 450", runner_source)
        self.assertIn("Write-P917HeapJsonAtomic ([string]$Diagnostic.workerAuthorizationPath) $authorization", runner_source)
        self.assertIn("launcherProcessId = $launcherProcessId", start_trace)
        self.assertIn("workerAuthorizationSha256", runner_source)
        status_read = start_trace.index("$launchStatus = Read-P914Json ([string]$Diagnostic.workerLaunchStatusPath)")
        status_validation = start_trace.index("if ([string]$launchStatus.schema")
        self.assertLess(status_read, status_validation)
        self.assertLess(status_validation, authorization_write)
        self.assertIn("$process.WaitForExit($TimeoutSeconds * 1000)", worker_source)
        self.assertIn("$process.WaitForExit(5000)", worker_source)
        self.assertIn("Elevated WPR worker process image path or hash differs from its standard-runner launch configuration.", worker_source)
        self.assertIn("workerImageSha256 = $workerImageSha256", worker_source)
        self.assertIn("AddSeconds([int]$config.sessionWatchdogSeconds)", worker_source)
        authorization_wait = worker_source.index("$authorization = Get-Content -LiteralPath ([string]$config.authorizationPath)")
        first_wpr_command = worker_source.index("$initialStatus = Invoke-P917HeapWprCommand")
        self.assertLess(authorization_wait, first_wpr_command)
        self.assertIn("authorizationTimeoutSeconds", worker_source)
        self.assertIn("activationAuthorizationSha256", worker_source)
        self.assertIn("request.command -ceq 'snapshot'", worker_source)
        self.assertIn("request.command -ceq 'stop'", worker_source)
        self.assertIn("accepted = $false", worker_source)
        self.assertIn("accepted = $false", runner_source)
        self.assertNotIn("Invoke-P917HeapWprCommand", runner_source)

    def test_heap_snapshot_runner_and_worker_parse_without_launching_wpr_or_uac(self) -> None:
        if sys.platform != "win32":
            self.skipTest("The P9-17 heap snapshot harness uses Windows PowerShell.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for parser validation.")
        worker_path = RUNNER_PATH.with_name("p9_17_heap_snapshot_elevated_worker.ps1")
        command = r"""
$runnerPath = $env:P917_RUNNER_PATH
$workerPath = $env:P917_WORKER_PATH
$scriptAsts = @{}
foreach ($path in @($runnerPath, $workerPath)) {
    $tokens = $null
    $errors = $null
    $scriptAst = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
    if ($errors.Count -gt 0) { throw "$path`n$($errors | Out-String)" }
    $scriptAsts[$path] = $scriptAst
}
$startFunction = $scriptAsts[$runnerPath].Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Start-P917HeapSnapshotTrace' }, $true)
$brokerBuilder = $scriptAsts[$runnerPath].Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'New-P917HeapSnapshotBrokerBootstrap' }, $true)
$stopBroker = $scriptAsts[$runnerPath].Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Stop-P917HeapSnapshotLaunchBroker' }, $true)
if ($null -eq $brokerBuilder -or $null -eq $stopBroker) { throw 'Bounded PowerShell launch-broker helpers are missing.' }
$standardBrokerCommands = @($startFunction.FindAll({ param($node) $node -is [System.Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Start-Process' }, $true))
if ($standardBrokerCommands.Count -ne 1) { throw "Expected one standard-integrity Start-Process launch inside the WPR wrapper; found $($standardBrokerCommands.Count)." }
$standardBrokerParameters = @($standardBrokerCommands[0].CommandElements | Where-Object { $_ -is [System.Management.Automation.Language.CommandParameterAst] } | ForEach-Object { $_.ParameterName })
foreach ($parameterName in @('FilePath', 'ArgumentList', 'WorkingDirectory', 'PassThru', 'WindowStyle')) {
    if ($standardBrokerParameters -notcontains $parameterName) { throw "Standard-integrity broker launch omits -$parameterName." }
}
if ($standardBrokerParameters -contains 'Verb' -or $standardBrokerCommands[0].Extent.Text -match 'RedirectStandard(Output|Error)|UseShellExecute\s*=\s*\$false') { throw 'Route runner must launch only a non-elevated broker without redirected streams.' }
if ($startFunction.Extent.Text -match 'ProcessStartInfo|RedirectStandard(Output|Error)|CreateNoWindow|UseShellExecute\s*=\s*\$false') { throw 'WPR route wrapper must not use a raw redirected process launch.' }
Invoke-Expression $brokerBuilder.Extent.Text
$sampleBrokerConfig = @{ runId = 'synthetic-run'; nonce = 'synthetic-nonce'; launchStatusPath = 'C:\temp\launch.json'; authorizationPath = 'C:\temp\authorization.json'; authorizationWaitSeconds = 60; workerPowerShell = 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'; workerArguments = '-NoLogo'; workingDirectory = 'C:\temp' }
$sampleBrokerConfigBase64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes((ConvertTo-Json -InputObject $sampleBrokerConfig -Compress)))
$brokerBootstrap = New-P917HeapSnapshotBrokerBootstrap $sampleBrokerConfigBase64
$brokerTokens = $null
$brokerErrors = $null
$brokerAst = [System.Management.Automation.Language.Parser]::ParseInput($brokerBootstrap, [ref]$brokerTokens, [ref]$brokerErrors)
if ($brokerErrors.Count -gt 0) { throw "Generated launch-broker bootstrap`n$($brokerErrors | Out-String)" }
$runAsCommands = @($brokerAst.FindAll({ param($node) $node -is [System.Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Start-Process' }, $true))
if ($runAsCommands.Count -ne 1) { throw "Expected one sole RunAs launch in broker bootstrap; found $($runAsCommands.Count)." }
$runAsParameters = @($runAsCommands[0].CommandElements | Where-Object { $_ -is [System.Management.Automation.Language.CommandParameterAst] } | ForEach-Object { $_.ParameterName })
foreach ($parameterName in @('FilePath', 'ArgumentList', 'WorkingDirectory', 'Verb', 'PassThru', 'WindowStyle')) {
    if ($runAsParameters -notcontains $parameterName) { throw "WPR worker RunAs launch omits -$parameterName." }
}
if ($runAsCommands[0].Extent.Text -notmatch '-Verb\s+RunAs' -or $runAsCommands[0].Extent.Text -notmatch '-WindowStyle\s+Hidden') { throw 'Only the WPR worker launch may use user-approved RunAs.' }
if ($brokerBootstrap -match 'ProcessStartInfo|RedirectStandard(Output|Error)|CreateNoWindow|UseShellExecute\s*=\s*\$false|(?im)^\s*(?:\[IO\.File\]::WriteAllText|Set-Content|Add-Content|Move-Item)[^\r\n]*authorizationPath') { throw 'The PowerShell broker must not use raw redirected process creation or write worker authorization.' }
$runAsExtentIndex = $brokerBootstrap.IndexOf($runAsCommands[0].Extent.Text, [StringComparison]::Ordinal)
$statusMoveIndex = $brokerBootstrap.IndexOf('[IO.File]::Move($launchTempPath, $launchStatusPath)', [StringComparison]::Ordinal)
if ($runAsExtentIndex -lt 0 -or $statusMoveIndex -le $runAsExtentIndex) { throw 'Broker must publish a worker PID status only after the user-approved RunAs request returns.' }
if ($stopBroker.Extent.Text -notmatch 'Stop-Process\s+-Id' -or $stopBroker.Extent.Text -notmatch 'WaitForExit\(\$TimeoutMilliseconds\)') { throw 'Broker timeout helper must terminate and wait for the broker process.' }
$bootstrapAssignment = $startFunction.Find({ param($node) $node -is [System.Management.Automation.Language.AssignmentStatementAst] -and $node.Left -is [System.Management.Automation.Language.VariableExpressionAst] -and $node.Left.VariablePath.UserPath -ceq 'workerBootstrap' }, $true)
if ($null -eq $bootstrapAssignment) { throw 'Elevated WPR worker bootstrap is missing.' }
$workerPathLiteral = "'C:\temp\worker.ps1'"
$configPathLiteral = "'C:\temp\worker-config.json'"
Invoke-Expression ('$workerBootstrap = ' + $bootstrapAssignment.Right.Extent.Text)
$bootstrapTokens = $null
$bootstrapErrors = $null
$null = [System.Management.Automation.Language.Parser]::ParseInput($workerBootstrap, [ref]$bootstrapTokens, [ref]$bootstrapErrors)
if ($bootstrapErrors.Count -gt 0) { throw "Generated worker bootstrap`n$($bootstrapErrors | Out-String)" }
ConvertTo-Json -InputObject @{ runnerParsed = $true; workerParsed = $true; brokerParsed = $true } -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        environment["P917_WORKER_PATH"] = str(worker_path.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env=environment,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            json.loads(result.stdout.strip().splitlines()[-1]),
            {"runnerParsed": True, "workerParsed": True, "brokerParsed": True},
        )

    def test_shared_lifecycle_helper_is_pinned_as_a_harness_dependency(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        shared_helper = "scripts/phase9/p9_14_packaged_lifecycle_common.ps1"
        self.assertEqual(
            validator.P917_HARNESS_SOURCES,
            validator.P917_OWNED_SOURCES + (shared_helper,),
        )
        self.assertIn(
            "$HarnessSources = @($OwnedSources + 'scripts/phase9/p9_14_packaged_lifecycle_common.ps1')",
            runner_source,
        )
        self.assertIn(
            "$harnessRows = @($CurrentSource.files | Where-Object { $HarnessSources -contains [string]$_.path })",
            runner_source,
        )
        self.assertIn("includedRoots = $HarnessSources", runner_source)

        current_rows = validator._scan_source_inventory(REPOSITORY, [shared_helper])
        self.assertEqual(len(current_rows), 1)
        row = current_rows[0]
        helper_path = REPOSITORY / shared_helper
        self.assertEqual(row["path"], shared_helper)
        self.assertEqual(row["sizeBytes"], helper_path.stat().st_size)
        self.assertEqual(row["sha256"], hashlib.sha256(helper_path.read_bytes()).hexdigest())

    def test_natural_close_runner_records_a_finalized_action_after_exit_evidence(self) -> None:
        common_source = (
            REPOSITORY / "scripts/phase9/p9_14_packaged_lifecycle_common.ps1"
        ).read_text(encoding="utf-8")
        close_source = common_source.split(
            "function Invoke-P914NaturalClose(", 1
        )[1].split("\nfunction ", 1)[0]

        close_call = close_source.index("$pattern.Close()")
        completion = close_source.index("$closeAction.completedAtUtc = $closeAction.closeReturnedAtUtc")
        process_exit = close_source.index("$exit = Wait-P914ProcessExit")
        finalized_write = close_source.index("Add-P914JsonLine", process_exit)
        self.assertLess(close_call, completion)
        self.assertLess(completion, process_exit)
        self.assertLess(process_exit, finalized_write)
        self.assertNotIn(
            "Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $closeAction",
            close_source[:close_call],
        )
        self.assertIn("Add-P914JsonLine (Join-Path $EvidenceDirectory 'uia-actions.jsonl') $closeAction", close_source)

    def test_section_roster_control_filter_uses_powershell_51_compatible_string_api(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        section_checkpoint = runner_source.split(
            "function Get-P917SectionRosterAbsenceCheckpoint(", 1
        )[1].split("\nfunction ", 1)[0]

        self.assertIn(
            "$automationId.IndexOf('.selectedClassRosterTab.', [StringComparison]::Ordinal) -ge 0",
            section_checkpoint,
        )
        self.assertIn(
            "$automationId.IndexOf('.selectedClassRosterScrollArea.', [StringComparison]::Ordinal) -ge 0",
            section_checkpoint,
        )
        self.assertNotIn("$automationId.Contains(", section_checkpoint)

    def test_default_process_heap_diagnostic_is_app_opt_in_and_uses_ordered_app_events(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        memory_source = (REPOSITORY / "src/core/process_memory_snapshot.cpp").read_text(encoding="utf-8")
        lifecycle_source = (REPOSITORY / "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp").read_text(encoding="utf-8")
        roster_source = (REPOSITORY / "src/next/ui/roster_editor_widget.cpp").read_text(encoding="utf-8")
        roster_header = (REPOSITORY / "src/next/ui/roster_editor_widget.h").read_text(encoding="utf-8")
        selected_editor_source = (REPOSITORY / "src/next/ui/selected_class_editor.cpp").read_text(encoding="utf-8")
        roster_tests = (REPOSITORY / "tests/next/roster_editor_tests.cpp").read_text(encoding="utf-8")

        self.assertIn("CLASSMNGR_P9_17_DEFAULT_PROCESS_HEAP_DIAGNOSTIC", runner_source)
        self.assertIn("-DefaultProcessHeapDiagnostic", runner_source)
        self.assertIn("[string]$DefaultProcessHeapDiagnosticEvidenceRoot = 'artifacts/p9h'", runner_source)
        self.assertIn(
            "$traceFileName = if ($EnableDefaultProcessHeapDiagnostic) { 'exit.jsonl' } else { 'application-shutdown.jsonl' }",
            runner_source,
        )
        self.assertIn("function Assert-P917ClassicMaxPath", runner_source)
        self.assertIn("return [ordered]@{ label = $Label; path = $fullPath; characterCount = [int]$fullPath.Length; maxPathExclusive = 260 }", runner_source)
        self.assertIn("Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1", runner_source)
        self.assertIn("fixturePreparerArgument$argumentIndex", runner_source)
        self.assertIn("applicationArguments = @($appArguments)", runner_source)
        self.assertIn("applicationEnvironmentPathValues = [ordered]@", runner_source)
        self.assertIn("workspaceOpenedThroughUia = $true", runner_source)
        self.assertIn("$pathEnvironmentEntries = @(([string]$envMap.PATH -split ';')", runner_source)
        self.assertIn('"pathEnvironmentEntry$pathIndex"', runner_source)
        self.assertIn("$context = Start-P917App $evidenceDirectory ([string]$package.applicationPath) $package.executionContext $fixture", runner_source)
        self.assertIn("$fixture $package ([bool]$EnableDefaultProcessHeapDiagnostic)", runner_source)
        for audited_path in (
            "fixtureWorkspaceOpenedThroughUia",
            "fixtureWorkspaceSqliteWal",
            "fixtureWorkspaceSqliteShm",
            "fixtureWorkspaceSqliteJournal",
            "fixturePreparationReceipt",
            "fixtureBeforeSnapshot",
            "fixtureBaseWorkspace",
            "fixtureSeedSql",
            "settingsProfile",
            "settingsAtomicSaveTemporaryFileTemplate",
            "localAppDataLocation",
            "appDataLocation",
            "temporaryDirectoryTemplate",
            "launchPathAuditArtifact",
            "packageRoot",
            "packageResourceRoot",
        ):
            self.assertIn(audited_path, runner_source)
        self.assertIn("$manifest.process.launchPathAudit = @($context.launchPathAudit)", runner_source)
        self.assertIn("$comparisonRoot = if ($DefaultProcessHeapDiagnostic) { $evidenceRoot }", runner_source)
        self.assertIn("('FullThenReduced', 'ReducedThenFull')", runner_source)
        self.assertIn("-EnableDefaultProcessHeapDiagnostic:$DefaultProcessHeapDiagnostic", runner_source)
        self.assertIn("Add-P917DefaultProcessHeapEvidence", runner_source)
        self.assertIn("eventSequenceContiguous = $true", runner_source)
        self.assertIn("sameActionsAcrossArms = $actionSignatures.Count -eq 1", runner_source)
        self.assertIn("sameReadbacksAcrossArms = $readbackSignatures.Count -eq 1", runner_source)

        heap_sampler = memory_source.split("DefaultProcessHeapSnapshot defaultProcessHeapSnapshot()", 1)[1]
        self.assertIn("GetProcessHeap()", heap_sampler)
        self.assertIn('GetProcAddress(kernel, "HeapSummary")', heap_sampler)
        self.assertNotIn("HeapWalk", heap_sampler)
        self.assertIn('QStringLiteral("cbAllocated")', lifecycle_source)
        self.assertIn('QStringLiteral("cbCommitted")', lifecycle_source)
        self.assertIn('QStringLiteral("cbReserved")', lifecycle_source)
        self.assertIn('QStringLiteral("cbMaxReserve")', lifecycle_source)
        self.assertIn('QStringLiteral("callDurationNanoseconds")', lifecycle_source)
        self.assertIn("rosterSaveStageDiagnosticRecord", lifecycle_source)
        self.assertIn("roster-save-stage-checkpoint", lifecycle_source)
        self.assertIn("classmngr-roster-save-stage-diagnostic-v1", lifecycle_source)
        self.assertIn("save_sequence", lifecycle_source)
        self.assertIn("recordRosterSaveStageCheckpoint", lifecycle_source)
        self.assertIn("roles.contains(Qt::EditRole)", roster_source)
        self.assertIn("topLeft == bottomRight", roster_source)
        self.assertIn("roster-widget-destroyed-after-successful-leave", roster_source)
        self.assertIn("roster-widget-after-delete-returned-after-child-destruction", roster_source)
        self.assertIn("postDestructionDiagnosticCheckpoint", roster_header)
        delete_position = selected_editor_source.index("delete editor;", selected_editor_source.index("void SelectedClassEditor::releaseRosterEditor"))
        after_delete_position = selected_editor_source.index("postDestructionCheckpoint();", delete_position)
        self.assertLess(delete_position, after_delete_position)
        self.assertIn("defaultHeapAfterDeleteCheckpointFollowsWidgetAndChildDestruction", roster_tests)
        self.assertIn("classmngr-default-process-heap-checkpoints-v3", runner_source)
        self.assertIn("roster-save-stage-checkpoint", runner_source)
        self.assertIn("classmngr-roster-save-stage-diagnostic-v1", runner_source)
        self.assertIn("saveStageEvents.Count -eq 7", runner_source)
        self.assertIn("saveSequence = [Int64]$measuredGroups[$cycleIndex].saveDiagnosticSequence", runner_source)
        for stage in (
            "before-snapshot",
            "after-snapshot",
            "after-preparation",
            "before-row-source-setup",
            "after-row-source-setup",
            "before-row-wise-repository-write",
            "after-row-wise-repository-write",
        ):
            self.assertIn(stage, runner_source)
        self.assertIn("private_working_set_bytes", runner_source)
        self.assertIn("'expectedBeforeValue', 'details', 'roster', 'student', 'header', 'name'", runner_source)
        self.assertIn("checkpointEventCounts = $stageEventCounts", runner_source)
        self.assertIn("cycleAttribution = 'ordered-app-event-sequence-roster-widget-instance-id-and-save-sequence'", runner_source)
        self.assertIn("afterDeleteEventSequence", runner_source)
        heap_validator = runner_source.split("function Add-P917DefaultProcessHeapEvidence", 1)[1].split("\nfunction ", 1)[0]
        self.assertIn("$expectedCheckpointEvents", heap_validator)
        self.assertIn("$group.destroyedSequence + 1", heap_validator)
        self.assertIn("captured_at", heap_validator)

        long_id = "0" * 52
        worst_case_arm = (
            REPOSITORY
            / "artifacts/p9"
            / long_id
            / "ReducedThenFull"
            / "reduced-observer"
            / long_id
        )
        worst_case_paths = (
            worst_case_arm / "package",
            worst_case_arm / "package/bin/resources",
            worst_case_arm / "package/bin/ClassMngrNext.exe",
            worst_case_arm / "package/bin",
            worst_case_arm / "logs/process-stdout.log",
            worst_case_arm / "logs/process-stderr.log",
            worst_case_arm / "runtime/diagnostics/exit.jsonl",
            worst_case_arm / "runtime/diagnostics/launch-path-audit.json",
            worst_case_arm,
            worst_case_arm / "runtime",
            worst_case_arm / "logs",
            worst_case_arm / "runtime/settings",
            worst_case_arm / "runtime/settings/PaperCloud/ClassMngr.ini",
            worst_case_arm / "runtime/settings/PaperCloud/ClassMngr.ini.XXXXXX",
            worst_case_arm / "runtime/local-app-data",
            worst_case_arm / "runtime/local-app-data/PaperCloud/ClassMngrNext",
            worst_case_arm / "runtime/app-data",
            worst_case_arm / "runtime/app-data/PaperCloud/ClassMngrNext",
            worst_case_arm / "runtime/temp",
            worst_case_arm / "runtime/temp/qt_temp-XXXXXX",
            worst_case_arm / "workspace/large-roster.tps",
            worst_case_arm / "workspace/large-roster.tps-wal",
            worst_case_arm / "workspace/large-roster.tps-shm",
            worst_case_arm / "workspace/large-roster.tps-journal",
            worst_case_arm / "workspace/preparation-receipt.json",
            worst_case_arm / "workspace/before-snapshot.json",
            BASE,
            SEED,
        )
        self.assertTrue(all(len(str(path)) < 260 for path in worst_case_paths))

    def test_build_helper_and_observer_query_and_enforce_the_actual_standard_token_context(self) -> None:
        helper_path = REPOSITORY / validator.P917_BUILD_HELPER_RELATIVE
        helper_source = helper_path.read_text(encoding="utf-8")
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        validator_source = VALIDATOR_PATH.read_text(encoding="utf-8")
        self.assertIn("[string]$BuildDirectory = ''", helper_source)
        self.assertIn("$standardBuildDirectory", helper_source)
        self.assertIn("build/p9-17-save-stage-diagnostic-release", helper_source)
        self.assertIn("$P917SaveStageDiagnosticBuildDirectory", runner_source)
        self.assertIn("if ($DefaultProcessHeapDiagnostic)", runner_source)
        self.assertIn("P9-17 acceptance must use the standard Windows x64 Release build directory", runner_source)
        self.assertIn('build/windows-x64-release', validator_source)
        for source in (helper_source, runner_source):
            self.assertIn("GetTokenInformation", source)
            self.assertIn("TokenIntegrityLevelInformation = 25", source)
            self.assertIn("TokenElevationInformation = 20", source)
            self.assertIn("WTSGetActiveConsoleSessionId", source)
            self.assertIn("Environment.UserInteractive", source)
            self.assertIn("WindowsBuiltInRole.Administrator", source)
            self.assertIn("ProcessSessionId", source)
        self.assertNotIn("FELT-DESKTOP\\wfelt", helper_source)
        self.assertIn("classmngr-phase9-p9-17-exact-release-build-v3", helper_source)
        self.assertIn("classmngr-phase9-p9-17-exact-release-build-v3", runner_source)
        self.assertIn("$process.SessionId", runner_source)
        self.assertIn("measuredProcessSessionId", runner_source)
        self.assertIn("function Test-P917JsonInteger", runner_source)
        self.assertIn("Test-P917JsonInteger -Value $receipt.build.exitCode", runner_source)
        self.assertIn("Test-P917JsonInteger -Value $receipt.sourceFileCount", runner_source)
        self.assertEqual(validator.P917_BUILD_RECEIPT_SCHEMA, "classmngr-phase9-p9-17-exact-release-build-v3")
        self.assertIn("build_helper_path == expected_helper_path", validator_source)
        self.assertIn('sha256_file(build_helper_path) == str(receipt.get("buildHelperSha256", ""))', validator_source)
        self.assertIn("allowed_root in receipt_path.parents", validator_source)

    def test_release_build_toolchain_contract_rejects_v145_and_ambiguous_receipts(self) -> None:
        helper_source = (REPOSITORY / validator.P917_BUILD_HELPER_RELATIVE).read_text(encoding="utf-8")
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        validator_source = VALIDATOR_PATH.read_text(encoding="utf-8")
        expected_cache = {
            "CMAKE_GENERATOR": validator.P917_RELEASE_GENERATOR,
            "CMAKE_GENERATOR_PLATFORM": validator.P917_RELEASE_PLATFORM,
            "CMAKE_GENERATOR_TOOLSET": validator.P917_RELEASE_TOOLSET,
            "CMAKE_GENERATOR_INSTANCE": validator.P917_RELEASE_GENERATOR_INSTANCE,
        }
        expected_compiler = {
            "path": validator.P917_RELEASE_COMPILER_PATH,
            "fileVersion": validator.P917_RELEASE_COMPILER_VERSION,
            "productVersion": "14.44.35230.0",
            "sha256": "a" * 64,
            "toolsetVersion": validator.P917_RELEASE_TOOLSET_VERSION,
            "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
            "windowsSdkIncludeRoot": validator.P917_RELEASE_WINDOWS_SDK_INCLUDE_ROOT,
        }
        valid_receipt = {
            "schema": validator.P917_BUILD_RECEIPT_SCHEMA,
            "generator": validator.P917_RELEASE_GENERATOR,
            "generatorPlatform": validator.P917_RELEASE_PLATFORM,
            "generatorToolset": validator.P917_RELEASE_TOOLSET,
            "generatorInstance": validator.P917_RELEASE_GENERATOR_INSTANCE,
            "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
            "compiler": expected_compiler,
            "generatedProjectToolchain": {
                "platformToolset": validator.P917_RELEASE_TOOLSET,
                "windowsSdkVersion": validator.P917_RELEASE_WINDOWS_SDK_VERSION,
                "projectFileCount": 12,
            },
            "build": {
                "actualCompilerPath": validator.P917_RELEASE_COMPILER_PATH,
                "compilerObservedInLog": True,
            },
        }
        accepted = validator.validate_exact_release_build_toolchain(valid_receipt, expected_cache)
        self.assertEqual(accepted["toolset"], "v143")
        self.assertEqual(accepted["compilerFileVersion"], "19.44.35230.0")

        invalid_receipts = []
        generic_generator = json.loads(json.dumps(valid_receipt))
        generic_generator["generator"] = "Visual Studio"
        invalid_receipts.append(("generic generator", generic_generator, expected_cache))
        vs18_v145 = json.loads(json.dumps(valid_receipt))
        vs18_v145.update({
            "generator": "Visual Studio 18 2026",
            "generatorToolset": "v145",
            "generatorInstance": r"C:\Program Files\Microsoft Visual Studio\18\Community",
        })
        v145_cache = {
            **expected_cache,
            "CMAKE_GENERATOR": "Visual Studio 18 2026",
            "CMAKE_GENERATOR_TOOLSET": "v145",
            "CMAKE_GENERATOR_INSTANCE": r"C:\Program Files\Microsoft Visual Studio\18\Community",
        }
        invalid_receipts.append(("VS18/v145", vs18_v145, v145_cache))
        missing_toolset = json.loads(json.dumps(valid_receipt))
        del missing_toolset["generatorToolset"]
        invalid_receipts.append(("missing receipt toolset", missing_toolset, expected_cache))
        old_schema = json.loads(json.dumps(valid_receipt))
        old_schema["schema"] = "classmngr-phase9-p9-17-exact-release-build-v2"
        invalid_receipts.append(("old schema", old_schema, expected_cache))
        missing_compiler = json.loads(json.dumps(valid_receipt))
        del missing_compiler["compiler"]
        invalid_receipts.append(("missing compiler", missing_compiler, expected_cache))
        malformed_compiler_version = json.loads(json.dumps(valid_receipt))
        malformed_compiler_version["compiler"]["fileVersion"] = "19.51.36231.0"
        invalid_receipts.append(("wrong compiler version", malformed_compiler_version, expected_cache))
        malformed_compiler_hash = json.loads(json.dumps(valid_receipt))
        malformed_compiler_hash["compiler"]["sha256"] = "not-a-sha256"
        invalid_receipts.append(("malformed compiler hash", malformed_compiler_hash, expected_cache))
        malformed_generated_toolset = json.loads(json.dumps(valid_receipt))
        malformed_generated_toolset["generatedProjectToolchain"]["platformToolset"] = "v145"
        invalid_receipts.append(("wrong generated project toolset", malformed_generated_toolset, expected_cache))
        for label, receipt, cache in invalid_receipts:
            with self.subTest(label=label):
                with self.assertRaises(ValueError):
                    validator.validate_exact_release_build_toolchain(receipt, cache)

        missing_cache_toolset = dict(expected_cache)
        del missing_cache_toolset["CMAKE_GENERATOR_TOOLSET"]
        with self.assertRaisesRegex(ValueError, "CMAKE_GENERATOR_TOOLSET"):
            validator.validate_exact_release_build_toolchain(valid_receipt, missing_cache_toolset)

        for source in (helper_source, runner_source, validator_source):
            self.assertIn("Visual Studio 17 2022", source)
            self.assertIn("v143", source)
            self.assertIn("10.0.26100.0", source)
        self.assertIn("19.44.35230.0", helper_source)
        self.assertIn("Microsoft.VCToolsVersion.v143.default.txt", helper_source)
        self.assertIn("compilerObservedInLog", helper_source)
        self.assertIn("Assert-P917V143ReleaseToolchain", runner_source)
        self.assertIn("validate_exact_release_build_toolchain", validator_source)
        self.assertTrue(validator.p917_acceptance_eligible("exact-current-source-release"))
        for supplemental_mode in (
            "fresh-exact-tree-release",
            "verified-accepted-p916-package-fallback",
        ):
            with self.subTest(supplemental_mode=supplemental_mode):
                self.assertFalse(validator.p917_acceptance_eligible(supplemental_mode))

    def test_build_receipt_exit_code_requires_an_exact_json_integer_zero(self) -> None:
        self.assertEqual(validator.validate_exact_build_exit_code({"exitCode": 0}), 0)

        for value in (False, "0", 1, 0.0, None):
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, "exact JSON integer 0"):
                    validator.validate_exact_build_exit_code({"exitCode": value})
        with self.assertRaisesRegex(ValueError, "exact JSON integer 0"):
            validator.validate_exact_build_exit_code({})
        with self.assertRaisesRegex(ValueError, "build result is missing"):
            validator.validate_exact_build_exit_code(None)

    def test_receipt_source_file_count_requires_exact_verified_inventory_row_count(self) -> None:
        build_source = {"fileCount": 2, "files": [{"path": "a"}, {"path": "b"}]}
        self.assertEqual(
            validator.validate_receipt_source_file_count({"sourceFileCount": 2}, build_source), 2
        )

        for value in (False, "2", 2.0, None, 1, 3):
            with self.subTest(sourceFileCount=value):
                with self.assertRaisesRegex(ValueError, "exact integer build inventory row count"):
                    validator.validate_receipt_source_file_count(
                        {"sourceFileCount": value}, build_source
                    )
        with self.assertRaisesRegex(ValueError, "exact integer build inventory row count"):
            validator.validate_receipt_source_file_count({}, build_source)

        for value in (False, "2", 2.0, None, 1, 3):
            with self.subTest(inventoryFileCount=value):
                with self.assertRaisesRegex(ValueError, "exact integer row count"):
                    validator.validate_receipt_source_file_count(
                        {"sourceFileCount": 2}, {"fileCount": value, "files": build_source["files"]}
                    )

    def test_execution_context_validator_rejects_elevation_wrong_integrity_and_non_console_sessions(self) -> None:
        valid = {
            "identity": "Felt-Desktop\\CodexSandboxOffline",
            "userSid": "S-1-5-21-1-2-3-1003",
            "isElevated": False,
            "isAdministratorRole": False,
            "integrityLevelRid": 8192,
            "integrityLevel": "Medium",
            "userInteractive": True,
            "processSessionId": 1,
            "activeConsoleSessionId": 1,
            "observedAtUtc": "2026-11-22T03:00:00.0000000+00:00",
        }
        self.assertEqual(validator.validate_windows_execution_context(valid, "test"), valid)

        mutations = [
            ("isElevated", True, "was elevated"),
            ("isAdministratorRole", True, "Administrator role"),
            ("integrityLevelRid", 12288, "Medium integrity RID"),
            ("integrityLevelRid", 0, "Medium integrity RID"),
            ("integrityLevel", "High", "not Medium integrity"),
            ("integrityLevel", "Unknown", "not Medium integrity"),
            ("userInteractive", False, "not UserInteractive"),
            ("activeConsoleSessionId", 2, "active local console"),
            ("processSessionId", 0, "positive observed integer"),
            ("processSessionId", -1, "positive observed integer"),
            ("processSessionId", 4294967295, "active local console"),
            ("activeConsoleSessionId", 0, "positive observed integer"),
            ("activeConsoleSessionId", -1, "positive observed integer"),
            ("activeConsoleSessionId", 4294967295, "active local console"),
            ("isElevated", 0, "observed JSON Boolean"),
        ]
        for field, value, message in mutations:
            with self.subTest(field=field, value=value):
                invalid = dict(valid)
                invalid[field] = value
                with self.assertRaisesRegex(ValueError, message):
                    validator.validate_windows_execution_context(invalid, "test")

        missing_fields = (
            ("identity", "observed Windows identity"),
            ("userSid", "Windows user SID"),
            ("isElevated", "observed JSON Boolean"),
            ("isAdministratorRole", "observed JSON Boolean"),
            ("integrityLevelRid", "Medium integrity RID"),
            ("integrityLevel", "not Medium integrity"),
            ("userInteractive", "observed JSON Boolean"),
            ("processSessionId", "positive observed integer"),
            ("activeConsoleSessionId", "positive observed integer"),
            ("observedAtUtc", "observation timestamp"),
        )
        for field, message in missing_fields:
            for mutation in ("missing", None, 7):
                with self.subTest(field=field, mutation=mutation):
                    invalid = dict(valid)
                    if mutation == "missing":
                        invalid.pop(field)
                    else:
                        invalid[field] = "wrong-session-id" if field in ("processSessionId", "activeConsoleSessionId") and mutation == 7 else mutation
                    with self.assertRaisesRegex(ValueError, message):
                        validator.validate_windows_execution_context(invalid, "test")

        for bad_sid_value in ("not-a-windows-sid", "S-1-5", "S-1-5-x-1003", "S-1-5-4294967296", "S-1-5-01"):
            with self.subTest(userSid=bad_sid_value):
                bad_sid = dict(valid, userSid=bad_sid_value)
                with self.assertRaisesRegex(ValueError, "structurally valid Windows user SID"):
                    validator.validate_windows_execution_context(bad_sid, "test")
        bad_timestamp = dict(valid, observedAtUtc="not-ISO-8601")
        with self.assertRaisesRegex(ValueError, "timezone-aware ISO-8601"):
            validator.validate_windows_execution_context(bad_timestamp, "test")

    def test_build_receipt_requires_stable_start_and_completion_token_observations(self) -> None:
        context = {
            "identity": "Felt-Desktop\\CodexSandboxOffline",
            "userSid": "S-1-5-21-1-2-3-1003",
            "isElevated": False,
            "isAdministratorRole": False,
            "integrityLevelRid": 8192,
            "integrityLevel": "Medium",
            "userInteractive": True,
            "processSessionId": 1,
            "activeConsoleSessionId": 1,
            "observedAtUtc": "2026-11-22T03:00:00.0000000+00:00",
        }
        receipt_context = {
            "start": dict(context),
            "completion": dict(context),
            "sameIdentityAndConsoleSession": True,
        }
        self.assertEqual(validator.validate_build_execution_context(receipt_context), receipt_context)

        changed = {
            "start": {**context, "processSessionId": 2, "activeConsoleSessionId": 2},
            "completion": dict(context),
            "sameIdentityAndConsoleSession": True,
        }
        with self.assertRaisesRegex(ValueError, "changed between its start and completion"):
            validator.validate_build_execution_context(changed)

        unverified = {**receipt_context, "sameIdentityAndConsoleSession": False}
        with self.assertRaisesRegex(ValueError, "does not verify stable build identity"):
            validator.validate_build_execution_context(unverified)

        for field, value in (
            ("userSid", "S-1-5-21-9-9-9-9"),
            ("session", 2),
        ):
            with self.subTest(field=field):
                completion = dict(context)
                if field == "userSid":
                    completion[field] = value
                else:
                    completion["processSessionId"] = value
                    completion["activeConsoleSessionId"] = value
                mismatch = {
                    "start": dict(context),
                    "completion": completion,
                    "sameIdentityAndConsoleSession": True,
                }
                with self.assertRaisesRegex(ValueError, "changed between its start and completion"):
                    validator.validate_build_execution_context(mismatch)

    def test_measured_app_session_must_match_source_build_observer_and_active_console(self) -> None:
        context = {
            "identity": "Felt-Desktop\\CodexSandboxOffline",
            "userSid": "S-1-5-21-1-2-3-1003",
            "isElevated": False,
            "isAdministratorRole": False,
            "integrityLevelRid": 8192,
            "integrityLevel": "Medium",
            "userInteractive": True,
            "processSessionId": 1,
            "activeConsoleSessionId": 1,
            "observedAtUtc": "2026-11-22T03:00:00.1234567+00:00",
        }
        build = {"start": dict(context), "completion": dict(context), "sameIdentityAndConsoleSession": True}
        app_context = {
            "buildIdentity": context["identity"],
            "buildUserSid": context["userSid"],
            "observerIdentity": context["identity"],
            "observerUserSid": context["userSid"],
            "buildProcessSessionId": 1,
            "observerProcessSessionId": 1,
            "measuredProcessSessionId": 1,
            "activeConsoleSessionId": 1,
            "sharedSessionVerified": True,
        }
        package = {"executionContext": {"build": build, "observer": dict(context)}}
        process = {"sessionId": 1, "executionContext": app_context}

        result = validator.validate_package_execution_context(package, process, build)
        self.assertTrue(result["passed"])
        self.assertEqual(result["measuredAppSessionId"], 1)

        offset_build = {
            "start": dict(context, observedAtUtc="2026-11-22T12:00:00.1234567+09:00"),
            "completion": dict(context, observedAtUtc="2026-11-22T12:00:00.1234567+09:00"),
            "sameIdentityAndConsoleSession": True,
        }
        self.assertTrue(validator.build_execution_contexts_match(offset_build, build))
        offset_package = {"executionContext": {"build": offset_build, "observer": dict(context)}}
        self.assertTrue(validator.validate_package_execution_context(offset_package, process, build)["passed"])

        one_tick_later = {
            "start": dict(context, observedAtUtc="2026-11-22T12:00:00.1234568+09:00"),
            "completion": dict(context, observedAtUtc="2026-11-22T12:00:00.1234567+09:00"),
            "sameIdentityAndConsoleSession": True,
        }
        self.assertFalse(validator.build_execution_contexts_match(one_tick_later, build))
        tick_divergent_package = {
            "executionContext": {"build": one_tick_later, "observer": dict(context)}
        }
        with self.assertRaisesRegex(ValueError, "differs from the source-bound receipt"):
            validator.validate_package_execution_context(tick_divergent_package, process, build)

        one_second_later = {
            "start": dict(context, observedAtUtc="2026-11-22T12:00:01.1234567+09:00"),
            "completion": dict(context, observedAtUtc="2026-11-22T12:00:00.1234567+09:00"),
            "sameIdentityAndConsoleSession": True,
        }
        self.assertFalse(validator.build_execution_contexts_match(one_second_later, build))
        divergent_package = {"executionContext": {"build": one_second_later, "observer": dict(context)}}
        with self.assertRaisesRegex(ValueError, "differs from the source-bound receipt"):
            validator.validate_package_execution_context(divergent_package, process, build)

        for invalid_timestamp in (
            "not-ISO-8601",
            "2026-11-22T03:00:00.1234567",
            "2026-11-22T03:00:00.12345678+00:00",
        ):
            invalid_build = {
                "start": dict(context, observedAtUtc=invalid_timestamp),
                "completion": dict(context),
                "sameIdentityAndConsoleSession": True,
            }
            with self.subTest(invalid_timestamp=invalid_timestamp), self.assertRaises(ValueError):
                validator.build_execution_contexts_match(invalid_build, build)

        with self.assertRaisesRegex(ValueError, "differs from the build session"):
            validator.validate_package_execution_context(
                package, {"sessionId": 2, "executionContext": dict(app_context)}, build
            )
        with self.assertRaisesRegex(ValueError, "observed Windows session ID"):
            validator.validate_package_execution_context(
                package, {"sessionId": True, "executionContext": dict(app_context)}, build
            )

        mismatched_app_context = dict(app_context)
        mismatched_app_context["measuredProcessSessionId"] = 2
        with self.assertRaisesRegex(ValueError, "differs from the app context session"):
            validator.validate_package_execution_context(
                package, {"sessionId": 1, "executionContext": mismatched_app_context}, build
            )

        observer_session_mismatch = {**context, "processSessionId": 2, "activeConsoleSessionId": 2}
        observer_mismatch_package = {"executionContext": {"build": build, "observer": observer_session_mismatch}}
        with self.assertRaisesRegex(ValueError, "Build and observer are not in the same active console session"):
            validator.validate_package_execution_context(observer_mismatch_package, process, build)

        mismatched_build_session_record = dict(app_context, buildProcessSessionId=2)
        with self.assertRaisesRegex(ValueError, "session record differs from the build or observer session IDs"):
            validator.validate_package_execution_context(
                package, {"sessionId": 1, "executionContext": mismatched_build_session_record}, build
            )

    def test_app_image_readiness_uses_bounded_poll_and_fails_closed(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        start = runner_source.index("function Start-P917App(")
        end = runner_source.index("\nfunction Get-P917DialogRoot", start)
        start_app = runner_source[start:end]

        self.assertIn("$moduleReadinessTimeoutSeconds = 10", start_app)
        self.assertIn("[DateTime]::UtcNow.AddSeconds($moduleReadinessTimeoutSeconds)", start_app)
        self.assertIn("while ($null -eq $observedPath -and [DateTime]::UtcNow -lt $moduleReadinessDeadline)", start_app)
        self.assertIn("$process.Refresh()", start_app)
        self.assertIn("$process.HasExited", start_app)
        self.assertIn("$module.PSObject.Properties['FileName']", start_app)
        self.assertIn("NativeErrorCode -eq 5", start_app)
        self.assertIn("Access denied while reading the launched P9-17 process main module", start_app)
        self.assertIn("Timed out after $moduleReadinessTimeoutSeconds seconds", start_app)
        self.assertIn("Live P9-17 process image path differs from the package", start_app)
        self.assertIn("Live P9-17 process image hash differs from the package", start_app)
        self.assertNotIn("$process.MainModule.FileName", start_app)

    def test_pre_context_startup_failure_cleans_exact_process_and_records_evidence(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        cleanup_start = runner_source.index("function Stop-P917StartedAppAfterContextFailure(")
        start_app_at = runner_source.index("\nfunction Start-P917App(", cleanup_start)
        cleanup_helper = runner_source[cleanup_start:start_app_at]
        start_app_end = runner_source.index("\nfunction Get-P917DialogRoot", start_app_at)
        start_app = runner_source[start_app_at:start_app_end]
        lifecycle_start = runner_source.index("function Invoke-P917PackagedLifecycle(")
        lifecycle_end = runner_source.index("\nfunction Invoke-P917UiaObserverComparison", lifecycle_start)
        lifecycle = runner_source[lifecycle_start:lifecycle_end]

        self.assertIn("$Process.CloseMainWindow()", cleanup_helper)
        self.assertIn("$gracefulCloseTimeoutSeconds = 5", cleanup_helper)
        self.assertIn("$Process.WaitForExit(100)", cleanup_helper)
        self.assertIn("$Process.Kill()", cleanup_helper)
        self.assertIn("$forceProcessId -eq $StartedProcessId", cleanup_helper)
        self.assertIn("$forceStartTimeUtc.Ticks -eq $verifiedStartTimeUtc.Ticks", cleanup_helper)
        self.assertIn("forceStopIdentityVerified = $true", cleanup_helper)
        self.assertIn("Process.Kill() on that same retained process instance", cleanup_helper)
        self.assertIn("startTimeCheckAvailable = $startTimeCheckAvailable", cleanup_helper)
        self.assertIn("pidMatchedRetainedProcessInstance = $true", cleanup_helper)
        self.assertIn("processWasStillAliveImmediatelyBeforeKill = $true", cleanup_helper)

        self.assertIn("Stop-P917StartedAppAfterContextFailure -Process $process", start_app)
        self.assertIn("try { $process = Start-Process", start_app)
        self.assertIn("pre-context-launch-cleanup.json", start_app)
        self.assertIn("originalFailureMessage", start_app)
        self.assertIn("pre-context cleanup $cleanupSummary", start_app)
        self.assertIn("preContextLaunchCleanup = Read-P914Json $preContextCleanupPath", lifecycle)

    def test_open_profile_click_reacquires_a_bounded_stable_physical_owner_pair(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        sample_start = runner_source.index("function Get-P917OpenProfileGeometrySample(")
        wait_start = runner_source.index("function Wait-P917StableOpenProfileTarget(", sample_start)
        click_start = runner_source.index("function Invoke-P917StableOpenProfileClick(", wait_start)
        child_start = runner_source.index("function Find-P917ChildNamed(", click_start)
        sample = runner_source[sample_start:wait_start]
        wait = runner_source[wait_start:click_start]
        click = runner_source[click_start:child_start]
        open_start = runner_source.index("function Open-P917Workspace(")
        open_end = runner_source.index("\nfunction ", open_start + 10)
        open_workspace = runner_source[open_start:open_end]

        self.assertIn("$maximumGeometryReadAttempts = 8", wait)
        self.assertIn("$consecutiveStableReadsRequired = 2", wait)
        self.assertIn("for ($attempt = 1; $attempt -le $maximumGeometryReadAttempts", wait)
        self.assertIn("Find-P917Suffix $Context 'classMngrNextOpenTeacherProfile' 'main' 'ControlType.MenuItem'", wait)
        self.assertIn("$sample.signature -ceq [string]$previousValidSample.signature", wait)
        self.assertIn("Start-Sleep -Milliseconds 75", wait)

        self.assertIn("$current.NativeWindowHandle", sample)
        self.assertIn("$mappedOwnerHandle", sample)
        self.assertIn("if ($nativeWindowHandle -gt 0)", sample)
        self.assertIn("no measured-root UIA provenance for host correlation", sample)
        self.assertIn("FindVisibleHostAtPhysicalScreenPoint($centerX, $centerY", sample)
        self.assertIn("[P917ClickGeometryNative]::ReadPhysicalBounds", sample)
        self.assertIn("[bool]$geometry.IsVisible", sample)
        self.assertIn("[Int64]$geometry.ProcessId -ne [Int64]$Context.process.Id", sample)
        self.assertIn("$sample.selectedOwner.fullyContainsTarget", sample)
        self.assertIn("PER_MONITOR_AWARE_V2", runner_source)
        self.assertIn("ClickPhysicalScreenPoint", click)
        self.assertIn("$finalSample.signature -cne [string]$StableTarget.geometry.signature", click)
        self.assertIn("Wait-P917StableOpenProfileTarget $Context", open_workspace)
        self.assertIn("Invoke-P917StableOpenProfileClick $Context", open_workspace)
        self.assertNotIn("Invoke-P917BoundedClick $Context $Manifest $EvidenceDirectory 'open-teacher-profile'", open_workspace)

    def test_open_profile_geometry_uses_one_physical_dpi_space_on_mixed_scale_displays(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        context_start = runner_source.index("public static object ReadWithPhysicalCoordinateContext(")
        context_end = runner_source.index("public static P917PhysicalWindowGeometry ReadPhysicalBounds(", context_start)
        coordinate_context = runner_source[context_start:context_end]
        sample_start = runner_source.index("function Get-P917OpenProfileGeometrySample(")
        wait_start = runner_source.index("function Wait-P917StableOpenProfileTarget(", sample_start)
        sample = runner_source[sample_start:wait_start]
        failure_start = runner_source.index("function Save-P917OpenProfileClickFailure(")
        failure_end = runner_source.index("function Wait-P917StableOpenProfileTarget(", failure_start)
        failure = runner_source[failure_start:failure_end]
        click_start = runner_source.index("function Invoke-P917StableOpenProfileClick(")
        click_end = runner_source.index("function Find-P917ChildNamed(", click_start)
        click = runner_source[click_start:click_end]

        # Captured on a 96-DPI secondary monitor while Windows system DPI was 120.
        # The captured System-Aware UIA values differ by the observed 120/96 ratio.
        # Production does not scale coordinates; it reads UIA and HWND geometry in PMv2.
        system_aware_target = (5761, 1921, 616, 41)
        physical_target = (4609, 1537, 493, 33)
        for virtualized, physical in zip(system_aware_target, physical_target):
            self.assertAlmostEqual(virtualized, physical * 120 / 96, delta=0.5)
        owner = (4600, 1441, 5638, 2198)
        self.assertGreaterEqual(physical_target[0], owner[0])
        self.assertGreaterEqual(physical_target[1], owner[1])
        self.assertLessEqual(physical_target[0] + physical_target[2], owner[2])
        self.assertLessEqual(physical_target[1] + physical_target[3], owner[3])

        self.assertIn("SetThreadDpiAwarenessContext(new IntPtr(-4))", coordinate_context)
        self.assertIn("result = operation()", coordinate_context)
        self.assertIn("SetThreadDpiAwarenessContext(priorContext)", coordinate_context)
        self.assertIn("ThrowWithDpiRestoreFailure(operationFailure, restoreFailure)", coordinate_context)
        self.assertIn("ReadWithPhysicalCoordinateContext([System.Func[object]]", sample)
        self.assertIn("$current.BoundingRectangle", sample)
        self.assertIn("boundingRectangleReadContext = 'PER_MONITOR_AWARE_V2'", sample)
        self.assertIn("uiAutomationBoundsReadContext = 'PER_MONITOR_AWARE_V2'", sample)
        self.assertIn("ReadPhysicalBounds([IntPtr]$sample.selectedOwner.windowHandle", sample)
        self.assertIn("if (-not $hasMappedOwner -or $mappedOwnerHandle -le 0)", sample)
        self.assertIn("FindVisibleHostAtPhysicalScreenPoint($centerX, $centerY", sample)
        self.assertIn("measuredRootWindowHandle = $mappedOwnerHandle", sample)
        self.assertIn("visible measured-process WindowFromPoint host", sample)
        self.assertNotIn("else { $mappedOwnerHandle }", sample)
        self.assertIn("ReadWithPhysicalCoordinateContext([System.Func[object]]", failure)
        self.assertIn("$rootCurrent.BoundingRectangle", failure)
        self.assertIn("$current.BoundingRectangle", failure)
        self.assertIn("ReadWithPhysicalCoordinateContext([System.Func[object]]", click)
        self.assertIn("Add-P917Action $Context", click)

        host_start = runner_source.index("public static P917WindowFromPointDiagnostic FindVisibleHostAtPhysicalScreenPoint(")
        host_end = runner_source.index("public static P917PhysicalWindowGeometry ReadPhysicalBounds(", host_start)
        host_helper = runner_source[host_start:host_end]
        self.assertIn("SetThreadDpiAwarenessContext(new IntPtr(-4))", host_helper)
        self.assertIn("WindowFromPoint(new NativePoint { X = screenX, Y = screenY })", host_helper)
        self.assertIn("RequireProcess(host, expectedProcessId)", host_helper)
        self.assertIn("IsWindowVisible(host)", host_helper)
        self.assertIn("GetWindowRect(host, out rectangle)", host_helper)
        self.assertIn("screenX < rectangle.Left", host_helper)
        self.assertIn("ThrowWithDpiRestoreFailure(operationFailure, restoreFailure)", host_helper)

    def test_open_profile_geometry_contains_physical_target_rejects_mismatch_and_preserves_signed_coordinates(self) -> None:
        def contains(target: tuple[int, int, int, int], owner: tuple[int, int, int, int]) -> bool:
            tx, ty, tw, th = target
            ox, oy, ow, oh = owner
            return tx >= ox and ty >= oy and tx + tw <= ox + ow and ty + th <= oy + oh

        # Corrected target and its actual QMenu popup host from the 96-DPI probe.
        target = (4609, 1537, 493, 33)
        popup_host = (4608, 1503, 495, 269)
        self.assertTrue(contains(target, popup_host))
        target_center = (target[0] + target[2] // 2, target[1] + target[3] // 2)
        self.assertEqual(target_center, (4855, 1553))
        self.assertGreaterEqual(target_center[0], popup_host[0])
        self.assertLess(target_center[0], popup_host[0] + popup_host[2])
        self.assertGreaterEqual(target_center[1], popup_host[1])
        self.assertLess(target_center[1], popup_host[1] + popup_host[3])

        # Comparing the old system-aware UIA result to the PMv2 root produces
        # the observed 121-pixel right-edge gap and must remain a refusal.
        virtualized_target = (5761, 1921, 616, 41)
        physical_main_window = (4600, 1441, 1040, 759)
        self.assertEqual(virtualized_target[0] - (physical_main_window[0] + physical_main_window[2]), 121)
        self.assertFalse(contains(virtualized_target, physical_main_window))

        # Screen coordinates can be negative on monitors left of the primary.
        negative_owner = (-1600, 100, 800, 600)
        self.assertTrue(contains((-1500, 120, 200, 30), negative_owner))
        self.assertFalse(contains((-700, 120, 200, 30), negative_owner))
        popup_target_pid = 39468
        popup_host_pid = 39468
        popup_host_visible = True
        self.assertEqual(popup_target_pid, popup_host_pid)
        self.assertTrue(popup_host_visible)

        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        sample_start = runner_source.index("function Get-P917OpenProfileGeometrySample(")
        sample_end = runner_source.index("function Get-P917OpenProfileGeometrySignature(", sample_start)
        sample = runner_source[sample_start:sample_end]
        click_start = runner_source.index("function Invoke-P917StableOpenProfileClick(")
        click_end = runner_source.index("function Find-P917ChildNamed(", click_start)
        click = runner_source[click_start:click_end]
        require_process_start = runner_source.index("private static uint RequireProcess(")
        require_process_end = runner_source.index("private static void ThrowWithDpiRestoreFailure(", require_process_start)
        require_process = runner_source[require_process_start:require_process_end]
        host_start = runner_source.index("public static P917WindowFromPointDiagnostic FindVisibleHostAtPhysicalScreenPoint(")
        host_end = runner_source.index("public static P917PhysicalWindowGeometry ReadPhysicalBounds(", host_start)
        host_helper = runner_source[host_start:host_end]

        self.assertIn("if (window == IntPtr.Zero", require_process)
        self.assertIn("if (-not $hasMappedOwner -or $mappedOwnerHandle -le 0)", sample)
        self.assertIn("if (-not $contains)", sample)
        self.assertIn("no visible same-process owner HWND correlated", sample)
        self.assertIn("$sample.selectedOwner.hostCorrelation.processId = [Int64]$geometry.ProcessId", sample)
        self.assertNotIn("screenX < 0", host_helper)
        self.assertNotIn("screenY < 0", host_helper)
        self.assertLess(click.index("if (-not [bool]$finalSample.valid)"), click.index("ClickPhysicalScreenPoint"))

    @unittest.skipUnless(shutil.which("pwsh") or shutil.which("powershell"), "PowerShell is required to exercise the P9-17 hit-test projection.")
    def test_window_from_point_diagnostic_projects_same_and_foreign_process_results(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        conversion_start = runner_source.index("function Convert-P917WindowFromPointDiagnostic(")
        setter_start = runner_source.index("function Set-P917WindowFromPointDiagnostic(", conversion_start)
        setter_end = runner_source.index("\nfunction Get-P917OpenProfileGeometrySample(", setter_start)
        conversion = runner_source[conversion_start:setter_end]
        sample_start = runner_source.index("function Get-P917OpenProfileGeometrySample(")
        sample_end = runner_source.index("function Get-P917OpenProfileGeometrySignature(", sample_start)
        sample = runner_source[sample_start:sample_end]
        host_start = runner_source.index("public static P917WindowFromPointDiagnostic FindVisibleHostAtPhysicalScreenPoint(")
        host_end = runner_source.index("public static P917PhysicalWindowGeometry ReadPhysicalBounds(", host_start)
        host = runner_source[host_start:host_end]

        for field in (
            "windowHandle = [Int64]$NativeDiagnostic.WindowHandle",
            "expectedProcessId = [Int64]$NativeDiagnostic.ExpectedProcessId",
            "processId = if ([bool]$NativeDiagnostic.ProcessIdAvailable)",
            "ownerProcessMatchesExpected =",
            "isVisible = if ([bool]$NativeDiagnostic.VisibilityAvailable)",
            "physicalBounds = $physicalBounds",
            "screenPoint = [ordered]@{ x = [int]$NativeDiagnostic.ScreenX; y = [int]$NativeDiagnostic.ScreenY }",
        ):
            self.assertIn(field, conversion)
        self.assertIn("Set-P917WindowFromPointDiagnostic $sample.selectedOwner.hostCorrelation $hitTest", sample)
        self.assertIn("Get-P917WindowFromPointDiagnostic $_.Exception", sample)
        self.assertIn("Set-P917WindowFromPointDiagnostic $sample.selectedOwner.hostCorrelation $hitTestFailure", sample)
        self.assertLess(host.index("CaptureWindowFromPointDiagnostic(host"), host.index("RequireProcess(host, expectedProcessId)"))
        self.assertIn("ex.Data[\"P917WindowFromPointDiagnostic\"] = diagnostic", host)
        self.assertIn("if (actualProcessId != expectedProcessId)", runner_source)
        self.assertIn("if (!IsWindowVisible(host))", host)
        self.assertIn("if (screenX < rectangle.Left || screenX >= rectangle.Right || screenY < rectangle.Top || screenY >= rectangle.Bottom)", host)

        same = "[pscustomobject]@{WindowHandle=[long]111;ExpectedProcessId=[long]101;ProcessIdAvailable=$true;ProcessId=[long]101;VisibilityAvailable=$true;IsVisible=$true;PhysicalBoundsAvailable=$true;Left=-20;Top=40;Width=180;Height=90;ScreenX=25;ScreenY=55;ReadContext='PER_MONITOR_AWARE_V2';ProcessIdReadError=$null;VisibilityReadError=$null;PhysicalBoundsReadError=$null}"
        foreign = "[pscustomobject]@{WindowHandle=[long]222;ExpectedProcessId=[long]101;ProcessIdAvailable=$true;ProcessId=[long]202;VisibilityAvailable=$true;IsVisible=$true;PhysicalBoundsAvailable=$true;Left=-20;Top=40;Width=180;Height=90;ScreenX=25;ScreenY=55;ReadContext='PER_MONITOR_AWARE_V2';ProcessIdReadError=$null;VisibilityReadError=$null;PhysicalBoundsReadError=$null}"
        script = "\n".join((
            conversion,
            f"$same = {same}",
            f"$foreign = {foreign}",
            "$sameCorrelation = [ordered]@{windowHandle=$null;processId=$null;isVisible=$null}",
            "$foreignCorrelation = [ordered]@{windowHandle=$null;processId=$null;isVisible=$null}",
            "Set-P917WindowFromPointDiagnostic $sameCorrelation $same",
            "Set-P917WindowFromPointDiagnostic $foreignCorrelation $foreign",
            "$records = @($sameCorrelation, $foreignCorrelation)",
            "$records | ConvertTo-Json -Depth 6 -Compress",
        ))
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        completed = subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        records = json.loads(completed.stdout)
        self.assertEqual(len(records), 2)
        self.assertTrue(records[0]["hitTest"]["ownerProcessMatchesExpected"])
        self.assertEqual(records[0]["windowHandle"], 111)
        self.assertEqual(records[0]["processId"], 101)
        self.assertEqual(records[0]["hitTest"]["physicalBounds"], {"x": -20, "y": 40, "width": 180, "height": 90, "boundsReadContext": "PER_MONITOR_AWARE_V2"})
        self.assertFalse(records[1]["hitTest"]["ownerProcessMatchesExpected"])
        self.assertEqual(records[1]["windowHandle"], 222)
        self.assertEqual(records[1]["processId"], 202)
        self.assertTrue(records[1]["isVisible"])
        self.assertEqual(records[1]["hitTest"]["screenPoint"], {"x": 25, "y": 55})
        self.assertEqual(records[1]["hitTest"]["physicalBounds"]["width"], 180)

    def test_open_profile_click_failure_persists_geometry_and_original_error(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        start = runner_source.index("function Save-P917OpenProfileClickFailure(")
        end = runner_source.index("\nfunction Wait-P917StableOpenProfileTarget(", start)
        persist = runner_source[start:end]
        click_start = runner_source.index("function Invoke-P917StableOpenProfileClick(")
        click_end = runner_source.index("\nfunction Find-P917ChildNamed(", click_start)
        click = runner_source[click_start:click_end]

        for field in (
            "originalFailure = $OriginalFailure",
            "nativeWindowHandle",
            "boundingRectangle",
            "root = $rootRecord",
            "rootRecord.ownerGeometry = [ordered]@{",
            "selectedOwner = $selectedOwner",
            "geometrySamples = $failureSamples.ToArray()",
            "failureTimeScreenshot = $screenshot",
            "Capture-P914Screenshot $Context $Manifest $EvidenceDirectory 'open-teacher-profile-click-failure' $root",
            "diagnosticOnlyReacquisition = $diagnosticOnlyReacquisition",
            "open-teacher-profile-click-geometry-failure.json",
        ):
            self.assertIn(field, persist)
        self.assertIn("$rootCurrent.BoundingRectangle", persist)
        self.assertIn("$current.NativeWindowHandle", persist)
        self.assertIn("$sample.selectedOwner.dpiAwareness", runner_source)
        self.assertIn("try { $null = Save-P917OpenProfileClickFailure", click)
        self.assertIn("$originalFailure = Get-P917PrimaryFailureMessage $_.Exception", click)
        failure_path = click[click.index("$originalFailure = Get-P917PrimaryFailureMessage $_.Exception"):]
        self.assertLess(failure_path.index("Save-P917OpenProfileClickFailure"), failure_path.index("throw"))

    def test_dpi_restore_failure_preserves_primary_error_and_reports_both_branches(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        helper_start = runner_source.index("private static void ThrowWithDpiRestoreFailure(")
        helper_end = runner_source.index("public static long[] ReadDpiAwarenessDetails(", helper_start)
        throw_helper = runner_source[helper_start:helper_end]
        read_start = runner_source.index("public static P917PhysicalWindowGeometry ReadPhysicalBounds(")
        click_start = runner_source.index("public static void ClickPhysicalScreenPoint(", read_start)
        click_end = runner_source.index("\n}\n'@", click_start)
        read_geometry = runner_source[read_start:click_start]
        click_geometry = runner_source[click_start:click_end]

        self.assertIn("operationFailure.Data[\"DpiContextRestoreFailureMessage\"] = restoreFailure.Message", throw_helper)
        self.assertIn("operationFailure.Data[\"P917PrimaryOperationFailureMessage\"] = operationFailure.Message", throw_helper)
        self.assertIn("ExceptionDispatchInfo.Capture(operationFailure).Throw()", throw_helper)
        self.assertIn("restoreFailure.Data[\"DpiContextRestoreFailureMessage\"] = restoreFailure.Message", throw_helper)
        self.assertIn("restoreFailure.Data[\"P917PrimaryOperationFailureMessage\"] = restoreFailure.Message", throw_helper)
        self.assertIn("ExceptionDispatchInfo.Capture(restoreFailure).Throw()", throw_helper)
        for operation in (read_geometry, click_geometry):
            self.assertIn("Exception operationFailure = null", operation)
            self.assertIn("operationFailure = ex", operation)
            self.assertIn("Exception restoreFailure = null", operation)
            self.assertIn("restoreFailure = ex", operation)
            self.assertIn("ThrowWithDpiRestoreFailure(operationFailure, restoreFailure)", operation)
            self.assertNotIn("finally", operation)

        self.assertIn("function Get-P917DpiRestoreFailureMessage(", runner_source)
        self.assertIn("function Get-P917PrimaryFailureMessage(", runner_source)
        self.assertIn("$sample.failure = Get-P917PrimaryFailureMessage $_.Exception", runner_source)
        self.assertIn("$sample.dpiContextRestoreFailure = Get-P917DpiRestoreFailureMessage $_.Exception", runner_source)
        self.assertIn("$_.Exception.Data['P917OpenProfileGeometrySample'] = $sample", runner_source)
        self.assertIn("dpiContextRestoreFailure = $DpiContextRestoreFailure", runner_source)

    def test_p917_sampler_summary_labels_inherited_stability_limit_separately(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("$WorkingSetCeiling = [Int64]471859200", runner_source)
        self.assertIn("workingSetCeilingBytes = $WorkingSetCeiling; workingSetCeilingExclusive = $false", runner_source)
        self.assertIn("inclusive 471859200-byte ceiling is enforced", runner_source)
        self.assertIn("inheritedSamplerStableCheckpointLimitBytesExclusive", runner_source)
        self.assertIn("$metrics.Remove('stableCheckpointLimitBytesExclusive')", runner_source)
        self.assertIn(
            "Inherited P9-14 sampler metadata only; it is independent of P9-17 acceptance memory gates.",
            runner_source,
        )
        self.assertIn(
            "$metrics['p917WorkingSetAcceptance'] = [ordered]@{ targetBytesExclusive = 268435456; ceilingBytesInclusive = 471859200; operationPeakReportedSeparately = $true }",
            runner_source,
        )

    def test_roster_editor_test_delta_is_pinned_to_its_current_file_hash(self) -> None:
        path = REPOSITORY / "tests/next/roster_editor_tests.cpp"
        change = next(
            item for item in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
            if item["path"] == "tests/next/roster_editor_tests.cpp"
        )
        current_sha = hashlib.sha256(path.read_bytes()).hexdigest()
        self.assertEqual(path.stat().st_size, 126850)
        self.assertEqual(current_sha, "b02cb7f7e7fc294643d94287df8a9ce6c146ecbd57a02279b6f22919ac98dc08")
        self.assertEqual(change["afterSizeBytes"], path.stat().st_size)
        self.assertEqual(change["afterSha256"], current_sha)

    def test_uia_observer_diagnostic_modes_are_isolated_from_default_acceptance(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("[switch]$UiaObserverDiagnostic", runner_source)
        self.assertIn("observerModes = @('full-observer', 'reduced-observer')", runner_source)
        self.assertIn("-ObserverMode $mode -DiagnosticOnly", runner_source)
        self.assertIn("[int]$MeasuredCycleCount = 10", runner_source)
        self.assertIn("$baselineCount = 5", runner_source)
        self.assertIn("$cycleCount -ne 10 -or $ObserverMode -cne 'full-observer'", runner_source)
        self.assertIn("$editedReadbacks = New-P917ImmediateSetValueReadbacks", runner_source)
        self.assertIn("$savedReadbacks = Read-P917Targets", runner_source)
        self.assertIn("'cycle-final-reentry-contract'", runner_source)
        self.assertIn("automation-client-metrics.jsonl", runner_source)
        self.assertIn("status = 'diagnostic-only'", runner_source)

    def test_uia_observer_diagnostic_order_is_validated_and_bound_to_arm_manifests(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")

        self.assertIn(
            "[ValidateSet('FullThenReduced', 'ReducedThenFull', '')][string]$UiaObserverDiagnosticOrder = ''",
            runner_source,
        )
        self.assertIn(
            "-UiaObserverDiagnosticOrder can only be used with -UiaObserverDiagnostic.",
            runner_source,
        )
        self.assertIn(
            "function Get-P917UiaObserverOrderPlan([string]$OrderSelection)",
            runner_source,
        )
        self.assertIn("$OrderSelection -ieq 'FullThenReduced'", runner_source)
        self.assertIn("$OrderSelection -ieq 'ReducedThenFull'", runner_source)
        self.assertIn("$UiaObserverDiagnosticOrder = [string]$UiaObserverDiagnosticOrderPlan.selection", runner_source)
        self.assertIn("$armModes = @($UiaObserverDiagnosticOrderPlan.modes)", runner_source)
        self.assertIn(
            "armOrderSelection = $UiaObserverDiagnosticOrder; armExecutionOrder = $armModes",
            runner_source,
        )
        self.assertIn("armPosition = ($armIndex + 1); armOrderSelection = $UiaObserverDiagnosticOrder", runner_source)
        self.assertIn("-ComparisonArmOrderSelection $UiaObserverDiagnosticOrder", runner_source)
        self.assertIn("-ComparisonArmPosition ($armIndex + 1)", runner_source)
        self.assertIn("selectedArmOrderMatches =", runner_source)
        self.assertIn(
            "observerDiagnosticArmOrderSelection = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmOrderSelection }",
            runner_source,
        )
        self.assertIn(
            "observerDiagnosticArmPosition = if ($hasClientGcReferenceArm) { $null } else { $ComparisonArmPosition }",
            runner_source,
        )

    def test_lowercase_full_then_reduced_resolves_to_canonical_full_first_order(self) -> None:
        if sys.platform != "win32":
            self.skipTest("The P9-17 runner is PowerShell for local Windows.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for evaluating the runner's order-plan helper.")

        command = r"""
$path = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
$functionAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Get-P917UiaObserverOrderPlan' }, $true)
if ($null -eq $functionAst) { throw 'Order-plan helper is missing.' }
Invoke-Expression $functionAst.Extent.Text
$plans = @(
    (Get-P917UiaObserverOrderPlan 'fullthenreduced')
    (Get-P917UiaObserverOrderPlan '')
    (Get-P917UiaObserverOrderPlan 'ReducedThenFull')
)
ConvertTo-Json -InputObject $plans -Depth 4 -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env=environment,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        plans = json.loads(result.stdout.strip().splitlines()[-1])
        self.assertEqual(
            plans,
            [
                {"selection": "FullThenReduced", "modes": ["full-observer", "reduced-observer"]},
                {"selection": "FullThenReduced", "modes": ["full-observer", "reduced-observer"]},
                {"selection": "ReducedThenFull", "modes": ["reduced-observer", "full-observer"]},
            ],
        )

    def test_client_gc_reference_diagnostic_is_separate_from_acceptance_and_ordered(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("[switch]$ClientGcReferenceDiagnostic", runner_source)
        self.assertIn("[ValidateSet('NoGcThenGc', 'GcThenNoGc', '')][string]$ClientGcReferenceDiagnosticOrder = ''", runner_source)
        self.assertIn("Client GC reference diagnostic parameters can only be used with -ClientGcReferenceDiagnostic.", runner_source)
        self.assertIn("function Get-P917ClientGcReferenceOrderPlan([string]$OrderSelection)", runner_source)
        self.assertIn("$OrderSelection -ieq 'NoGcThenGc'", runner_source)
        self.assertIn("$OrderSelection -ieq 'GcThenNoGc'", runner_source)
        self.assertIn("function Invoke-P917ClientGcReferenceComparison", runner_source)
        self.assertIn("Test-P917ClientGcReferenceArmMetadata $armManifest $mode $ClientGcReferenceDiagnosticOrder $position", runner_source)
        self.assertIn("samePowerShellClientProcess =", runner_source)
        self.assertIn("sameComparisonGroupId = $left.comparisonGroupId -ceq $comparisonId -and $right.comparisonGroupId -ceq $comparisonId", runner_source)
        self.assertIn("sameUiAutomationActions =", runner_source)
        self.assertIn("bothArmMetadataValid =", runner_source)
        self.assertIn("measuredCycleCount = $armRecord.measuredCycleCount; observerMode = [string]$armRecord.observerMode", runner_source)
        self.assertIn("rosterRows = $armRecord.rosterRows; rosterColumns = $armRecord.rosterColumns", runner_source)
        self.assertIn("$cycleCount -ne 10 -or $ObserverMode -cne 'full-observer'", runner_source)
        self.assertIn("Client GC reference arms require diagnostic-only full-observer mode with exactly five measured cycles.", runner_source)

        if sys.platform != "win32":
            self.skipTest("The P9-17 runner is PowerShell for local Windows.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for evaluating the runner's order-plan helper.")
        command = r"""
$path = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
$functionAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Get-P917ClientGcReferenceOrderPlan' }, $true)
if ($null -eq $functionAst) { throw 'Client GC reference order-plan helper is missing.' }
Invoke-Expression $functionAst.Extent.Text
$plans = @(
    (Get-P917ClientGcReferenceOrderPlan 'NoGcThenGc')
    (Get-P917ClientGcReferenceOrderPlan 'nogcthengc')
    (Get-P917ClientGcReferenceOrderPlan 'GcThenNoGc')
)
$invalidRejected = $false
try { $null = Get-P917ClientGcReferenceOrderPlan 'NoGcOnly' } catch { $invalidRejected = $true }
ConvertTo-Json -InputObject @{ plans = $plans; invalidRejected = $invalidRejected } -Depth 4 -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env=environment,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout.strip().splitlines()[-1])
        self.assertTrue(payload["invalidRejected"])
        self.assertEqual(
            payload["plans"],
            [
                {"selection": "NoGcThenGc", "modes": ["no-gc", "client-gc"]},
                {"selection": "NoGcThenGc", "modes": ["no-gc", "client-gc"]},
                {"selection": "GcThenNoGc", "modes": ["client-gc", "no-gc"]},
            ],
        )

        for parameter in (
            ("-ClientGcReferenceDiagnosticOrder", "NoGcThenGc"),
            ("-ClientGcReferenceDiagnosticEvidenceRoot", "artifacts/phase9/should-not-run"),
        ):
            with self.subTest(parameter=parameter[0]):
                rejected = subprocess.run(
                    [powershell, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(RUNNER_PATH), parameter[0], parameter[1]],
                    capture_output=True,
                    text=True,
                    timeout=20,
                    check=False,
                )
                self.assertNotEqual(rejected.returncode, 0, rejected.stdout + rejected.stderr)
                self.assertIn("can only be used with -ClientGcReferenceDiagnostic", rejected.stderr + rejected.stdout)

    def test_client_gc_reference_metadata_accepts_both_arms_and_rejects_inconsistent_order(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        event_start = runner_source.index("function Invoke-P917ClientGcReferenceEvent(")
        event_end = runner_source.index("\nfunction Get-P917RosterTable", event_start)
        event_helper = runner_source[event_start:event_end]
        self.assertIn("[GC]::Collect()", event_helper)
        self.assertIn("[GC]::WaitForPendingFinalizers()", event_helper)
        self.assertIn("collectionSequence = if ($Mode -ceq 'client-gc') { '[GC]::Collect(); [GC]::WaitForPendingFinalizers(); [GC]::Collect()' } else { $null }", event_helper)
        self.assertIn("collectionTarget = 'PowerShell automation client only'", event_helper)
        self.assertIn("appGcRequested = $false; osWorkingSetTrimRequested = $false; heapCompactRequested = $false", event_helper)
        self.assertIn("uiAutomationReferenceScopesEnded = $true", event_helper)
        self.assertIn("retainedUiAutomationRootsReleased = $true", event_helper)
        self.assertIn("function Test-P917ClientGcReferenceArmMetadata", runner_source)
        self.assertIn("$eventEnd -gt $sampleStart", runner_source)
        self.assertIn("generation2Collections') -lt 2", runner_source)

        if sys.platform != "win32":
            self.skipTest("The P9-17 runner is PowerShell for local Windows.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for exercising client GC metadata checks.")
        command = r"""
$path = $env:P917_RUNNER_PATH
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
foreach ($functionName in @('Get-P917OptionalProperty', 'Test-P917ClientGcReferenceArmMetadata', 'Invoke-P917ClientGcReferenceEvent')) {
    $functionAst = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $functionName }, $true)
    if ($null -eq $functionAst) { throw "Required metadata helper is missing: $functionName" }
    Invoke-Expression $functionAst.Extent.Text
}
function Get-P914Utc { [DateTimeOffset]::UtcNow.ToString('o') }
function New-TestArm([string]$Mode) {
    $cycles = [Collections.Generic.List[object]]::new()
    $events = [Collections.Generic.List[object]]::new()
    $windows = [Collections.Generic.List[object]]::new()
    for ($cycle = 1; $cycle -le 5; $cycle++) {
        $base = [DateTimeOffset]::UtcNow.AddSeconds($cycle * 20)
        $proof = $base.ToString('o')
        $start = $base.AddSeconds(1).ToString('o')
        $collectStart = $base.AddSeconds(2).ToString('o')
        $collectEnd = $base.AddSeconds(3).ToString('o')
        $end = $base.AddSeconds(4).ToString('o')
        $sample = $base.AddSeconds(5).ToString('o')
        $windowId = "return-$cycle"
        $checkpoint = [ordered]@{ freshActiveTreeCaptured = $true; activeUiAutomationRosterTablePresent = $false; activeTreeRosterControlCount = 0; activeTreeCapturedAtUtc = $proof }
        $cycles.Add([ordered]@{ cycleNumber = $cycle; postSuspensionWindowId = $windowId; suspensionCheckpoint = $checkpoint })
        $windows.Add([ordered]@{ windowId = $windowId; startedAtUtc = $sample })
        $forced = $Mode -ceq 'client-gc'
        $afterCount = if ($forced) { 2 } else { 0 }
        $events.Add([ordered]@{
            schema = 'classmngr-phase9-p9-17-client-gc-reference-event-v1'
            cycleNumber = $cycle; mode = $Mode; armOrderSelection = 'NoGcThenGc'; armPosition = 2
            clientProcessId = 1234; appProcessId = 4567; postSuspensionWindowId = $windowId
            freshActiveTreeCaptured = $true; activeTreeCapturedAtUtc = $proof; activeTreeRosterControlCount = 0; activeUiAutomationRosterTablePresent = $false
            uiAutomationReferenceScopesEnded = $true; retainedUiAutomationRootsReleased = $true
            collectionTarget = 'PowerShell automation client only'; appGcRequested = $false; osWorkingSetTrimRequested = $false; heapCompactRequested = $false
            eventStartedAtUtc = $start; eventCompletedAtUtc = $end
            collectionSequence = if ($forced) { '[GC]::Collect(); [GC]::WaitForPendingFinalizers(); [GC]::Collect()' } else { $null }
            collectionStartedAtUtc = if ($forced) { $collectStart } else { $null }
            collectionCompletedAtUtc = if ($forced) { $collectEnd } else { $null }
            collectionDurationMilliseconds = if ($forced) { 100 } else { $null }
            forcedCollectionRequested = $forced; forcedCollectionCallCount = if ($forced) { 2 } else { 0 }
            runtime = [ordered]@{
                frameworkVersion = '4.0.30319.42000'
                maxGeneration = 2
                before = [ordered]@{ generation0Collections = 0; generation1Collections = 0; generation2Collections = 0; managedBytesEstimate = 1000 }
                after = [ordered]@{ generation0Collections = $afterCount; generation1Collections = $afterCount; generation2Collections = $afterCount; managedBytesEstimate = 500 }
                delta = [ordered]@{ generation0Collections = $afterCount; generation1Collections = $afterCount; generation2Collections = $afterCount; managedBytesEstimate = -500 }
            }
        })
    }
    $diagnostic = [ordered]@{
        schema = 'classmngr-phase9-p9-17-client-gc-reference-diagnostic-v1'; diagnosticOnly = $true
        mode = $Mode; armOrderSelection = 'NoGcThenGc'; armPosition = 2; clientProcessId = 1234
        collectionEvents = $events.ToArray(); automationClientMetrics = [ordered]@{ processId = 1234 }
        appGcInvoked = $false; osWorkingSetTrimInvoked = $false; heapCompactInvoked = $false
        collectionTarget = 'PowerShell automation client only'
    }
    return [ordered]@{
        status = 'diagnostic-only'; diagnosticOnly = $true; process = [ordered]@{ processId = 4567 }
        observerDiagnostic = [ordered]@{ mode = 'full-observer' }; clientGcReferenceDiagnostic = $diagnostic
        route = [ordered]@{
            observerMode = 'full-observer'; measuredCycleCount = 5; clientGcReferenceDiagnosticOnly = $true
            clientGcReferenceMode = $Mode; clientGcReferenceOrderSelection = 'NoGcThenGc'; clientGcReferenceArmPosition = 2
            modelCounts = [ordered]@{ rosterRows = 100; rosterColumns = 156; verifiedHeaderCount = 156 }; cycleRecords = $cycles.ToArray()
        }
        memory = [ordered]@{ windows = $windows.ToArray() }
    }
}
$gcArm = New-TestArm 'client-gc'
$noGcArm = New-TestArm 'no-gc'
$gcResult = Test-P917ClientGcReferenceArmMetadata $gcArm 'client-gc' 'NoGcThenGc' 2
$noGcResult = Test-P917ClientGcReferenceArmMetadata $noGcArm 'no-gc' 'NoGcThenGc' 2
$gcArm.clientGcReferenceDiagnostic.collectionEvents[0].armPosition = 1
$invalidResult = Test-P917ClientGcReferenceArmMetadata $gcArm 'client-gc' 'NoGcThenGc' 2
$smokeDiagnostic = [pscustomobject]@{ clientProcessId = [Int64]$PID; armOrderSelection = 'NoGcThenGc'; armPosition = 2; collectionEvents = [Collections.Generic.List[object]]::new() }
$smokeContext = [pscustomobject]@{ mainWindowRoot = $null; dialogRoot = $null; process = [pscustomobject]@{ Id = 98765 } }
$smokeProof = [pscustomobject]@{ freshActiveTreeCaptured = $true; activeUiAutomationRosterTablePresent = $false; activeTreeRosterControlCount = 0; activeTreeCapturedAtUtc = [DateTimeOffset]::UtcNow.ToString('o') }
$noGcEvent = Invoke-P917ClientGcReferenceEvent $smokeContext $smokeDiagnostic $smokeProof 1 'smoke-no-gc' 'no-gc'
$gcEvent = Invoke-P917ClientGcReferenceEvent $smokeContext $smokeDiagnostic $smokeProof 2 'smoke-client-gc' 'client-gc'
$eventSmoke = [ordered]@{
    noGcForced = [bool]$noGcEvent.forcedCollectionRequested
    gcForced = [bool]$gcEvent.forcedCollectionRequested
    gcCallCount = [Int64]$gcEvent.forcedCollectionCallCount
    gcGeneration2Delta = [Int64]$gcEvent.runtime.delta.generation2Collections
    collectionTarget = [string]$gcEvent.collectionTarget
    forbiddenOperationsRequested = [bool]$gcEvent.appGcRequested -or [bool]$gcEvent.osWorkingSetTrimRequested -or [bool]$gcEvent.heapCompactRequested
    timelineValid = ([DateTimeOffset]::Parse($gcEvent.activeTreeCapturedAtUtc) -le [DateTimeOffset]::Parse($gcEvent.eventStartedAtUtc)) -and ([DateTimeOffset]::Parse($gcEvent.collectionStartedAtUtc) -le [DateTimeOffset]::Parse($gcEvent.collectionCompletedAtUtc)) -and ([DateTimeOffset]::Parse($gcEvent.collectionCompletedAtUtc) -le [DateTimeOffset]::Parse($gcEvent.eventCompletedAtUtc))
}
ConvertTo-Json -InputObject @{ gcPassed = $gcResult.passed; noGcPassed = $noGcResult.passed; invalidPassed = $invalidResult.passed; invalidReasons = @($invalidResult.failureReasons); eventSmoke = $eventSmoke } -Depth 6 -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True,
            text=True,
            env=environment,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        payload = json.loads(result.stdout.strip().splitlines()[-1])
        self.assertTrue(payload["gcPassed"], payload)
        self.assertTrue(payload["noGcPassed"], payload)
        self.assertFalse(payload["invalidPassed"], payload)
        self.assertTrue(any("mismatched cycle, arm, order" in reason for reason in payload["invalidReasons"]))
        self.assertFalse(payload["eventSmoke"]["noGcForced"])
        self.assertTrue(payload["eventSmoke"]["gcForced"])
        self.assertEqual(payload["eventSmoke"]["gcCallCount"], 2)
        self.assertGreaterEqual(payload["eventSmoke"]["gcGeneration2Delta"], 2)
        self.assertEqual(payload["eventSmoke"]["collectionTarget"], "PowerShell automation client only")
        self.assertFalse(payload["eventSmoke"]["forbiddenOperationsRequested"])
        self.assertTrue(payload["eventSmoke"]["timelineValid"])

    def test_normal_validator_rejects_uia_observer_diagnostic_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            manifest_path = Path(directory) / "manifest.json"
            manifest_path.write_text(
                json.dumps({
                    "schema": validator.SCHEMA,
                    "taskId": "P9_17_PACKAGED_NEXT_LARGE_ROSTER_EDITING",
                    "status": "completed",
                    "diagnosticOnly": True,
                }),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "UIA observer diagnostic-only manifests cannot be accepted"):
                validator.validate_manifest(manifest_path)

    def test_validate_manifest_reaches_and_enforces_cleanup_gates(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            manifest_path = make_normal_cleanup_gate_manifest(Path(directory))

            def validate_with_memory(memory_summary: dict[str, object]) -> dict[str, object]:
                with mock.patch.multiple(
                    validator,
                    verify_source_inventory=mock.Mock(return_value={"path": "synthetic-source.json"}),
                    verify_exact_current_source_build=mock.Mock(return_value={"executionContext": {"build": {}}}),
                    verify_package_inventory=mock.Mock(return_value={"sha256": "synthetic-package", "fileCount": 1}),
                    _pe_machine=mock.Mock(return_value=0x8664),
                    validate_snapshot_pair=mock.Mock(return_value={"passed": True}),
                    validate_package_execution_context=mock.Mock(return_value={"passed": True}),
                    validate_natural_process_close_action=mock.Mock(return_value={"passed": True}),
                    validate_roster_tab_selection_actions=mock.Mock(return_value={"passed": True}),
                    validate_section_suspension_checkpoint=mock.Mock(return_value={"passed": True}),
                    validate_roster_absence_checkpoint=mock.Mock(return_value={"passed": True}),
                    validate_class_selection_evidence=mock.Mock(return_value={"passed": True}),
                    validate_cycle_uia_contract=mock.Mock(return_value={"passed": True}),
                    validate_final_cycle_uia_contract=mock.Mock(return_value={"passed": True}),
                    validate_memory=mock.Mock(return_value=memory_summary),
                ):
                    return validator.validate_manifest(manifest_path)

            passed_memory = {
                "cleanupGateResults": {
                    "coldBaselineToFirstReturn": {"passed": True, "failureReasons": []},
                    "postPrimingPlateauAndCycleReturns": {"passed": True, "failureReasons": []},
                }
            }
            result = validate_with_memory(passed_memory)
            self.assertEqual(result["status"], "passed")
            self.assertTrue(result["acceptanceEligible"])
            self.assertEqual(result["memory"], passed_memory)

            failed_memory = {
                "cleanupGateResults": {
                    "coldBaselineToFirstReturn": {"passed": True, "failureReasons": []},
                    "postPrimingPlateauAndCycleReturns": {
                        "passed": False,
                        "failureReasons": ["synthetic plateau drift"],
                    },
                }
            }
            with self.assertRaisesRegex(ValueError, "Required memory cleanup gates failed.*synthetic plateau drift"):
                validate_with_memory(failed_memory)

    def test_payload_comparator_accepts_only_the_declared_sparse_cell_payload_difference(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)

            report = validator.compare_p917_payload_diagnostic(filled_path, sparse_path)

            self.assertEqual(report["status"], "diagnostic-only")
            self.assertTrue(report["diagnosticOnly"])
            self.assertFalse(report["accepted"])
            self.assertTrue(all(report["comparisonChecks"].values()), report["comparisonChecks"])
            self.assertEqual(report["fixture"]["customCellRecordCount"], 15000)
            self.assertEqual(report["fixture"]["filledCustomCellValueCount"], 15000)
            self.assertEqual(report["fixture"]["sparseCustomCellValueCount"], 15000)
            self.assertEqual(report["fixture"]["sparseBlankCustomCellValueCount"], 0)
            self.assertEqual(report["fixture"]["sparseCompactSentinelValue"], validator.PAYLOAD_SPARSE_SENTINEL)
            self.assertEqual(report["fixture"]["sparseCompactSentinelValueCount"], 14997)
            memory = report["memoryComparison"]
            self.assertEqual(len(memory["perWindowStageMedianDeltas"]), 36)
            final_reentry = next(
                item for item in memory["perWindowStageMedianDeltas"]
                if item["windowId"] == "cycle-final-reentry"
            )
            self.assertEqual(final_reentry["stage"], "final-reentry")
            self.assertEqual(len(memory["fittedStageSlopes"]), 2)
            self.assertTrue(all(item["sampleCount"] == 10 for item in memory["fittedStageSlopes"]))
            self.assertFalse(any(item["stage"] == "final-reentry" for item in memory["fittedStageSlopes"]))
            self.assertTrue(all(item["stage"] == "cyclePostSuspensionReturn" for item in memory["fittedStageSlopes"]))
            expected_return_ids = [f"cycle-{cycle:02d}-return" for cycle in range(1, 11)]
            self.assertTrue(all(item["windowIds"] == expected_return_ids for item in memory["fittedStageSlopes"]))
            self.assertIn("Neither metric alone establishes", memory["interpretation"])

    def test_roster_record_count_comparator_accepts_only_declared_custom_row_deletion(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_record_count_diagnostic_arm(root, "filled", 4567)
            reduced_path = make_record_count_diagnostic_arm(root, "reduced-rows", 7654)

            report = validator.compare_p917_roster_record_count_diagnostic(filled_path, reduced_path)

            self.assertEqual(report["status"], "diagnostic-only")
            self.assertTrue(report["diagnosticOnly"])
            self.assertFalse(report["accepted"])
            self.assertTrue(all(report["comparisonChecks"].values()), report["comparisonChecks"])
            self.assertEqual(report["fixture"]["filledCustomCellRecordCount"], 15000)
            self.assertEqual(report["fixture"]["reducedRowsCustomCellRecordCount"], 3)
            self.assertEqual(report["fixture"]["removedCustomCellRecordCount"], 14997)
            self.assertEqual(report["fixture"]["filledDatabaseRosterCellCount"], 22325)
            self.assertEqual(report["fixture"]["reducedRowsDatabaseRosterCellCount"], 7328)
            self.assertEqual(
                report["memoryComparison"]["trendWindowIds"],
                [f"cycle-{cycle:02d}-return" for cycle in range(1, 11)],
            )
            self.assertTrue(report["memoryComparison"]["finalReentryIsSeparateStage"])
            self.assertEqual(len(report["memoryComparison"]["perWindowStageMedianDeltas"]), 36)
            self.assertEqual(len(report["memoryComparison"]["fittedStageSlopes"]), 2)
            self.assertTrue(all(row["sampleCount"] == 10 for row in report["memoryComparison"]["fittedStageSlopes"]))
            self.assertIn("do not establish", report["memoryComparison"]["interpretation"])

            cli_result = subprocess.run(
                [
                    sys.executable, str(VALIDATOR_PATH),
                    "--compare-roster-record-count-diagnostic",
                    "--filled-manifest", str(filled_path),
                    "--reduced-rows-manifest", str(reduced_path),
                ],
                capture_output=True, text=True, timeout=60, check=False,
            )
            self.assertEqual(cli_result.returncode, 0, cli_result.stdout + cli_result.stderr)
            cli_report = json.loads(cli_result.stdout.strip().splitlines()[-1])
            self.assertEqual(cli_report["status"], "diagnostic-only")
            self.assertFalse(cli_report["accepted"])

    def test_roster_record_count_comparator_rejects_unexpected_deletion_probe_name_or_schedule_change(self) -> None:
        def mutate_after_snapshot(manifest_path: Path, mutation) -> None:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            snapshot_path = manifest_path.parent / manifest["fixture"]["afterSnapshot"]["relativePath"]
            snapshot = json.loads(snapshot_path.read_text(encoding="utf-8"))
            mutation(snapshot)
            snapshot_path.write_text(json.dumps(snapshot, ensure_ascii=False), encoding="utf-8")
            manifest["fixture"]["afterSnapshot"]["sha256"] = hashlib.sha256(snapshot_path.read_bytes()).hexdigest()
            manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
            _refresh_arm_evidence_inventory(manifest_path)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_record_count_diagnostic_arm(root, "filled", 4567)
            reduced_path = make_record_count_diagnostic_arm(root, "reduced-rows", 7654)

            mutations = {}
            def delete_unexpected_roster_row(snapshot: dict[str, object]) -> None:
                table = snapshot["workspaceTables"]["roster_data"]
                columns = table["columns"]
                key_indexes = [columns.index(name) for name in ("class_id", "row_index", "col_index")]
                table["rows"] = [
                    row for row in table["rows"]
                    if tuple(row[index] for index in key_indexes) != (1, 0, 6)
                ]
                table["rowCount"] -= 1
            mutations["unexpected row deletion"] = delete_unexpected_roster_row

            def change_probe(snapshot: dict[str, object]) -> None:
                table = snapshot["workspaceTables"]["roster_data"]
                key_indexes = [table["columns"].index(name) for name in ("class_id", "row_index", "col_index")]
                value_index = table["columns"].index("value")
                row = next(row for row in table["rows"] if tuple(row[index] for index in key_indexes) == (1, 0, 6))
                row[value_index] = "probe-mismatch"
            mutations["probe mismatch"] = change_probe

            def change_name(snapshot: dict[str, object]) -> None:
                table = snapshot["workspaceTables"]["roster_data"]
                key_indexes = [table["columns"].index(name) for name in ("class_id", "row_index", "col_index")]
                value_index = table["columns"].index("value")
                row = next(row for row in table["rows"] if tuple(row[index] for index in key_indexes) == (1, 0, 0))
                row[value_index] = "name-mismatch"
            mutations["name mismatch"] = change_name

            def change_schedule(snapshot: dict[str, object]) -> None:
                table = snapshot["workspaceTables"]["class_times"]
                table["rows"][0][-1] = "schedule-mismatch"
            mutations["schedule mismatch"] = change_schedule

            for label, mutation in mutations.items():
                with self.subTest(case=label), tempfile.TemporaryDirectory() as case_directory:
                    case_root = Path(case_directory)
                    filled = make_record_count_diagnostic_arm(case_root, "filled", 4567)
                    reduced = make_record_count_diagnostic_arm(case_root, "reduced-rows", 7654)
                    mutate_after_snapshot(reduced, mutation)
                    with self.assertRaisesRegex(ValueError, "changed|probe|rows|roster_data|class_times"):
                        validator.compare_p917_roster_record_count_diagnostic(filled, reduced)

    def test_runner_wires_record_count_arms_as_full_observer_diagnostic_only(self) -> None:
        source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("[switch]$RosterRecordCountDiagnostic", source)
        self.assertIn("selection = 'FilledThenReducedRows'", source)
        self.assertIn("modes = [string[]]@('filled', 'reduced-rows')", source)
        self.assertIn("--record-count-mode", source)
        self.assertIn("[switch]$RecordCountDiagnosticOnly", source)
        self.assertIn("-RecordCountDiagnosticOnly:$hasRecordCountDiagnosticArm", source)
        self.assertIn("-RecordCountDiagnosticMode $mode", source)
        self.assertIn("--compare-roster-record-count-diagnostic", source)
        self.assertIn("--reduced-rows-manifest", source)
        self.assertIn("if ($RosterRecordCountDiagnostic) {", source)
        self.assertIn("and -not ($RosterRecordCountDiagnostic -and [string]$result.status -ceq 'diagnostic-only')", source)
        lifecycle = source.split("function Invoke-P917PackagedLifecycle(", 1)[1].split("\nfunction ", 1)[0]
        self.assertIn("if ($hasRecordCountDiagnosticArm -and (", lifecycle)
        self.assertIn("$manifest.accepted = $false", lifecycle)
        self.assertIn("Filled/reduced-row custom-record cardinality comparison is diagnostic-only", lifecycle)

        if sys.platform != "win32":
            self.skipTest("The P9-17 runner is PowerShell for local Windows.")
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable for runner parser validation.")
        command = r"""
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($env:P917_RUNNER_PATH, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) { throw ($errors | Out-String) }
ConvertTo-Json -InputObject @{ parseErrorCount = $errors.Count; recordModeFound = ($ast.Extent.Text.Contains('-RosterRecordCountDiagnostic')) } -Compress
"""
        environment = os.environ.copy()
        environment["P917_RUNNER_PATH"] = str(RUNNER_PATH.resolve())
        result = subprocess.run(
            [powershell, "-NoLogo", "-NoProfile", "-Command", command],
            capture_output=True, text=True, env=environment, timeout=20, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        parsed = json.loads(result.stdout.strip().splitlines()[-1])
        self.assertEqual(parsed["parseErrorCount"], 0)
        self.assertTrue(parsed["recordModeFound"])

    def test_payload_comparator_requires_final_reentry_in_ordered_window_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)
            original_manifest_bytes = sparse_path.read_bytes()
            original_manifest = json.loads(original_manifest_bytes)
            expected_cycle_ids = list(original_manifest["memory"]["plateauCycleWindowIds"])
            self.assertEqual(expected_cycle_ids[-1], "cycle-final-reentry")

            invalid_orders = {
                "missing final re-entry": expected_cycle_ids[:-1],
                "reordered final re-entry": [expected_cycle_ids[-1], *expected_cycle_ids[:-1]],
            }
            for label, cycle_ids in invalid_orders.items():
                with self.subTest(contract=label):
                    manifest = json.loads(original_manifest_bytes)
                    manifest["memory"]["plateauCycleWindowIds"] = cycle_ids
                    sparse_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, "sample-window IDs omit or reorder"):
                        validator.compare_p917_payload_diagnostic(filled_path, sparse_path)

            sparse_path.write_bytes(original_manifest_bytes)

    def test_payload_comparator_rejects_a_different_action_signature(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)
            sparse_manifest = json.loads(sparse_path.read_text(encoding="utf-8"))
            sparse_manifest["route"]["actions"][0]["valueAfter"] = "different-probe-value"
            sparse_path.write_text(json.dumps(sparse_manifest, ensure_ascii=False), encoding="utf-8")

            report = validator.compare_p917_payload_diagnostic(filled_path, sparse_path)

            self.assertEqual(report["status"], "failed")
            self.assertFalse(report["comparisonChecks"]["sameTenCycleActionSignature"])
            self.assertIn("sameTenCycleActionSignature", report["failure"]["failedChecks"])

    def test_payload_comparator_rejects_non_sentinel_sparse_cell_after_route(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)
            sparse_manifest = json.loads(sparse_path.read_text(encoding="utf-8"))
            after_snapshot_path = sparse_path.parent / sparse_manifest["fixture"]["afterSnapshot"]["relativePath"]
            after_snapshot = json.loads(after_snapshot_path.read_text(encoding="utf-8"))
            roster = after_snapshot["workspaceTables"]["roster_data"]
            indexes = {name: roster["columns"].index(name) for name in ("class_id", "row_index", "col_index", "value")}
            target_row = next(
                row for row in roster["rows"]
                if int(row[indexes["class_id"]]) == 1
                and int(row[indexes["row_index"]]) == 0
                and int(row[indexes["col_index"]]) == 7
            )
            target_row[indexes["value"]] = "unexpected"
            after_snapshot_path.write_text(json.dumps(after_snapshot, ensure_ascii=False), encoding="utf-8")
            sparse_manifest["fixture"]["afterSnapshot"]["sha256"] = hashlib.sha256(
                after_snapshot_path.read_bytes()
            ).hexdigest()
            sparse_path.write_text(json.dumps(sparse_manifest, ensure_ascii=False), encoding="utf-8")
            _refresh_arm_evidence_inventory(sparse_path)

            with self.assertRaisesRegex(ValueError, "sparse-arm custom payload is not the compact sentinel"):
                validator.compare_p917_payload_diagnostic(filled_path, sparse_path)

    def test_payload_comparator_rejects_missing_or_tampered_provenance_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)
            sparse_manifest = json.loads(sparse_path.read_text(encoding="utf-8"))
            package = sparse_manifest["packageProvenance"]
            receipt_path = Path(package["releaseBuildReceiptPath"])
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            build_source_path = Path(receipt["sourceInventoryPath"])
            current_source_path = sparse_path.parent / package["currentSourceInventory"]["relativePath"]
            package_inventory_path = sparse_path.parent / package["packageInventory"]["relativePath"]
            resource_inventory_path = sparse_path.parent / package["resourceInventory"]["relativePath"]
            evidence_inventory_path = sparse_path.parent / sparse_manifest["evidenceInventory"]["relativePath"]
            application_path = Path(package["applicationPath"])
            release_executable_path = Path(receipt["releaseExecutablePath"])

            original_manifest_bytes = sparse_path.read_bytes()
            sparse_manifest["packageProvenance"] = {
                key: package[key]
                for key in (
                    "buildMode", "exactCurrentSourceBuild", "applicationSha256",
                    "productSourceSha256", "currentSourceSha256", "packageSha256",
                    "resourceInventorySha256",
                )
            }
            sparse_path.write_text(json.dumps(sparse_manifest, ensure_ascii=False), encoding="utf-8")
            with self.assertRaises((OSError, ValueError)):
                validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
            sparse_path.write_bytes(original_manifest_bytes)

            def reject_artifact(
                label: str,
                path: Path,
                *,
                refresh_outer_inventory: bool = False,
                mutate_contents: bool = False,
            ) -> None:
                original = path.read_bytes()
                try:
                    if mutate_contents:
                        path.write_bytes(original + b"tampered\n")
                    else:
                        path.unlink()
                    if refresh_outer_inventory:
                        _refresh_arm_evidence_inventory(sparse_path)
                    with self.subTest(artifact=label):
                        with self.assertRaises((OSError, ValueError)):
                            validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
                finally:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(original)
                    if refresh_outer_inventory:
                        _refresh_arm_evidence_inventory(sparse_path)

            reject_artifact("missing Release receipt", receipt_path)
            reject_artifact("tampered Release receipt", receipt_path, mutate_contents=True)
            reject_artifact("missing Release source inventory", build_source_path)
            reject_artifact("tampered Release source inventory", build_source_path, mutate_contents=True)
            reject_artifact("tampered Release executable", release_executable_path, mutate_contents=True)
            reject_artifact("tampered packaged executable", application_path, refresh_outer_inventory=True, mutate_contents=True)
            reject_artifact("missing route current-source inventory", current_source_path, refresh_outer_inventory=True)
            reject_artifact("tampered route current-source inventory", current_source_path, refresh_outer_inventory=True, mutate_contents=True)
            reject_artifact("missing package inventory", package_inventory_path, refresh_outer_inventory=True)
            reject_artifact("tampered package inventory", package_inventory_path, refresh_outer_inventory=True, mutate_contents=True)
            reject_artifact("missing staged-resource inventory", resource_inventory_path, refresh_outer_inventory=True)
            reject_artifact("tampered staged-resource inventory", resource_inventory_path, refresh_outer_inventory=True, mutate_contents=True)
            reject_artifact("missing evidence inventory", evidence_inventory_path)
            reject_artifact("tampered evidence inventory", evidence_inventory_path, mutate_contents=True)

            report = validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
            self.assertEqual(report["status"], "diagnostic-only")
            self.assertFalse(report["accepted"])

    def test_payload_comparator_enforces_the_full_v3_release_receipt_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            filled_path = make_payload_diagnostic_arm(root, "filled", 4567)
            sparse_path = make_payload_diagnostic_arm(root, "sparse", 7654)
            sparse_manifest = json.loads(sparse_path.read_text(encoding="utf-8"))
            receipt_path = Path(sparse_manifest["packageProvenance"]["releaseBuildReceiptPath"])
            repository_root = Path(sparse_manifest["repositoryRoot"])
            original_receipt_bytes = receipt_path.read_bytes()
            original_manifest_bytes = {
                path: path.read_bytes() for path in (filled_path, sparse_path)
            }

            def reject_receipt_change(label: str, mutate) -> None:
                receipt = json.loads(original_receipt_bytes)
                mutate(receipt)
                receipt_path.write_text(json.dumps(receipt, ensure_ascii=False), encoding="utf-8")
                receipt_hash = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
                for manifest_path in (filled_path, sparse_path):
                    manifest = json.loads(original_manifest_bytes[manifest_path])
                    manifest["packageProvenance"]["releaseBuildReceiptSha256"] = receipt_hash
                    manifest["packageProvenance"]["acceptedReleaseReceiptSha256"] = receipt_hash
                    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
                try:
                    with self.subTest(receiptField=label):
                        with self.assertRaises((OSError, ValueError)):
                            validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
                finally:
                    receipt_path.write_bytes(original_receipt_bytes)
                    for manifest_path, original_bytes in original_manifest_bytes.items():
                        manifest_path.write_bytes(original_bytes)

            receipt_mutations = [
                ("missing buildDirectory", lambda value: value.pop("buildDirectory")),
                ("tampered buildDirectory", lambda value: value.__setitem__("buildDirectory", str(repository_root / "build/other"))),
                ("missing cachePath", lambda value: value.pop("cachePath")),
                ("tampered cachePath", lambda value: value.__setitem__("cachePath", str(repository_root / "build/other/CMakeCache.txt"))),
                ("missing cacheSha256", lambda value: value.pop("cacheSha256")),
                ("tampered cacheSha256", lambda value: value.__setitem__("cacheSha256", "0" * 64)),
                ("missing generator", lambda value: value.pop("generator")),
                ("tampered generator", lambda value: value.__setitem__("generator", "other generator")),
                ("missing generatorPlatform", lambda value: value.pop("generatorPlatform")),
                ("tampered generatorPlatform", lambda value: value.__setitem__("generatorPlatform", "Win32")),
                ("missing generatorToolset", lambda value: value.pop("generatorToolset")),
                ("tampered generatorToolset", lambda value: value.__setitem__("generatorToolset", "v142")),
                ("missing generatorInstance", lambda value: value.pop("generatorInstance")),
                ("tampered generatorInstance", lambda value: value.__setitem__("generatorInstance", "C:/Other/VS")),
                ("missing windowsSdkVersion", lambda value: value.pop("windowsSdkVersion")),
                ("tampered windowsSdkVersion", lambda value: value.__setitem__("windowsSdkVersion", "10.0.19041.0")),
                ("missing generatedProjectToolchain", lambda value: value.pop("generatedProjectToolchain")),
                ("tampered generatedProjectToolchain", lambda value: value["generatedProjectToolchain"].__setitem__("platformToolset", "v142")),
                ("missing compiler", lambda value: value.pop("compiler")),
                ("tampered compiler hash", lambda value: value["compiler"].__setitem__("sha256", "0" * 64)),
                ("missing buildHelperPath", lambda value: value.pop("buildHelperPath")),
                ("tampered buildHelperPath", lambda value: value.__setitem__("buildHelperPath", str(repository_root / "src/diagnostic_product.cpp"))),
                ("missing buildHelperSha256", lambda value: value.pop("buildHelperSha256")),
                ("tampered buildHelperSha256", lambda value: value.__setitem__("buildHelperSha256", "0" * 64)),
                ("missing preflightPath", lambda value: value.pop("preflightPath")),
                ("tampered preflightPath", lambda value: value.__setitem__("preflightPath", value["sourceInventoryPath"])),
                ("missing preflightSha256", lambda value: value.pop("preflightSha256")),
                ("tampered preflightSha256", lambda value: value.__setitem__("preflightSha256", "0" * 64)),
                ("missing packageOption", lambda value: value.pop("packageOption")),
                ("tampered packageOption", lambda value: value.__setitem__("packageOption", "OFF")),
                ("missing incrementalBuild", lambda value: value.pop("incrementalBuild")),
                ("tampered incrementalBuild", lambda value: value.__setitem__("incrementalBuild", False)),
                ("missing targets", lambda value: value.pop("targets")),
                ("missing executionContext", lambda value: value.pop("executionContext")),
                ("missing build log path", lambda value: value["build"].pop("logPath")),
                ("tampered build log hash", lambda value: value["build"].__setitem__("logSha256", "0" * 64)),
                ("missing generatedResources", lambda value: value.pop("generatedResources")),
            ]
            for label, mutate in receipt_mutations:
                reject_receipt_change(label, mutate)

            def reject_package_change(label: str, mutate) -> None:
                original_bytes = sparse_path.read_bytes()
                manifest = json.loads(original_bytes)
                mutate(manifest["packageProvenance"])
                sparse_path.write_text(json.dumps(manifest, ensure_ascii=False), encoding="utf-8")
                try:
                    with self.subTest(packageField=label):
                        with self.assertRaises((OSError, ValueError)):
                            validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
                finally:
                    sparse_path.write_bytes(original_bytes)

            package_mutations = [
                ("missing buildDirectory", lambda value: value.pop("buildDirectory")),
                ("tampered buildDirectory", lambda value: value.__setitem__("buildDirectory", str(repository_root / "build/other"))),
                ("missing cmakeHomeDirectory", lambda value: value.pop("cmakeHomeDirectory")),
                ("tampered cmakeHomeDirectory", lambda value: value.__setitem__("cmakeHomeDirectory", str(repository_root / "other"))),
                ("missing cachePath", lambda value: value.pop("cachePath")),
                ("tampered cachePath", lambda value: value.__setitem__("cachePath", str(repository_root / "build/other/CMakeCache.txt"))),
                ("missing cacheSha256", lambda value: value.pop("cacheSha256")),
                ("tampered cacheSha256", lambda value: value.__setitem__("cacheSha256", "0" * 64)),
                ("missing generator", lambda value: value.pop("generator")),
                ("tampered generator", lambda value: value.__setitem__("generator", "other generator")),
                ("missing generatorPlatform", lambda value: value.pop("generatorPlatform")),
                ("tampered generatorPlatform", lambda value: value.__setitem__("generatorPlatform", "Win32")),
                ("missing generatorToolset", lambda value: value.pop("generatorToolset")),
                ("tampered generatorToolset", lambda value: value.__setitem__("generatorToolset", "v142")),
                ("missing generatorInstance", lambda value: value.pop("generatorInstance")),
                ("tampered generatorInstance", lambda value: value.__setitem__("generatorInstance", "C:/Other/VS")),
                ("missing windowsSdkVersion", lambda value: value.pop("windowsSdkVersion")),
                ("tampered windowsSdkVersion", lambda value: value.__setitem__("windowsSdkVersion", "10.0.19041.0")),
                ("missing compiler", lambda value: value.pop("compiler")),
                ("tampered compiler", lambda value: value["compiler"].__setitem__("sha256", "0" * 64)),
                ("missing incrementalBuild", lambda value: value.pop("incrementalBuild")),
                ("tampered incrementalBuild", lambda value: value.__setitem__("incrementalBuild", False)),
                ("missing buildLogPath", lambda value: value.pop("buildLogPath")),
                ("tampered buildLogPath", lambda value: value.__setitem__("buildLogPath", str(repository_root / "src/diagnostic_product.cpp"))),
                ("missing buildLogSha256", lambda value: value.pop("buildLogSha256")),
                ("tampered buildLogSha256", lambda value: value.__setitem__("buildLogSha256", "0" * 64)),
                ("missing releaseExecutablePath", lambda value: value.pop("releaseExecutablePath")),
                ("tampered releaseExecutablePath", lambda value: value.__setitem__("releaseExecutablePath", str(repository_root / "src/diagnostic_product.cpp"))),
                ("missing releaseExecutableSha256", lambda value: value.pop("releaseExecutableSha256")),
                ("tampered releaseExecutableSha256", lambda value: value.__setitem__("releaseExecutableSha256", "0" * 64)),
                ("missing executionContext", lambda value: value.pop("executionContext")),
            ]
            for label, mutate in package_mutations:
                reject_package_change(label, mutate)

            report = validator.compare_p917_payload_diagnostic(filled_path, sparse_path)
            self.assertEqual(report["status"], "diagnostic-only")
            self.assertFalse(report["accepted"])

    def test_source_delta_accepts_only_the_named_p917_harness_additions(self) -> None:
        prior = [
            {"path": "src/roster_model.cpp", "sizeBytes": 10, "sha256": "a" * 64},
            {"path": "scripts/phase9/p9_14_packaged_lifecycle_common.ps1", "sizeBytes": 12, "sha256": "b" * 64},
        ]
        current = prior + [
            {"path": path, "sizeBytes": index + 1, "sha256": format(index, "064x")}
            for index, path in enumerate(validator.P917_OWNED_SOURCES)
        ]

        added = validator.validate_source_inventory_delta(prior, current, validator.P917_OWNED_SOURCES)

        self.assertEqual(added, sorted(validator.P917_OWNED_SOURCES, key=str.casefold))
        product_rows = validator.filter_product_source_rows(
            current,
            set(validator.P917_OWNED_SOURCES) | {"scripts/phase9/p9_14_packaged_lifecycle_common.ps1"},
        )
        self.assertEqual([row["path"] for row in product_rows], ["src/roster_model.cpp"])

    def test_source_delta_rejects_changed_product_files_and_unlisted_additions(self) -> None:
        prior = [{"path": "src/roster_model.cpp", "sizeBytes": 10, "sha256": "a" * 64}]
        changed = [{"path": "src/roster_model.cpp", "sizeBytes": 11, "sha256": "c" * 64}]
        with self.assertRaisesRegex(ValueError, "source changed"):
            validator.validate_source_inventory_delta(prior, changed, validator.P917_OWNED_SOURCES)

        unexpected = prior + [{"path": "src/new_feature.cpp", "sizeBytes": 5, "sha256": "d" * 64}]
        with self.assertRaisesRegex(ValueError, "Unexpected source additions"):
            validator.validate_source_inventory_delta(prior, unexpected, validator.P917_OWNED_SOURCES)

    def test_heap_diagnostic_source_paths_are_pinned_and_unrelated_product_or_test_changes_fail(self) -> None:
        diagnostic_product_paths = {
            "src/core/process_memory_snapshot.cpp",
            "src/core/process_memory_snapshot.h",
            "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
            "src/next/bootstrap/qt_page_lifecycle_diagnostics.h",
        }
        diagnostic_test_paths = {"tests/next/page_host_tests.cpp"}
        all_product_changes = validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES
        all_test_changes = validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
        self.assertTrue(diagnostic_product_paths.issubset(
            {change["path"] for change in all_product_changes}
        ))
        self.assertTrue(diagnostic_test_paths.issubset(
            {change["path"] for change in all_test_changes}
        ))

        diagnostic_changes = [
            change for change in (*all_product_changes, *all_test_changes)
            if change["path"] in diagnostic_product_paths | diagnostic_test_paths
        ]
        self.assertEqual(
            {change["path"] for change in diagnostic_changes},
            diagnostic_product_paths | diagnostic_test_paths,
        )
        complete_allowlist = validator.p917_source_delta_allowlist()
        diagnostic_allowlist = {
            path: complete_allowlist[path]
            for path in diagnostic_product_paths | diagnostic_test_paths
        }
        prior = [
            {
                "path": change["path"],
                "sizeBytes": change["beforeSizeBytes"],
                "sha256": change["beforeSha256"],
            }
            for change in diagnostic_changes
        ]
        current = [
            {
                "path": change["path"],
                "sizeBytes": change["afterSizeBytes"],
                "sha256": change["afterSha256"],
            }
            for change in diagnostic_changes
        ]
        self.assertEqual(
            validator.validate_source_inventory_delta(
                prior, current, expected_additions=(), allowed_changes=diagnostic_allowlist
            ),
            [],
        )

        for unrelated_path in (
            "src/next/ui/unrelated_heap_product.cpp",
            "tests/next/unrelated_heap_test.cpp",
        ):
            unrelated_prior = [
                *prior,
                {"path": unrelated_path, "sizeBytes": 3, "sha256": "a" * 64},
            ]
            unrelated_current = [
                *current,
                {"path": unrelated_path, "sizeBytes": 4, "sha256": "b" * 64},
            ]
            with self.assertRaisesRegex(
                ValueError, f"Previously inventoried source changed: {re.escape(unrelated_path)}"
            ):
                validator.validate_source_inventory_delta(
                    unrelated_prior,
                    unrelated_current,
                    expected_additions=(),
                    allowed_changes=diagnostic_allowlist,
                )

    def test_preserved_inventories_prove_the_exact_p917_source_delta(self) -> None:
        accepted_manifest_path = REPOSITORY / validator.P916_MANIFEST_RELATIVE
        accepted_manifest = json.loads(accepted_manifest_path.read_text(encoding="utf-8"))
        accepted_package = accepted_manifest["packageProvenance"]
        accepted_source_path = accepted_manifest_path.parent / accepted_package["sourceInventoryPath"]
        accepted_source = json.loads(accepted_source_path.read_text(encoding="utf-8"))
        self.assertEqual(
            hashlib.sha256(accepted_source_path.read_bytes()).hexdigest(),
            accepted_package["sourceInventoryFileSha256"],
        )

        receipt = json.loads(P917_BUILD_RECEIPT.read_text(encoding="utf-8"))
        current_source_path = Path(receipt["sourceInventoryPath"])
        if not current_source_path.is_absolute():
            current_source_path = P917_BUILD_RECEIPT.parent / current_source_path
        current_source_path = current_source_path.resolve(strict=True)
        captured_build_source = json.loads(current_source_path.read_text(encoding="utf-8"))
        self.assertEqual(
            hashlib.sha256(current_source_path.read_bytes()).hexdigest(),
            receipt["sourceInventoryFileSha256"],
        )
        self.assertEqual(captured_build_source["sha256"], receipt["sourceSha256"])
        current_source_files = validator._scan_source_inventory(REPOSITORY, accepted_source["includedRoots"])
        current_roster_test = next(
            row for row in current_source_files
            if row["path"] == "tests/next/roster_editor_tests.cpp"
        )
        pinned_roster_test = next(
            change for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
            if change["path"] == "tests/next/roster_editor_tests.cpp"
        )
        self.assertEqual(current_roster_test["sizeBytes"], pinned_roster_test["afterSizeBytes"])
        self.assertEqual(current_roster_test["sha256"], pinned_roster_test["afterSha256"])

        additions = validator.validate_source_inventory_delta(
            accepted_source["files"],
            current_source_files,
            validator.P917_OWNED_SOURCES,
            validator.p917_source_delta_allowlist(),
        )
        self.assertEqual(additions, sorted(validator.P917_OWNED_SOURCES, key=str.casefold))

        p913_manifest_path = Path(accepted_package["priorP913ManifestPath"]).resolve(strict=True)
        p913_manifest = json.loads(p913_manifest_path.read_text(encoding="utf-8"))
        derivation_path = p913_manifest_path.parent / p913_manifest["source"]["derivedReceiptPath"]
        derivation = json.loads(derivation_path.read_text(encoding="utf-8"))
        legacy_exclusions = {
            str(path).casefold()
            for path in derivation["source"]["allowedPaths"]
        }
        legacy_exclusions.update(
            path.casefold()
            for path in (
                *validator.P914_HARNESS_SOURCES,
                *validator.P915_HARNESS_SOURCES,
                *validator.P916_HARNESS_SOURCES,
            )
        )
        product_paths = {
            str(change["path"]).casefold()
            for change in validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES
        }
        test_paths = {
            str(change["path"]).casefold()
            for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
        }
        product_exclusions = (
            legacy_exclusions
            | {path.casefold() for path in validator.P917_OWNED_SOURCES}
            | test_paths
        )
        current_product = validator.filter_product_source_rows(
            current_source_files, product_exclusions
        )
        current_product_by_path = {
            row["path"].casefold(): row for row in current_product
        }
        self.assertTrue(product_paths.issubset(current_product_by_path))
        for change in validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES:
            row = current_product_by_path[change["path"].casefold()]
            self.assertEqual(row["sizeBytes"], change["afterSizeBytes"])
            self.assertEqual(row["sha256"], change["afterSha256"])

        common_exclusions = product_exclusions | product_paths
        accepted_common = validator.filter_product_source_rows(
            accepted_source["files"], common_exclusions
        )
        current_common = validator.filter_product_source_rows(
            current_source_files, common_exclusions
        )
        self.assertEqual(
            validator._source_row_map(accepted_common, "Accepted common product"),
            validator._source_row_map(current_common, "Current common product"),
        )

    def test_roster_repository_streaming_delta_is_exact_and_rejects_other_product_changes(self) -> None:
        path = "src/data/repositories/roster_repository.cpp"
        pinned = next(
            change
            for change in validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES
            if change["path"] == path
        )
        self.assertEqual(
            pinned,
            {
                "path": path,
                "beforeSizeBytes": 146469,
                "beforeSha256": "41c594afab83cbe6635cadc7f4c800e125843fb2336ed66b63c8ae6d33bbf719",
                "afterSizeBytes": 154045,
                "afterSha256": "b4805038b7589b18eaad8c7eee93b4424af187ce652521f44e24cbcc220b0fdb",
            },
        )

        accepted_manifest_path = REPOSITORY / validator.P916_MANIFEST_RELATIVE
        accepted_manifest = json.loads(accepted_manifest_path.read_text(encoding="utf-8"))
        accepted_package = accepted_manifest["packageProvenance"]
        accepted_source_path = accepted_manifest_path.parent / accepted_package["sourceInventoryPath"]
        accepted_source = json.loads(accepted_source_path.read_text(encoding="utf-8"))
        prior_row = next(row for row in accepted_source["files"] if row["path"] == path)
        current_source = validator._scan_source_inventory(REPOSITORY, accepted_source["includedRoots"])
        current_row = next(row for row in current_source if row["path"] == path)
        self.assertEqual(
            (prior_row["sizeBytes"], prior_row["sha256"]),
            (pinned["beforeSizeBytes"], pinned["beforeSha256"]),
        )
        self.assertEqual(
            (current_row["sizeBytes"], current_row["sha256"]),
            (pinned["afterSizeBytes"], pinned["afterSha256"]),
        )

        allowed = {path: validator.p917_source_delta_allowlist()[path]}
        self.assertEqual(
            validator.validate_source_inventory_delta(
                [prior_row], [current_row], expected_additions=(), allowed_changes=allowed
            ),
            [],
        )

        tampered = dict(current_row)
        tampered["sha256"] = "f" * 64
        with self.assertRaisesRegex(ValueError, "pinned before/after hashes"):
            validator.validate_source_inventory_delta(
                [prior_row], [tampered], expected_additions=(), allowed_changes=allowed
            )

        unrelated_path = "src/data/unapproved_repository.cpp"
        prior_with_unrelated = [
            prior_row,
            {"path": unrelated_path, "sizeBytes": 10, "sha256": "a" * 64},
        ]
        current_with_unrelated = [
            current_row,
            {"path": unrelated_path, "sizeBytes": 11, "sha256": "b" * 64},
        ]
        with self.assertRaisesRegex(
            ValueError, f"Previously inventoried source changed: {re.escape(unrelated_path)}"
        ):
            validator.validate_source_inventory_delta(
                prior_with_unrelated,
                current_with_unrelated,
                expected_additions=(),
                allowed_changes=allowed,
            )
        repository_header = (REPOSITORY / "src/data/repositories/roster_repository.h").read_text(
            encoding="utf-8"
        )
        repository_source = (REPOSITORY / "src/data/repositories/roster_repository.cpp").read_text(
            encoding="utf-8"
        )
        platform_source = (
            REPOSITORY / "src/next/platform/workspace_store_roster_actions.cpp"
        ).read_text(encoding="utf-8")
        platform_header = (
            REPOSITORY / "src/next/platform/workspace_store_roster_actions.h"
        ).read_text(encoding="utf-8")

        self.assertIn("struct WindowSaveRowSource final", repository_header)
        self.assertIn("are consumed synchronously during the save call", repository_header)
        self.assertIn("Callback exceptions map to Technical", repository_header)
        for accessor in ("columnNameAt", "columnWidthAt", "rowCellCountAt", "cellAt"):
            self.assertIn(accessor, repository_header)
        self.assertIn("saveRosterWindowWithParentCheck(source)", platform_source)
        self.assertNotIn("legacyRoster(request.roster)", platform_source)
        self.assertIn("[&request](const std::size_t row", platform_source)
        self.assertIn("source.cellAt(row, column)", repository_source)
        self.assertIn("const QString value = column < rowCellCount", repository_source)
        self.assertIn("Roster window row source callback failed.", repository_source)
        self.assertIn("writeRosterWindowRows(window)", repository_source)
        self.assertIn(
            "RepositoryStatus saveRoster(\n        int classId,\n        const Roster& roster",
            repository_header,
        )
        self.assertIn(
            "saveRosterAtPath(\n    const QString& databasePath,\n    Application::RosterSaveRequest request",
            platform_header,
        )

    def test_exact_p917_source_delta_pins_product_test_and_harness_changes(self) -> None:
        product_changes = validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES
        test_changes = validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
        self.assertEqual(
            {change["path"] for change in product_changes},
            {
                "src/next/bootstrap/application_bootstrap.cpp",
                "src/core/process_memory_snapshot.cpp",
                "src/core/process_memory_snapshot.h",
                "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
                "src/next/bootstrap/qt_page_lifecycle_diagnostics.h",
                "src/data/repositories/roster_repository.cpp",
                "src/data/repositories/roster_repository.h",
                "src/next/application/roster_save_preparation.h",
                "src/next/application/roster_save_use_case.h",
                "src/next/platform/workspace_store_roster_actions.cpp",
                "src/next/platform/workspace_store_roster_actions.h",
                "src/next/ui/roster_editor_widget.cpp",
                "src/next/ui/roster_editor_widget.h",
                "src/next/ui/roster_table_model.cpp",
                "src/next/ui/roster_table_model.h",
                "src/next/ui/selected_class_editor.cpp",
                "src/next/ui/selected_class_editor.h",
            },
        )
        self.assertEqual(
            {change["path"] for change in validator.P917_NEW_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES},
            {
                "tests/next/page_host_tests.cpp",
                "tests/next/roster_editor_tests.cpp",
                "tests/next_application_roster_save_use_case_tests.cpp",
                "tests/next/classes_navigator_page_tests.cpp",
                "tests/next_platform_application_services_roster_save_port_tests.cpp",
            },
        )
        self.assertEqual(len(product_changes), 17)
        self.assertEqual(len(validator.P917_NEW_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES), 5)
        roster_save_test_change = next(
            change
            for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
            if change["path"] == "tests/next_platform_application_services_roster_save_port_tests.cpp"
        )
        self.assertEqual(
            roster_save_test_change,
            {
                "path": "tests/next_platform_application_services_roster_save_port_tests.cpp",
                "beforeSizeBytes": 38542,
                "beforeSha256": "471e010aac3912c2cdf140dbd18f8097cf69a3e146c107dbeb25ca76c7ac1dca",
                "afterSizeBytes": 46186,
                "afterSha256": "2694a4d2d77640481e5e53c20f98669d002069a3f01a758e23e8c30862e9cbbb",
                "target": "ClassMngrNextPlatformApplicationServicesRosterSavePortTests",
                "cmakeLines": "7308-7316",
                "cmakeBoundaryEvidence": (
                    {
                        "path": "cmake/tests/next.cmake",
                        "lines": "7308-7316",
                        "finding": "The changed test source is compiled only into standalone ClassMngrNextPlatformApplicationServicesRosterSavePortTests.",
                    },
                    *validator.P917_TEST_ONLY_SOURCE_BOUNDARY_EVIDENCE,
                ),
            },
        )

        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        runner_start = runner_source.index("$P917AuthorizedProductSourceChanges = @(")
        runner_end = runner_source.index("\n)", runner_start)
        runner_records = re.findall(
            r"\[ordered\]@\{ path = '([^']+)'; beforeSizeBytes = \[Int64\](\d+); "
            r"beforeSha256 = '([0-9a-f]{64})'; afterSizeBytes = \[Int64\](\d+); "
            r"afterSha256 = '([0-9a-f]{64})'\s*\}",
            runner_source[runner_start:runner_end],
        )
        validator_records = [
            (
                str(change["path"]),
                str(change["beforeSizeBytes"]),
                str(change["beforeSha256"]),
                str(change["afterSizeBytes"]),
                str(change["afterSha256"]),
            )
            for change in product_changes
        ]
        self.assertEqual(runner_records, validator_records)

        runner_test_start = runner_source.index("$P917AuthorizedTestOnlySourceChanges = @(")
        runner_test_end = runner_source.index("\n)", runner_test_start)
        runner_test_records = re.findall(
            r"\[ordered\]@\{ path = '([^']+)'; beforeSizeBytes = \[Int64\](\d+); "
            r"beforeSha256 = '([0-9a-f]{64})'; afterSizeBytes = \[Int64\](\d+); "
            r"afterSha256 = '([0-9a-f]{64})'; target = '([^']+)'; cmakeLines = '([^']+)'\s*\}",
            runner_source[runner_test_start:runner_test_end],
        )
        validator_test_records = [
            (
                str(change["path"]),
                str(change["beforeSizeBytes"]),
                str(change["beforeSha256"]),
                str(change["afterSizeBytes"]),
                str(change["afterSha256"]),
                str(change["target"]),
                str(change["cmakeLines"]),
            )
            for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
        ]
        self.assertEqual(runner_test_records, validator_test_records)

        changes = (
            *product_changes,
            *test_changes,
            *validator.P917_AUTHORIZED_HARNESS_SOURCE_CHANGES,
        )
        prior = [{"path": "src/unchanged_common.cpp", "sizeBytes": 10, "sha256": "a" * 64}]
        current = [dict(prior[0])]
        for change in changes:
            prior.append(
                {
                    "path": change["path"],
                    "sizeBytes": change["beforeSizeBytes"],
                    "sha256": change["beforeSha256"],
                }
            )
            current.append(
                {
                    "path": change["path"],
                    "sizeBytes": change["afterSizeBytes"],
                    "sha256": change["afterSha256"],
                }
            )
        current.extend(
            {"path": path, "sizeBytes": index + 1, "sha256": format(index, "064x")}
            for index, path in enumerate(validator.P917_OWNED_SOURCES)
        )
        allowed_change = validator.p917_source_delta_allowlist()

        added = validator.validate_source_inventory_delta(
            prior, current, validator.P917_OWNED_SOURCES, allowed_change
        )

        self.assertEqual(added, sorted(validator.P917_OWNED_SOURCES, key=str.casefold))
        test_paths = {change["path"] for change in test_changes}
        product_paths = {change["path"] for change in product_changes}
        exclusions = set(validator.P917_HARNESS_SOURCES) | test_paths
        previous_product = validator.filter_product_source_rows(prior, exclusions)
        current_product = validator.filter_product_source_rows(current, exclusions)
        current_product_by_path = {row["path"]: row for row in current_product}
        for change in product_changes:
            self.assertEqual(
                current_product_by_path[change["path"]],
                {"path": change["path"], "sizeBytes": change["afterSizeBytes"], "sha256": change["afterSha256"]},
            )
        self.assertTrue(product_paths.issubset(current_product_by_path))
        self.assertTrue(test_paths.isdisjoint(current_product_by_path))

        common_exclusions = exclusions | product_paths
        previous_common = validator.filter_product_source_rows(prior, common_exclusions)
        current_common = validator.filter_product_source_rows(current, common_exclusions)
        self.assertEqual(previous_common, current_common)
        self.assertEqual(
            validator.canonical_inventory_fingerprint(previous_common),
            validator.canonical_inventory_fingerprint(current_common),
        )

        tampered = [dict(row) for row in current]
        tampered_path = product_changes[0]["path"]
        next(row for row in tampered if row["path"] == tampered_path)["sha256"] = "f" * 64
        with self.assertRaisesRegex(ValueError, "pinned before/after hashes"):
            validator.validate_source_inventory_delta(
                prior, tampered, validator.P917_OWNED_SOURCES, allowed_change
            )

    def test_exact_allowlist_rejects_extra_product_and_unlisted_test_changes(self) -> None:
        prior = [
            {"path": "src/extra_production.cpp", "sizeBytes": 10, "sha256": "a" * 64},
            {"path": "tests/next/unlisted_test.cpp", "sizeBytes": 11, "sha256": "b" * 64},
        ]
        for change in (
            *validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES,
            *validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES,
            *validator.P917_AUTHORIZED_HARNESS_SOURCE_CHANGES,
        ):
            prior.append({"path": change["path"], "sizeBytes": change["beforeSizeBytes"], "sha256": change["beforeSha256"]})
        current = [dict(row) for row in prior]
        current.extend(
            {"path": path, "sizeBytes": index + 1, "sha256": format(index, "064x")}
            for index, path in enumerate(validator.P917_OWNED_SOURCES)
        )
        allowed = validator.p917_source_delta_allowlist()

        next(row for row in current if row["path"] == "src/extra_production.cpp").update(sizeBytes=12, sha256="c" * 64)
        with self.assertRaisesRegex(ValueError, "Previously inventoried source changed: src/extra_production.cpp"):
            validator.validate_source_inventory_delta(prior, current, validator.P917_OWNED_SOURCES, allowed)

        current = [dict(row) for row in prior]
        current.extend(
            {"path": path, "sizeBytes": index + 1, "sha256": format(index, "064x")}
            for index, path in enumerate(validator.P917_OWNED_SOURCES)
        )
        next(row for row in current if row["path"] == "tests/next/unlisted_test.cpp").update(sizeBytes=13, sha256="d" * 64)
        with self.assertRaisesRegex(ValueError, "Previously inventoried source changed: tests/next/unlisted_test.cpp"):
            validator.validate_source_inventory_delta(prior, current, validator.P917_OWNED_SOURCES, allowed)

        missing_declared = [
            row for row in prior
            if row["path"] != validator.P917_AUTHORIZED_PRODUCT_SOURCE_CHANGES[0]["path"]
        ]
        missing_declared.extend(
            {"path": path, "sizeBytes": index + 1, "sha256": format(index, "064x")}
            for index, path in enumerate(validator.P917_OWNED_SOURCES)
        )
        with self.assertRaisesRegex(ValueError, "Previously inventoried source disappeared"):
            validator.validate_source_inventory_delta(prior, missing_declared, validator.P917_OWNED_SOURCES, allowed)

    def test_p917_delta_record_order_is_ignored_but_records_remain_exact(self) -> None:
        expected = [
            {
                "path": change["path"],
                "beforeSizeBytes": int(change["beforeSizeBytes"]),
                "beforeSha256": str(change["beforeSha256"]),
                "afterSizeBytes": int(change["afterSizeBytes"]),
                "afterSha256": str(change["afterSha256"]),
                "classification": "test-only-source-change-excluded-from-product-fingerprint",
                "productFingerprintExcluded": True,
                "cmakeBoundaryEvidence": list(change["cmakeBoundaryEvidence"]),
            }
            for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
        ]

        validator.require_exact_path_records(
            list(reversed(expected)), expected, "Shuffled authorized test-only records were rejected."
        )
        validator.require_exact_paths(
            [row["path"] for row in reversed(expected)],
            [row["path"] for row in expected],
            "Shuffled authorized test-only paths were rejected.",
        )

        extra = [*expected, {**expected[0], "path": "tests/next/extra.cpp"}]
        missing = expected[:-1]
        altered = [dict(row) for row in expected]
        altered[0]["afterSha256"] = "0" * 64
        altered_boundary = [dict(row) for row in expected]
        altered_boundary[0]["cmakeBoundaryEvidence"] = []
        for records in (extra, missing, altered, altered_boundary):
            with self.assertRaises(ValueError):
                validator.require_exact_path_records(
                    records, expected, "Invalid P9-17 test-only records were accepted."
                )

    def test_source_proof_rejects_numeric_boolean_and_duplicate_exclusion_paths(self) -> None:
        change = validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES[0]
        expected_record = {
            "path": change["path"],
            "beforeSizeBytes": int(change["beforeSizeBytes"]),
            "beforeSha256": str(change["beforeSha256"]),
            "afterSizeBytes": int(change["afterSizeBytes"]),
            "afterSha256": str(change["afterSha256"]),
            "classification": "test-only-source-change-excluded-from-product-fingerprint",
            "productFingerprintExcluded": True,
            "cmakeBoundaryEvidence": list(change["cmakeBoundaryEvidence"]),
        }
        numeric_boolean_record = dict(expected_record)
        numeric_boolean_record["productFingerprintExcluded"] = 1
        with self.assertRaisesRegex(ValueError, "not a JSON boolean"):
            validator.require_exact_path_records(
                [numeric_boolean_record], [expected_record], "Numeric JSON booleans must be rejected."
            )

        excluded_path = "tests/next/sub_prep_page_tests.cpp"
        with self.assertRaisesRegex(ValueError, "duplicate path"):
            validator.require_exact_paths(
                [excluded_path, excluded_path.upper()],
                [excluded_path],
                "Duplicate product-fingerprint exclusions must be rejected.",
            )

    def test_source_delta_sizes_require_json_integers(self) -> None:
        expected = [{
            "path": "tests/next/synthetic_size_delta.cpp",
            "beforeSizeBytes": 3,
            "afterSizeBytes": 4,
        }]
        validator.require_exact_path_records(
            [dict(expected[0])], expected, "Valid integer byte sizes were rejected."
        )

        for field in ("beforeSizeBytes", "afterSizeBytes"):
            for invalid_value in (3.0, True):
                invalid = dict(expected[0])
                invalid[field] = invalid_value
                with self.assertRaisesRegex(ValueError, "not a JSON integer"):
                    validator.require_exact_path_records(
                        [invalid], expected, "Non-integer byte sizes must be rejected."
                    )

    def test_test_only_source_change_carries_the_exact_cmake_boundary(self) -> None:
        expected = {
            "tests/next/sub_prep_page_tests.cpp": ("ClassMngrNextSubPrepPageTests", "4014-4023,4059-4070"),
            "tests/next/shared_staff_directory_visual_capture.cpp": (
                "ClassMngrNextSharedStaffDirectoryVisualCapture",
                "997-998,1035-1036",
            ),
            "tests/next/korean_teacher_directory_visual_capture.cpp": (
                "ClassMngrNextTeacherDirectoryVisualCapture",
                "1085-1086,1144-1145",
            ),
            "tests/next/upcoming_birthdays_visual_capture.cpp": (
                "ClassMngrNextUpcomingBirthdaysVisualCapture",
                "5281-5282,5310-5311",
            ),
            "cmake/tests/next.cmake": (
                "ClassMngrNextRosterEditorTests",
                "4398-4406",
            ),
            "tests/next/page_host_tests.cpp": (
                "ClassMngrNextPageHostTests",
                "843-894",
            ),
            "tests/next/roster_editor_tests.cpp": (
                "ClassMngrNextRosterEditorTests",
                "4398-4446",
            ),
            "tests/next_application_roster_save_use_case_tests.cpp": (
                "ClassMngrNextApplicationRosterSaveUseCaseTests",
                "5937-5944",
            ),
            "tests/next/classes_navigator_page_tests.cpp": (
                "ClassMngrNextClassesNavigatorPageTests",
                "4264-4277",
            ),
            "tests/next_platform_application_services_roster_save_port_tests.cpp": (
                "ClassMngrNextPlatformApplicationServicesRosterSavePortTests",
                "7308-7316",
            ),
        }
        self.assertEqual(
            {change["path"]: (change["target"], change["cmakeLines"])
             for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES},
            expected,
        )
        for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES:
            boundary = change["cmakeBoundaryEvidence"]
            self.assertEqual(
                [item["path"] for item in boundary],
                [
                    "cmake/tests/next.cmake",
                    "cmake/next_app/CMakeLists.txt",
                    "cmake/next_resources.cmake",
                ],
            )
            self.assertIn("standalone", boundary[0]["finding"])
        cmake_change_boundary = next(
            change["cmakeBoundaryEvidence"][0]
            for change in validator.P917_AUTHORIZED_TEST_ONLY_SOURCE_CHANGES
            if change["path"] == "cmake/tests/next.cmake"
        )
        self.assertIn("implementation sources", cmake_change_boundary["finding"])

    def test_roster_tab_selection_requires_uia_selection_pattern_readback(self) -> None:
        actions = [
            {
                "actionKind": action_kind,
                "method": "SelectionItemPattern.Select with IsSelected readback",
                "selectedTabName": "Roster",
                "expectedSection": "Roster",
                "selectionItemPatternAvailable": True,
                "selectionItemPatternIsSelected": True,
                "target": {"name": "Roster", "controlType": "ControlType.TabItem"},
            }
            for action_kind in ("open-roster-tab", "reenter-roster-tab")
        ]

        result = validator.validate_roster_tab_selection_actions(actions)

        self.assertTrue(result["open-roster-tab"]["passed"])
        actions[0]["selectionItemPatternIsSelected"] = False
        with self.assertRaisesRegex(ValueError, "exact Roster tab selection"):
            validator.validate_roster_tab_selection_actions(actions)

    def test_section_suspension_requires_fresh_active_tree_roster_absence_and_makes_no_model_claim(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ui_path = root / "uia" / "roster-section-suspended-tree.json"
            ui_path.parent.mkdir(parents=True)
            process_id = 2317
            hwnd = 0x01234567
            captured_at = "2026-11-22T06:20:01.700Z"
            snapshot_payload = {
                "name": ui_path.stem,
                "capturedAtUtc": captured_at,
                "root": {
                    "processId": process_id,
                    "ownerWindowHandle": hwnd,
                    "ownerWindowProcessId": process_id,
                },
                "descendants": [],
            }
            ui_path.write_text(json.dumps(snapshot_payload), encoding="utf-8")
            screenshot_path = root / "screenshots" / "roster-section-suspended.png"
            screenshot_path.parent.mkdir(parents=True)
            screenshot_path.write_bytes(b"screenshot")
            screenshot_captured = "2026-11-22T06:20:01.800Z"
            checkpoint = {
                "selectedSection": "Class Details and Times",
                "selectedTabVerified": True,
                "selectedTabReadbackSource": validator.P917_ROSTER_SELECTION_READBACK_SOURCE,
                "selectedTabReadbackAtUtc": "2026-11-22T06:20:01.600Z",
                "settledMilliseconds": 700,
                "activeUiAutomationRosterTablePresent": False,
                "activeTreeRosterControlCount": 0,
                "freshActiveTreeCaptured": True,
                "proofKind": validator.P917_ROSTER_ABSENCE_PROOF_KIND,
                "leaveActionKind": "leave-roster-section",
                "leaveActionIndex": 0,
                "processId": process_id,
                "mainWindowHandle": hwnd,
                "activeTreeRootProcessId": process_id,
                "activeTreeRootOwnerWindowHandle": hwnd,
                "activeTreeCapturedAtUtc": captured_at,
                "uiaSnapshotRelativePath": "uia/roster-section-suspended-tree.json",
                "screenshotRelativePath": "screenshots/roster-section-suspended.png",
                "screenshotCapturedAtUtc": screenshot_captured,
            }
            route = {
                "modelCounts": {"suspendedRosterRootRows": None, "suspendedRosterColumnCount": None},
                "actions": [{
                    "actionKind": "leave-roster-section",
                    "expectedSection": "Class Details and Times",
                    "selectedTabName": "Class Details and Times",
                    "completedAtUtc": "2026-11-22T06:20:01.000Z",
                }],
                "sectionSuspensionCheckpoint": checkpoint,
                "uiaSnapshots": [{
                    "relativePath": checkpoint["uiaSnapshotRelativePath"],
                    "sha256": hashlib.sha256(ui_path.read_bytes()).hexdigest(),
                    "sizeBytes": ui_path.stat().st_size,
                    "capturedAtUtc": captured_at,
                    "processId": process_id,
                    "windowHandle": hwnd,
                }],
                "screenshots": [{
                    "relativePath": checkpoint["screenshotRelativePath"],
                    "sha256": hashlib.sha256(screenshot_path.read_bytes()).hexdigest(),
                    "sizeBytes": screenshot_path.stat().st_size,
                    "capturedAtUtc": screenshot_captured,
                    "processId": process_id,
                    "windowHandle": hwnd,
                }],
            }

            result = validator.validate_section_suspension_checkpoint(root, route, process_id, hwnd)

            self.assertTrue(result["passed"])
            self.assertEqual(result["proofKind"], validator.P917_ROSTER_ABSENCE_PROOF_KIND)
            self.assertEqual(result["settledMilliseconds"], 700)
            self.assertFalse(result["modelDestructionClaimedByUia"])

            with self.subTest("500 ms claim with 100 ms timestamp evidence"):
                checkpoint["settledMilliseconds"] = 500
                checkpoint["selectedTabReadbackAtUtc"] = "2026-11-22T06:20:01.100Z"
                checkpoint["activeTreeCapturedAtUtc"] = "2026-11-22T06:20:01.200Z"
                checkpoint["screenshotCapturedAtUtc"] = "2026-11-22T06:20:01.300Z"
                snapshot_payload["capturedAtUtc"] = checkpoint["activeTreeCapturedAtUtc"]
                ui_path.write_text(json.dumps(snapshot_payload), encoding="utf-8")
                route["uiaSnapshots"][0]["sha256"] = hashlib.sha256(ui_path.read_bytes()).hexdigest()
                route["uiaSnapshots"][0]["sizeBytes"] = ui_path.stat().st_size
                route["uiaSnapshots"][0]["capturedAtUtc"] = checkpoint["activeTreeCapturedAtUtc"]
                route["screenshots"][0]["capturedAtUtc"] = checkpoint["screenshotCapturedAtUtc"]
                with self.assertRaisesRegex(ValueError, "timestamps prove less than 500 ms"):
                    validator.validate_section_suspension_checkpoint(root, route, process_id, hwnd)
                checkpoint["settledMilliseconds"] = 700
                checkpoint["selectedTabReadbackAtUtc"] = "2026-11-22T06:20:01.600Z"
                checkpoint["activeTreeCapturedAtUtc"] = captured_at
                checkpoint["screenshotCapturedAtUtc"] = screenshot_captured
                snapshot_payload["capturedAtUtc"] = captured_at
                ui_path.write_text(json.dumps(snapshot_payload), encoding="utf-8")
                route["uiaSnapshots"][0]["sha256"] = hashlib.sha256(ui_path.read_bytes()).hexdigest()
                route["uiaSnapshots"][0]["sizeBytes"] = ui_path.stat().st_size
                route["uiaSnapshots"][0]["capturedAtUtc"] = captured_at
                route["screenshots"][0]["capturedAtUtc"] = screenshot_captured

            with self.subTest("retained nextRosterTable"):
                ui_path.write_text(json.dumps({
                    "name": ui_path.stem,
                    "capturedAtUtc": captured_at,
                    "root": {
                        "processId": process_id,
                        "ownerWindowHandle": hwnd,
                        "ownerWindowProcessId": process_id,
                    },
                    "descendants": [{
                        "automationId": "main.selectedClassRosterScrollArea.qt_scrollarea_viewport.QWidget.nextRosterTable",
                        "controlType": "ControlType.Table",
                        "processId": process_id,
                    }],
                }), encoding="utf-8")
                route["uiaSnapshots"][0]["sha256"] = hashlib.sha256(ui_path.read_bytes()).hexdigest()
                route["uiaSnapshots"][0]["sizeBytes"] = ui_path.stat().st_size
                with self.assertRaisesRegex(ValueError, "still exposes roster controls.*retained nextRosterTable"):
                    validator.validate_section_suspension_checkpoint(root, route, process_id, hwnd)

            route["uiaSnapshots"][0]["sha256"] = hashlib.sha256(ui_path.read_bytes()).hexdigest()
            route["uiaSnapshots"][0]["sizeBytes"] = ui_path.stat().st_size
            ui_path.write_text(json.dumps({
                "name": ui_path.stem,
                "capturedAtUtc": captured_at,
                "root": {
                    "processId": process_id,
                    "ownerWindowHandle": hwnd,
                    "ownerWindowProcessId": process_id,
                },
                "descendants": [],
            }), encoding="utf-8")
            route["uiaSnapshots"][0]["sha256"] = hashlib.sha256(ui_path.read_bytes()).hexdigest()
            route["uiaSnapshots"][0]["sizeBytes"] = ui_path.stat().st_size

            with self.subTest("stale model claim"):
                route["modelCounts"]["suspendedRosterRootRows"] = 100
                with self.assertRaisesRegex(ValueError, "must not claim a stale GridPattern/model row count"):
                    validator.validate_section_suspension_checkpoint(root, route, process_id, hwnd)
                route["modelCounts"]["suspendedRosterRootRows"] = None

            for field, value in (
                ("freshActiveTreeCaptured", False),
                ("proofKind", "widget-model-destroyed"),
                ("activeTreeRosterControlCount", 1),
                ("activeUiAutomationRosterTablePresent", True),
                ("selectedTabReadbackSource", "stale tab object"),
            ):
                with self.subTest(field=field):
                    original = checkpoint[field]
                    checkpoint[field] = value
                    with self.assertRaises(ValueError):
                        validator.validate_section_suspension_checkpoint(root, route, process_id, hwnd)
                    checkpoint[field] = original

    def test_runner_does_not_reuse_the_pre_leave_roster_element_for_grid_readback(self) -> None:
        runner_source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn("function Get-P917SectionRosterAbsenceCheckpoint(", runner_source)
        self.assertIn("$table = $null", runner_source)
        self.assertIn("fresh-active-tree-roster-absence", runner_source)
        self.assertNotIn("retained-table-element-after-model-clear", runner_source)
        self.assertNotIn("$sectionSuspendedTable", runner_source)
        self.assertNotIn("$retainedReturnTable", runner_source)
        self.assertNotIn("$retainedTable = if ($null -ne $activeTable)", runner_source)
        measured_start = runner_source.index("function Invoke-P917MeasuredRosterCycles(")
        measured_end = runner_source.index("\nfunction Invoke-P917Route(", measured_start)
        measured_cycles = runner_source[measured_start:measured_end]
        section_exit_call = measured_cycles.index("Invoke-P917MeasuredCycleSectionExit $Context $Manifest $EvidenceDirectory $cycleTag")
        table_release_index = measured_cycles.rfind("$table = $null", 0, section_exit_call)
        self.assertGreater(table_release_index, -1)
        self.assertLess(measured_cycles.count("\n", table_release_index, section_exit_call), 7)
        section_exit_start = runner_source.index("function Invoke-P917MeasuredCycleSectionExit(")
        section_exit_end = runner_source.index("\nfunction Invoke-P917ClientGcReferenceEvent(", section_exit_start)
        section_exit = runner_source[section_exit_start:section_exit_end]
        self.assertIn("Get-P917SectionRosterAbsenceCheckpoint", section_exit)
        self.assertIn("fresh-active-tree-roster-absence", runner_source)
        self.assertNotIn("Get-P917GridMetrics", measured_cycles[section_exit_call:])
        self.assertIn("$cycleGridRows = [int]$gridMetrics.rowCount", measured_cycles)
        self.assertIn("$cycleGridColumns = [int]$gridMetrics.columnCount", measured_cycles)
        self.assertIn("gridRows = $cycleGridRows", measured_cycles)
        self.assertIn("gridColumns = $cycleGridColumns", measured_cycles)

        for leave_marker in ("$leaveAction = Invoke-P917TabClick $Context $Manifest $EvidenceDirectory 'leave-roster-section'",):
            leave_index = runner_source.index(leave_marker)
            table_release_index = runner_source.rfind("$table = $null", 0, leave_index)
            self.assertGreater(table_release_index, -1)
            self.assertLess(runner_source.count("\n", table_release_index, leave_index), 3)
            checkpoint_index = runner_source.index("Get-P917SectionRosterAbsenceCheckpoint", leave_index)
            self.assertNotIn("Get-P917GridMetrics", runner_source[leave_index:checkpoint_index])
        measured_call = runner_source.index("Invoke-P917MeasuredRosterCycles $Context")
        caller_release_start = runner_source.rfind("$classList = $null", 0, measured_call)
        self.assertGreater(caller_release_start, -1)
        caller_release = runner_source[caller_release_start:measured_call]
        for released_reference in (
            "$classList = $null",
            "$targetSearch = $null",
            "$listItem = $null",
            "$selectedPattern = $null",
            "$detailsTab = $null",
            "$rosterTab = $null",
            "$gridMetrics = $null",
            "$reentryTargetSearch = $null",
            "$reentrySelectionPattern = $null",
            "$reentryMetrics = $null",
            "$Context.mainWindowRoot = $null",
            "$Context.dialogRoot = $null",
        ):
            self.assertIn(released_reference, caller_release)
        self.assertNotIn("$CurrentTable", runner_source)
        self.assertIn("$baselineLeaveAction = Invoke-P917TabClick", runner_source)

    def test_class_selection_requires_the_exact_fixture_derived_visible_list_item(self) -> None:
        process_id = 2317
        hwnd = 0x01234567
        accessible_name = "E4 Level 02 • M/F 5:00; M/T 6:00; T/W 3:00; W/Th 4:00 • Teacher 01"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            snapshot_path = root / "uia" / "class-list-items-target.json"
            snapshot_path.parent.mkdir(parents=True)
            snapshot_data = {
                "name": "class-list-items-target-search-01",
                "classList": {
                    "controlType": "ControlType.List",
                    "processId": process_id,
                    "ownerWindowHandle": hwnd,
                    "rectangle": {"x": 80, "y": 50, "width": 220, "height": 500},
                },
                "itemCount": 1,
                "items": [
                    {
                        "name": accessible_name,
                        "automationId": "QApplication.classMngrNextApplicationShell.classesNavigatorClassList",
                        "controlType": "ControlType.ListItem",
                        "processId": process_id,
                        "isEnabled": True,
                        "isOffscreen": False,
                        "rectangle": {"x": 100, "y": 100, "width": 300, "height": 40},
                    }
                ],
            }
            raw = (json.dumps(snapshot_data, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            snapshot_path.write_bytes(raw)
            snapshot_record = {
                "relativePath": "uia/class-list-items-target.json",
                "sha256": hashlib.sha256(raw).hexdigest(),
                "sizeBytes": len(raw),
                "processId": process_id,
                "windowHandle": hwnd,
            }
            scroll_action = {
                "actionKind": "scroll-class-list-for-target",
                "method": "verified-window-message-wheel",
                "target": {
                    "automationId": "QApplication.classMngrNextApplicationShell.classesNavigatorClassList",
                    "controlType": "ControlType.List",
                    "processId": process_id,
                    "ownerWindowHandle": hwnd,
                },
                "scrollInput": {
                    "windowHandle": hwnd,
                    "expectedProcessId": process_id,
                    "windowProcessId": process_id,
                    "wheelDelta": -120,
                    "waitForInputIdleSucceeded": True,
                    "messageSent": True,
                    "visibleRowsChanged": True,
                },
            }
            select_action = {
                "actionKind": "select-target-class",
                "selectedClassId": 1,
                "selectedClassName": "Heavy Fixture Class 001",
                "selectedListItemName": accessible_name,
                "selectionItemPatternIsSelected": True,
                "target": {
                    "name": accessible_name,
                    "controlType": "ControlType.ListItem",
                    "processId": process_id,
                    "ownerWindowHandle": hwnd,
                    "rectangle": {"x": 100, "y": 100, "width": 300, "height": 40},
                },
                "pointerAction": {
                    "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                    "screenX": 200,
                    "screenY": 120,
                    "windowHandle": hwnd,
                    "windowProcessId": process_id,
                    "targetProcessId": process_id,
                    "clickRegion": {
                        "strategy": "intersection-of-target-and-visible-classes-list-bounds",
                        "targetRectangle": {"x": 100, "y": 100, "width": 300, "height": 40},
                        "clippingRectangle": {"x": 80, "y": 50, "width": 220, "height": 500},
                        "clickRectangle": {"x": 100, "y": 100, "width": 200, "height": 40},
                        "clippingControl": {
                            "controlType": "ControlType.List",
                            "processId": process_id,
                            "ownerWindowHandle": hwnd,
                        },
                    },
                },
            }
            route = {
                "targetClassSelection": {
                    "selected": True,
                    "classId": 1,
                    "className": "Heavy Fixture Class 001",
                    "listItemName": accessible_name,
                    "expectedAccessibleName": accessible_name,
                    "selectionProperty": "Exact UIA ListItem.Name derived from fixture class id 1",
                    "actionIndex": 1,
                },
                "targetClassSearch": {
                    "className": "Heavy Fixture Class 001",
                    "targetAccessibleName": accessible_name,
                    "selectionProperty": "Exact UIA ListItem.Name",
                    "maximumScrolls": 40,
                    "matched": True,
                    "pages": [
                        {"page": 0, "snapshotRelativePath": "uia/class-list-items-before.json", "matchingAccessibleNameCount": 0},
                        {"page": 1, "snapshotRelativePath": "uia/class-list-items-target.json", "matchingAccessibleNameCount": 1},
                    ],
                },
            }
            receipt = {
                "identity": {
                    "targetClassId": 1,
                    "targetClassName": "Heavy Fixture Class 001",
                    "targetClassListAccessibleName": accessible_name,
                }
            }

            result = validator.validate_class_selection_evidence(
                root,
                route,
                receipt,
                [scroll_action, select_action],
                [snapshot_record],
                process_id,
                hwnd,
            )

            self.assertTrue(result["passed"])
            self.assertEqual(result["visibleMatchPage"], 1)
            self.assertEqual(result["scrollActionCount"], 1)

            select_action["pointerAction"]["screenX"] = 350
            with self.assertRaisesRegex(ValueError, "outside the visible ListItem"):
                validator.validate_class_selection_evidence(
                    root,
                    route,
                    receipt,
                    [scroll_action, select_action],
                    [snapshot_record],
                    process_id,
                    hwnd,
                )
            select_action["pointerAction"]["screenX"] = 200

            route["targetClassSelection"]["listItemName"] = "Different row"
            with self.assertRaisesRegex(ValueError, "Exact target class selection evidence"):
                validator.validate_class_selection_evidence(
                    root,
                    route,
                    receipt,
                    [scroll_action, select_action],
                    [snapshot_record],
                    process_id,
                    hwnd,
                )

    def _prepared_workspace(self, root: Path):
        workspace = root / "large-roster.tps"
        receipt_path = root / "preparation-receipt.json"
        receipt = fixture.prepare(BASE, SEED, workspace)
        receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
        before = fixture.snapshot("before", workspace, receipt_path)
        return workspace, receipt_path, receipt, before

    def _write_all_target_edits(self, workspace: Path, receipt: dict) -> None:
        connection = sqlite3.connect(workspace)
        try:
            for target in receipt["expectedEditTargets"]:
                connection.execute(
                    "UPDATE roster_data SET value=? WHERE class_id=? AND row_index=? AND col_index=?",
                    (
                        target["afterValue"], target["classId"], target["row"], target["storedColumn"],
                    ),
                )
            connection.commit()
        finally:
            connection.close()

    def _minimal_snapshot_pair(self):
        receipt = {
            "expectedEditTargets": [
                {"classId": 1, "row": 0, "storedColumn": 6, "beforeValue": "before-first", "afterValue": "after-first"},
                {"classId": 1, "row": 49, "storedColumn": 77, "beforeValue": "before-middle", "afterValue": "after-middle"},
                {"classId": 1, "row": 99, "storedColumn": 155, "beforeValue": "before-last", "afterValue": "after-last"},
            ]
        }
        before_tables = {
            "roster_columns": {
                "columns": ["id", "class_id", "name", "position", "width"],
                "declaration": [],
                "rowCount": 5,
                "rows": [
                    [1, 1, "Middle", 2, 160],
                    [2, 1, "Front", 1, 120],
                    [3, 1, "Tail", 3, 180],
                    [4, 2, "Other Front", 1, 100],
                    [5, 2, "Other Tail", 2, 140],
                ],
            },
            "roster_data": {
                "columns": ["class_id", "row_index", "col_index", "value"],
                "declaration": [],
                "rowCount": 3,
                "rows": [
                    [1, 0, 6, "before-first"],
                    [1, 49, 77, "before-middle"],
                    [1, 99, 155, "before-last"],
                ],
            },
            "classes": {
                "columns": ["id", "name"],
                "declaration": [],
                "rowCount": 1,
                "rows": [[1, "Fixture Class"]],
            },
        }
        before = {
            "schema": validator.FIXTURE_SCHEMA,
            "phase": "before",
            "status": "passed",
            "integrityCheck": "ok",
            "foreignKeyErrors": [],
            "schemaObjects": [["table", name] for name in before_tables],
            "workspaceTableCount": len(before_tables),
            "workspaceTables": before_tables,
        }
        after = json.loads(json.dumps(before))
        after["phase"] = "after"
        after["workspaceTables"]["roster_columns"]["rows"] = [
            [4, 2, "Other Front", 1, 100],
            [5, 2, "Other Tail", 2, 140],
            [101, 1, "Tail", 3, 180],
            [102, 1, "Front", 1, 120],
            [103, 1, "Middle", 2, 160],
        ]
        after["workspaceTables"]["roster_data"]["rows"] = [
            [1, 0, 6, "after-first"],
            [1, 49, 77, "after-middle"],
            [1, 99, 155, "after-last"],
        ]
        return receipt, before, after

    def _regenerate_target_roster_column_ids(self, workspace: Path, class_id: int):
        connection = sqlite3.connect(workspace)
        try:
            descriptors = list(
                connection.execute(
                    "SELECT name,position,width FROM roster_columns WHERE class_id=? ORDER BY position,id",
                    (class_id,),
                )
            )
            old_ids = [
                int(row[0])
                for row in connection.execute(
                    "SELECT id FROM roster_columns WHERE class_id=? ORDER BY position,id", (class_id,)
                )
            ]
            connection.execute("DELETE FROM roster_columns WHERE class_id=?", (class_id,))
            connection.executemany(
                "INSERT INTO roster_columns (class_id,name,position,width) VALUES (?,?,?,?)",
                [(class_id, name, position, width) for name, position, width in descriptors],
            )
            new_ids = [
                int(row[0])
                for row in connection.execute(
                    "SELECT id FROM roster_columns WHERE class_id=? ORDER BY position,id", (class_id,)
                )
            ]
            connection.commit()
            return old_ids, new_ids
        finally:
            connection.close()

    def test_full_workspace_diff_accepts_regenerated_target_column_ids_and_three_target_values(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace, receipt_path, receipt, before = self._prepared_workspace(root)
            old_ids, new_ids = self._regenerate_target_roster_column_ids(
                workspace, receipt["identity"]["targetClassId"]
            )
            self.assertFalse(set(old_ids).intersection(new_ids))
            self._write_all_target_edits(workspace, receipt)
            after = fixture.snapshot("after", workspace, receipt_path)

            result = validator.validate_snapshot_pair(receipt, before, after)

            self.assertTrue(result["passed"])
            self.assertTrue(result["onlyTargetCellValuesChanged"])
            self.assertTrue(result["targetRosterColumnDescriptorsUnchanged"])
            self.assertEqual(result["changedCellKeys"], [[1, 0, 6], [1, 49, 77], [1, 99, 155]])

    def test_snapshot_diff_rejects_target_roster_column_descriptor_changes(self) -> None:
        cases = (
            ("name", "Target roster_columns descriptors changed."),
            ("position", "Target roster_columns descriptors changed."),
            ("width", "Target roster_columns descriptors changed."),
            ("order", "Target roster_columns descriptors changed."),
            ("count", "Row count changed in table roster_columns."),
        )
        for change, message in cases:
            with self.subTest(change=change):
                receipt, before, after = self._minimal_snapshot_pair()
                rows = after["workspaceTables"]["roster_columns"]["rows"]
                target_rows = [row for row in rows if row[1] == 1]
                if change == "name":
                    target_rows[0][2] = "Renamed"
                elif change == "position":
                    target_rows[1][3] = 5
                elif change == "width":
                    target_rows[2][4] = 181
                elif change == "order":
                    target_rows[0][3], target_rows[1][3] = target_rows[1][3], target_rows[0][3]
                else:
                    rows.remove(target_rows[0])
                    after["workspaceTables"]["roster_columns"]["rowCount"] -= 1

                with self.assertRaisesRegex(ValueError, message):
                    validator.validate_snapshot_pair(receipt, before, after)

    def test_snapshot_diff_rejects_non_target_roster_column_id_changes(self) -> None:
        receipt, before, after = self._minimal_snapshot_pair()
        non_target_row = next(
            row for row in after["workspaceTables"]["roster_columns"]["rows"] if row[1] == 2
        )
        non_target_row[0] = 40

        with self.assertRaisesRegex(ValueError, "Non-target roster_columns rows changed"):
            validator.validate_snapshot_pair(receipt, before, after)

    def test_snapshot_diff_rejects_unexpected_roster_data_value_changes(self) -> None:
        receipt, before, after = self._minimal_snapshot_pair()
        after["workspaceTables"]["roster_data"]["rows"][0][3] = "unexpected edit"

        with self.assertRaisesRegex(ValueError, "Target persisted value differs"):
            validator.validate_snapshot_pair(receipt, before, after)

    def test_full_workspace_diff_rejects_changes_outside_the_cell_allowlist(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace, receipt_path, receipt, before = self._prepared_workspace(root)
            self._write_all_target_edits(workspace, receipt)
            connection = sqlite3.connect(workspace)
            try:
                connection.execute("UPDATE classes SET name='Unexpected' WHERE id=2")
                connection.commit()
            finally:
                connection.close()
            after = fixture.snapshot("after", workspace, receipt_path)

            with self.assertRaisesRegex(ValueError, "Rows changed outside targeted roster data: classes"):
                validator.validate_snapshot_pair(receipt, before, after)

    def test_cycle_contract_checks_headers_values_and_process_binding(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence_root = Path(directory)
            contract_path = evidence_root / "uia" / "cycle-01-roster-loaded-contract.json"
            contract_path.parent.mkdir(parents=True)
            fixture_targets = {
                name: {"name": name, "row": index * 10, "modelColumn": index * 20 + 6, "storedColumn": index * 20 + 6}
                for index, name in enumerate(("start", "middle", "end"))
            }
            expected_values = {name: f"value-{index}" for index, name in enumerate(fixture_targets)}
            expected_readbacks = [
                {**target, "expected": expected_values[name], "actual": expected_values[name], "matched": True}
                for name, target in fixture_targets.items()
            ]
            contract = {
                "schema": "classmngr-phase9-p9-17-roster-uia-contract-v1",
                "name": "cycle-01-roster-loaded-contract",
                "processId": 2317,
                "mainWindowHandle": 0x01234567,
                "grid": {"rowCount": 100, "columnCount": 156},
                "headers": [
                    {"column": index, "text": value}
                    for index, value in enumerate(validator.expected_roster_headers())
                ],
                "observedValues": expected_readbacks,
            }

            def write_contract() -> dict[str, object]:
                contract_path.write_text(json.dumps(contract), encoding="utf-8")
                return {
                    "relativePath": "uia/cycle-01-roster-loaded-contract.json",
                    "sha256": hashlib.sha256(contract_path.read_bytes()).hexdigest(),
                    "sizeBytes": contract_path.stat().st_size,
                }

            snapshot_record = write_contract()
            validator.validate_cycle_uia_contract(
                evidence_root, snapshot_record, "cycle-01-roster-loaded-contract", 2317, 0x01234567,
                fixture_targets, expected_values, expected_readbacks,
            )

            contract["headers"][-1]["text"] = "wrong-header"
            snapshot_record = write_contract()
            with self.assertRaisesRegex(ValueError, "156 ordered headers"):
                validator.validate_cycle_uia_contract(
                    evidence_root, snapshot_record, "cycle-01-roster-loaded-contract", 2317, 0x01234567,
                    fixture_targets, expected_values, expected_readbacks,
                )

    def test_cycle_contract_rejects_mutated_fixture_coordinates(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence_root = Path(directory)
            contract_path = evidence_root / "uia" / "cycle-01-edited-contract.json"
            contract_path.parent.mkdir(parents=True)
            fixture_targets = {
                name: {"name": name, "row": index, "modelColumn": index + 7, "storedColumn": index + 7}
                for index, name in enumerate(("start", "middle", "end"))
            }
            expected_values = {name: f"after-{name}" for name in fixture_targets}
            route_readbacks = [
                {**target, "expected": expected_values[name], "actual": expected_values[name], "matched": True}
                for name, target in fixture_targets.items()
            ]
            contract = {
                "schema": "classmngr-phase9-p9-17-roster-uia-contract-v1",
                "name": "cycle-01-edited-contract",
                "processId": 2317,
                "mainWindowHandle": 0x01234567,
                "grid": {"rowCount": 100, "columnCount": 156},
                "headers": [
                    {"column": index, "text": value}
                    for index, value in enumerate(validator.expected_roster_headers())
                ],
                "observedValues": [dict(item) for item in route_readbacks],
            }

            def write_contract() -> dict[str, object]:
                contract_path.write_text(json.dumps(contract), encoding="utf-8")
                return {
                    "relativePath": "uia/cycle-01-edited-contract.json",
                    "sha256": hashlib.sha256(contract_path.read_bytes()).hexdigest(),
                    "sizeBytes": contract_path.stat().st_size,
                }

            for source in ("contract", "route"):
                for field in ("row", "modelColumn", "storedColumn"):
                    with self.subTest(source=source, field=field):
                        corrupted_route = [dict(item) for item in route_readbacks]
                        if source == "contract":
                            contract["observedValues"][0][field] = 999
                        else:
                            corrupted_route[0][field] = 999
                        snapshot_record = write_contract()
                        with self.assertRaisesRegex(ValueError, "mutated fixture coordinate"):
                            validator.validate_cycle_uia_contract(
                                evidence_root, snapshot_record, "cycle-01-edited-contract", 2317, 0x01234567,
                                fixture_targets, expected_values, corrupted_route,
                            )
                        contract["observedValues"] = [dict(item) for item in route_readbacks]

    def test_final_cycle_contract_is_required_and_checks_after_values_and_coordinates(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            evidence_root = Path(directory)
            contract_path = evidence_root / "uia" / "cycle-final-reentry-contract.json"
            contract_path.parent.mkdir(parents=True)
            fixture_targets = {
                name: {"name": name, "row": index * 9, "modelColumn": index * 11 + 6, "storedColumn": index * 11 + 6}
                for index, name in enumerate(("start", "middle", "end"))
            }
            after_values = {name: f"final-after-{name}" for name in fixture_targets}
            final_readbacks = [
                {**target, "expected": after_values[name], "actual": after_values[name], "matched": True}
                for name, target in fixture_targets.items()
            ]
            contract = {
                "schema": "classmngr-phase9-p9-17-roster-uia-contract-v1",
                "name": "cycle-final-reentry-contract",
                "processId": 2317,
                "mainWindowHandle": 0x01234567,
                "grid": {"rowCount": 100, "columnCount": 156},
                "headers": [
                    {"column": index, "text": value}
                    for index, value in enumerate(validator.expected_roster_headers())
                ],
                "observedValues": [dict(item) for item in final_readbacks],
            }
            contract_path.write_text(json.dumps(contract), encoding="utf-8")
            snapshot_record = {
                "relativePath": "uia/cycle-final-reentry-contract.json",
                "sha256": hashlib.sha256(contract_path.read_bytes()).hexdigest(),
                "sizeBytes": contract_path.stat().st_size,
            }
            snapshots_by_path = {snapshot_record["relativePath"]: snapshot_record}

            validator.validate_final_cycle_uia_contract(
                evidence_root, snapshots_by_path, snapshot_record["relativePath"], 2317, 0x01234567,
                fixture_targets, after_values, final_readbacks,
            )
            with self.assertRaisesRegex(ValueError, "path is missing"):
                validator.validate_final_cycle_uia_contract(
                    evidence_root, snapshots_by_path, None, 2317, 0x01234567,
                    fixture_targets, after_values, final_readbacks,
                )

            contract["observedValues"][0]["row"] = 999
            contract_path.write_text(json.dumps(contract), encoding="utf-8")
            snapshots_by_path[snapshot_record["relativePath"]] = {
                **snapshot_record,
                "sha256": hashlib.sha256(contract_path.read_bytes()).hexdigest(),
                "sizeBytes": contract_path.stat().st_size,
            }
            with self.assertRaisesRegex(ValueError, "mutated fixture coordinate"):
                validator.validate_final_cycle_uia_contract(
                    evidence_root, snapshots_by_path, snapshot_record["relativePath"], 2317, 0x01234567,
                    fixture_targets, after_values, final_readbacks,
                )

    def _cycle_memory_fixture(self, plateau_baselines=None, cycle_returns=None,
                              initial_baseline_values=None, initial_return_value=100_000_000):
        process_id = 2317
        hwnd = 0x01234567
        windows = []
        base_time = datetime(2026, 1, 1, tzinfo=timezone.utc)

        def stamp(seconds):
            return (base_time + timedelta(seconds=seconds)).isoformat(timespec="milliseconds").replace("+00:00", "Z")

        actions = [
            {"actionKind": "select-target-class", "completedAtUtc": stamp(1), "issuedAtUtc": stamp(0.9)},
            {"actionKind": "open-roster-tab", "issuedAtUtc": stamp(4), "completedAtUtc": stamp(4.1)},
            {"actionKind": "leave-roster-section", "issuedAtUtc": stamp(8), "completedAtUtc": stamp(8.1)},
            {"actionKind": "navigate-my_workspace", "issuedAtUtc": stamp(10), "completedAtUtc": stamp(10.1)},
            {"actionKind": "reenter-roster-tab", "issuedAtUtc": stamp(14), "completedAtUtc": stamp(14.1)},
            {"actionKind": "cycle-baseline-select-class-details", "issuedAtUtc": stamp(20), "completedAtUtc": stamp(21)},
        ]

        for cycle_number in range(1, 11):
            cycle_base = 50 + (cycle_number - 1) * 30
            actions.append({
                "actionKind": f"cycle-{cycle_number:02d}-open-roster",
                "issuedAtUtc": stamp(cycle_base),
                "completedAtUtc": stamp(cycle_base + 0.1),
            })
            actions.append({
                "actionKind": f"cycle-{cycle_number:02d}-leave-roster",
                "issuedAtUtc": stamp(cycle_base + 8),
                "completedAtUtc": stamp(cycle_base + 8.1),
            })
        actions.append({
            "actionKind": "cycle-final-reenter-roster",
            "issuedAtUtc": stamp(340),
            "completedAtUtc": stamp(340.1),
        })

        def add_window(window_id, values, *, phase="normal-use", state="roster-state", matched_state=None, cycle_number=None):
            target_exceeded = [index for index, value in enumerate(values) if value >= validator.WORKING_SET_TARGET]
            ceiling_exceeded = [index for index, value in enumerate(values) if value > validator.WORKING_SET_CEILING]
            if window_id == "matched-baseline":
                start_seconds = 2
            elif window_id == "matched-after-suspension":
                start_seconds = 8.5
            elif window_id == "re-entry":
                start_seconds = 16
            elif window_id.startswith("plateau-baseline-"):
                start_seconds = 25 + (int(window_id.rsplit("-", 1)[1]) - 1) * 3
            elif window_id.startswith("cycle-") and window_id.endswith("-return"):
                cycle_number_from_id = int(window_id.split("-")[1])
                start_seconds = 60 + (cycle_number_from_id - 1) * 30
            elif window_id == "cycle-final-reentry":
                start_seconds = 350
            else:
                start_seconds = len(windows) * 2
            window_started_at = stamp(start_seconds)
            window_completed_at = stamp(start_seconds + 1)
            samples = [
                {
                    "sampleIndex": index,
                    "elapsedMilliseconds": index * 100,
                    "capturedAtUtc": stamp(start_seconds + index / 10),
                    "workingSetBytes": value,
                    "privateBytes": value + 1_000_000,
                    "peakWorkingSetBytes": value + 2_000_000,
                    "handles": 500,
                    "threadCount": 30,
                    "processState": "running",
                    "metricErrors": [],
                }
                for index, value in enumerate(values)
            ]
            window = {
                "windowId": window_id,
                "processId": process_id,
                "mainWindowHandle": hwnd,
                "state": state,
                "phase": phase,
                "matchedState": matched_state,
                "startedAtUtc": window_started_at,
                "completedAtUtc": window_completed_at,
                "sampleCount": len(samples),
                "sampleIntervalMilliseconds": 100,
                "samplingCompleted": True,
                "samplingErrorCount": 0,
                "samplingErrors": [],
                "workingSetTargetBytes": validator.WORKING_SET_TARGET,
                "workingSetTargetExclusive": True,
                "workingSetTargetMet": not target_exceeded,
                "workingSetTargetExceededSampleIndexes": target_exceeded,
                "workingSetCeilingBytes": validator.WORKING_SET_CEILING,
                "workingSetCeilingExclusive": False,
                "workingSetCeilingPassed": not ceiling_exceeded,
                "workingSetCeilingExceededSampleIndexes": ceiling_exceeded,
                "samples": samples,
            }
            if window_id.startswith("plateau-baseline-") or window_id.startswith("cycle-"):
                window["settleDelaySeconds"] = 2
            if cycle_number is not None:
                window["cycleNumber"] = cycle_number
            windows.append(window)

        required_ids = (
            "startup-ready", "workspace-entry", "class-page-entry", "roster-entry", "post-edit",
            "post-save", "matched-baseline", "matched-after-suspension", "page-suspended", "re-entry",
        )
        cold_baseline_values = initial_baseline_values or [100_000_000] * 10
        for window_id in required_ids:
            value = 100_000_000
            if window_id == "matched-after-suspension":
                values = [initial_return_value] * 10
            elif window_id == "matched-baseline":
                values = cold_baseline_values
            else:
                values = [value] * 10
            if window_id == "roster-entry":
                value = validator.WORKING_SET_TARGET + 1
                values = [value] * 10
            add_window(window_id, values, matched_state="classes-details-selected-class")

        baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
        baseline_values = plateau_baselines or [120_000_000] * 5
        for window_id, value in zip(baseline_ids, baseline_values):
            add_window(window_id, [value] * 10, phase="baseline", state="classes-details-selected",
                       matched_state="classes-details-selected-class")

        cycle_window_ids = []
        cycle_records = []
        return_values = cycle_returns or [120_000_000] * 10
        for cycle_number, return_value in enumerate(return_values, start=1):
            tag = f"{cycle_number:02d}"
            loaded_id = f"cycle-{tag}-roster-loaded"
            save_id = f"cycle-{tag}-post-save"
            return_id = f"cycle-{tag}-return"
            add_window(loaded_id, [270_000_000] * 10, phase="cycle-loaded", cycle_number=cycle_number)
            add_window(save_id, [280_000_000] * 10, phase="cycle-post-save", cycle_number=cycle_number)
            add_window(return_id, [return_value] * 10, phase="cycle", state="classes-details-selected-after-roster-clear",
                       matched_state="classes-details-selected-class", cycle_number=cycle_number)
            cycle_window_ids.extend((loaded_id, save_id, return_id))
            before_value = "after" if cycle_number % 2 else "before"
            after_value = "before" if cycle_number % 2 else "after"
            readbacks = lambda value: [
                {"name": name, "actual": value, "matched": True}
                for name in ("start", "middle", "end")
            ]
            cycle_records.append({
                "schema": "classmngr-phase9-p9-17-repeated-roster-cycle-v1",
                "cycleNumber": cycle_number,
                "targetClassId": 1,
                "targetClassName": "Heavy Fixture Class 001",
                "selectedClassAccessibleName": "fixture-class-accessible-name",
                "classSelectionVerified": True,
                "parity": "after-to-before" if cycle_number % 2 else "before-to-after",
                "rosterLoadedWindowId": loaded_id,
                "postSaveWindowId": save_id,
                "postSuspensionWindowId": return_id,
                "initialReadbacks": readbacks(before_value),
                "editedReadbacks": readbacks(after_value),
                "savedReadbacks": readbacks(after_value),
                "gridRows": 100,
                "gridColumns": 156,
                "verifiedHeaderCount": 156,
                "saveActionIndex": 3,
                "suspensionCheckpoint": {
                    "leaveActionIndex": 7 + (cycle_number - 1) * 2,
                    "observedAtUtc": stamp(59 + (cycle_number - 1) * 30),
                    "screenshotCapturedAtUtc": stamp(59.2 + (cycle_number - 1) * 30),
                    "selectedSection": "Class Details and Times",
                    "selectedTabVerified": True,
                    "activeUiAutomationRosterTablePresent": False,
                    "freshActiveTreeCaptured": True,
                    "proofKind": validator.P917_ROSTER_ABSENCE_PROOF_KIND,
                    "activeTreeRosterControlCount": 0,
                },
            })
        add_window("cycle-final-reentry", [280_000_000] * 10, phase="final-reentry")
        cycle_window_ids.append("cycle-final-reentry")

        cold_baseline_range = max(cold_baseline_values) - min(cold_baseline_values)
        cold_baseline_median = validator.median(cold_baseline_values)
        initial_return_median = float(initial_return_value)
        initial_allowance_ceiling = cold_baseline_median + cold_baseline_range
        memory = {
            "workingSetTargetBytes": validator.WORKING_SET_TARGET,
            "workingSetTargetExclusive": True,
            "workingSetCeilingBytes": validator.WORKING_SET_CEILING,
            "workingSetCeilingExclusive": False,
            "windows": windows,
            "initialPrimingReturnAllowance": {
                "gating": True,
                "observedBaselineRangeBytes": cold_baseline_range,
                "baselineMedianWorkingSetBytes": cold_baseline_median,
                "postSuspensionMedianWorkingSetBytes": initial_return_median,
                "maximumAllowedPostSuspensionMedianBytes": initial_allowance_ceiling,
                "passed": initial_return_median <= initial_allowance_ceiling,
            },
            "plateauBaselineWindowIds": baseline_ids,
            "plateauCycleWindowIds": cycle_window_ids,
            "operationPeakWorkingSetBytes": 300_000_000,
            "operationPeakAdvisoryLimitBytes": validator.OPERATION_PEAK_ADVISORY_LIMIT,
        }
        route = {
            "primingCycleCount": 1,
            "baselineWindowsPerState": 5,
            "measuredCycleCount": 10,
            "cycleEvidenceSchema": "classmngr-phase9-p9-17-repeated-roster-cycle-v1",
            "repeatedCycleProtocol": "One priming cycle, five no-action baselines, and ten measured cycles.",
            "cycleRecords": cycle_records,
            "actions": actions,
            "sectionSuspensionActionIndex": 2,
            "sectionSuspensionCheckpoint": {
                "leaveActionIndex": 2,
                "observedAtUtc": stamp(8.2),
                "screenshotCapturedAtUtc": stamp(8.3),
            },
            "postPrimingClearCheckpoint": {"observedAtUtc": stamp(22), "screenshotCapturedAtUtc": stamp(22.2)},
            "finalCycleReadback": [
                {"name": name, "actual": "after", "matched": True}
                for name in ("start", "middle", "end")
            ],
        }
        return memory, route, process_id, hwnd

    def test_memory_gate_reports_initial_cleanup_comparison_and_uses_plateau_cycles(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        result = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertTrue(result["passed"])
        self.assertTrue(result["initialPrimingReturnAllowance"]["gating"])
        self.assertTrue(result["initialPrimingReturnAllowance"]["recordedGating"])
        self.assertTrue(result["initialPrimingReturnAllowance"]["passed"])
        self.assertEqual(result["initialPrimingReturnAllowance"]["initialUpliftBytes"], 0)
        self.assertTrue(result["plateauCycleAnalysis"]["passed"])
        self.assertEqual(result["measuredCycleCount"], 10)
        self.assertFalse(result["allWindowsMetTarget"])
        self.assertIn("cycle-01-roster-loaded", result["windowsAboveTarget"])

    def test_initial_cleanup_return_at_allowance_equality_passes(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture(
            initial_baseline_values=[100_000_000] * 5 + [101_000_000] * 5,
            initial_return_value=101_500_000,
        )
        result = validator.validate_memory(memory, route, process_id, hwnd)

        allowance = result["initialPrimingReturnAllowance"]
        self.assertTrue(allowance["gating"])
        self.assertTrue(allowance["passed"])
        self.assertEqual(allowance["coldBaselineMedianWorkingSetBytes"], 100_500_000)
        self.assertEqual(allowance["coldBaselineRangeBytes"], 1_000_000)
        self.assertEqual(allowance["maximumAllowedColdReturnBytes"], 101_500_000)
        self.assertEqual(allowance["initialPostSuspensionMedianWorkingSetBytes"], 101_500_000)

    def test_initial_cleanup_return_one_byte_above_allowance_fails(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture(
            initial_baseline_values=[100_000_000] * 5 + [101_000_000] * 5,
            initial_return_value=101_500_001,
        )
        result = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertFalse(result["passed"])
        self.assertFalse(result["cleanupGateResults"]["coldBaselineToFirstReturn"]["passed"])
        with self.assertRaisesRegex(ValueError, "coldBaselineToFirstReturn.passed=False.*empirical allowance"):
            validator.require_memory_cleanup_gates_passed(result)

    def test_initial_cleanup_comparison_must_be_recorded_as_a_gate(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        memory["initialPrimingReturnAllowance"]["gating"] = False

        with self.assertRaisesRegex(ValueError, "must be recorded as a required gate"):
            validator.validate_memory(memory, route, process_id, hwnd)

    def test_runner_records_the_required_cold_cleanup_comparison(self) -> None:
        runner = RUNNER_PATH.read_text(encoding="utf-8")

        self.assertIn("gating = $true", runner)
        self.assertIn("initialPrimingReturnAllowance = $null; initialPrimingReturnGating = $true", runner)
        self.assertIn(
            "measurementRole = 'required-cold-baseline-to-first-return cleanup gate, evaluated separately from the post-priming plateau and measured cycles'",
            runner,
        )
        self.assertIn("$comparisonPassed = $returnMedian -le $ceiling", runner)
        self.assertIn("passed = $comparisonPassed", runner)
        self.assertIn("failureReasons = $comparisonFailureReasons.ToArray()", runner)
        self.assertIn("evidenceFailureReasons = $evidenceFailureReasons.ToArray()", runner)

    def test_memory_gate_fails_unstable_plateau_and_upward_cycle_growth(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture(
            plateau_baselines=[100_000_000, 300_000_000, 100_000_000, 300_000_000, 100_000_000]
        )
        unstable = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertFalse(unstable["plateauCycleAnalysis"]["passed"])
        with self.assertRaisesRegex(ValueError, "postPrimingPlateauAndCycleReturns.passed=False.*baseline is unstable/inconclusive"):
            validator.require_memory_cleanup_gates_passed(unstable)

        increasing = [119_000_000 + int(index * 123_000) for index in range(10)]
        memory, route, process_id, hwnd = self._cycle_memory_fixture(
            plateau_baselines=[119_000_000, 120_000_000, 121_000_000, 120_000_000, 120_000_000],
            cycle_returns=increasing,
        )
        trending = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertFalse(trending["plateauCycleAnalysis"]["passed"])
        with self.assertRaisesRegex(ValueError, "postPrimingPlateauAndCycleReturns.passed=False.*trend"):
            validator.require_memory_cleanup_gates_passed(trending)

    def test_cleanup_windows_are_bound_to_their_lifecycle_intervals(self) -> None:
        cases = (
            ("matched-baseline", 0, 1, "Cold matched-baseline window falls outside"),
            ("matched-after-suspension", 10.2, 11.2, "Cold suspension-return window falls outside"),
            ("plateau-baseline-01", 21, 22, "Post-priming baseline window plateau-baseline-01 falls outside"),
            ("cycle-01-return", 81, 82, "Cycle 1 return window falls outside"),
        )
        for window_id, start_seconds, end_seconds, expected_error in cases:
            with self.subTest(window_id=window_id):
                memory, route, process_id, hwnd = self._cycle_memory_fixture()
                window = next(item for item in memory["windows"] if item["windowId"] == window_id)
                window["startedAtUtc"] = self._cycle_memory_fixture_stamp(start_seconds)
                window["completedAtUtc"] = self._cycle_memory_fixture_stamp(end_seconds)
                for index, sample in enumerate(window["samples"]):
                    sample["capturedAtUtc"] = self._cycle_memory_fixture_stamp(start_seconds + index / 10)
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_memory(memory, route, process_id, hwnd)

    def test_checkpoint_screenshot_fallback_bounds_cold_and_post_priming_windows(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        route["sectionSuspensionCheckpoint"].pop("observedAtUtc")
        route["postPrimingClearCheckpoint"].pop("observedAtUtc")
        self.assertTrue(validator.validate_memory(memory, route, process_id, hwnd)["passed"])

        for checkpoint_key, window_id, screenshot_seconds, expected_error in (
            ("sectionSuspensionCheckpoint", "matched-after-suspension", 8.6, "Cold suspension-return window falls outside"),
            ("postPrimingClearCheckpoint", "plateau-baseline-01", 26, "Post-priming baseline window plateau-baseline-01 falls outside"),
        ):
            with self.subTest(checkpoint=checkpoint_key):
                memory, route, process_id, hwnd = self._cycle_memory_fixture()
                checkpoint = route[checkpoint_key]
                checkpoint.pop("observedAtUtc")
                checkpoint["screenshotCapturedAtUtc"] = self._cycle_memory_fixture_stamp(screenshot_seconds)
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_memory(memory, route, process_id, hwnd)

        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        cycle_checkpoint = route["cycleRecords"][0]["suspensionCheckpoint"]
        cycle_checkpoint.pop("observedAtUtc")
        cycle_checkpoint["screenshotCapturedAtUtc"] = self._cycle_memory_fixture_stamp(90.1)
        with self.assertRaisesRegex(ValueError, "Cycle 1 return window falls outside"):
            validator.validate_memory(memory, route, process_id, hwnd)

    def test_later_screenshot_timestamp_is_the_checkpoint_bound(self) -> None:
        for checkpoint_key, window_id, screenshot_seconds, expected_error in (
            ("sectionSuspensionCheckpoint", "matched-after-suspension", 8.6, "Cold suspension-return window falls outside"),
            ("postPrimingClearCheckpoint", "plateau-baseline-01", 26, "Post-priming baseline window plateau-baseline-01 falls outside"),
        ):
            with self.subTest(checkpoint=checkpoint_key):
                memory, route, process_id, hwnd = self._cycle_memory_fixture()
                route[checkpoint_key]["screenshotCapturedAtUtc"] = self._cycle_memory_fixture_stamp(screenshot_seconds)
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_memory(memory, route, process_id, hwnd)

    def test_window_start_must_equal_first_sample_timestamp(self) -> None:
        for start_seconds, expected_error in (
            (1.5, "startedAtUtc must equal the first sample timestamp"),
            (2.05, "sample timestamps fall outside the recorded memory window"),
        ):
            with self.subTest(start_seconds=start_seconds):
                memory, route, process_id, hwnd = self._cycle_memory_fixture()
                baseline = next(item for item in memory["windows"] if item["windowId"] == "matched-baseline")
                baseline["startedAtUtc"] = self._cycle_memory_fixture_stamp(start_seconds)
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_memory(memory, route, process_id, hwnd)

        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        normal_use_window = next(item for item in memory["windows"] if item["windowId"] == "startup-ready")
        normal_use_window["startedAtUtc"] = self._cycle_memory_fixture_stamp(-0.1)
        with self.assertRaisesRegex(ValueError, "startedAtUtc must equal the first sample timestamp"):
            validator.validate_memory(memory, route, process_id, hwnd)

    def test_route_action_timestamps_must_be_ordered_and_non_overlapping(self) -> None:
        cases = (
            ("reversed completion", self._cycle_memory_fixture_stamp(8.2), self._cycle_memory_fixture_stamp(8.0), "issuedAtUtc is later than completedAtUtc"),
            ("before previous action", self._cycle_memory_fixture_stamp(3.0), self._cycle_memory_fixture_stamp(3.1), "timestamps are not monotonic after action 1"),
        )
        for label, issued, completed, expected_error in cases:
            with self.subTest(label=label):
                memory, route, process_id, hwnd = self._cycle_memory_fixture()
                leave_action = route["actions"][2]
                leave_action["issuedAtUtc"] = issued
                leave_action["completedAtUtc"] = completed
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_memory(memory, route, process_id, hwnd)

    def test_natural_close_chronology_accepts_close_return_completion_then_later_exit(self) -> None:
        memory, route, process_exit = self._natural_close_fixture()
        close_summary = validator.validate_natural_process_close_action(route, process_exit)
        self.assertEqual(close_summary["completedAtUtc"], close_summary["closeReturnedAtUtc"])
        self.assertLess(
            datetime.fromisoformat(close_summary["issuedAtUtc"].replace("Z", "+00:00")),
            datetime.fromisoformat(close_summary["completedAtUtc"].replace("Z", "+00:00")),
        )
        self.assertLess(
            datetime.fromisoformat(close_summary["completedAtUtc"].replace("Z", "+00:00")),
            datetime.fromisoformat(close_summary["exitedAtUtc"].replace("Z", "+00:00")),
        )
        route["workflowCompletedAtUtc"] = self._cycle_memory_fixture_stamp(340.15)
        by_id = {window["windowId"]: window for window in memory["windows"]}
        chronology = validator.validate_cleanup_memory_window_chronology(memory, route, by_id)
        self.assertTrue(chronology["passed"])

    def test_missing_action_completion_timestamp_still_fails_closed(self) -> None:
        memory, route, _process_exit = self._natural_close_fixture()
        route["actions"][-1].pop("completedAtUtc")
        by_id = {window["windowId"]: window for window in memory["windows"]}
        with self.assertRaisesRegex(ValueError, "Route action .* completed timestamp is missing"):
            validator.validate_cleanup_memory_window_chronology(memory, route, by_id)

    def test_natural_close_requires_a_present_and_ordered_close_return(self) -> None:
        _memory, route, process_exit = self._natural_close_fixture()
        route["actions"][-1].pop("closeReturnedAtUtc")
        with self.assertRaisesRegex(ValueError, "Natural close return timestamp is missing"):
            validator.validate_natural_process_close_action(route, process_exit)

        _memory, route, process_exit = self._natural_close_fixture()
        route["actions"][-1]["closeReturnedAtUtc"] = self._cycle_memory_fixture_stamp(340.1)
        with self.assertRaisesRegex(ValueError, "close-return/completion timestamps are missing, reversed, or out of order"):
            validator.validate_natural_process_close_action(route, process_exit)

    def test_failed_forced_or_mismatched_exit_cannot_prove_a_natural_close(self) -> None:
        cases = (
            ("failed exit", "normalExit", False, "successful normal exit"),
            ("forced exit", "forcedTermination", True, "forced or mismatched close method"),
            ("mismatched exit code", "exitCode", 1, "does not record exit code zero"),
        )
        for label, field, value, expected_error in cases:
            with self.subTest(label=label):
                _memory, route, process_exit = self._natural_close_fixture()
                process_exit[field] = value
                route["actions"][-1]["processExitEvidence"][field] = value
                with self.assertRaisesRegex(ValueError, expected_error):
                    validator.validate_natural_process_close_action(route, process_exit)

        _memory, route, process_exit = self._natural_close_fixture()
        route["actions"][-1]["processExitEvidence"]["exitedAtUtc"] = self._cycle_memory_fixture_stamp(340.6)
        with self.assertRaisesRegex(ValueError, "differs from the manifest process-exit record"):
            validator.validate_natural_process_close_action(route, process_exit)

    def _natural_close_fixture(self):
        memory, route, _process_id, _hwnd = self._cycle_memory_fixture()
        exit_evidence = {
            "observed": True,
            "normalExit": True,
            "exitCode": 0,
            "closeMethod": "WindowPattern.Close",
            "forcedTermination": False,
            "exitedAtUtc": self._cycle_memory_fixture_stamp(340.5),
        }
        route["actions"].append({
            "actionKind": "close-main-window-naturally",
            "method": "WindowPattern.Close",
            "issuedAtUtc": self._cycle_memory_fixture_stamp(340.2),
            "closeReturnedAtUtc": self._cycle_memory_fixture_stamp(340.4),
            "completedAtUtc": self._cycle_memory_fixture_stamp(340.4),
            "forcedTermination": False,
            "processExitEvidence": dict(exit_evidence),
        })
        return memory, route, exit_evidence

    def test_cycle_ten_return_must_finish_before_final_reentry(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        cycle_ten_return = next(item for item in memory["windows"] if item["windowId"] == "cycle-10-return")
        cycle_ten_return["startedAtUtc"] = self._cycle_memory_fixture_stamp(339.5)
        cycle_ten_return["completedAtUtc"] = self._cycle_memory_fixture_stamp(340.5)
        for index, sample in enumerate(cycle_ten_return["samples"]):
            sample["capturedAtUtc"] = self._cycle_memory_fixture_stamp(339.5 + index / 10)
        with self.assertRaisesRegex(ValueError, "Cycle 10 return window falls outside"):
            validator.validate_memory(memory, route, process_id, hwnd)

    @staticmethod
    def _cycle_memory_fixture_stamp(seconds):
        base_time = datetime(2026, 1, 1, tzinfo=timezone.utc)
        return (base_time + timedelta(seconds=seconds)).isoformat(timespec="milliseconds").replace("+00:00", "Z")

    def test_both_cleanup_failures_are_reported_before_validation_fails(self) -> None:
        memory, route, process_id, hwnd = self._cycle_memory_fixture(
            plateau_baselines=[100_000_000, 300_000_000, 100_000_000, 300_000_000, 100_000_000],
            initial_return_value=102_000_000,
        )
        result = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertFalse(result["passed"])
        self.assertFalse(result["cleanupGateResults"]["coldBaselineToFirstReturn"]["passed"])
        self.assertFalse(result["cleanupGateResults"]["postPrimingPlateauAndCycleReturns"]["passed"])
        with self.assertRaisesRegex(
            ValueError,
            "coldBaselineToFirstReturn.passed=False.*postPrimingPlateauAndCycleReturns.passed=False",
        ):
            validator.require_memory_cleanup_gates_passed(result)
    def test_memory_gate_accepts_inclusive_ceiling_and_fails_above_it(self) -> None:
        self.assertEqual(validator.WORKING_SET_TARGET, 256 * 1024 * 1024)
        self.assertEqual(validator.WORKING_SET_CEILING, 450 * 1024 * 1024)
        self.assertEqual(validator.OPERATION_PEAK_ADVISORY_LIMIT, 512 * 1024 * 1024)
        memory, route, process_id, hwnd = self._cycle_memory_fixture()
        windows = {window["windowId"]: window for window in memory["windows"]}
        window = windows["cycle-01-roster-loaded"]
        for sample in window["samples"]:
            sample["workingSetBytes"] = validator.WORKING_SET_TARGET - 1
        window["samples"][0]["workingSetBytes"] = validator.WORKING_SET_CEILING
        window["samples"][1]["workingSetBytes"] = validator.WORKING_SET_TARGET
        window["workingSetTargetMet"] = False
        window["workingSetTargetExceededSampleIndexes"] = [0, 1]
        window["workingSetCeilingPassed"] = True
        window["workingSetCeilingExceededSampleIndexes"] = []

        result = validator.validate_memory(memory, route, process_id, hwnd)
        self.assertTrue(result["passed"])
        self.assertEqual(window["samples"][0]["workingSetBytes"], 450 * 1024 * 1024)
        self.assertFalse(window["workingSetTargetMet"])
        self.assertFalse(result["workingSetCeilingExclusive"])
        self.assertFalse(result["allWindowsMetTarget"])
        self.assertIn("cycle-01-roster-loaded", result["windowsAboveTarget"])
        self.assertTrue(window["workingSetCeilingPassed"])

        window["samples"][0]["workingSetBytes"] = validator.WORKING_SET_CEILING + 1
        window["workingSetCeilingPassed"] = False
        window["workingSetCeilingExceededSampleIndexes"] = [0]
        with self.assertRaisesRegex(ValueError, "exceeded the 450 MiB ceiling"):
            validator.validate_memory(memory, route, process_id, hwnd)

if __name__ == "__main__":
    unittest.main()
