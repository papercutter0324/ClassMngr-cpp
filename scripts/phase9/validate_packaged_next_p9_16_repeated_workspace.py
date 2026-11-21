#!/usr/bin/env python3
"""Fail-closed validator for the P9-16 packaged workspace open/close route."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import statistics
import struct
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA = "classmngr-phase9-p9-16-packaged-repeated-workspace-v2"
PREPARATION_SCHEMA = "classmngr-phase9-p9-16-repeated-workspace-fixture-v1"
TASK_ID = "P9_16_PACKAGED_NEXT_WORKSPACE_REPEATED_CYCLES"
STRICT_WS_LIMIT = 262_144_000
OPERATION_WS_LIMIT = 536_870_912
PROFILE_IDENTITIES = {
    "tiny": {"classCount": 0, "classInfoCount": 0, "scheduleEntryCount": 0},
    "large": {"classCount": 96, "classInfoCount": 96, "scheduleEntryCount": 768},
}
CLASS_IDENTITY = PROFILE_IDENTITIES["large"]
P916_SOURCES = {
    "scripts/phase9/prepare_p9_16_workspace.py",
    "scripts/phase9/test_prepare_p9_16_workspace.py",
    "scripts/phase9/run_packaged_next_p9_16_repeated_workspace.ps1",
    "scripts/phase9/validate_packaged_next_p9_16_repeated_workspace.py",
    "scripts/phase9/test_validate_packaged_next_p9_16_repeated_workspace.py",
}
P915_SOURCES = {
    "scripts/phase9/prepare_p9_15_schedule_workspace.py",
    "scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1",
    "scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py",
    "scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py",
}
P914_SOURCES = {
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/prepare_p9_14_staff_ai_workspaces.py",
    "scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1",
    "scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_p9_14_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py",
}


class ValidationError(ValueError):
    pass


class InconclusiveMeasurement(ValidationError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"Cannot read valid UTF-8 JSON at {path}: {error}") from error


def canonical_json_sha256(value: Any) -> str:
    raw = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def inventory_fingerprint(rows: list[dict[str, Any]]) -> str:
    ordered = sorted(rows, key=lambda row: str(row["path"]).lower())
    canonical = "\n".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}"
        for row in ordered
    ) + "\n"
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def contained_path(root: Path, relative: str) -> Path:
    candidate = PurePosixPath(relative)
    require(
        not candidate.is_absolute() and ".." not in candidate.parts,
        f"Unsafe relative evidence path: {relative!r}",
    )
    target = (root / Path(*candidate.parts)).resolve(strict=True)
    resolved_root = root.resolve(strict=True)
    require(
        target == resolved_root or resolved_root in target.parents,
        f"Evidence artifact escaped its run directory: {relative}",
    )
    require(target.is_file(), f"Evidence artifact is not a file: {relative}")
    return target


def verify_artifact(root: Path, relative: str, expected_sha256: str) -> Path:
    target = contained_path(root, relative)
    require(sha256_file(target) == expected_sha256.lower(), f"Evidence SHA-256 mismatch: {relative}")
    return target


def verify_absolute_file(path_text: str, expected_sha256: str, label: str) -> Path:
    path = Path(path_text).resolve(strict=True)
    require(path.is_file(), f"{label} is not a file: {path}")
    require(sha256_file(path) == expected_sha256.lower(), f"{label} SHA-256 mismatch: {path}")
    return path


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        require(stream.read(2) == b"MZ", "Packaged application lacks the DOS executable signature.")
        stream.seek(0x3C)
        offset_raw = stream.read(4)
        require(len(offset_raw) == 4, "Packaged application PE offset is truncated.")
        offset = struct.unpack("<I", offset_raw)[0]
        stream.seek(offset)
        require(stream.read(4) == b"PE\0\0", "Packaged application lacks a PE signature.")
        machine = stream.read(2)
        require(len(machine) == 2, "Packaged application PE machine field is truncated.")
        return struct.unpack("<H", machine)[0]


def package_inventory(root: Path) -> list[dict[str, Any]]:
    rows = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        rows.append({"path": relative, "sizeBytes": path.stat().st_size, "sha256": sha256_file(path)})
    return rows


def validate_package_provenance(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    proof = manifest.get("packageProvenance", {})
    reused = proof.get("reusedPackage") is True
    require(proof.get("reusedPackage") in (True, False), "Package reuse status is missing or invalid.")
    prior_path = verify_absolute_file(
        str(proof.get("priorRunManifestPath", "")),
        str(proof.get("priorRunManifestSha256", "")),
        "Accepted P9-15 manifest",
    )
    prior = read_json(prior_path)
    require(
        prior.get("taskId") == "P9_15_PACKAGED_NEXT_SCHEDULE_LOCALE_THEME"
        and prior.get("status") == "passed",
        "Package ancestry is not the accepted passed P9-15 run.",
    )
    prior_root = prior_path.parent
    prior_proof = prior.get("packageProvenance", {})
    prior_source_path = verify_artifact(
        prior_root,
        str(prior_proof.get("sourceInventoryPath", "")),
        str(prior_proof.get("sourceInventoryFileSha256", "")),
    )
    prior_source = read_json(prior_source_path)
    current_source_path = verify_artifact(
        root,
        str(proof.get("sourceInventoryPath", "")),
        str(proof.get("sourceInventoryFileSha256", "")),
    )
    current_source = read_json(current_source_path)
    require(
        current_source.get("sha256") == proof.get("currentSourceSha256")
        and current_source.get("includedRoots") == prior_source.get("includedRoots"),
        "Current source inventory does not match its fingerprint or accepted inventory roots.",
    )
    prior_rows = {str(row["path"]).lower(): row for row in prior_source.get("files", [])}
    current_rows = {str(row["path"]).lower(): row for row in current_source.get("files", [])}
    require(len(prior_rows) == len(prior_source.get("files", [])), "P9-15 source inventory has duplicate paths.")
    require(len(current_rows) == len(current_source.get("files", [])), "P9-16 source inventory has duplicate paths.")
    if reused:
        for path, old in prior_rows.items():
            require(path in current_rows, f"Previously accepted source disappeared: {old['path']}")
            now = current_rows[path]
            require(
                int(old["sizeBytes"]) == int(now["sizeBytes"])
                and str(old["sha256"]).lower() == str(now["sha256"]).lower(),
                f"Previously accepted source changed: {old['path']}",
            )
        added = {row["path"] for key, row in current_rows.items() if key not in prior_rows}
        require(added == P916_SOURCES, f"Unexpected source additions after accepted P9-15: {sorted(added)!r}")
    else:
        require(
            proof.get("freshExactTreeRebuild") is True and str(proof.get("reuseFailureReason", "")).strip(),
            "A locally rebuilt package lacks a recorded P9-15 reuse failure and fresh package declaration.",
        )
        require(set(prior_rows).issubset(current_rows), "A local package build removed an accepted source path from its inventory.")

    p913_path = verify_absolute_file(
        str(proof.get("priorP913ManifestPath", "")),
        str(proof.get("priorP913ManifestSha256", "")),
        "P9-13 source-derivation ancestry manifest",
    )
    p913 = read_json(p913_path)
    require(
        p913.get("taskId") == "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE"
        and p913.get("status") == "passed",
        "P9-13 product-source ancestry is not a passed accepted run.",
    )
    p913_root = p913_path.parent
    derived_ref = p913.get("source", {})
    derived_path = verify_artifact(
        p913_root,
        str(derived_ref.get("derivedReceiptPath", "")),
        str(derived_ref.get("derivedReceiptSha256", "")),
    )
    derived = read_json(derived_path)
    require(
        derived.get("status") == "verified-script-only-derivation"
        and derived.get("source", {}).get("productSourceSha256") == prior_proof.get("productSourceSha256")
        and Path(str(proof.get("priorP913ManifestPath", ""))).resolve() == p913_path
        and proof.get("priorP913ManifestSha256") == sha256_file(p913_path),
        "P9-13 source derivation receipt does not support the accepted package ancestry.",
    )
    ignored = {
        str(path).lower()
        for path in derived.get("source", {}).get("allowedPaths", [])
    }
    ignored.update(path.lower() for path in P914_SOURCES | P915_SOURCES | P916_SOURCES)
    product_rows = [row for key, row in current_rows.items() if key not in ignored]
    product_hash = inventory_fingerprint(product_rows)
    require(product_hash == proof.get("productSourceSha256"), "Product-source fingerprint differs from the recorded P9-16 provenance.")
    if reused:
        require(product_hash == prior_proof.get("productSourceSha256"), "Current product sources differ from accepted P9-15; stale package reuse is not allowed.")
        require(
            Path(str(proof.get("packageRoot", ""))).resolve() == Path(str(prior_proof.get("packageRoot", ""))).resolve()
            and proof.get("packageSha256") == prior_proof.get("packageSha256")
            and int(proof.get("packageFileCount", -1)) == int(prior_proof.get("packageFileCount", -2))
            and proof.get("applicationSha256") == prior_proof.get("applicationSha256")
            and Path(str(proof.get("acceptedReleaseReceiptPath", ""))).resolve() == Path(str(prior_proof.get("acceptedReleaseReceiptPath", ""))).resolve(),
            "Reused P9-16 package proof does not reference the exact accepted P9-15 package and Release receipt.",
        )

    package_root = Path(str(proof.get("packageRoot", ""))).resolve(strict=True)
    current_package_rows = package_inventory(package_root)
    current_package_hash = inventory_fingerprint(current_package_rows)
    require(
        current_package_hash == proof.get("packageSha256")
        and len(current_package_rows) == int(proof.get("packageFileCount", -1)),
        "Current packaged tree differs from its P9-15 accepted fingerprint.",
    )
    current_package_inventory_path = verify_artifact(
        root,
        str(proof.get("packageInventoryPath", "")),
        str(proof.get("packageInventoryFileSha256", "")),
    )
    current_package_inventory = read_json(current_package_inventory_path)
    require(
        inventory_fingerprint(current_package_inventory.get("files", [])) == current_package_hash
        and int(current_package_inventory.get("fileCount", -1)) == len(current_package_rows),
        "Current package inventory artifact does not match the package tree.",
    )
    if reused:
        package_ref = verify_artifact(
            prior_root,
            str(prior_proof.get("packageInventoryPath", "")),
            str(prior_proof.get("packageInventoryFileSha256", "")),
        )
        accepted_inventory = read_json(package_ref)
        require(
            inventory_fingerprint(accepted_inventory.get("files", [])) == current_package_hash
            and int(accepted_inventory.get("fileCount", -1)) == len(current_package_rows),
            "Current package files differ from the accepted complete P9-15 inventory.",
        )
    else:
        require(root.resolve(strict=True) in package_root.parents, "Fallback package escaped its evidence run directory.")
    app_path = package_root / "ClassMngrNext.exe"
    require(app_path.is_file(), "Verified package does not contain ClassMngrNext.exe.")
    app_hash = sha256_file(app_path)
    require(app_hash == proof.get("applicationSha256") and pe_machine(app_path) == 0x8664,
            "Package executable hash or x64 architecture differs from P9-16 provenance.")
    if reused:
        require(app_hash == prior_proof.get("applicationSha256"), "Reused package executable differs from P9-15.")
    else:
        require(proof.get("freshExactTreeRebuild") is True, "Fallback package is not marked as a fresh local Release package.")
    release_path = verify_absolute_file(
        str(proof.get("acceptedReleaseReceiptPath", "")),
        str(proof.get("acceptedReleaseReceiptSha256", "")),
        "Accepted Release receipt",
    )
    release = read_json(release_path)
    require(
        release.get("status") == "succeeded"
        and release.get("configuration") == "Release"
        and release.get("platform") == "x64"
        and int(release.get("build", {}).get("exitCode", -1)) == 0
        and release.get("releaseExecutableSha256") == app_hash,
        "Release receipt does not prove the packaged x64 Release executable.",
    )
    if reused:
        require(
            sha256_file(release_path) == prior_proof.get("acceptedReleaseReceiptSha256"),
            "Reused Release receipt differs from the P9-15 accepted receipt.",
        )
    else:
        receipt_relative = verify_artifact(
            root,
            str(proof.get("releaseReceiptPath", "")),
            str(proof.get("releaseReceiptSha256", "")),
        )
        require(receipt_relative == release_path, "Local Release receipt path is not the run-bound evidence artifact.")
        require(
            int(release.get("resourceInstallExitCode", -1)) == 0
            and int(release.get("deployExitCode", -1)) == 0
            and release.get("productSourceSha256") == product_hash
            and release.get("packageSha256") == current_package_hash,
            "Local Release receipt does not prove the current-source build, resource install, deployment, and package tree.",
        )
    cache_path = verify_absolute_file(
        str(proof.get("configuredCachePath", "")),
        str(proof.get("configuredCacheSha256", "")),
        "Configured Release CMake cache",
    )
    require(sha256_file(cache_path) == release.get("cacheSha256After"), "Release CMake cache differs from build receipt.")
    resource_root = Path(str(proof.get("resourceRoot", ""))).resolve(strict=True)
    resource_rows = package_inventory(resource_root)
    resource_hash = inventory_fingerprint(resource_rows)
    require(
        resource_hash == proof.get("resourceInventorySha256")
        and resource_hash == release.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256"),
        "Staged package resource inputs differ from accepted Release evidence.",
    )
    if not reused:
        local_build = proof.get("localBuild", {})
        for path_key, hash_key in (
            ("buildLogPath", "buildLogSha256"),
            ("resourceInstallLogPath", "resourceInstallLogSha256"),
            ("deployLogPath", "deployLogSha256"),
        ):
            verify_artifact(root, str(local_build.get(path_key, "")), str(local_build.get(hash_key, "")))
        deploy_tool = verify_absolute_file(
            str(local_build.get("windeployqtPath", "")),
            str(local_build.get("windeployqtSha256", "")),
            "Configured Qt windeployqt",
        )
        require(deploy_tool.name.lower() == "windeployqt.exe", "Local package deployment tool is not windeployqt.exe.")
        require(
            int(local_build.get("buildExitCode", -1)) == 0
            and int(local_build.get("resourceInstallExitCode", -1)) == 0
            and int(local_build.get("deployExitCode", -1)) == 0,
            "Local Release build, resource install, or deployment did not complete successfully.",
        )
    resource_inventory_path = verify_artifact(
        root,
        str(proof.get("resourceInventoryPath", "")),
        str(proof.get("resourceInventoryFileSha256", "")),
    )
    require(
        inventory_fingerprint(read_json(resource_inventory_path).get("files", [])) == resource_hash,
        "Current resource inventory artifact is inconsistent.",
    )
    return {
        "packageRoot": str(package_root),
        "packageSha256": current_package_hash,
        "packageFileCount": len(current_package_rows),
        "applicationPath": str(app_path),
        "applicationSha256": app_hash,
        "architecture": "x64",
        "configuration": "Release",
        "productSourceSha256": product_hash,
        "currentSourceSha256": str(current_source.get("sha256")),
        "freshExactTreeRebuild": not reused,
        "reusedPackage": reused,
        "reusedFromTaskId": prior.get("taskId"),
    }


def validate_fixture(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    fixture = manifest.get("fixture", {})
    require(fixture.get("kind") == "repeated-workspace-open-close", "Fixture kind is not P9-16 repeated workspace.")
    preparation_path = verify_artifact(
        root,
        fixture.get("preparationReceipt", {}).get("relativePath", ""),
        fixture.get("preparationReceipt", {}).get("sha256", ""),
    )
    preparation = read_json(preparation_path)
    profile_size = str(fixture.get("profileSize", preparation.get("profileSize", "large")))
    expected_identity = PROFILE_IDENTITIES.get(profile_size)
    require(
        preparation.get("schema") == PREPARATION_SCHEMA
        and preparation.get("status") == "passed"
        and preparation.get("kind") == "repeated-workspace-open-close"
        and profile_size in PROFILE_IDENTITIES
        and preparation.get("profileSize", "large") == profile_size,
        "P9-16 preparation receipt identity/status is invalid.",
    )
    workspace = Path(str(fixture.get("workspacePath", ""))).resolve(strict=True)
    require(workspace == Path(str(preparation.get("workspace", ""))).resolve(), "Measured workspace differs from preparation receipt.")
    require(
        preparation.get("identity") == expected_identity
        and fixture.get("preparationReceipt", {}).get("classCount") == expected_identity["classCount"]
        and fixture.get("preparationReceipt", {}).get("classInfoCount") == expected_identity["classInfoCount"]
        and fixture.get("preparationReceipt", {}).get("scheduleEntryCount") == expected_identity["scheduleEntryCount"],
        "Preparation receipt or manifest does not prove the declared tiny or large current-schema fixture identity.",
    )
    require(preparation.get("baseWorkspaceSha256") == "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9",
            "Fixture derivation does not use the pinned current-schema blank workspace.")
    if profile_size == "large":
        require(preparation.get("seedSqlSha256") == "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3",
                "Large fixture derivation does not use the pinned large_startup.sql.")
    else:
        require(preparation.get("seedSql") is None and preparation.get("seedSqlSha256") is None,
                "Tiny fixture must be derived directly from the pinned blank profile without seed SQL.")
    for phase in ("before", "after"):
        reference = fixture.get(f"{phase}Snapshot", {})
        snapshot_path = verify_artifact(root, reference.get("relativePath", ""), reference.get("sha256", ""))
        snapshot = read_json(snapshot_path)
        require(
            snapshot.get("schema") == PREPARATION_SCHEMA
            and snapshot.get("status") == "passed"
            and snapshot.get("phase") == phase,
            f"Workspace {phase} snapshot identity/status is invalid.",
        )
        require(snapshot.get("integrityCheck") == "ok" and snapshot.get("foreignKeyErrors") == [], f"Workspace {phase} SQLite integrity failed.")
        require(
            snapshot.get("identity") == expected_identity
            and snapshot.get("workspaceTables") == preparation.get("baselineTables")
            and snapshot.get("schemaObjects") == preparation.get("baselineSchemaObjects")
            and snapshot.get("workspaceTablesSha256") == preparation.get("baselineTablesSha256")
            and snapshot.get("schemaObjectsSha256") == preparation.get("baselineSchemaObjectsSha256"),
            f"Workspace {phase} tables/schema/state differ from the prepared fixture.",
        )
    before = read_json(contained_path(root, fixture["beforeSnapshot"]["relativePath"]))
    after = read_json(contained_path(root, fixture["afterSnapshot"]["relativePath"]))
    require(
        before.get("workspaceTables") == after.get("workspaceTables")
        and before.get("schemaObjects") == after.get("schemaObjects"),
        "SQLite table rows or schema changed during the repeated route.",
    )
    base_path = Path(str(fixture.get("sourceBaseWorkspacePath", "")))
    require(base_path.is_file() and sha256_file(base_path) == preparation.get("baseWorkspaceSha256"),
            "Fixture blank-workspace source path or fingerprint is invalid.")
    if profile_size == "large":
        seed_path = Path(str(fixture.get("sourceSeedSqlPath", "")))
        require(seed_path.is_file() and sha256_file(seed_path) == preparation.get("seedSqlSha256"),
                "Large fixture seed SQL source path or fingerprint is invalid.")
    else:
        require(fixture.get("sourceSeedSqlPath") in (None, ""),
                "Tiny current-schema profile unexpectedly declares a seed SQL source.")
    return {
        "workspace": str(workspace),
        "profileSize": profile_size,
        "profileSize": profile_size,
        "classCount": expected_identity["classCount"],
        "classInfoCount": expected_identity["classInfoCount"],
        "scheduleEntryCount": expected_identity["scheduleEntryCount"],
        "tableCount": int(before["workspaceTableCount"]),
        "tablesSha256": str(before["workspaceTablesSha256"]),
        "schemaObjectsSha256": str(before["schemaObjectsSha256"]),
        "integrityBeforeAndAfter": "ok",
        "rowsAndSchemaEqual": True,
        "seedSqlSha256": preparation.get("seedSqlSha256"),
        "baseWorkspaceSha256": str(preparation["baseWorkspaceSha256"]),
    }


def parse_ini(path: Path) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    active = ""
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            active = line[1:-1]
            result.setdefault(active, {})
        elif active and "=" in line:
            key, value = line.split("=", 1)
            result[active][key.strip()] = value.strip()
    return result


def validate_settings(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    settings = manifest.get("settings", {})
    before_path = verify_artifact(
        root,
        settings.get("beforeLaunchInventoryPath", ""),
        settings.get("beforeLaunchInventoryFileSha256", ""),
    )
    before = read_json(before_path)
    require(
        before.get("sha256") == settings.get("beforeLaunchInventorySha256")
        and before.get("fileCount") == 1,
        "Before-launch settings inventory is inconsistent or not a single isolated profile.",
    )
    settings_root = Path(str(settings.get("root", ""))).resolve(strict=True)
    run_root = root.resolve(strict=True)
    require(run_root in settings_root.parents, "Isolated settings root escaped the evidence run directory.")
    profile_path = settings_root / "PaperCloud" / "ClassMngr.ini"
    require(profile_path.is_file(), "Isolated ClassMngr.ini profile is missing.")
    initial_path = verify_artifact(
        root,
        settings.get("initialPreferenceSnapshotPath", ""),
        settings.get("initialPreferenceSnapshotSha256", ""),
    )
    initial = parse_ini(initial_path)
    require(
        initial.get("options", {}).get("language") == "1"
        and initial.get("options", {}).get("theme") == "1",
        "Run did not start with the pinned English/Light preferences.",
    )
    before_profile_row = [
        row for row in before.get("files", [])
        if row.get("path") == "PaperCloud/ClassMngr.ini"
    ]
    require(
        len(before_profile_row) == 1
        and before_profile_row[0].get("sha256") == settings.get("initialPreferenceFileSha256")
        and sha256_file(initial_path) == settings.get("initialPreferenceFileSha256"),
        "Before-launch isolated settings inventory does not match the saved initial profile.",
    )
    cleanup = manifest.get("cleanup", {})
    after_ref = cleanup.get("settingsInventoryAfterExit", {})
    after_path = verify_artifact(
        root,
        after_ref.get("relativePath", ""),
        after_ref.get("inventoryFileSha256", ""),
    )
    after = read_json(after_path)
    require(
        after.get("sha256") == after_ref.get("sha256")
        and after.get("fileCount") == 1
        and len(after.get("files", [])) == 1
        and after["files"][0].get("path") == "PaperCloud/ClassMngr.ini"
        and after["files"][0].get("sha256") == sha256_file(profile_path),
        "After-exit inventory does not fingerprint the final single isolated settings file.",
    )
    final = parse_ini(profile_path)
    require(
        final.get("options", {}).get("language") == "1"
        and final.get("options", {}).get("theme") == "1",
        "Final isolated preferences differ from the pinned English/Light state.",
    )
    require(
        settings.get("globalDefaultChanged") is False
        and settings.get("settingsRootFromEnvironment") is True
        and settings.get("preferenceActionsCount") == 0
        and settings.get("globalDefaultsWereReadOrWritten") is False,
        "Run does not prove settings isolation and absence of user preference actions.",
    )
    process_isolation = manifest.get("process", {}).get("environmentIsolation", {})
    expected_override_keys = {
        "CLASSMNGR_SETTINGS_ROOT", "CLASSMNGR_NEXT_SHUTDOWN_TRACE_PATH", "LOCALAPPDATA",
        "APPDATA", "TEMP", "TMP", "QT_FORCE_STDERR_LOGGING", "PATH",
    }
    require(
        process_isolation.get("settingsRootFromEnvironment") is True
        and process_isolation.get("globalDefaultsChanged") is False
        and set(process_isolation.get("overriddenKeys", [])) == expected_override_keys,
        "Process environment does not prove the exact isolated settings/runtime override set.",
    )
    return {
        "isolatedProfileFileCount": 1,
        "initialState": "English/Light",
        "finalState": "English/Light",
        "preferenceActionsCount": 0,
        "globalDefaultChanged": False,
        "beforeInventorySha256": before.get("sha256"),
        "afterInventorySha256": after.get("sha256"),
    }


def quantile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * q
    lower = math.floor(position)
    upper = math.ceil(position)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def median(values: list[float]) -> float:
    return float(statistics.median(values))


def linear_fitted_change(values: list[float]) -> float:
    if len(values) < 2:
        return 0.0
    x_values = list(range(len(values)))
    x_mean = statistics.mean(x_values)
    y_mean = statistics.mean(values)
    denominator = sum((x - x_mean) ** 2 for x in x_values)
    slope = sum((x - x_mean) * (y - y_mean) for x, y in zip(x_values, values)) / denominator
    return slope * (len(values) - 1)


def summarize_window(window: dict[str, Any]) -> dict[str, Any]:
    samples = window.get("samples")
    require(isinstance(samples, list) and len(samples) == 10, f"Memory window {window.get('windowId')} must contain exactly 10 samples.")
    for counter in ("workingSetBytes", "privateBytes"):
        values = [float(sample.get(counter, -1)) for sample in samples]
        require(all(value >= 0 for value in values), f"Memory window is missing valid {counter} samples.")
    times = [int(sample.get("elapsedMilliseconds", -1)) for sample in samples]
    require(all(0 <= value <= 2000 for value in times), "Memory window sample elapsed time is outside its bounded 1-second interval.")
    require(times == sorted(times) and len(set(times)) == len(times), "Memory window sample times are not strictly increasing.")
    require(times[-1] >= 750, "Memory window does not span a settled one-second sampling interval.")
    require(
        all(35 <= current - previous <= 350 for previous, current in zip(times, times[1:])),
        "Memory window samples are not spaced at the required approximately 100 ms interval.",
    )
    summaries = {}
    for counter in ("workingSetBytes", "privateBytes"):
        values = [float(sample[counter]) for sample in samples]
        summaries[counter] = {
            "median": median(values),
            "p10": quantile(values, 0.10),
            "p90": quantile(values, 0.90),
            "minimum": min(values),
            "maximum": max(values),
        }
    strict_cap_violations = [
        {
            "sampleIndex": index,
            "workingSetBytes": int(sample.get("workingSetBytes", STRICT_WS_LIMIT)),
            "exclusiveLimitBytes": STRICT_WS_LIMIT,
        }
        for index, sample in enumerate(samples)
        if int(sample.get("workingSetBytes", STRICT_WS_LIMIT)) >= STRICT_WS_LIMIT
    ]
    for sample in samples:
        require(int(sample.get("handles", -1)) >= 0 and int(sample.get("threadCount", -1)) > 0,
                "Memory sample lacks handle/thread diagnostics.")
    duration = times[-1] - times[0]
    return {
        "windowId": window.get("windowId"),
        "state": window.get("state"),
        "phase": window.get("phase"),
        "cycleNumber": window.get("cycleNumber"),
        "sampleCount": 10,
        "sampleDurationMilliseconds": duration,
        "counters": summaries,
        "strictWorkingSetCapPassed": not strict_cap_violations,
        "strictWorkingSetCapViolations": strict_cap_violations,
        "handleCountRange": [min(int(sample["handles"]) for sample in samples), max(int(sample["handles"]) for sample in samples)],
        "threadCountRange": [min(int(sample["threadCount"]) for sample in samples), max(int(sample["threadCount"]) for sample in samples)],
    }


def analyze_memory(manifest: dict[str, Any], expected_pid: int, expected_hwnd: int) -> dict[str, Any]:
    memory = manifest.get("memory", {})
    windows = memory.get("windows", [])
    route = manifest.get("route", {})
    measured_cycle_count = int(route.get("measuredCycleCount", 10))
    priming_cycle_count = int(route.get("primingCycleCount", 0))
    baseline_windows_per_state = int(route.get("baselineWindowsPerState", 5))
    require(isinstance(windows, list), "Memory windows are missing.")
    warmup = [window for window in windows if window.get("phase") == "warmup"]
    baselines = [window for window in windows if window.get("phase") == "baseline"]
    cycles = [window for window in windows if window.get("phase") == "cycle"]
    require(len(warmup) == 2, "Warm-up must report one settled open and one settled closed memory window.")
    require(baseline_windows_per_state == 5 and measured_cycle_count >= 1 and priming_cycle_count >= 0,
            "Memory plan must declare five baseline windows per state and valid priming/measured cycle counts.")
    require(len(baselines) == baseline_windows_per_state * 2,
            "Exactly five no-action baseline windows per state are required after priming.")
    require(len(cycles) == measured_cycle_count * 2,
            "Every measured close/reopen cycle must have one open and one closed memory window.")
    expected_window_ids = ["warmup-open", "warmup-closed"]
    expected_window_ids.extend(f"baseline-closed-{index}" for index in range(1, baseline_windows_per_state + 1))
    expected_window_ids.extend(f"baseline-open-{index}" for index in range(1, baseline_windows_per_state + 1))
    for index in range(1, measured_cycle_count + 1):
        expected_window_ids.extend((f"cycle-{index}-closed", f"cycle-{index}-open"))
    require(
        [str(window.get("windowId", "")) for window in windows] == expected_window_ids,
        "Memory windows are not ordered as warm-up, five no-action baselines per state, then ten measured close/reopen cycles.",
    )
    for window in windows:
        require(
            int(window.get("processId", 0)) == expected_pid
            and int(window.get("mainWindowHandle", 0)) == expected_hwnd,
            f"Memory window {window.get('windowId')} is not bound to the original process and main HWND.",
        )
    summaries = [summarize_window(window) for window in windows]
    by_id = {str(item["windowId"]): item for item in summaries}

    baseline_windows = {
        state: [window for window in baselines if window.get("state") == state]
        for state in ("open", "closed")
    }
    cycle_windows = {
        state: sorted(
            [window for window in cycles if window.get("state") == state],
            key=lambda window: int(window.get("cycleNumber", 0)),
        )
        for state in ("open", "closed")
    }
    for state in ("open", "closed"):
        require(len(baseline_windows[state]) == baseline_windows_per_state, f"State {state} lacks the declared baseline windows.")
        require(len(cycle_windows[state]) == measured_cycle_count, f"State {state} lacks the declared measured cycle windows.")
        require(
            [int(window.get("cycleNumber", 0)) for window in cycle_windows[state]] == list(range(1, measured_cycle_count + 1)),
            f"State {state} cycle windows are missing or duplicated.",
        )
    analyses: dict[str, Any] = {}
    working_set_unstable = []
    working_set_growth_failures = []
    private_bytes_unstable = []
    private_bytes_growth_trends = []
    for state in ("open", "closed"):
        analyses[state] = {}
        for counter in ("workingSetBytes", "privateBytes"):
            baseline_medians = [
                float(by_id[str(window["windowId"])]["counters"][counter]["median"])
                for window in baseline_windows[state]
            ]
            center = median(baseline_medians)
            envelope = max(abs(value - center) for value in baseline_medians)
            baseline_change = linear_fitted_change(baseline_medians)
            baseline_reason = None
            if center <= 0 or envelope >= center:
                baseline_reason = "baseline-noise-envelope-is-at-least-the-baseline-median"
            elif abs(baseline_change) > envelope:
                baseline_reason = "baseline-has-directional-drift-beyond-its-envelope"
            cycle_medians = [
                float(by_id[str(window["windowId"])]["counters"][counter]["median"])
                for window in cycle_windows[state]
            ]
            upper_bound = center + envelope
            above_upper_bound = [
                {"cycleNumber": index + 1, "median": value, "upperBound": upper_bound}
                for index, value in enumerate(cycle_medians)
                if value > upper_bound
            ]
            cycle_change = linear_fitted_change(cycle_medians)
            within_upper_bound = not above_upper_bound
            settled_cap_failures = [
                {
                    "windowId": str(window["windowId"]),
                    "violations": by_id[str(window["windowId"])]["strictWorkingSetCapViolations"],
                }
                for window in cycle_windows[state]
                if not by_id[str(window["windowId"])]["strictWorkingSetCapPassed"]
            ] if counter == "workingSetBytes" else []
            final_median = cycle_medians[-1]
            final_above_upper_bound = final_median > upper_bound
            positive_cycle_trend = cycle_change > 0
            positive_cycle_trend_beyond_envelope = cycle_change > envelope
            cumulative_growth = final_above_upper_bound and positive_cycle_trend_beyond_envelope
            working_set_gate_applies = counter == "workingSetBytes"
            if working_set_gate_applies:
                if baseline_reason:
                    working_set_unstable.append({"state": state, "reason": baseline_reason})
                elif cumulative_growth:
                    working_set_growth_failures.append({
                        "state": state,
                        "cycleFinalMedian": final_median,
                        "finalMedianAboveUpperBound": final_above_upper_bound,
                        "positiveCycleTrendBeyondNoiseEnvelope": positive_cycle_trend_beyond_envelope,
                        "cumulativeUpwardGrowth": cumulative_growth,
                        "cycleMediansAboveUpperBound": above_upper_bound,
                        "regressionFittedChangeDiagnosticOnly": cycle_change,
                        "trendNoiseEnvelopeThreshold": envelope,
                        "upperBound": upper_bound,
                        "noiseEnvelope": envelope,
                    })
                state_status = "inconclusive" if baseline_reason else ("failed" if cumulative_growth else "passed")
                no_growth_pass: bool | None = baseline_reason is None and not cumulative_growth
            else:
                if baseline_reason:
                    private_bytes_unstable.append({"state": state, "reason": baseline_reason})
                elif cumulative_growth:
                    private_bytes_growth_trends.append({
                        "state": state,
                        "cycleFinalMedian": final_median,
                        "finalMedianAboveUpperBound": final_above_upper_bound,
                        "positiveCycleTrendBeyondNoiseEnvelope": positive_cycle_trend_beyond_envelope,
                        "cumulativeUpwardTrendObserved": cumulative_growth,
                        "cycleMediansAboveUpperBound": above_upper_bound,
                        "regressionFittedChangeDiagnosticOnly": cycle_change,
                        "trendNoiseEnvelopeThreshold": envelope,
                        "upperBound": upper_bound,
                        "noiseEnvelope": envelope,
                    })
                state_status = "baseline-inconclusive" if baseline_reason else "diagnostic-only"
                no_growth_pass = None
            if working_set_gate_applies:
                same_state_pass = baseline_reason is None and not cumulative_growth
            else:
                same_state_pass = None
            analyses[state][counter] = {
                "status": state_status,
                "gateAppliesToPrimaryMemoryStatus": working_set_gate_applies,
                "baselineWindowMedians": baseline_medians,
                "baselineMedian": center,
                "noiseEnvelope": envelope,
                "upperBound": upper_bound,
                "baselineRegressionFittedChangeDiagnostic": baseline_change,
                "baselineDirectionalDriftMagnitude": abs(baseline_change),
                "baselineStable": baseline_reason is None,
                "baselineReason": baseline_reason,
                "cycleMedians": cycle_medians,
                "cycleMediansWithinUpperBound": within_upper_bound,
                "cycleMediansAboveUpperBound": above_upper_bound,
                "cycleFinalMedian": final_median,
                "finalMedianAboveUpperBound": final_above_upper_bound,
                "positiveCycleTrend": positive_cycle_trend,
                "positiveCycleTrendBeyondNoiseEnvelope": positive_cycle_trend_beyond_envelope,
                "cumulativeUpwardGrowth": cumulative_growth,
                "cycleRegressionFittedChangeDiagnostic": cycle_change,
                "trendNoiseEnvelopeThreshold": envelope,
                "settledWorkingSetCapFailures": settled_cap_failures,
                "sameStateNoGrowthPass": no_growth_pass,
                "privateBytesTrendOnly": not working_set_gate_applies,
            }
    metrics = manifest.get("process", {}).get("metrics", {})
    metrics_path = verify_artifact(
        Path(str(manifest.get("_evidenceRoot", "."))),
        metrics.get("path", ""),
        metrics.get("sha256", ""),
    )
    process_samples = []
    for line_number, line in enumerate(metrics_path.read_text(encoding="utf-8").splitlines(), 1):
        try:
            process_samples.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise ValidationError(f"Process metric line {line_number} is invalid JSON.") from error
    require(bool(process_samples), "Process metric sampler produced no samples.")
    for sample in process_samples:
        require(int(sample.get("pid", 0)) == expected_pid, "Process metric sample belongs to another PID.")
    operation_peaks = [int(item["workingSetBytes"]) for item in process_samples if item.get("category") == "operation"]
    lifetime_peaks = [int(item.get("peakWorkingSetBytes", 0)) for item in process_samples]
    operation_peak = max(operation_peaks, default=0)
    require(operation_peaks, "Continuous process sampler captured no open/close operation samples.")
    lifetime_peak = max(lifetime_peaks, default=0)
    strict_cap_pass = all(item["strictWorkingSetCapPassed"] for item in summaries)
    working_set_no_growth_status = "failed" if working_set_growth_failures else ("inconclusive" if working_set_unstable else "passed")
    strict_cap_status = "passed" if strict_cap_pass else "failed"
    if strict_cap_status == "failed" or working_set_no_growth_status == "failed":
        status = "failed"
    else:
        status = working_set_no_growth_status
    private_bytes_status = (
        "unresolved-positive-trend"
        if private_bytes_growth_trends
        else ("baseline-inconclusive" if private_bytes_unstable else "no-positive-cumulative-trend-observed")
    )
    return {
        "status": status,
        "workingSetNoGrowthStatus": working_set_no_growth_status,
        "strictWorkingSetCapStatus": strict_cap_status,
        "claim": "primary memory status is determined by same-state Working Set no-growth and the strict settled Working Set cap; Private Bytes is a separate secondary diagnostic",
        "windowPolicy": {
            "warmupWindows": 2,
            "primingCyclesBeforeBaseline": priming_cycle_count,
            "baselineWindowsPerState": baseline_windows_per_state,
            "cycleWindowsPerState": measured_cycle_count,
            "samplesPerWindow": 10,
            "sampleIntervalMilliseconds": 100,
            "percentileMethod": "linear interpolation over sorted sample values",
            "noiseEnvelopeMethod": "largest absolute deviation of the five baseline-window medians from their median",
            "primaryCounter": "workingSetBytes",
            "secondaryDiagnosticCounter": "privateBytes",
            "noGrowthRule": "for a stable Working Set baseline, cumulative growth fails only when the final same-state median exceeds baseline median plus the measured absolute envelope and the signed fitted cycle change exceeds that same envelope; transient earlier points are reported but do not fail alone",
            "trendMethod": "signed ordinary least-squares fitted first-to-last change; for Working Set the positive fitted change must exceed the empirical baseline envelope and the final median must exceed the one-sided upper bound",
            "tooUnstableRule": "baseline envelope at least as large as its median, or absolute baseline directional drift greater than the envelope, is inconclusive",
        },
        "windowSummaries": summaries,
        "states": analyses,
        "inconclusiveReasons": working_set_unstable,
        "workingSetGrowthFailures": working_set_growth_failures,
        "privateBytesTrend": {
            "status": private_bytes_status,
            "diagnosticOnly": True,
            "affectsPrimaryMemoryStatus": False,
            "claim": "Private Bytes baseline/cycle series and trends are reported as secondary diagnostics and do not affect Working Set no-growth or strict-cap status.",
            "states": {state: analyses[state]["privateBytes"] for state in ("open", "closed")},
            "unresolvedPositiveTrendStates": private_bytes_growth_trends,
            "baselineInconclusiveStates": private_bytes_unstable,
        },
        "settledWorkingSetCapFailures": [
            {"windowId": item["windowId"], "violations": item["strictWorkingSetCapViolations"]}
            for item in summaries if not item["strictWorkingSetCapPassed"]
        ],
        "operationPeakWorkingSetBytes": operation_peak,
        "operationPeakAdvisoryExclusiveBytes": OPERATION_WS_LIMIT,
        "operationPeakAdvisoryExceeded": operation_peak >= OPERATION_WS_LIMIT,
        "lifetimePeakWorkingSetBytesDiagnosticOnly": lifetime_peak,
        "handlesAndThreadsDiagnosticOnly": True,
        "strictSettledWorkingSetExclusiveBytes": STRICT_WS_LIMIT,
        "strictCapPassedForEverySettledSample": strict_cap_pass,
    }


def workspace_route_transitions(route_events: list[dict[str, Any]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    open_events = [
        event for event in route_events
        if event.get("fromPageId") == "initial_setup" and event.get("targetPageId") == "my_workspace"
    ]
    closed_events = [
        event for event in route_events
        if event.get("fromPageId") == "my_workspace" and event.get("targetPageId") == "initial_setup"
    ]
    return open_events, closed_events


def action_kind_count(actions: list[dict[str, Any]], action_kind: str) -> int:
    """Return zero for an action kind that is correctly absent from a route arm."""
    return sum(1 for action in actions if str(action.get("actionKind", "")) == action_kind)


def validate_route(root: Path, manifest: dict[str, Any], pid: int, hwnd: int, workspace: Path) -> dict[str, Any]:
    route = manifest.get("route", {})
    open_mode = str(route.get("openMode", ""))
    experiment = manifest.get("experiment", {})
    experiment_mode = str(experiment.get("mode", route.get("experimentMode", "standard")))
    fixture_size = str(experiment.get("fixtureSize", route.get("fixtureSize", manifest.get("fixture", {}).get("profileSize", "large"))))
    priming_count = int(experiment.get("primingCycleCount", route.get("primingCycleCount", 0)))
    measured_count = int(experiment.get("measuredCycleCount", route.get("measuredCycleCount", 10)))
    baseline_count = int(experiment.get("baselineWindowsPerState", route.get("baselineWindowsPerState", 5)))
    expected_identity = PROFILE_IDENTITIES.get(fixture_size)
    fixture_size_experiment = experiment_mode == "fixture-size-ab"
    expected_role = "primary-acceptance" if not fixture_size_experiment and open_mode == "dialog" else "diagnostic-only"
    require(open_mode in {"dialog", "recent"} and manifest.get("openMode") == open_mode
            and route.get("acceptanceRole") == expected_role and manifest.get("acceptanceRole") == expected_role,
            "Run must declare a matching dialog or recent open mode in its manifest and route.")
    require(expected_identity is not None and manifest.get("fixture", {}).get("profileSize", fixture_size) == fixture_size,
            "Route fixture size is unsupported or differs from the prepared profile.")
    if fixture_size_experiment:
        require(open_mode == "recent" and fixture_size in {"tiny", "large"}
                and experiment.get("kind") == "fixture-size-ab-diagnostic"
                and bool(experiment.get("pairId")) and priming_count == 3 and measured_count == 20 and baseline_count == 5,
                "Fixture-size A/B manifest must use Recent Files, a pair ID, three priming cycles, five baselines per state, and twenty measured cycles.")
    else:
        require(experiment_mode == "standard" and fixture_size == "large"
                and priming_count == 0 and measured_count == 10 and baseline_count == 5,
                "Standard P9-16 route must retain its pinned large fixture and ten measured cycles.")
    actions = route.get("actions", [])
    action_ref = route.get("actionLog", {})
    action_path = verify_artifact(root, action_ref.get("relativePath", ""), action_ref.get("sha256", ""))
    action_lines = [json.loads(line) for line in action_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(action_lines == actions, "UIA action log differs from manifest action sequence.")
    require(len(actions) >= 40, "UIA route log is too short to prove the repeated open/close path.")
    action_counts: dict[str, int] = {}
    for action in actions:
        kind = str(action.get("actionKind", ""))
        action_counts[kind] = action_counts.get(kind, 0) + 1
        target = action.get("target", {})
        require(
            int(target.get("processId", 0)) == pid
            and int(target.get("ownerWindowProcessId", 0)) == pid,
            f"UIA action {kind!r} is not bound to the measured PID.",
        )
        if action.get("mainWindowAction") is True:
            require(int(target.get("ownerWindowHandle", 0)) == hwnd, f"Main-window UIA action {kind!r} is bound to a different HWND.")
    cycles = route.get("cycles", [])
    require(len(cycles) == 1 + priming_count + measured_count,
            "Route evidence cycle count differs from its warm-up, priming, and measured plan.")
    require(cycles[0].get("cycleKind") == "warmup" and cycles[0].get("cycleNumber") == 0, "Warm-up cycle is not reported separately.")
    priming = [cycle for cycle in cycles if cycle.get("cycleKind") == "priming"]
    require(len(priming) == priming_count
            and [int(item.get("cycleNumber", 0)) for item in priming] == list(range(1, priming_count + 1)),
            "Priming cycles are missing, duplicated, or out of order.")
    measured = [cycle for cycle in cycles if cycle.get("cycleKind") == "measured"]
    require(len(measured) == measured_count
            and [int(item.get("cycleNumber", 0)) for item in measured] == list(range(1, measured_count + 1)),
            f"Measured cycles are not exactly 1 through {measured_count}.")
    route_log_ref = route.get("eventLog", {})
    route_path = verify_artifact(root, route_log_ref.get("relativePath", ""), route_log_ref.get("sha256", ""))
    route_events = [json.loads(line) for line in route_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(route_events == route.get("routeEvents", []), "Application route event log differs from manifest.")

    def matches_fixture_identity(value: Any) -> bool:
        return (
            isinstance(value, dict)
            and {key: value.get(key) for key in expected_identity} == expected_identity
            and re.fullmatch(r"[0-9a-f]{64}", str(value.get("workspaceTablesSha256", ""))) is not None
        )

    open_events, closed_events = workspace_route_transitions(route_events)
    expected_opens = 2 + priming_count + measured_count
    expected_closes = 1 + priming_count + measured_count
    require(len(open_events) == expected_opens and len(closed_events) == expected_closes,
            "Route/event logs do not include the warm-up, priming, baseline-entry, and measured transitions.")
    for cycle in cycles:
        close = cycle.get("close", {})
        opened = cycle.get("open", {})
        require(close.get("state") == "closed" and opened.get("state") == "open", "A cycle did not verify Closed then My Workspace state.")
        require(close.get("closeActionEnabled") is False and opened.get("closeActionEnabled") is True,
                "Close Teacher Profile enabled state does not match workspace state.")
        require(close.get("currentPage") == "Initial Setup" and opened.get("currentPage") == "My Workspace",
                "Route UIA page identity differs from the closed/open workspace state.")
        require(close.get("mainWindowHandle") == hwnd == opened.get("mainWindowHandle"),
                "Main HWND changed during a repeated close/reopen cycle.")
        require(close.get("processId") == pid == opened.get("processId"), "Process PID changed during a cycle.")
        opened_path = str(opened.get("selectedWorkspacePath", ""))
        require(Path(opened_path).resolve() == workspace, "An open action selected a different .tps fixture.")
        expected_open_method = "dialog" if cycle.get("cycleKind") == "warmup" or open_mode == "dialog" else "recent"
        expected_open_action = "open-teacher-profile" if expected_open_method == "dialog" else "open-recent-workspace"
        require(opened.get("openMethod") == expected_open_method,
                "A cycle opened through a route different from the declared P9-16 arm.")
        require(matches_fixture_identity(opened.get("identity")), "An open state lacks the fixture-size database identity.")
        require(close.get("cleanStateCheck", {}).get("passed") is True and opened.get("cleanStateCheck", {}).get("passed") is True,
                "A cycle state did not prove the clean, dialog-free route condition.")
        require(close.get("cleanStateCheck", {}).get("noUserEditsPerformed") is True,
                "A close was not preceded by the no-edits check.")
        require(close.get("closedWithoutSavePrompt") is True and close.get("cleanStateCheck", {}).get("closedWithoutSavePrompt") is True,
                "A close did not complete directly without a save prompt.")
        close_event = close.get("routeEvent", {})
        open_event = opened.get("routeEvent", {})
        require(close_event.get("targetPageId") == "initial_setup" and open_event.get("targetPageId") == "my_workspace",
                "UIA state observations are not paired with the expected application route events.")
        close_index = int(cycle.get("closeActionIndex", -1))
        open_index = int(cycle.get("openActionIndex", -1))
        require(0 <= close_index < len(actions) and 0 <= open_index < len(actions), "A cycle action reference is outside the UIA action log.")
        require(actions[close_index].get("actionKind") == "close-teacher-profile" and actions[open_index].get("actionKind") == expected_open_action,
                "A cycle action reference does not point to the declared File open route and exact Close QAction.")
        if cycle.get("cycleKind") in {"warmup", "priming"}:
            require(open_index < close_index and int(open_event.get("arrivalIndex", 0)) < int(close_event.get("arrivalIndex", 0)),
                    "Warm-up or priming cycle is not recorded as open then close.")
        else:
            require(close_index < open_index and int(close_event.get("arrivalIndex", 0)) < int(open_event.get("arrivalIndex", 0)),
                    "Measured cycle is not recorded as close then reopen.")
    baseline_entry = route.get("baselineOpenEntry", {})
    require(
        baseline_entry.get("selectedWorkspacePath")
        and Path(str(baseline_entry["selectedWorkspacePath"])).resolve() == workspace
        and baseline_entry.get("currentPage") == "My Workspace"
        and baseline_entry.get("closeActionEnabled") is True
        and baseline_entry.get("openMethod") == ("dialog" if open_mode == "dialog" else "recent")
        and matches_fixture_identity(baseline_entry.get("identity")),
        "Baseline open state did not use the same identified fixture.",
    )
    require(
        route.get("initialStartupState", {}).get("currentPage") == "Initial Setup"
        and route.get("initialStartupState", {}).get("closeActionEnabled") is False,
        "Process did not begin in a clean Initial Setup state with Close disabled.",
    )
    dialogs = route.get("fileDialogSessions", [])
    expected_dialog_count = expected_opens if open_mode == "dialog" else 1
    require(len(dialogs) == expected_dialog_count,
            "File-dialog session count differs from the declared dialog or recent diagnostic arm.")
    for dialog in dialogs:
        automation_id = str(dialog.get("automationId", ""))
        require(
            (automation_id == "classmngrNextFileDialog" or automation_id.endswith(".classmngrNextFileDialog"))
            and dialog.get("controlType") == "ControlType.Window"
            and dialog.get("className") == "QFileDialog"
            and dialog.get("processId") == pid
            and dialog.get("ownerWindowHandle") != hwnd
            and Path(str(dialog.get("selectedWorkspacePath", ""))).resolve() == workspace,
            "A file-open interaction lacks the exact isolated non-native QFileDialog and fixture identity.",
        )
        field = dialog.get("fileNameControl", {})
        button = dialog.get("openButtonControl", {})
        require(
            str(field.get("automationId", "")).endswith("fileNameEdit")
            and field.get("className") == "QLineEdit"
            and int(field.get("ownerWindowHandle", 0)) == int(dialog.get("ownerWindowHandle", -1))
            and button.get("name") == "Open"
            and button.get("className") == "QPushButton"
            and button.get("controlType") == "ControlType.Button"
            and int(button.get("ownerWindowHandle", 0)) == int(dialog.get("ownerWindowHandle", -1)),
            "File dialog path and Open button UIA controls are not bound to the isolated QFileDialog HWND.",
        )
    expected_recent_count = 0 if open_mode == "dialog" else priming_count + 1 + measured_count
    expected_open_action_count = expected_dialog_count if open_mode == "dialog" else 1
    require(action_counts.get("close-teacher-profile") == expected_closes,
            "UIA Close Teacher Profile action count differs from the declared route plan.")
    require(action_counts.get("open-teacher-profile") == expected_open_action_count,
            "File > Open Teacher Profile action count differs from the declared P9-16 route arm.")
    require(action_counts.get("set-workspace-dialog-path") == expected_dialog_count,
            "Every expected QFileDialog session must enter the exact fixture path through UIA.")
    require(action_counts.get("confirm-workspace-file-dialog") == expected_dialog_count,
            "Every expected QFileDialog session must confirm through its exact Open button.")
    require(action_kind_count(actions, "open-recent-workspace") == expected_recent_count,
            "Recent Files UIA action count differs from the declared P9-16 route arm.")
    for action in actions:
        if action.get("actionKind") == "open-teacher-profile":
            require(
                str(action.get("target", {}).get("automationId", "")).endswith("classMngrNextOpenTeacherProfile")
                and action.get("target", {}).get("ownerWindowHandle") == hwnd,
                "File > Open Teacher Profile action is not the exact main-window QAction.",
            )
        elif action.get("actionKind") == "close-teacher-profile":
            require(
                str(action.get("target", {}).get("automationId", "")).endswith("classMngrNextCloseWorkspace")
                and action.get("target", {}).get("ownerWindowHandle") == hwnd,
                "File > Close Teacher Profile action is not the exact main-window QAction.",
            )
        elif action.get("actionKind") == "set-workspace-dialog-path":
            require(
                str(action.get("target", {}).get("automationId", "")).endswith("fileNameEdit")
                and action.get("target", {}).get("className") == "QLineEdit"
                and int(action.get("target", {}).get("ownerWindowHandle", 0)) != hwnd
                and Path(str(action.get("valueAfter", ""))).resolve() == workspace,
                "Open action did not enter the exact fixture path in the QFileDialog path control.",
            )
        elif action.get("actionKind") == "confirm-workspace-file-dialog":
            require(
                action.get("target", {}).get("name") == "Open"
                and action.get("target", {}).get("className") == "QPushButton"
                and int(action.get("target", {}).get("ownerWindowHandle", 0)) != hwnd,
                "Open Teacher Profile did not confirm selection through the dialog's exact Open button.",
            )
        elif action.get("actionKind") == "open-recent-workspace":
            target = action.get("target", {})
            if action.get("recentItemNameMatchesPath") is True:
                item_path_matches = Path(str(action.get("recentItemNamePath", ""))).resolve() == workspace
            else:
                item_path_matches = (
                    action.get("recentItemHelpTextMatchesPath") is True
                    and str(action.get("recentItemHelpText", "")) == str(workspace)
                )
            require(
                Path(str(action.get("selectedWorkspacePath", ""))).resolve() == workspace
                and action.get("recentFilesMenuName") == "Recent Files"
                and action.get("recentFilesMenuExpanded") is True
                and item_path_matches
                and action.get("recentItemDiscoveredUnderRecentMenu") is True
                and target.get("controlType") == "ControlType.MenuItem"
                and int(target.get("processId", 0)) == pid
                and int(target.get("ownerWindowProcessId", 0)) == pid,
                "Recent Files did not invoke the exact PID-bound dynamic menu item for the pinned absolute fixture path.",
            )

    recent_sessions = route.get("recentOpenSessions", [])
    require(len(recent_sessions) == expected_recent_count,
            "Recent Files evidence sessions differ from the declared P9-16 route arm.")
    expected_recent_purposes = [] if open_mode == "dialog" else [
        *[f"priming-{index}" for index in range(1, priming_count + 1)],
        "baseline-entry",
        *[f"cycle-{index}" for index in range(1, measured_count + 1)],
    ]
    require([str(item.get("purpose", "")) for item in recent_sessions] == expected_recent_purposes,
            "Recent Files opens are not the baseline entry followed by measured cycles 1 through 10.")
    expected_recent_action_indices = []
    if open_mode == "recent":
        expected_recent_action_indices.extend(
            int(cycle.get("openActionIndex", -1))
            for cycle in priming
        )
        baseline_open_index = int(baseline_entry.get("openActionIndex", -1))
        require(0 <= baseline_open_index < len(actions)
                and actions[baseline_open_index].get("actionKind") == "open-recent-workspace",
                "Recent baseline entry does not reference the UIA Recent Files action.")
        expected_recent_action_indices.append(baseline_open_index)
        expected_recent_action_indices.extend(int(cycle.get("openActionIndex", -1)) for cycle in measured)
        require(
            len(dialogs) == 1 and dialogs[0].get("purpose") == "warmup",
            "Recent diagnostic arm must use exactly one dialog warm-up and no further QFileDialog sessions.",
        )
    require(
        [int(item.get("openActionIndex", -1)) for item in recent_sessions] == expected_recent_action_indices,
        "Recent Files session action references differ from the baseline-entry and ten measured opens.",
    )
    for session in recent_sessions:
        require(
            Path(str(session.get("selectedWorkspacePath", ""))).resolve() == workspace
            and session.get("processId") == pid
            and session.get("mainWindowHandle") == hwnd
            and session.get("recentFilesMenuName") == "Recent Files"
            and session.get("recentFilesMenuExpanded") is True
            and session.get("recentItemDiscoveredUnderRecentMenu") is True
            and (session.get("recentItemNameMatchesPath") is True or session.get("recentItemHelpTextMatchesPath") is True),
            "Recent Files session does not identify the exact fixture path and measured process.",
        )
        control = session.get("recentItemControl", {})
        require(
            control.get("controlType") == "ControlType.MenuItem"
            and control.get("isEnabled") is True
            and control.get("isOffscreen") is False
            and int(control.get("processId", 0)) == pid
            and int(control.get("ownerWindowProcessId", 0)) == pid,
            "Recent Files dynamic item lacks a visible, enabled, same-process UIA MenuItem control.",
        )
        capture_ref = session.get("uiTree", {})
        capture_path = verify_artifact(root, capture_ref.get("relativePath", ""), capture_ref.get("sha256", ""))
        capture = read_json(capture_path)
        require(Path(str(capture.get("expectedWorkspacePath", ""))).resolve() == workspace,
                "Recent Files UIA capture identifies a different expected fixture path.")
        menu_control = capture.get("recentFilesMenu", {})
        require(
            menu_control.get("name") == "Recent Files"
            and menu_control.get("controlType") == "ControlType.MenuItem"
            and int(menu_control.get("processId", 0)) == pid
            and int(menu_control.get("ownerWindowProcessId", 0)) == pid,
            "Recent Files UIA capture is not rooted at the measured Recent Files submenu.",
        )
        selected = capture.get("selectedItem")
        if isinstance(selected, list):
            require(len(selected) == 1, "Recent Files UIA capture does not contain exactly one selected dynamic menu item.")
            selected = selected[0]
        require(
            isinstance(selected, dict)
            and (Path(str(selected.get("namePath", ""))).resolve() == workspace or
                 selected.get("helpText") == str(workspace)),
            "Recent Files UIA capture does not bind the selected dynamic item to the pinned absolute path.",
        )
        visible_items = capture.get("visibleMenuItems", [])
        exact_candidates = [
            item for item in visible_items
            if item.get("controlType") == "ControlType.MenuItem"
            and (item.get("nameMatchesExpectedPath") is True or item.get("helpTextMatchesPath") is True)
        ]
        require(len(exact_candidates) == 1,
                "Recent Files UIA capture does not prove a unique dynamic menu item for the exact fixture path.")
        session_action = actions[int(session.get("openActionIndex", -1))]
        require(
            session_action.get("actionKind") == "open-recent-workspace"
            and session_action.get("recentMenuCapture") == capture_ref
            and session_action.get("recentItemName") == session.get("recentItemName")
            and session_action.get("selectedWorkspacePath") == session.get("selectedWorkspacePath"),
            "Recent Files route action does not match its captured dynamic menu selection.",
        )
    require(route.get("dedicatedWorkspaceCloseLifecycleEventClaimed") is False,
            "P9-16 must not claim a dedicated workspace-close lifecycle event.")
    return {
        "openMode": open_mode,
        "experimentMode": experiment_mode,
        "fixtureSize": fixture_size,
        "experimentPairId": experiment.get("pairId"),
        "acceptanceRole": expected_role,
        "cycleCount": len(cycles),
        "warmupCycleCount": 1,
        "primingCycleCount": priming_count,
        "measuredCycleCount": measured_count,
        "uiAOpenActions": action_counts.get("open-teacher-profile", 0),
        "uiACloseActions": action_counts.get("close-teacher-profile", 0),
        "nonNativeDialogSessions": len(dialogs),
        "recentOpenActions": action_counts.get("open-recent-workspace", 0),
        "workspaceOpenRouteEvents": len(open_events),
        "workspaceCloseRouteEvents": len(closed_events),
        "processId": pid,
        "mainWindowHandle": hwnd,
        "fixturePath": str(workspace),
        "cleanPageBeforeEveryClose": True,
        "closeLifecycleEventClaimed": False,
    }


def validate_process(root: Path, manifest: dict[str, Any], package: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process", {})
    pid, hwnd = int(process.get("processId", 0)), int(process.get("mainWindowHandle", 0))
    require(pid > 0 and hwnd > 0 and int(process.get("windowProcessId", 0)) == pid and int(process.get("hwndProcessId", 0)) == pid,
            "Measured packaged process PID/HWND identity is missing or inconsistent.")
    require(
        process.get("architecture") == "x64"
        and str(Path(process.get("processImagePath", "")).resolve()) == package["applicationPath"]
        and process.get("processImageSha256") == package["applicationSha256"],
        "Live process image differs from the verified x64 packaged executable.",
    )
    fixture_path = Path(str(manifest.get("fixture", {}).get("workspacePath", ""))).resolve()
    require(str(Path(process.get("processImagePath", "")).resolve()) == package["applicationPath"], "Process image is not the accepted package executable.")
    require(process.get("startedWithoutWorkspaceArgument") is True, "Run did not start in a clean no-workspace Initial Setup state.")
    require(
        str(Path(process.get("workspacePath", "")).resolve())
        == str(Path(manifest.get("fixture", {}).get("workspacePath", "")).resolve()),
        "Process evidence identifies a different workspace fixture path.",
    )
    exit_record = manifest.get("processExit", {})
    require(
        exit_record.get("observed") is True
        and exit_record.get("normalExit") is True
        and exit_record.get("exitCode") == 0
        and exit_record.get("closeMethod") == "WindowPattern.Close"
        and exit_record.get("forcedTermination") is False,
        "Packaged process did not exit naturally with code 0.",
    )
    require(process.get("forcedTermination") is False, "Process record claims forced termination.")
    require(str(fixture_path) == str(Path(str(manifest.get("fixture", {}).get("workspacePath", ""))).resolve()), "Fixture path normalization failed.")
    for key in ("stdoutPath", "stderrPath", "processMetricsPath"):
        require(contained_path(root, str(process.get(key, ""))).is_file(), f"Process {key} log is missing.")
    metrics_ref = process.get("metrics", {})
    metrics_path = verify_artifact(root, metrics_ref.get("path", ""), metrics_ref.get("sha256", ""))
    require(metrics_path.is_file(), "Process metric log is unavailable.")
    return {
        "processId": pid,
        "mainWindowHandle": hwnd,
        "architecture": "x64",
        "exitCode": 0,
        "naturalExit": True,
        "forcedTermination": False,
    }


def validate_workspace_sidecars(root: Path, manifest: dict[str, Any]) -> None:
    require(manifest.get("cleanup", {}).get("processExited") is True, "Cleanup did not observe process exit.")
    require(manifest.get("cleanup", {}).get("forcedTermination") is False, "Cleanup claims forced termination.")
    require(manifest.get("cleanup", {}).get("workspaceSidecarFilesAfterExit") == [], "SQLite sidecar files remain after natural exit.")
    fixture = manifest.get("fixture", {})
    for suffix in ("-wal", "-shm", "-journal"):
        require(not Path(str(fixture.get("workspacePath", "")) + suffix).exists(), f"Workspace sidecar remains after exit: {suffix}")


PAGE_LIFECYCLE_MARKER = "ClassMngrNext page lifecycle checkpoint="


def analyze_application_diagnostics(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    references = manifest.get("applicationDiagnostics", {}) or {}
    application_reference = references.get("applicationOutput", {}) if isinstance(references, dict) else {}
    if application_reference.get("relativePath"):
        application_path = verify_artifact(root, application_reference["relativePath"], application_reference.get("sha256", ""))
        application_integrity = "verified"
    else:
        application_path = contained_path(root, "application-output.jsonl")
        require(application_path.is_file(), "Application diagnostic output log is missing.")
        application_integrity = "legacy-unpinned"

    stderr_reference = references.get("processStderr", {}) if isinstance(references, dict) else {}
    process = manifest.get("process", {})
    stderr_relative = stderr_reference.get("relativePath") or process.get("stderrPath", "logs/process-stderr.log")
    if stderr_reference.get("relativePath"):
        stderr_path = verify_artifact(root, stderr_relative, stderr_reference.get("sha256", ""))
        stderr_integrity = "verified"
    else:
        stderr_path = contained_path(root, str(stderr_relative))
        require(stderr_path.is_file(), "Process stderr diagnostic log is missing.")
        stderr_integrity = "legacy-unpinned"

    lifecycle: list[dict[str, Any]] = []
    application_rows: list[dict[str, Any]] = []
    for line_number, raw in enumerate(application_path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw.strip():
            continue
        try:
            row = json.loads(raw)
        except json.JSONDecodeError as error:
            raise ValidationError(f"Application output JSONL line {line_number} is invalid JSON.") from error
        require(isinstance(row, dict) and isinstance(row.get("line"), str),
                f"Application output line {line_number} has an invalid record shape.")
        application_rows.append(row)
        text = row["line"]
        if PAGE_LIFECYCLE_MARKER in text:
            payload = text.split(PAGE_LIFECYCLE_MARKER, 1)[1]
            try:
                record = json.loads(payload)
            except json.JSONDecodeError as error:
                raise ValidationError(f"Page lifecycle checkpoint in application output line {line_number} is invalid JSON.") from error
            require(isinstance(record, dict) and record.get("event") in {"page-lifecycle", "page-instance-destroyed"}
                    and record.get("page_key") in {"initial_setup", "my_workspace"},
                    f"Application output line {line_number} has an invalid page lifecycle checkpoint.")
            lifecycle.append({
                "arrivalIndex": row.get("arrivalIndex"),
                "stream": row.get("stream"),
                "lineNumber": line_number,
                "checkpoint": record,
            })

    remove_mentions: list[dict[str, Any]] = []
    in_use_warnings: list[dict[str, Any]] = []
    for source, path in (("application-output.jsonl", application_path), ("process-stderr.log", stderr_path)):
        for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            line = raw
            arrival_index = None
            if source == "application-output.jsonl":
                try:
                    row = json.loads(raw)
                except json.JSONDecodeError:
                    continue
                line = str(row.get("line", ""))
                arrival_index = row.get("arrivalIndex")
            lowered = line.casefold()
            if "removedatabase" not in lowered or not re.search(r"qsqldatabase(?:private)?::removedatabase", lowered):
                continue
            item = {"source": source, "lineNumber": line_number, "arrivalIndex": arrival_index, "line": line}
            remove_mentions.append(item)
            if re.search(r"still\s+in\s+use|\bin\s+use\b|queries\s+will\s+cease", lowered):
                in_use_warnings.append(item)

    event_counts: dict[str, dict[str, int]] = {}
    page_metrics: dict[str, dict[str, Any]] = {}
    truncation_counts: dict[str, int] = {}
    for item in lifecycle:
        record = item["checkpoint"]
        page_key = str(record["page_key"])
        event_state = str(record.get("state", "destroyed" if record.get("event") == "page-instance-destroyed" else "unknown"))
        event_key = f"{record.get('event')}:{event_state}"
        page_counts = event_counts.setdefault(page_key, {})
        page_counts[event_key] = page_counts.get(event_key, 0) + 1
        page = record.get("page", {}) if isinstance(record.get("page"), dict) else {}
        summary = page_metrics.setdefault(page_key, {"checkpointCount": 0, "availableInstanceCount": 0, "metricObservationCounts": {}, "metricRanges": {}, "modelClassObservationCounts": {}})
        summary["checkpointCount"] += 1
        summary["availableInstanceCount"] += int(record.get("page_instance_available") is True)
        for metric in ("widget_count", "item_model_count", "item_model_root_rows", "table_widget_count", "table_item_count", "editor_control_widget_count", "view_index_widget_count"):
            value = page.get(metric)
            if isinstance(value, int):
                counts = summary["metricObservationCounts"]
                counts[metric] = counts.get(metric, 0) + 1
                bounds = summary["metricRanges"].setdefault(metric, [value, value])
                bounds[0] = min(bounds[0], value)
                bounds[1] = max(bounds[1], value)
        models = page.get("item_models")
        if isinstance(models, list):
            for model in models:
                class_name = str(model.get("class_name", "unknown"))
                summary["modelClassObservationCounts"][class_name] = summary["modelClassObservationCounts"].get(class_name, 0) + 1
        for flag in ("item_models_truncated", "table_scan_truncated", "widget_scan_truncated", "view_index_widget_scan_truncated"):
            if page.get(flag) is True:
                truncation_counts[flag] = truncation_counts.get(flag, 0) + 1

    return {
        "applicationOutput": {"relativePath": str(application_path.relative_to(root)), "sha256": sha256_file(application_path), "integrity": application_integrity, "recordCount": len(application_rows)},
        "processStderr": {"relativePath": str(stderr_path.relative_to(root)), "sha256": sha256_file(stderr_path), "integrity": stderr_integrity},
        "pageLifecycleCheckpointCount": len(lifecycle),
        "pageInstanceAndLifecycleEventCounts": event_counts,
        "pageModelMetricRangesAndObservationCounts": page_metrics,
        "truncationTrueCounts": truncation_counts,
        "pageLifecycleCheckpoints": lifecycle,
        "sqliteRemoveDatabaseMentions": remove_mentions,
        "sqliteRemoveDatabaseInUseWarnings": in_use_warnings,
        "warningInterpretation": "Warnings are reported from captured log text only; zero mentions does not prove absence of unlogged database ownership issues.",
    }


def overall_run_status(open_mode: str, memory_status: str) -> str:
    if open_mode == "recent" and memory_status == "passed":
        return "diagnostic-only"
    return memory_status


def validate_run(evidence_dir: Path) -> dict[str, Any]:
    root = evidence_dir.resolve(strict=True)
    manifest_path = root / "manifest.json"
    manifest = read_json(manifest_path)
    require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID, "Manifest schema/task ID is invalid.")
    require(manifest.get("status") in {"evidence-collected", "passed", "inconclusive", "failed"}
            and manifest.get("failure") is None,
            "P9-16 run is incomplete or contains an execution/evidence-collection failure.")
    manifest["_evidenceRoot"] = str(root)
    package = validate_package_provenance(root, manifest)
    identity = validate_process(root, manifest, package)
    pid, hwnd = identity["processId"], identity["mainWindowHandle"]
    fixture = validate_fixture(root, manifest)
    workspace = Path(fixture["workspace"]).resolve()
    settings = validate_settings(root, manifest)
    route = validate_route(root, manifest, pid, hwnd, workspace)
    memory = analyze_memory(manifest, pid, hwnd)
    validate_workspace_sidecars(root, manifest)
    application_diagnostics = analyze_application_diagnostics(root, manifest)
    result_status = overall_run_status(str(manifest.get("openMode", "")), memory["status"])
    return {
        "taskId": TASK_ID,
        "openMode": manifest.get("openMode"),
        "acceptanceRole": manifest.get("acceptanceRole"),
        "status": result_status,
        "overallStatus": result_status,
        "evidenceIntegrityAndProvenanceStatus": "passed",
        "evidenceValidationStatus": "passed",
        "memoryStatus": memory["status"],
        "workingSetNoGrowthStatus": memory["workingSetNoGrowthStatus"],
        "strictWorkingSetCapStatus": memory["strictWorkingSetCapStatus"],
        "privateBytesTrend": memory["privateBytesTrend"],
        "evidenceDirectory": str(root),
        "package": package,
        "process": identity,
        "fixture": fixture,
        "settings": settings,
        "route": route,
        "memory": memory,
        "applicationDiagnostics": application_diagnostics,
        "limitations": [
            "Current Next does not emit a dedicated workspace-close lifecycle event; route, UIA enabled state, SQLite snapshots, and process samples provide the evidence.",
            "DB equality proves persisted table/schema/state identity only; it does not claim hidden object, SQLite cache, or allocator release.",
            "Working Set is the strict settled normal-use cap; operation peak is advisory, lifetime peak is diagnostic only, and handles/threads are diagnostics.",
        ],
    }


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = validate_run(args.evidence_dir)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        if result["status"] in {"passed", "diagnostic-only"}:
            return 0
        if result["status"] == "inconclusive":
            return 2
        return 1
    except (ValidationError, OSError, KeyError, TypeError, ValueError) as error:
        print(f"P9-16 validation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
