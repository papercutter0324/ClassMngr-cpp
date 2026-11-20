#!/usr/bin/env python3
"""Fail-closed validator for the two P9-14 packaged Windows lifecycle routes."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA = "classmngr-phase9-p9-14-packaged-lifecycle-v1"
PREPARATION_SCHEMA = "classmngr-phase9-p9-14-workspace-v1"
STRICT_WS_LIMIT = 262_144_000
OPERATION_WS_LIMIT = 536_870_912
STAFF_SOURCE_WORKSPACE_RELATIVE_PATH = "docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/generated-large-staff-directory.tps"
STAFF_SOURCE_WORKSPACE_SHA256 = "d60176721613ef7eef127856a95e51805c7a9b12671353b35289a5e1e8ee7363"
NEW_SOURCE_PATHS = {
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/prepare_p9_14_staff_ai_workspaces.py",
    "scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1",
    "scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_p9_14_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py",
}


class ValidationError(ValueError):
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


def inventory_fingerprint(rows: list[dict[str, Any]]) -> str:
    ordered = sorted(rows, key=lambda row: str(row["path"]).lower())
    canonical = "\n".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}"
        for row in ordered
    ) + "\n"
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def contained_path(root: Path, relative: str) -> Path:
    candidate = PurePosixPath(relative)
    require(not candidate.is_absolute() and ".." not in candidate.parts, f"Artifact path is not a safe relative path: {relative!r}")
    target = (root / Path(*candidate.parts)).resolve(strict=True)
    root_resolved = root.resolve(strict=True)
    require(target == root_resolved or root_resolved in target.parents, f"Artifact escaped its evidence directory: {relative}")
    require(target.is_file(), f"Artifact is not a file: {relative}")
    return target


def verify_artifact(root: Path, relative: str, expected_sha256: str) -> Path:
    target = contained_path(root, relative)
    actual = sha256_file(target)
    require(actual == expected_sha256.lower(), f"Artifact SHA-256 mismatch for {relative}: {actual}")
    return target


def verify_absolute_file(path_text: str, expected_sha256: str, label: str) -> Path:
    path = Path(path_text).resolve(strict=True)
    require(path.is_file(), f"{label} is not a file: {path}")
    actual = sha256_file(path)
    require(actual == expected_sha256.lower(), f"{label} SHA-256 mismatch: {path}")
    return path


def deterministic_comment(index: int) -> str:
    letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    zero_based = index - 1
    name = f"AI Candidate {letters[zero_based // len(letters)]}{letters[zero_based % len(letters)]}"
    return (
        f"P9-14 review for {name}: your speaking shows a clear central idea and steady organization. "
        "Next, add one precise supporting detail and explain how it connects to your main point. "
        "This will help your listener follow the reasoning and understand why the example matters."
    )


def is_valid_ai_review_status(status: str) -> bool:
    return status in {"Ready", "Ready — name placeholder was omitted"}


def expected_ai_rows() -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    hangul = "가나다라마바사아자차"
    for index in range(1, 101):
        rows.append(
            {
                "rowIndex": index - 1,
                "studentId": f"STUDENT_{index:02d}",
                "englishName": f"AI Candidate {chr(ord('A') + ((index - 1) // 26))}{chr(ord('A') + ((index - 1) % 26))}",
                "koreanName": f"{hangul[(index - 1) // 10]}{hangul[(index - 1) % 10]}가",
                "grade": "E4",
                "didWell": f"Explains one idea clearly in candidate {index:03d}.",
                "needsImprovement": f"Add a supporting detail for candidate {index:03d}.",
                "expectedComment": deterministic_comment(index) if index <= 25 else "",
            }
        )
    return rows


def validate_staff_snapshots(preparation: dict[str, Any], before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    require(preparation.get("kind") == "staff-directory", "Staff preparation receipt has the wrong fixture kind.")
    expected_by_key = {
        "nativeEnglish": preparation.get("nativeEnglish", {}).get("rows"),
        "gsTeam": preparation.get("gsTeam", {}).get("rows"),
    }
    require(all(isinstance(rows, list) and len(rows) == 96 for rows in expected_by_key.values()), "Staff preparation receipt must retain both exact 96-row identity lists.")
    for key, expected_rows in expected_by_key.items():
        expected_ids = [int(row["id"]) for row in expected_rows]
        require(expected_ids == list(range(1, 97)), f"Prepared {key} row identities are not the exact primary keys 1 through 96.")
        for snapshot, phase in ((before, "before"), (after, "after")):
            table = snapshot.get("tables", {}).get(key, {})
            require(table.get("rowCount") == 96 and table.get("rows") == expected_rows, f"{phase} {key} snapshot differs from its exact 96-row fixture identities.")
    require(before["tables"] == after["tables"], "Staff Directory data changed between before and after snapshots.")
    return {"nativeEnglishRows": 96, "gsTeamRows": 96, "identitiesVerifiedBeforeAndAfter": True}


def validate_ai_snapshots(preparation: dict[str, Any], before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    require(preparation.get("kind") == "speaking-evaluation-ai-batch", "AI preparation receipt has the wrong fixture kind.")
    expected_students = preparation.get("rows")
    require(isinstance(expected_students, list) and len(expected_students) == 100, "AI preparation receipt must retain the exact 100 candidate identities.")
    require(expected_students == expected_ai_rows(), "AI candidate identity, observations, selection order, or expected comments differ from the deterministic fixture contract.")
    expected_evaluation = {"id": 1, "classId": 1, "name": "Winter", "grade": "E4"}
    for snapshot, phase in ((before, "before"), (after, "after")):
        require(snapshot.get("evaluation") == expected_evaluation, f"{phase} AI evaluation identity, selected class, name, or supported grade changed.")
        students = snapshot.get("students")
        require(isinstance(students, list) and len(students) == 100 and snapshot.get("rowCount") == 100, f"{phase} AI snapshot does not contain exactly 100 candidate rows.")
        for index, (actual, expected) in enumerate(zip(students, expected_students, strict=True), start=1):
            require(
                (actual.get("rowIndex"), actual.get("englishName"), actual.get("koreanName"))
                == (expected["rowIndex"], expected["englishName"], expected["koreanName"]),
                f"{phase} AI candidate identity changed at row {index - 1}.",
            )
            expected_comment = "" if phase == "before" or index > 25 else deterministic_comment(index)
            require(actual.get("comment") == expected_comment, f"{phase} AI comment state differs at row {index - 1}.")
            expected_notes = f"[Did Well]\n{expected['didWell']}\n[Needs Improvement]\n{expected['needsImprovement']}"
            require(actual.get("notes") == expected_notes, f"{phase} AI observation cells changed at row {index - 1}.")

    baseline_columns = preparation.get("baselineSpeakingEvalDataColumns")
    baseline_rows = preparation.get("baselineSpeakingEvalDataRows")
    require(isinstance(baseline_columns, list) and isinstance(baseline_rows, list), "AI fixture receipt is missing the full speaking_eval_data baseline.")
    require(before.get("allDataColumns") == baseline_columns and before.get("allDataRows") == baseline_rows, "Before snapshot is not the exact full speaking_eval_data fixture baseline.")
    expected_after_rows = [list(row) for row in baseline_rows]
    require(len(baseline_columns) == 13 and baseline_columns[11] == "col_9", "AI full-table fixture columns do not identify comment cells at column 9.")
    for row in expected_after_rows:
        if int(row[0]) == 1 and 0 <= int(row[1]) < 25:
            row[11] = deterministic_comment(int(row[1]) + 1)
    require(after.get("allDataColumns") == baseline_columns and after.get("allDataRows") == expected_after_rows, "After snapshot changed a nonselected comment or any other Speaking Evaluation cell.")
    require(before.get("speakingEvalDataRowsSha256") == preparation.get("speakingEvalDataRowsSha256"), "Before full Speaking Evaluation row hash differs from the preparation baseline.")
    return {"candidateRows": 100, "selectedCommentRows": list(range(25)), "preservedCandidateRows": list(range(25, 100)), "allOtherSpeakingEvaluationCellsPreserved": True}


def validate_response_payload(text: str, expected_students: list[dict[str, Any]] | None = None) -> list[dict[str, Any]]:
    expected_students = expected_students or expected_ai_rows()
    matches = list(re.finditer(r"<<<(STUDENT_\d{2,3})>>>\n([^\n]+)\n<<<END_\1>>>", text))
    require(len(matches) == 25, f"Deterministic response contains {len(matches)} complete blocks instead of exactly 25.")
    require(matches[0].start() == 0 and matches[-1].end() == len(text), "Response contains unparsed text outside its 25 student blocks.")
    blocks: list[dict[str, Any]] = []
    for index, match in enumerate(matches, start=1):
        student_id, comment = match.group(1), match.group(2)
        expected = expected_students[index - 1]
        require(student_id == expected["studentId"], f"Response block {index} targets {student_id}, expected {expected['studentId']}.")
        require(comment == deterministic_comment(index), f"Response block {student_id} differs from the deterministic local comment.")
        require(100 <= len(comment) <= 420, f"Response block {student_id} comment has {len(comment)} characters outside the 100–420 bound.")
        if index < 25:
            require(match.end() + 1 == matches[index].start(), f"Response block {student_id} is not followed by exactly one newline separator.")
        blocks.append({"studentId": student_id, "commentCharacters": len(comment), "comment": comment})
    return blocks


def verify_inventory_file(root: Path, inventory_path: Path, expected_inventory_hash: str, label: str, verify_tree: bool) -> dict[str, Any]:
    inventory = read_json(inventory_path)
    files = inventory.get("files")
    require(isinstance(files, list), f"{label} inventory is missing its file rows.")
    require(inventory_fingerprint(files) == inventory.get("sha256"), f"{label} inventory fingerprint is internally inconsistent.")
    require(inventory.get("sha256") == expected_inventory_hash, f"{label} inventory differs from its accepted fingerprint.")
    require(int(inventory.get("fileCount", -1)) == len(files), f"{label} inventory file count is inconsistent.")
    if verify_tree:
        base = Path(str(inventory.get("root", ""))).resolve(strict=True)
        actual_rows: list[dict[str, Any]] = []
        for item in files:
            relative = PurePosixPath(str(item["path"]))
            require(not relative.is_absolute() and ".." not in relative.parts, f"{label} inventory has an unsafe path: {item['path']}")
            target = (base / Path(*relative.parts)).resolve(strict=True)
            require(base == target.parent or base in target.parents, f"{label} inventory file escaped its root: {relative}")
            require(target.is_file(), f"{label} inventory target is not a file: {target}")
            require(int(item["sizeBytes"]) == target.stat().st_size and str(item["sha256"]).lower() == sha256_file(target), f"{label} file differs from its inventory: {relative}")
            actual_rows.append({"path": relative.as_posix(), "sizeBytes": target.stat().st_size, "sha256": sha256_file(target)})
        actual_names = {row["path"] for row in actual_rows}
        ignored = {"__pycache__", ".pytest_cache", ".mypy_cache"}
        disk_names = {
            item.relative_to(base).as_posix()
            for item in base.rglob("*")
            if item.is_file() and not any(part in ignored for part in item.relative_to(base).parts)
            and item.suffix not in {".pyc", ".pyo"}
        }
        require(actual_names == disk_names, f"{label} tree has missing or unlisted files.")
        require(inventory_fingerprint(actual_rows) == expected_inventory_hash, f"{label} tree fingerprint changed after capture.")
    return inventory


def verify_pe_x64(path: Path) -> None:
    with path.open("rb") as stream:
        require(stream.read(2) == b"MZ", "Measured package application has no DOS executable signature.")
        stream.seek(0x3C)
        raw_offset = stream.read(4)
        require(len(raw_offset) == 4, "Measured package executable has a truncated PE header offset.")
        offset = struct.unpack("<I", raw_offset)[0]
        stream.seek(offset)
        require(stream.read(4) == b"PE\0\0", "Measured package application has no PE signature.")
        machine = stream.read(2)
        require(len(machine) == 2 and struct.unpack("<H", machine)[0] == 0x8664, "Measured package application is not x64.")


def validate_package_provenance(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    provenance = manifest.get("packageProvenance", {})
    require(provenance.get("reusedPackage") is True, "Run did not prove accepted P9-13 package reuse.")
    require(provenance.get("priorTaskId") == "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE", "Package ancestry is not the accepted P9-13 packaged lifecycle.")
    require(provenance.get("configuration") == "Release" and provenance.get("platform") == "x64" and provenance.get("architecture") == "x64", "Package provenance is not Windows x64 Release.")
    require(provenance.get("productSourceSha256") == "fcf0d919d105b44d52eaca319eef5a42e222c3721e8b022ea970127d41d41fe8", "P9-14 package source equivalence does not match accepted product sources.")

    prior_path = verify_absolute_file(str(provenance.get("priorRunManifestPath", "")), str(provenance.get("priorRunManifestSha256", "")), "Accepted P9-13 manifest")
    prior = read_json(prior_path)
    require(prior.get("status") == "passed" and prior.get("taskId") == provenance.get("priorTaskId"), "P9-13 ancestry manifest is not passed or task-matched.")
    prior_root = prior_path.parent
    prior_source_path = verify_artifact(prior_root, str(prior.get("source", {}).get("inventoryPath", "")), str(prior.get("source", {}).get("inventoryFileSha256", "")))
    prior_derived_path = verify_artifact(prior_root, str(prior.get("source", {}).get("derivedReceiptPath", "")), str(prior.get("source", {}).get("derivedReceiptSha256", "")))
    old_source = read_json(prior_source_path)
    derived = read_json(prior_derived_path)
    require(derived.get("status") == "verified-script-only-derivation", "P9-13 source equivalence receipt does not prove a script-only derivation.")
    require(derived.get("source", {}).get("productSourceSha256") == provenance.get("productSourceSha256"), "P9-13 derivation and P9-14 product-source fingerprints disagree.")

    source_path = verify_artifact(root, str(provenance.get("currentSourceInventoryPath", "")), str(provenance.get("currentSourceInventorySha256", "")))
    current_source = read_json(source_path)
    current_files = current_source.get("files")
    require(isinstance(current_files, list) and inventory_fingerprint(current_files) == current_source.get("sha256"), "Current source inventory fingerprint is inconsistent.")
    require(current_source.get("sha256") == provenance.get("currentSourceSha256"), "Captured source inventory differs from package provenance.")
    old_map = {str(item["path"]): item for item in old_source.get("files", [])}
    current_map = {str(item["path"]): item for item in current_files}
    require(set(current_map) - set(old_map) == NEW_SOURCE_PATHS, "P9-14 source additions are not exactly its six owned harness files.")
    for path, old in old_map.items():
        now = current_map.get(path)
        require(now is not None and old.get("sizeBytes") == now.get("sizeBytes") and old.get("sha256") == now.get("sha256"), f"A P9-13 inventoried source changed during P9-14: {path}")
    ignored = set(derived.get("source", {}).get("allowedPaths", [])) | NEW_SOURCE_PATHS
    product_rows = [row for row in current_files if str(row["path"]) not in ignored]
    product_hash = inventory_fingerprint(product_rows)
    require(product_hash == provenance.get("productSourceSha256") == prior.get("source", {}).get("productSourceSha256"), "Current product sources do not match the P9-13/P9-12 package.")
    source_by_path = {str(item["path"]): item for item in current_files}
    if manifest.get("mode") == "ai-batch-review":
        implementation = provenance.get("nextImplementationProof", {})
        next_page = implementation.get("nextPageSource", {})
        next_dialog = implementation.get("nextDialogSource", {})
        legacy_dialog = implementation.get("legacyDialogSource", {})
        for source_record, expected_path in (
            (next_page, "src/next/ui/speaking_evaluation_page.cpp"),
            (next_dialog, "src/next/ui/speaking_evaluation_ai_dialogs.cpp"),
            (legacy_dialog, "src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp"),
        ):
            require(source_record.get("path") == expected_path and source_by_path.get(expected_path, {}).get("sha256") == source_record.get("sha256"), f"Implementation source attribution is missing or hash-inconsistent: {expected_path}")
        require(implementation.get("routePageKey") == "speaking_evaluation", "AI route source proof does not identify the current Next page key.")
        require(next_dialog.get("className") == "SpeakingEvaluationAiBatchDialog" and next_dialog.get("objectName") == "speakingEvaluationAiBatchDialog", "Next AI dialog source identity is not the current Next implementation.")
        require(legacy_dialog.get("className") == "SpeakingEvalAiBatchDialog" and legacy_dialog.get("objectName") == "speakingEvalAiBatchDialog" and next_dialog.get("objectName") != legacy_dialog.get("objectName"), "Legacy dialog source is not distinguished from the Next dialog.")
        require(implementation.get("legacyDialogInvoked") is False and implementation.get("forbiddenLifecyclePageKey") == "lifecycle-speaking-evaluation", "Next route source proof does not explicitly exclude the Legacy dialog and lifecycle key.")
    elif manifest.get("mode") == "staff-directory":
        implementation = provenance.get("staffImplementationProof", {})
        expected_paths = {
            "page": "src/next/ui/shared_staff_directory_page.cpp",
            "pageHeader": "src/next/ui/shared_staff_directory_page.h",
            "tableModel": "src/next/ui/shared_staff_directory_table_model.cpp",
            "tableModelHeader": "src/next/ui/shared_staff_directory_table_model.h",
            "routeRegistration": "src/next/bootstrap/application_bootstrap.cpp",
        }
        attribution = implementation.get("sourceAttribution", {})
        require(set(attribution) == set(expected_paths), "Staff Next source proof does not retain the exact page, model, and route source set.")
        for role, expected_path in expected_paths.items():
            source_record = attribution.get(role, {})
            require(source_record.get("path") == expected_path and source_by_path.get(expected_path, {}).get("sha256") == source_record.get("sha256"), f"Staff Next source attribution is missing or hash-inconsistent: {expected_path}")
        require(implementation.get("routePageKeys") == ["native_english_teachers", "gs_team"], "Staff source proof does not identify both current Next route keys.")
        require(implementation.get("routeMode") == "current-next-staff-directory-ui-packaged-windows-x64-release-local", "Staff source proof does not identify the packaged current Next route mode.")
        require(implementation.get("sharedPageClassName") == "SharedStaffDirectoryPage" and implementation.get("tableModelClassName") == "SharedStaffDirectoryTableModel", "Staff source proof does not identify the shared page and table model classes.")
        require(implementation.get("tableAutomationIds") == ["nativeEnglishTeachersTable", "gsTeamTable"], "Staff source proof does not identify the exact Next table object names.")
        require(implementation.get("routeRegistrationContract") == "application_bootstrap.cpp registers sharedStaffDirectoryPageDescriptor for both route page keys", "Staff source proof does not state the Next route registration contract.")
    else:
        raise ValidationError("Package provenance does not identify an accepted P9-14 route mode.")

    package_inventory_path = verify_artifact(root, str(provenance.get("packageInventoryPath", "")), str(provenance.get("packageInventorySha256", "")))
    package = verify_inventory_file(Path(str(provenance.get("packageRoot", ""))), package_inventory_path, str(provenance.get("packageSha256", "")), "P9-13 packaged Release", True)
    require(int(package.get("fileCount", -1)) == int(provenance.get("packageFileCount", -2)), "Package file count differs from accepted P9-13 inventory.")
    app_path = verify_absolute_file(str(provenance.get("applicationPath", "")), str(provenance.get("applicationSha256", "")), "Packaged Next executable")
    verify_pe_x64(app_path)
    require(str(app_path) == str(Path(str(provenance.get("packageRoot", ""))).resolve() / "ClassMngrNext.exe"), "Measured executable is outside the proven package root.")

    receipt_path = verify_absolute_file(str(provenance.get("acceptedReleaseReceiptPath", "")), str(provenance.get("acceptedReleaseReceiptSha256", "")), "Accepted Release build receipt")
    receipt = read_json(receipt_path)
    require(receipt.get("status") == "succeeded" and receipt.get("configuration") == "Release" and receipt.get("platform") == "x64" and receipt.get("build", {}).get("exitCode") == 0, "Accepted Release receipt does not prove a successful x64 Release build.")
    require(receipt.get("releaseExecutableSha256") == sha256_file(app_path), "Packaged executable bytes differ from the accepted Release receipt.")
    cache_path = verify_absolute_file(str(provenance.get("configuredCachePath", "")), str(provenance.get("configuredCacheSha256", "")), "Configured Release CMake cache")
    require(str(cache_path) == str(Path(receipt.get("cachePath", "")).resolve()), "Configured cache path differs from the accepted Release receipt.")
    require(sha256_file(cache_path) == receipt.get("cacheSha256After"), "Configured Release cache hash differs from the accepted Release receipt.")

    resource_path = verify_artifact(root, str(provenance.get("resourceInventoryPath", "")), str(provenance.get("resourceInventoryFileSha256", "")))
    resource = verify_inventory_file(Path(str(provenance.get("resourceRoot", ""))), resource_path, str(provenance.get("resourceInventorySha256", "")), "Packaged resource input", True)
    require(resource.get("sha256") == receipt.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256"), "Staged package resource inputs differ from the accepted Release receipt.")
    return {
        "packageRoot": str(Path(str(provenance["packageRoot"])).resolve()),
        "packageSha256": str(provenance["packageSha256"]),
        "packageFileCount": int(provenance["packageFileCount"]),
        "applicationPath": str(app_path),
        "applicationSha256": sha256_file(app_path),
        "architecture": "x64",
        "configuration": "Release",
        "productSourceSha256": product_hash,
        "releaseReceiptSha256": sha256_file(receipt_path),
        "configuredCacheSha256": sha256_file(cache_path),
        "resourceInventorySha256": str(resource["sha256"]),
        "freshExactTreeRebuild": False,
        "implementationRoute": str(manifest.get("mode")),
    }


def validate_process(root: Path, manifest: dict[str, Any], provenance: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process", {})
    pid = int(process.get("processId", 0))
    hwnd = int(process.get("mainWindowHandle", 0))
    require(pid > 0 and hwnd > 0, "Measured process PID or main HWND is missing.")
    require(int(process.get("windowProcessId", 0)) == pid and int(process.get("hwndProcessId", 0)) == pid, "Main HWND provenance does not match the measured process PID.")
    require(process.get("architecture") == "x64", "Measured app process is not x64.")
    require(str(Path(process.get("processImagePath", "")).resolve()) == provenance["applicationPath"], "Live process image path differs from the proven package executable.")
    require(process.get("processImageSha256") == provenance["applicationSha256"], "Live process image hash differs from the proven package executable.")
    require(bool(str(process.get("commandLine", {}).get("workspaceArgument", ""))), "Measured process command line does not identify a fixture workspace argument.")
    require(str(Path(process["workspacePath"]).resolve()) == str(Path(manifest["fixture"]["workspacePath"]).resolve()), "Measured process workspace argument differs from its fixture workspace.")
    exit_record = manifest.get("processExit", {})
    require(exit_record.get("normalExit") is True and exit_record.get("forcedTermination") is False, "Process did not exit naturally without forced termination.")
    require(exit_record.get("exitCode") == 0 and exit_record.get("closeMethod") == "WindowPattern.Close", "Measured process did not exit with code 0 after UIA WindowPattern.Close.")
    require(process.get("forcedTermination") is False and process.get("closeMethod") == "WindowPattern.Close", "Process manifest claims forced termination or an unverified close method.")
    for key in ("stdoutPath", "stderrPath"):
        target = contained_path(root, str(process[key]))
        require(target.is_file(), f"Measured process {key} is missing.")
    return {"processId": pid, "mainWindowHandle": hwnd, "windowProcessId": pid, "applicationPath": provenance["applicationPath"], "exitCode": 0, "closeMethod": "WindowPattern.Close", "forcedTermination": False}


def validate_fixture(root: Path, manifest: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    fixture = manifest.get("fixture", {})
    preparation_ref = fixture.get("preparationReceipt", {})
    preparation_path = verify_artifact(root, preparation_ref.get("relativePath", ""), preparation_ref.get("sha256", ""))
    preparation = read_json(preparation_path)
    require(preparation.get("schema") == PREPARATION_SCHEMA and preparation.get("status") == "passed", "Workspace preparation receipt schema or status is invalid.")
    require(preparation.get("kind") == fixture.get("kind"), "Workspace preparation receipt kind differs from the run fixture.")
    require(preparation.get("workspaceSha256") == preparation_ref.get("workspaceSha256"), "Workspace hash differs from the pinned preparation receipt.")
    if fixture.get("kind") == "staff-directory":
        repository_root = Path(str(manifest.get("repositoryRoot", ""))).resolve(strict=True)
        expected_source = (repository_root / Path(*PurePosixPath(STAFF_SOURCE_WORKSPACE_RELATIVE_PATH).parts)).resolve(strict=True)
        source_path = Path(str(preparation.get("sourceWorkspace", ""))).resolve(strict=True)
        require(source_path == expected_source and preparation.get("sourceWorkspaceRelativePath") == STAFF_SOURCE_WORKSPACE_RELATIVE_PATH, "Staff preparation receipt does not identify the exact accepted source fixture path.")
        require(preparation.get("sourceWorkspaceSha256") == STAFF_SOURCE_WORKSPACE_SHA256 and sha256_file(source_path) == STAFF_SOURCE_WORKSPACE_SHA256, "Staff source fixture bytes differ from the pinned 96-row workspace fingerprint.")
        require(fixture.get("sourceWorkspacePath") == str(source_path) and fixture.get("sourceWorkspaceSha256") == STAFF_SOURCE_WORKSPACE_SHA256, "Staff route manifest does not retain the exact source fixture lineage.")
        require(preparation.get("workspaceSha256") == STAFF_SOURCE_WORKSPACE_SHA256, "Prepared Staff workspace is not an exact copy of the accepted source fixture.")
    else:
        require(preparation.get("sourceBaseWorkspaceSha256") == "85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9", "Fixture base workspace hash differs from accepted P9-11 blank workspace.")
        require(preparation.get("sourceSeedSqlSha256") == "1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3", "Fixture seed SQL hash differs from accepted large_startup.sql.")
    workspace = Path(str(fixture.get("workspacePath", ""))).resolve(strict=True)
    evidence_root = root.resolve(strict=True)
    require(evidence_root == workspace.parent or evidence_root in workspace.parents, "Fixture workspace is outside its evidence directory.")
    before_ref = fixture.get("beforeSnapshot", {})
    after_ref = fixture.get("afterSnapshot", {})
    before_path = verify_artifact(root, before_ref.get("relativePath", ""), before_ref.get("sha256", ""))
    after_path = verify_artifact(root, after_ref.get("relativePath", ""), after_ref.get("sha256", ""))
    before, after = read_json(before_path), read_json(after_path)
    require(before.get("phase") == "before" and after.get("phase") == "after", "Fixture snapshot phase labels are invalid.")
    require(before.get("integrityCheck") == "ok" and after.get("integrityCheck") == "ok" and not before.get("foreignKeyErrors") and not after.get("foreignKeyErrors"), "Before/after SQLite integrity or foreign-key check failed.")
    workspace_hash_summary = validate_workspace_hash_transition(preparation, before, after, sha256_file(workspace))
    require(not any(Path(str(workspace) + suffix).exists() for suffix in ("-wal", "-shm", "-journal")), "Workspace SQLite sidecars remain after natural exit.")
    if fixture.get("kind") == "staff-directory":
        summary = validate_staff_snapshots(preparation, before, after)
    elif fixture.get("kind") == "speaking-evaluation-ai-batch":
        summary = validate_ai_snapshots(preparation, before, after)
    else:
        raise ValidationError(f"Unknown P9-14 fixture kind: {fixture.get('kind')!r}")
    summary["workspaceHashTransition"] = workspace_hash_summary
    return preparation, before, {"after": after, "summary": summary, "workspace": workspace}


def validate_settings(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    settings = manifest.get("settings", {})
    inventory_path = verify_artifact(root, str(settings.get("beforeLaunchInventoryPath", "")), str(settings.get("beforeLaunchInventorySha256", "")))
    inventory = read_json(inventory_path)
    require(inventory.get("sha256") == settings.get("beforeLaunchInventorySha256Content"), "Before-launch QSettings inventory fingerprint is inconsistent.")
    cleanup = manifest.get("cleanup", {})
    if manifest.get("mode") == "ai-batch-review":
        require(settings.get("maximumRosterRows") == 100 and settings.get("rosterRowDisplayMode") == 0, "AI run did not pin maximumRosterRows=100 in its isolated profile.")
        profile = Path(str(settings.get("root", ""))) / "PaperCloud" / "ClassMngr.ini"
        require(profile.is_file(), "AI isolated settings profile is missing after natural app exit.")
        initial_files = inventory.get("files", [])
        require(inventory.get("fileCount") == 1 and len(initial_files) == 1 and initial_files[0].get("path") == "PaperCloud/ClassMngr.ini" and initial_files[0].get("sha256") == settings.get("preferenceFileSha256"), "Before-launch AI settings inventory does not identify its single pinned preference file.")
        sections: dict[str, dict[str, str]] = {}
        active_section = ""
        for raw_line in profile.read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if line.startswith("[") and line.endswith("]"):
                active_section = line[1:-1]
                sections.setdefault(active_section, {})
            elif active_section and "=" in line and not line.startswith(";"):
                key, value = line.split("=", 1)
                sections[active_section][key.strip()] = value.strip()
        require(sections.get("options", {}).get("maximumRosterRows") == "100" and sections.get("options", {}).get("rosterRowDisplayMode") == "0", "AI isolated SettingsManager ORG/APP profile no longer contains the pinned row limit and display mode after exit.")
        after_inventory_ref = cleanup.get("settingsInventoryAfterExit", {})
        after_inventory_path = verify_artifact(root, str(after_inventory_ref.get("relativePath", "")), str(after_inventory_ref.get("inventoryFileSha256", "")))
        after_inventory = read_json(after_inventory_path)
        after_files = after_inventory.get("files", [])
        require(after_inventory.get("sha256") == after_inventory_ref.get("sha256") and after_inventory.get("fileCount") == after_inventory_ref.get("fileCount") == 1, "After-exit isolated settings inventory is inconsistent.")
        require(len(after_files) == 1 and after_files[0].get("path") == "PaperCloud/ClassMngr.ini" and after_files[0].get("sha256") == sha256_file(profile), "After-exit isolated settings inventory does not fingerprint the exact persisted profile file.")
    else:
        require(settings.get("maximumRosterRows") is None and int(inventory.get("fileCount", -1)) == 0, "Staff route settings profile unexpectedly contains a maximum-row override.")
    require(cleanup.get("processExited") is True and cleanup.get("forcedTermination") is False, "Runtime cleanup does not prove natural process exit.")
    require(cleanup.get("workspaceSidecarFilesAfterExit") == [], "Runtime cleanup reports leftover workspace SQLite sidecars.")
    require(cleanup.get("returnToBaselineAllowance") == "none defined; only captured return and cleanup observations are reported", "Cleanup claims an unsupported return-to-baseline allowance.")
    return {"maximumRosterRows": settings.get("maximumRosterRows"), "isolatedSettingsFileCount": int(inventory["fileCount"]), "globalDefaultChanged": False, "cleanup": "natural exit; no workspace sidecars; no baseline allowance claimed"}


def validate_lifecycle_events(route: dict[str, Any], page_key: str, minimum_activations: int, minimum_suspensions: int) -> dict[str, Any]:
    events = [item for item in route.get("lifecycleEvents", []) if item.get("event") == "page-lifecycle" and item.get("pageKey") == page_key]
    activated = [item for item in events if item.get("state") == "activated"]
    suspended = [item for item in events if item.get("state") == "suspended"]
    require(len(activated) >= minimum_activations and len(suspended) >= minimum_suspensions, f"Lifecycle evidence for {page_key} lacks repeated activation/suspension events.")
    for item in activated:
        require(item.get("sampleAvailable") is True and item.get("pageInstanceAvailable") is True, f"Lifecycle activation for {page_key} lacks available page-instance evidence.")
        require(isinstance(item.get("page"), dict) and isinstance(item["page"].get("item_models"), list), f"Lifecycle activation for {page_key} omitted its visible item-model collection.")
    return {"activationCount": len(activated), "suspensionCount": len(suspended), "pageInstanceAndModelEvidence": True}


def validate_uia_snapshot(root: Path, route: dict[str, Any], relative: str, pid: int, expected_rows: int) -> dict[str, Any]:
    records = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == relative]
    require(len(records) == 1, f"UIA snapshot manifest reference is missing or duplicated: {relative}")
    record = records[0]
    path = verify_artifact(root, relative, str(record.get("sha256", "")))
    snapshot = read_json(path)
    control = snapshot.get("control", {})
    grid = snapshot.get("gridPattern", {})
    require(control.get("processId") == pid and control.get("ownerWindowProcessId") == pid and int(control.get("ownerWindowHandle", 0)) > 0, f"UIA control snapshot is not bound to measured PID/HWND: {relative}")
    require(grid.get("available") is True and grid.get("pattern") == "GridPattern" and grid.get("rowCount") == expected_rows, f"UIA table state does not expose exactly {expected_rows} rows: {relative}")
    return snapshot


def validate_reopened_matrix_descendants(snapshot: dict[str, Any], pid: int) -> dict[str, Any]:
    root = snapshot.get("root", {})
    require(root.get("automationId") == "QApplication.classMngrNextApplicationShell" and root.get("processId") == pid and root.get("ownerWindowProcessId") == pid and int(root.get("ownerWindowHandle", 0)) > 0, "Reopened Next main UIA descendant tree is not bound to the measured process/HWND.")
    matrices = [
        item for item in snapshot.get("descendants", [])
        if item.get("automationId", "").endswith(".speakingEvaluationMatrix")
        and item.get("controlType") == "ControlType.Table"
        and item.get("processId") == pid
        and item.get("gridPattern", {}).get("available") is True
        and item.get("gridPattern", {}).get("rowCount") == 100
        and item.get("gridPattern", {}).get("columnCount") == 11
    ]
    require(len(matrices) == 1, "Reopened Next UIA tree does not expose one exact 100-row by 11-column Speaking Evaluation matrix.")
    return {"matrixAutomationId": matrices[0]["automationId"], "rowCount": 100, "columnCount": 11, "processId": pid}


def validate_workspace_hash_transition(preparation: dict[str, Any], before: dict[str, Any], after: dict[str, Any], actual_workspace_sha256: str) -> dict[str, Any]:
    prepared_sha256 = str(preparation.get("workspaceSha256", "")).lower()
    before_sha256 = str(before.get("workspaceSha256", "")).lower()
    after_sha256 = str(after.get("workspaceSha256", "")).lower()
    require(bool(prepared_sha256) and before_sha256 == prepared_sha256, "Before snapshot does not match the pinned prepared workspace bytes.")
    require(bool(after_sha256) and str(actual_workspace_sha256).lower() == after_sha256, "Final fixture workspace bytes differ from the after snapshot.")
    return {"preparedWorkspaceSha256": prepared_sha256, "beforeSnapshotSha256": before_sha256, "finalWorkspaceSha256": after_sha256, "workspaceChangedDuringRoute": after_sha256 != prepared_sha256}


def validate_ai_cell_target(target: dict[str, Any], row: int, column: int, pid: int, label: str) -> None:
    identity = target.get("cellIdentity", {})
    control = target.get("cellControl", {})
    rectangle = target.get("cellRectangle", {})
    owner = target.get("ownerWindow", {})
    bounds = owner.get("bounds", {})
    point = target.get("inputPoint", {})
    require(target.get("row") == row and target.get("column") == column, f"{label} UIA cell coordinates differ from the requested review row/column.")
    require(identity.get("processId") == pid and identity.get("runtimeId") and identity.get("controlType"), f"{label} lacks a measured-process UIA cell identity/runtime ID.")
    require(control.get("processId") == pid and control.get("ownerWindowProcessId") == pid and int(control.get("ownerWindowHandle", 0)) > 0, f"{label} UIA cell control is not bound to the measured process/HWND.")
    require(control.get("name") == identity.get("name") and control.get("automationId") == identity.get("automationId") and control.get("controlType") == identity.get("controlType"), f"{label} nested cell identity differs from its control snapshot.")
    require(owner.get("processId") == pid and owner.get("automationId") == "QApplication.speakingEvaluationAiBatchDialog" and int(owner.get("windowHandle", 0)) == int(control.get("ownerWindowHandle", 0)) > 0, f"{label} is not bound to the measured Next AI dialog HWND.")
    require(rectangle.get("isOffscreen") is False and all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")), f"{label} lacks numeric visible UIA cell bounds.")
    x, y, width, height = (float(rectangle[key]) for key in ("x", "y", "width", "height"))
    control_rect = control.get("rectangle", {})
    require(all(float(control_rect.get(key, float("nan"))) == float(rectangle[key]) for key in ("x", "y", "width", "height")), f"{label} recorded cell bounds differ from the nested UIA control bounds.")
    left, top, owner_width, owner_height = (float(bounds.get(key, 0)) for key in ("x", "y", "width", "height"))
    require(width > 0 and height > 0 and owner_width > 0 and owner_height > 0, f"{label} has empty cell or dialog bounds.")
    require(x >= left and y >= top and x + width <= left + owner_width and y + height <= top + owner_height, f"{label} cell bounds leave the measured dialog HWND.")
    point_x, point_y = int(point.get("x", -1)), int(point.get("y", -1))
    require(point.get("basis") == "center of UIA GridPattern cell; Next custom delegate paints its check indicator at the centered cell position", f"{label} input point lacks source-grounded UIA cell basis.")
    require(point_x == round(x + width / 2) and point_y == round(y + height / 2), f"{label} input point is not the center of its freshly measured UIA cell bounds.")
    require(left <= point_x < left + owner_width and top <= point_y < top + owner_height, f"{label} input point leaves the measured AI dialog HWND.")


def validate_ai_cell_action(action: dict[str, Any], row: int, pid: int) -> None:
    method = action.get("method")
    require(method in {
        "TogglePattern.Toggle",
        "UIA-derived cell BoundingRectangle bounded WM mouse click",
        "UIA-derived cell BoundingRectangle bounded WM mouse click after TogglePattern produced no state transition",
    }, f"Review row {row} used an unverified checkbox input method.")
    before = action.get("cellTargetBefore", {})
    after = action.get("cellTargetAfter", {})
    validate_ai_cell_target(before, row, 0, pid, f"Review row {row} cellTargetBefore")
    validate_ai_cell_target(after, row, 0, pid, f"Review row {row} cellTargetAfter")
    before_identity = before["cellIdentity"]
    after_identity = after["cellIdentity"]
    require(all(before_identity.get(key) == after_identity.get(key) for key in ("name", "automationId", "controlType", "processId", "runtimeId")), f"Review row {row} changed UIA cell identity between input and verification.")
    require(action.get("target", {}).get("processId") == pid and action.get("target", {}).get("ownerWindowProcessId") == pid, f"Review row {row} action target is not PID-bound.")
    require(action["target"].get("automationId") == after["cellControl"].get("automationId") and action["target"].get("rectangle") == after["cellControl"].get("rectangle"), f"Review row {row} top-level action target differs from its verified cell control.")
    if method == "TogglePattern.Toggle":
        require(action.get("togglePatternAvailable") is True and action.get("toggleReturned") is True and action.get("toggleReturnedWithoutTransition") is False, f"Review row {row} TogglePattern evidence is inconsistent with the selected method.")
        require(action.get("pointerAction") is None, f"Review row {row} records an unneeded second input after TogglePattern succeeded.")
        return

    require(isinstance(action.get("pointerAction"), dict), f"Review row {row} bounded click omitted nested pointer action evidence.")
    pointer = action["pointerAction"]
    source = pointer.get("sourceTarget", {})
    source_rect = pointer.get("sourceRectangle", {})
    identity = before["cellIdentity"]
    rectangle = before["cellRectangle"]
    owner = before["ownerWindow"]
    owner_bounds = owner["bounds"]
    require(pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP", f"Review row {row} click does not record the bounded mouse input sequence.")
    require(all(source.get(key) == identity.get(key) for key in ("name", "automationId", "controlType", "processId", "runtimeId")), f"Review row {row} click nested UIA cell identity differs from the freshly reacquired target.")
    require(all(float(source_rect.get(key, float("nan"))) == float(rectangle[key]) for key in ("x", "y", "width", "height")), f"Review row {row} click bounds differ from the nested UIA cell bounds.")
    require(pointer.get("targetProcessId") == pointer.get("windowProcessId") == pid, f"Review row {row} click input is not bound to the measured process.")
    require(int(pointer.get("windowHandle", 0)) == int(owner.get("windowHandle", 0)) > 0, f"Review row {row} click HWND differs from the measured dialog HWND.")
    require(pointer.get("measuredDialogBounds") == owner_bounds, f"Review row {row} click did not retain the measured dialog bounds.")
    point = before["inputPoint"]
    require(pointer.get("screenX") == point.get("x") and pointer.get("screenY") == point.get("y"), f"Review row {row} click point differs from its UIA-derived cell center.")
    if method.endswith("after TogglePattern produced no state transition"):
        require(action.get("togglePatternAvailable") is True and action.get("toggleReturned") is True and action.get("toggleReturnedWithoutTransition") is True, f"Review row {row} second input lacks evidence that TogglePattern returned without a state transition.")
    else:
        require(action.get("togglePatternAvailable") is False and action.get("toggleReturned") is False and action.get("toggleReturnedWithoutTransition") is False, f"Review row {row} click fallback does not prove TogglePattern unavailability.")


def validate_screenshots(root: Path, route: dict[str, Any], expected_names: set[str], pid: int) -> dict[str, Any]:
    seen: set[str] = set()
    for screenshot in route.get("screenshots", []):
        name = str(screenshot.get("name", ""))
        path = verify_artifact(root, str(screenshot.get("relativePath", "")), str(screenshot.get("sha256", "")))
        require(path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", f"Screenshot is not a PNG image: {name}")
        require(screenshot.get("processId") == pid and screenshot.get("windowProcessId") == pid and int(screenshot.get("windowHandle", 0)) > 0, f"Screenshot provenance is not bound to measured PID/HWND: {name}")
        seen.add(name)
    require(expected_names.issubset(seen), f"Required route screenshots are missing: {sorted(expected_names - seen)}")
    return {"screenshots": sorted(expected_names), "pidBound": True}


def validate_actions(root: Path, route: dict[str, Any], pid: int) -> int:
    actions = route.get("actions", [])
    require(isinstance(actions, list) and actions, "Route contains no UIA-bound actions.")
    for action in actions:
        target = action.get("target", {})
        require(target.get("processId") == pid and target.get("ownerWindowProcessId") == pid and int(target.get("ownerWindowHandle", 0)) > 0, f"UIA action is not bound to the measured process/window: {action.get('actionKind')}")
        require(action.get("method") in {"InvokePattern.Invoke", "SelectionItemPattern.Select", "ValuePattern.SetValue", "WindowPattern.Close", "TogglePattern.Toggle", "ScrollItemPattern.ScrollIntoView", "ScrollPattern.Scroll", "ExpandCollapsePattern.Expand", "UIA-derived cell BoundingRectangle bounded WM mouse click", "UIA-derived cell BoundingRectangle bounded WM mouse click after TogglePattern produced no state transition", "UIA-targeted window click", "UIA-derived Button BoundingRectangle bounded WM mouse click", "PrintWindow-inspected disclosure indicator with screenshot-derived point bounded by UIA parent Tree and measured main HWND; bounded WM mouse click"}, f"UIA action used an unapproved input method: {action.get('actionKind')}")
        if action.get("method") == "UIA-targeted window click":
            validate_uia_window_click(action, pid, route.get("routeEvents", []))
        elif action.get("method") == "PrintWindow-inspected disclosure indicator with screenshot-derived point bounded by UIA parent Tree and measured main HWND; bounded WM mouse click":
            validate_navigation_group_click(action, pid, route.get("screenshots", []))
        elif action.get("method") == "UIA-derived Button BoundingRectangle bounded WM mouse click":
            validate_uia_button_click(action, pid)
        elif str(action.get("method", "")).startswith("UIA-derived cell BoundingRectangle bounded WM mouse click"):
            validate_ai_cell_action(action, int(action.get("row", -1)), pid)
    logged = [json.loads(line) for line in contained_path(root, "uia-actions.jsonl").read_text(encoding="utf-8").splitlines() if line.strip()]
    require(len(logged) == len(actions), "UIA action log and route manifest action counts differ.")
    require([item.get("actionKind") for item in logged] == [item.get("actionKind") for item in actions], "UIA action log order differs from the route manifest.")
    return len(actions)


def validate_uia_window_click(action: dict[str, Any], pid: int, route_events: list[dict[str, Any]]) -> None:
    target = action.get("target", {})
    pointer = action.get("pointerAction", {})
    source = pointer.get("sourceTarget", {})
    rectangle = target.get("rectangle", {})
    source_rectangle = source.get("rectangle", {})
    bounds = pointer.get("measuredMainWindowBounds", {})
    require(target.get("controlType") == source.get("controlType") == "ControlType.TreeItem", "UIA-targeted navigation click is not derived from a TreeItem.")
    require(target.get("name") == source.get("name") and target.get("automationId") == source.get("automationId"), "UIA-targeted click nested source identity differs from its measured UIA target.")
    require(target.get("processId") == pointer.get("targetProcessId") == pid and target.get("ownerWindowProcessId") == pointer.get("windowProcessId") == pid, "UIA-targeted click is not process bound at the action and nested input levels.")
    require(int(target.get("ownerWindowHandle", 0)) == int(pointer.get("windowHandle", 0)) > 0, "UIA-targeted click HWND differs from its UIA owner HWND.")
    require(all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")), "UIA-targeted click omitted numeric UIA bounds.")
    require({key: float(source_rectangle.get(key, float("nan"))) for key in ("x", "y", "width", "height")} == {key: float(rectangle[key]) for key in ("x", "y", "width", "height")}, "UIA-targeted click coordinates were not copied from its measured UIA target.")
    x, y, width, height = (float(rectangle[key]) for key in ("x", "y", "width", "height"))
    left, top, window_width, window_height = (float(bounds.get(key, 0)) for key in ("x", "y", "width", "height"))
    require(width > 0 and height > 0 and window_width > 0 and window_height > 0, "UIA-targeted click has empty UIA or owner-window bounds.")
    require(x >= left and y >= top and x + width <= left + window_width and y + height <= top + window_height, "UIA-targeted click UIA bounds leave the measured owner window.")
    require(pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP", "UIA-targeted click input method is not the recorded bounded mouse sequence.")
    require(int(pointer.get("screenX", -1)) == round(x + width / 2) and int(pointer.get("screenY", -1)) == round(y + height / 2), "UIA-targeted click point is not the center of the reacquired UIA bounds.")
    point_x, point_y = int(pointer["screenX"]), int(pointer["screenY"])
    require(left <= point_x < left + window_width and top <= point_y < top + window_height, "UIA-targeted click point is outside the measured owner window.")
    route_event = action.get("pageRouteEvent", {})
    arrival_index = route_event.get("arrivalIndex")
    matched_events = [item for item in route_events if item.get("arrivalIndex") == arrival_index]
    require(
        action.get("verifiedByFollowingPageRoute") is True
        and isinstance(arrival_index, int)
        and arrival_index > 0
        and len(matched_events) == 1
        and route_event.get("targetPageId") == action.get("targetPageId")
        and matched_events[0].get("targetPageId") == action.get("targetPageId")
        and matched_events[0].get("processId") == pid
        and route_event.get("processId") == pid
        and route_event.get("line") == matched_events[0].get("line")
        and re.match(
            rf"^ClassMngrNext navigation event=page-route outcome=completed cause=[^\s]+ from=[^\s]+ to={re.escape(str(action.get('targetPageId', '')))}(?:\s|$)",
            str(matched_events[0].get("line", "")),
        ) is not None,
        "UIA-targeted click lacks its matching observed Next page-route event.",
    )


def validate_uia_button_click(action: dict[str, Any], pid: int) -> None:
    target = action.get("target", {})
    pointer = action.get("pointerAction", {})
    source = pointer.get("sourceTarget", {})
    rectangle = target.get("rectangle", {})
    source_rectangle = source.get("rectangle", {})
    bounds = pointer.get("measuredDialogBounds", {})
    require(target.get("controlType") == source.get("controlType") == "ControlType.Button", "UIA bounded button click is not derived from a measured Button target.")
    require(target.get("automationId", "").endswith("speakingEvaluationAiBatchCreatePrompt") and target.get("name") == source.get("name") == "Create Class Prompt", "UIA bounded button click target is not the exact Next Create Class Prompt control.")
    require(target.get("automationId") == source.get("automationId"), "UIA bounded button click nested source identity differs from its target.")
    require(target.get("processId") == pointer.get("targetProcessId") == pid and target.get("ownerWindowProcessId") == pointer.get("windowProcessId") == pid, "UIA bounded button click is not bound to the measured process.")
    require(int(target.get("ownerWindowHandle", 0)) == int(pointer.get("windowHandle", 0)) > 0, "UIA bounded button click HWND differs from its UIA owner HWND.")
    require(all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")), "UIA bounded button click omitted numeric UIA bounds.")
    require({key: float(source_rectangle.get(key, float("nan"))) for key in ("x", "y", "width", "height")} == {key: float(rectangle[key]) for key in ("x", "y", "width", "height")}, "UIA bounded button click coordinates were not copied from its UIA target.")
    x, y, width, height = (float(rectangle[key]) for key in ("x", "y", "width", "height"))
    left, top, window_width, window_height = (float(bounds.get(key, 0)) for key in ("x", "y", "width", "height"))
    require(width > 0 and height > 0 and window_width > 0 and window_height > 0, "UIA bounded button click has empty control or dialog bounds.")
    require(x >= left and y >= top and x + width <= left + window_width and y + height <= top + window_height, "UIA bounded button click target bounds leave the measured dialog HWND.")
    require(pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP", "UIA bounded button click input is not the recorded mouse sequence.")
    require(int(pointer.get("screenX", -1)) == round(x + width / 2) and int(pointer.get("screenY", -1)) == round(y + height / 2), "UIA bounded button click point is not the center of its freshly measured UIA bounds.")
    require(action.get("verifiedByFollowingUiAStateTransition") is True and action.get("stateBefore", {}).get("copyPromptEnabled") is False and action.get("stateAfter", {}).get("copyPromptEnabled") is True, "UIA bounded button click lacks the observed prompt-generation state transition.")


def validate_navigation_group_click(action: dict[str, Any], pid: int, screenshots: list[dict[str, Any]]) -> None:
    target = action.get("target", {})
    pointer = action.get("pointerAction", {})
    source = pointer.get("sourceTarget", {})
    parent_tree = pointer.get("parentTreeTarget", {})
    parent_tree_rect = parent_tree.get("rectangle", {})
    rectangle = target.get("rectangle", {})
    source_rectangle = source.get("rectangle", {})
    bounds = pointer.get("measuredMainWindowBounds", {})
    require(target.get("name") == source.get("name") == "Campus Staff" and target.get("controlType") == source.get("controlType") == "ControlType.TreeItem", "Staff group click fallback is not bound to the exact Campus Staff UIA TreeItem.")
    require(target.get("automationId") == source.get("automationId") and target.get("processId") == pointer.get("targetProcessId") == pid and target.get("ownerWindowProcessId") == pointer.get("windowProcessId") == pid, "Staff group click fallback nested identity or process differs from its measured UIA target.")
    require(int(target.get("ownerWindowHandle", 0)) == int(pointer.get("windowHandle", 0)) > 0, "Staff group click fallback HWND differs from its measured UIA owner HWND.")
    require(all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")) and {key: float(source_rectangle.get(key, float("nan"))) for key in ("x", "y", "width", "height")} == {key: float(rectangle[key]) for key in ("x", "y", "width", "height")}, "Staff group click fallback omitted its exact nested UIA bounds.")
    x, y, width, height = (float(rectangle[key]) for key in ("x", "y", "width", "height"))
    left, top, window_width, window_height = (float(bounds.get(key, 0)) for key in ("x", "y", "width", "height"))
    require(width > 0 and height > 0 and window_width > 0 and window_height > 0 and x >= left and y >= top and x + width <= left + window_width and y + height <= top + window_height, "Staff group click fallback UIA bounds leave its measured main HWND.")
    require(parent_tree.get("controlType") == "ControlType.Tree" and parent_tree.get("automationId", "").endswith("classMngrNextNavigationTree"), "Staff disclosure click parent target is not the exact Next navigation UIA Tree.")
    require(parent_tree.get("processId") == parent_tree.get("ownerWindowProcessId") == pointer.get("targetProcessId") == pid and int(parent_tree.get("ownerWindowHandle", 0)) == int(pointer.get("windowHandle", 0)) > 0, "Staff disclosure click parent Tree PID/HWND identity differs from the measured process/window.")
    require(all(isinstance(parent_tree_rect.get(key), (int, float)) for key in ("x", "y", "width", "height")), "Staff disclosure click omitted fresh parent Tree bounds.")
    tree_x, tree_y, tree_width, tree_height = (float(parent_tree_rect[key]) for key in ("x", "y", "width", "height"))
    require(tree_width > 0 and tree_height > 0 and tree_x >= left and tree_y >= top and tree_x + tree_width <= left + window_width and tree_y + tree_height <= top + window_height, "Staff disclosure click parent Tree bounds leave its measured main HWND.")
    derivation = pointer.get("disclosurePointDerivation", {})
    screenshot = next((item for item in screenshots if item.get("name") == "campus-staff-navigation-before-disclosure-click"), None)
    image_pixel = derivation.get("selectedImagePixel", {})
    screen_bounds = derivation.get("screenshotScreenBounds", {})
    inspected_tree_bounds = derivation.get("uiaTreeBoundsAtInspection", {})
    inspected_item_bounds = derivation.get("uiaTreeItemBoundsAtInspection", {})
    screenshot_width = int(derivation.get("screenshotWidth", 0))
    screenshot_height = int(derivation.get("screenshotHeight", 0))
    require(screenshot is not None and derivation.get("method") == "operator-inspected fresh PrintWindow pixel converted through captured main-window screen origin and checked against the fresh UIA parent Tree bounds", "Staff group click fallback lacks the fresh inspected PrintWindow disclosure capture.")
    require(screenshot.get("relativePath") == derivation.get("screenshotRelativePath") and screenshot.get("sha256") == derivation.get("screenshotSha256") and screenshot.get("captureMethod", "").startswith("PrintWindow("), "Staff disclosure click screenshot identity/hash/capture method is incomplete.")
    require(screenshot.get("processId") == derivation.get("screenshotProcessId") == pid and screenshot.get("windowProcessId") == derivation.get("screenshotWindowProcessId") == pid and int(screenshot.get("windowHandle", 0)) == int(derivation.get("screenshotWindowHandle", 0)) == int(pointer.get("windowHandle", 0)), "Staff disclosure screenshot is not bound to the measured process and HWND.")
    screenshot_bounds = screenshot.get("screenBounds", {})
    require(screenshot_width == screenshot.get("width") and screenshot_height == screenshot.get("height") and {key: int(screen_bounds.get(key, -1)) for key in ("x", "y", "width", "height")} == {key: int(bounds.get(key, -2)) for key in ("x", "y", "width", "height")} == {key: int(screenshot_bounds.get(key, -3)) for key in ("x", "y", "width", "height")}, "Staff disclosure screenshot image dimensions or screen origin differ from the measured main HWND.")
    require({key: float(inspected_tree_bounds.get(key, float("nan"))) for key in ("x", "y", "width", "height")} == {key: float(parent_tree_rect[key]) for key in ("x", "y", "width", "height")}, "Staff disclosure click did not retain the parent Tree bounds seen during screenshot inspection.")
    require({key: float(inspected_item_bounds.get(key, float("nan"))) for key in ("x", "y", "width", "height")} == {key: float(rectangle[key]) for key in ("x", "y", "width", "height")}, "Staff disclosure click did not reacquire the same Campus Staff UIA TreeItem bounds seen during screenshot inspection.")
    require(isinstance(image_pixel.get("x"), int) and isinstance(image_pixel.get("y"), int) and 0 <= image_pixel["x"] < screenshot_width and 0 <= image_pixel["y"] < screenshot_height, "Staff disclosure click has no bounded screenshot pixel derived from visual inspection.")
    expected_screen_x = int(screen_bounds["x"]) + image_pixel["x"]
    expected_screen_y = int(screen_bounds["y"]) + image_pixel["y"]
    require(pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP" and int(pointer.get("screenX", -1)) == expected_screen_x and int(pointer.get("screenY", -1)) == expected_screen_y, "Staff disclosure click point does not map from the inspected PrintWindow pixel through its measured screen origin.")
    require(tree_x <= pointer["screenX"] < tree_x + tree_width and tree_y <= pointer["screenY"] < tree_y + tree_height, "Staff disclosure click point is outside the UIA parent Tree bounds.")
    require(pointer["screenX"] < x and y <= pointer["screenY"] < y + height, "Staff disclosure click point is not inside the parent Tree in the Campus Staff row's leading disclosure region.")
    require(action.get("expansionStateBefore") == "Collapsed" and action.get("expansionStateAfter") == "Expanded", "Staff group click fallback lacks the observed collapsed-to-expanded UIA state transition.")


def validate_memory(root: Path, manifest: dict[str, Any], pid: int, minimum_checkpoints: int) -> dict[str, Any]:
    memory = manifest.get("memory", {})
    thresholds = memory.get("thresholds", {})
    require(thresholds.get("stableWorkingSetExclusiveBytes") == STRICT_WS_LIMIT and thresholds.get("strictAtOrAboveFails") is True, "Stable working-set threshold is missing or not exclusive.")
    require(thresholds.get("operationPeakAdvisoryExclusiveBytes") == OPERATION_WS_LIMIT, "Operation peak advisory threshold differs from the accepted 512 MiB boundary.")
    require(thresholds.get("returnToBaselineAllowance") == "none defined; no baseline return allowance is asserted", "Memory evidence claims a return-to-baseline allowance.")
    checkpoints = memory.get("stableCheckpoints", [])
    require(len(checkpoints) >= minimum_checkpoints, f"Too few stable working-set checkpoints: {len(checkpoints)}.")
    for checkpoint in checkpoints:
        require(checkpoint.get("category") == "normal-use" and checkpoint.get("processId") == pid, "Stable checkpoint is missing normal-use category or measured PID.")
        require(0 < int(checkpoint.get("workingSetBytes", 0)) < STRICT_WS_LIMIT, f"Stable working set is not below the strict cap: {checkpoint.get('point')}.")
    process_metrics = memory.get("processMetrics", {})
    metrics_path = verify_artifact(root, str(process_metrics.get("path", "")), str(process_metrics.get("sha256", "")))
    samples = [json.loads(line) for line in metrics_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(len(samples) == process_metrics.get("sampleCount") and samples, "Process memory sample count is empty or inconsistent.")
    require(all(int(item.get("pid", 0)) == pid and int(item.get("workingSetBytes", 0)) > 0 for item in samples), "Process memory sample PID or working-set value is invalid.")
    operation_peak = max((int(item["workingSetBytes"]) for item in samples if item.get("category") == "operation"), default=0)
    normal_peak = max((int(item["workingSetBytes"]) for item in samples if item.get("category") == "normal-use"), default=0)
    require(operation_peak == process_metrics.get("operationPeakWorkingSetBytes"), "Recorded operation working-set peak differs from sampled evidence.")
    require(normal_peak == process_metrics.get("normalUsePeakWorkingSetBytes"), "Recorded normal-use working-set peak differs from sampled evidence.")
    require(normal_peak < STRICT_WS_LIMIT, "Normal-use sampled working set reached the strict exclusive cap.")
    advisory_exceeded = operation_peak >= OPERATION_WS_LIMIT
    require(advisory_exceeded == process_metrics.get("operationPeakAdvisoryExceeded"), "Operation peak advisory flag differs from sampled evidence.")
    return {"stableCheckpointCount": len(checkpoints), "maximumStableWorkingSetBytes": max(int(item["workingSetBytes"]) for item in checkpoints), "normalUsePeakWorkingSetBytes": normal_peak, "operationPeakWorkingSetBytes": operation_peak, "operationPeakAdvisoryExceeded": advisory_exceeded, "operationPeakAdvisoryThresholdBytesExclusive": OPERATION_WS_LIMIT, "sampleCount": len(samples), "privateBytesDiagnosticOnly": True, "returnToBaselineAllowance": "none defined"}


def validate_staff_table_evidence(snapshot: dict[str, Any], page: str, fixture_rows: list[dict[str, Any]], pid: int, step: str) -> dict[str, Any]:
    table_evidence = snapshot.get("staffTableEvidence", {})
    expected_headers = {
        "native_english_teachers": ["Name", "Position", "Phone Number", "Email", "Birthday", "Nationality"],
        "gs_team": ["Name", "Korean Name", "Position", "Phone Number", "Birthday"],
    }.get(page)
    require(expected_headers is not None, f"Staff UIA table evidence has an unrecognized Next route page: {page}.")
    require(table_evidence.get("source") == "UIA GridPattern.GetItem row identity plus UIA TablePattern.GetColumnHeaders", f"Staff UIA evidence source is not GridPattern/TablePattern for {step}.")
    headers = table_evidence.get("headers", [])
    require(table_evidence.get("rowCount") == 96 and table_evidence.get("columnCount") == len(expected_headers) == snapshot.get("gridPattern", {}).get("columnCount"), f"Staff UIA column and row counts differ for {step}.")
    require([item.get("text") for item in headers] == expected_headers and len(headers) == len(expected_headers), f"Staff UIA TablePattern headers differ for {step}.")
    for column, header in enumerate(headers):
        control = header.get("control", {})
        require(header.get("column") == column and control.get("processId") == control.get("ownerWindowProcessId") == pid and int(control.get("ownerWindowHandle", 0)) > 0 and control.get("isOffscreen") is False, f"Staff UIA header identity or visibility is invalid for {step} column {column}.")
    checks = table_evidence.get("identityCellChecks", [])
    require(table_evidence.get("allGridRowsExposed") is True and len(checks) == 96 and len(fixture_rows) == 96, f"Staff UIA GridPattern did not expose every fixture identity cell for {step}.")
    fixture_names = [str(item.get("name", "")) for item in fixture_rows]
    fixture_index_by_name = {name: index for index, name in enumerate(fixture_names)}
    require(all(fixture_names) and len(fixture_index_by_name) == 96 and table_evidence.get("fixtureIdentitySetVerified") is True, f"Pinned Staff fixture identity set is blank, duplicated, or not completely exposed for {step}.")
    observed_names: list[str] = []
    for row_index, cell in enumerate(checks):
        identity = cell.get("text")
        require(cell.get("row") == row_index and cell.get("column") == 0 and identity in fixture_index_by_name and identity == cell.get("expectedIdentity") and cell.get("fixtureRowIndex") == fixture_index_by_name.get(identity), f"Staff UIA GridPattern identity differs from the pinned fixture set at runtime table row {row_index} for {step}.")
        observed_names.append(str(identity))
        require(cell.get("processId") == pid and cell.get("runtimeId") and cell.get("controlType") and cell.get("automationId") is not None, f"Staff UIA identity cell lacks process and runtime identity for {step} row {row_index}.")
        rectangle = cell.get("rectangle", {})
        require(all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")) and rectangle.get("width", 0) >= 0 and rectangle.get("height", 0) >= 0, f"Staff UIA identity cell bounds are incomplete for {step} row {row_index}.")
    require(len(set(observed_names)) == 96 and set(observed_names) == set(fixture_names), f"Staff UIA GridPattern runtime rows are not the exact unique 96-name fixture set for {step}.")
    rendered_samples = table_evidence.get("renderedIdentitySamples", [])
    require([item.get("row") for item in rendered_samples] == [0, 48, 95], f"Staff UIA first, middle, and last rendered identity samples are missing for {step}.")
    for sample in rendered_samples:
        row_index = int(sample["row"])
        rectangle = sample.get("rectangle", {})
        identity = sample.get("text")
        require(identity in fixture_index_by_name and sample.get("text") == sample.get("expectedIdentity") and sample.get("fixtureRowIndex") == fixture_index_by_name.get(identity) and sample.get("processId") == pid and sample.get("runtimeId") and sample.get("isOffscreen") is False, f"Staff UIA rendered identity sample differs from the fixture set at row {row_index} for {step}.")
        require(all(isinstance(rectangle.get(key), (int, float)) for key in ("x", "y", "width", "height")) and rectangle.get("width", 0) > 0 and rectangle.get("height", 0) > 0, f"Staff UIA rendered identity sample has no visible rectangle at row {row_index} for {step}.")
    return {"columnCount": len(expected_headers), "headers": expected_headers, "identityCellCount": 96, "renderedIdentitySampleRows": [0, 48, 95]}


def validate_staff_title_snapshot(snapshot: dict[str, Any], expected_title: str, pid: int, hwnd: int) -> dict[str, Any]:
    control = snapshot.get("control", {})
    require(control.get("name") == expected_title and control.get("controlType") == "ControlType.Text", f"Staff runtime page-title snapshot is not the exact Text control '{expected_title}'.")
    require(control.get("processId") == pid and control.get("ownerWindowProcessId") == pid and int(control.get("ownerWindowHandle", 0)) == hwnd > 0, f"Staff runtime page-title control '{expected_title}' is not bound to the measured PID/HWND.")
    require(isinstance(control.get("automationId"), str), f"Staff runtime page-title control '{expected_title}' does not record the actual AutomationId property.")
    return control


def expected_staff_table_automation_id(page_title_automation_id: str, route_table_leaf: str) -> str:
    suffix = ".pageTitle"
    require(page_title_automation_id.endswith(suffix) and bool(route_table_leaf), "Runtime Staff page-title AutomationId does not provide an exact page-host prefix for the route table.")
    return page_title_automation_id[: -len(suffix)] + "." + route_table_leaf


def validate_staff_table_runtime_identity(snapshot: dict[str, Any], page_title_automation_id: str, route_table_leaf: str, summary_table_id: str, pid: int, hwnd: int) -> None:
    control = snapshot.get("control", {})
    route_table_id = expected_staff_table_automation_id(page_title_automation_id, route_table_leaf)
    require(control.get("automationId") == summary_table_id == route_table_id, "Staff table runtime AutomationId differs from the exact UIA table ID derived from the runtime page-title identity.")
    require(control.get("controlType") in {"ControlType.Table", "ControlType.DataGrid"} and control.get("processId") == pid and control.get("ownerWindowProcessId") == pid and int(control.get("ownerWindowHandle", 0)) == hwnd > 0, "Staff table runtime UIA type or PID/HWND identity is invalid.")


def validate_staff_post_route_snapshot(snapshot: dict[str, Any], pid: int, hwnd: int) -> dict[str, str]:
    root_control = snapshot.get("root", {})
    descendants = snapshot.get("descendants", [])
    require(root_control.get("automationId") == "QApplication.classMngrNextApplicationShell" and root_control.get("controlType") == "ControlType.Window" and root_control.get("processId") == pid and root_control.get("ownerWindowProcessId") == pid and int(root_control.get("ownerWindowHandle", 0)) == hwnd > 0, "Native English post-route UIA root is not the exact measured Next main window.")
    titles = [item for item in descendants if item.get("name") == "Native English Teachers" and item.get("controlType") == "ControlType.Text" and item.get("processId") == pid]
    require(len(titles) == 1 and isinstance(titles[0].get("automationId"), str), "Native English post-route UIA tree does not expose exactly one runtime Text title identity.")
    expected_table_id = expected_staff_table_automation_id(titles[0]["automationId"], "nativeEnglishTeachersTable")
    tables = [item for item in descendants if item.get("automationId") == expected_table_id and item.get("controlType") in {"ControlType.Table", "ControlType.DataGrid"} and item.get("processId") == pid]
    require(len(tables) == 1 and tables[0].get("gridPattern", {}).get("available") is True and tables[0].get("gridPattern", {}).get("rowCount") == 96 and tables[0].get("gridPattern", {}).get("columnCount") == 6, "Native English post-route UIA tree does not expose the exact 96 by 6 route table identity.")
    return {"pageTitleAutomationId": titles[0]["automationId"], "tableAutomationId": tables[0]["automationId"]}


def validate_staff_route(root: Path, manifest: dict[str, Any], pid: int) -> dict[str, Any]:
    route = manifest.get("route", {})
    proof = manifest.get("packageProvenance", {}).get("staffImplementationProof", {})
    runtime_implementation = route.get("staffImplementation", {})
    require(runtime_implementation.get("routePageKeys") == ["native_english_teachers", "gs_team"] and runtime_implementation.get("routeMode") == proof.get("routeMode"), "Observed Staff route is not attributed to the packaged current Next route keys and mode.")
    require(runtime_implementation.get("implementationProof") == proof, "Observed Staff route source attribution differs from the P9-14 package fingerprint proof.")
    observations = route.get("staffObservations", [])
    expected_steps = [
        ("native-english-first", "native_english_teachers", "nativeEnglishTeachersTable"),
        ("gs-team-first", "gs_team", "gsTeamTable"),
        ("native-english-reentry", "native_english_teachers", "nativeEnglishTeachersTable"),
        ("gs-team-reentry", "gs_team", "gsTeamTable"),
    ]
    require(len(observations) == 4, "Staff route does not record both tables and both re-entry activations.")
    preparation_ref = manifest.get("fixture", {}).get("preparationReceipt", {})
    preparation_path = verify_artifact(root, str(preparation_ref.get("relativePath", "")), str(preparation_ref.get("sha256", "")))
    preparation = read_json(preparation_path)
    expected_rows = {
        "native_english_teachers": preparation.get("nativeEnglish", {}).get("rows", []),
        "gs_team": preparation.get("gsTeam", {}).get("rows", []),
    }
    post_route_relative = str(route.get("nativeEnglishPostRouteUiaSnapshotRelativePath", ""))
    post_route_records = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == post_route_relative]
    require(bool(post_route_relative) and len(post_route_records) == 1, "Fresh Native English post-route UIA descendant snapshot reference is missing or duplicated.")
    post_route_path = verify_artifact(root, post_route_relative, str(post_route_records[0].get("sha256", "")))
    post_route_snapshot = read_json(post_route_path)
    post_route_identity = validate_staff_post_route_snapshot(post_route_snapshot, pid, int(manifest.get("process", {}).get("mainWindowHandle", 0)))
    for observation, (step, page, table_name) in zip(observations, expected_steps, strict=True):
        require(observation.get("step") == step and observation.get("pageId") == page and observation.get("lifecycleModelRowCount") == 96, f"Staff route observation differs from the 96-row expected route: {step}.")
        expected_title = "Native English Teachers" if page == "native_english_teachers" else "GS Team"
        if step == "native-english-first":
            require(observation.get("postRoutePageUiaSnapshotRelativePath") == post_route_relative, "Native English first activation summary does not retain its fresh post-route UIA snapshot.")
        title_relative = str(observation.get("pageTitleSnapshotRelativePath", ""))
        title_snapshot_records = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == title_relative]
        require(len(title_snapshot_records) == 1 and bool(title_relative), f"Staff runtime page-title UIA snapshot reference is missing or duplicated for {step}.")
        title_snapshot_path = verify_artifact(root, title_relative, str(title_snapshot_records[0].get("sha256", "")))
        title_snapshot = read_json(title_snapshot_path)
        title_control = validate_staff_title_snapshot(title_snapshot, expected_title, pid, int(manifest.get("process", {}).get("mainWindowHandle", 0)))
        require(observation.get("pageTitleControl") == title_control and observation.get("pageTitleControl", {}).get("automationId") == title_control.get("automationId"), f"Staff route summary does not preserve the actual runtime page-title AutomationId for {step}.")
        if step == "native-english-first":
            expected_table_id = expected_staff_table_automation_id(str(title_control.get("automationId", "")), table_name)
            require(post_route_identity.get("tableAutomationId") == expected_table_id, "Native English post-route UIA snapshot table identity differs from the per-page table snapshot.")
        snapshot_path = str(observation.get("tableSnapshotRelativePath", ""))
        snapshot = validate_uia_snapshot(root, route, snapshot_path, pid, 96)
        validate_staff_table_runtime_identity(snapshot, str(title_control.get("automationId", "")), table_name, str(observation.get("tableRuntimeAutomationId", "")), pid, int(manifest.get("process", {}).get("mainWindowHandle", 0)))
        fixture_rows = expected_rows[page]
        table_summary = validate_staff_table_evidence(snapshot, page, fixture_rows, pid, step)
        require(observation.get("tableColumnCount") == table_summary["columnCount"] and observation.get("tableHeaders") == table_summary["headers"] and observation.get("identityCellCount") == table_summary["identityCellCount"] and observation.get("renderedIdentitySamples") == table_summary["renderedIdentitySampleRows"], f"Staff route summary omits the detailed UIA table evidence for {step}.")
        activation = observation.get("activationLifecycle", {})
        require(activation.get("sampleAvailable") is True and activation.get("pageInstanceAvailable") is True, f"Staff activation lifecycle record is unavailable for {step}.")
        require(int(activation.get("itemModelRootRows", 0)) >= 96 and any(int(item.get("rootRows", 0)) == 96 for item in activation.get("itemModels", [])), f"Staff lifecycle model does not expose 96 rows for {step}.")
    expanded_tree = route.get("navigationTreeAfterCampusStaffExpand", {})
    required_children = expanded_tree.get("requiredChildren", [])
    require(expanded_tree.get("campusStaffExpanded") == "Expanded" and [(item.get("name"), item.get("pageId")) for item in required_children] == [("Native English Teachers", "native_english_teachers"), ("GS Team", "gs_team")] and all(item.get("controlType") == "ControlType.TreeItem" and item.get("processId") == pid and item.get("ownerWindowHandle") == manifest.get("process", {}).get("mainWindowHandle") and item.get("rectangle", {}).get("width", 0) > 0 and item.get("rectangle", {}).get("height", 0) > 0 for item in required_children), "Staff route does not verify both visible PID-bound navigation child TreeItems after expansion.")
    route_targets = [item.get("targetPageId") for item in route.get("routeEvents", [])]
    required_targets = ["native_english_teachers", "gs_team", "native_english_teachers", "gs_team", "my_workspace"]
    filtered = [target for target in route_targets if target in {"native_english_teachers", "gs_team", "my_workspace"}]
    cursor = 0
    for target in filtered:
        if cursor < len(required_targets) and target == required_targets[cursor]:
            cursor += 1
    require(cursor == len(required_targets), "Captured page-route events do not prove staff activation, re-entry, and return order.")
    native_lifecycle = validate_lifecycle_events(route, "native_english_teachers", 2, 1)
    gs_lifecycle = validate_lifecycle_events(route, "gs_team", 2, 1)
    return_observation = route.get("returnObservation", {})
    return_title_relative = str(return_observation.get("workspaceTitleSnapshotRelativePath", ""))
    return_title_records = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == return_title_relative]
    require(return_observation.get("targetPageId") == "my_workspace" and len(return_title_records) == 1 and bool(return_title_relative), "Staff return to My Workspace is missing its runtime title snapshot.")
    return_title_path = verify_artifact(root, return_title_relative, str(return_title_records[0].get("sha256", "")))
    return_title_snapshot = read_json(return_title_path)
    return_title = validate_staff_title_snapshot(return_title_snapshot, "My Workspace", pid, int(manifest.get("process", {}).get("mainWindowHandle", 0)))
    require(return_observation.get("workspaceTitle") == return_title, "Staff return UIA title does not match the exact PID/HWND-bound My Workspace Text control.")
    screenshots = validate_screenshots(root, route, {"nativeEnglishTeachersTable", "gsTeamTable"}, pid)
    action_count = validate_actions(root, route, pid)
    return {"workflow": "staff-directory", "rowsPerTable": 96, "observations": [item[0] for item in expected_steps], "nativeEnglishLifecycle": native_lifecycle, "gsTeamLifecycle": gs_lifecycle, "screenshots": screenshots, "uiaActionCount": action_count, "returnTarget": "my_workspace"}


def validate_ai_route(root: Path, manifest: dict[str, Any], preparation: dict[str, Any], pid: int) -> dict[str, Any]:
    route = manifest.get("route", {})
    matrix = route.get("aiMatrix", {})
    require(matrix.get("modelRowCount") == 100 and matrix.get("isolatedMaximumRosterRows") == 100, "Speaking Evaluation model and isolated preference do not both prove 100 rows.")
    validate_uia_snapshot(root, route, str(matrix.get("matrixSnapshotRelativePath", "")), pid, 100)
    candidates = route.get("aiCandidates", {})
    require(candidates.get("candidateCount") == 100 and candidates.get("selectedForPrompt") == 25 and candidates.get("remainingCandidates") == 75, "AI candidate selection boundary is not exactly 25 of 100.")
    require(re.match(r"^25 of 100 students selected\.", str(candidates.get("statusLabel", ""))) is not None, "UIA AI candidate status does not establish 25 selected of 100.")
    validate_uia_snapshot(root, route, str(candidates.get("selectionTableSnapshotRelativePath", "")), pid, 100)

    prompt = route.get("aiPrompt", {})
    prompt_action = next((item for item in route.get("actions", []) if item.get("actionKind") == "create-class-prompt"), None)
    require(prompt.get("creationActionKind") == "create-class-prompt" and prompt_action is not None and prompt_action.get("method") == "InvokePattern.Invoke" and prompt_action.get("invocationReturnedAtUtc"), "Create Class Prompt was not invoked through UIA before response parsing.")
    fallback_action_kind = prompt.get("fallbackActionKind")
    if fallback_action_kind:
        fallback_action = next((item for item in route.get("actions", []) if item.get("actionKind") == fallback_action_kind), None)
        require(fallback_action_kind == "create-class-prompt-uia-bounded-click-fallback" and fallback_action is not None and prompt.get("creationMethod", "").startswith("UIA-derived bounded click"), "Prompt creation fallback action is missing or was not derived from the bounded UIA target.")
        validate_uia_button_click(fallback_action, pid)
    else:
        require(prompt.get("creationMethod", "").startswith("InvokePattern.Invoke"), "Prompt creation method is neither a verified UIA invoke nor its explicit bounded fallback.")
    before_copy = prompt.get("copyPromptButtonBefore", {})
    after_copy = prompt.get("copyPromptButtonAfter", {})
    require(before_copy.get("automationId", "").endswith("speakingEvaluationAiBatchCopyPrompt") and before_copy.get("isEnabled") is False, "Copy Prompt was not UIA-visible and disabled before local prompt generation.")
    require(after_copy.get("automationId") == before_copy.get("automationId") and after_copy.get("isEnabled") is True, "Create Class Prompt did not produce a verified UIA enabled-state transition for Copy Prompt.")
    require(all(item.get("processId") == pid and item.get("ownerWindowProcessId") == pid and item.get("ownerWindowHandle", 0) > 0 for item in (before_copy, after_copy)), "Prompt generation state is not bound to the measured Next process and modal HWND.")
    require(prompt.get("generatedPromptTextUiAExposed") is False and prompt.get("responseInputRequiresLocalPrompt") is True, "Prompt UIA text exposure was reported inconsistently with the recorded modal evidence.")
    require(prompt.get("providerUsed") is False and prompt.get("clipboardUsed") is False, "Generated prompt flow used an external provider or clipboard.")
    prompt_screenshot = str(prompt.get("screenshotRelativePath", ""))
    require(prompt_screenshot == "screenshots/ai-class-prompt-created.png", "Generated prompt screenshot is missing or misidentified.")

    review = route.get("aiReview", {})
    response_path = verify_artifact(root, str(review.get("responsePath", "")), str(review.get("responseSha256", "")))
    response_text = response_path.read_text(encoding="utf-8").rstrip("\n")
    blocks = validate_response_payload(response_text, preparation["rows"])
    require(len(blocks) == review.get("parsedValidCount") == 25 and review.get("parseSummary") == "25 of 25 selected students were parsed.", "AI review does not prove 25 valid parsed response blocks.")
    require(review.get("responseCharacterCount") == len(response_text), "AI response character count differs from the locally entered payload.")
    review_rows = review.get("rows", [])
    require(len(review_rows) == 25 and review.get("reviewModelRowCount") == 25, "AI review model does not expose exactly 25 parsed rows.")
    for index, row in enumerate(review_rows, start=1):
        expected = preparation["rows"][index - 1]
        comment = deterministic_comment(index)
        require(row.get("row") == index - 1 and row.get("studentName") == expected["englishName"], f"Review row {index - 1} changed candidate identity.")
        require(is_valid_ai_review_status(str(row.get("status", ""))) and row.get("initiallyChecked") == "Off", f"Review row {index - 1} did not start unchecked and valid.")
        require(row.get("comment") == comment and row.get("characterCount") == len(comment), f"Review row {index - 1} does not contain the exact deterministic valid comment.")
    checkbox_actions = review.get("checkboxActions", [])
    require(review.get("checkedRowCount") == 25 and len(checkbox_actions) == 25, "AI review did not record exactly 25 checked rows.")
    for index, action in enumerate(checkbox_actions):
        require(action.get("row") == index and action.get("column") == 0 and action.get("stateBefore", {}).get("state") == "Off" and action.get("stateAfter", {}).get("state") == "On", f"Review checkbox state is not Off-to-On at the selection cell for row {index}.")
        require(action.get("stateBefore", {}).get("available") is True and action.get("stateAfter", {}).get("available") is True and action.get("stateBefore", {}).get("source") == action.get("stateAfter", {}).get("source"), f"Review row {index} state transition was not measured through one stable UIA state source.")
        for key in ("cellTargetBefore", "cellTargetAfter"):
            target = action.get(key, {})
            validate_ai_cell_target(target, index, 0, pid, f"Review row {index} {key}")
        before_identity = action["cellTargetBefore"]["cellIdentity"]
        after_identity = action["cellTargetAfter"]["cellIdentity"]
        require(all(before_identity.get(key) == after_identity.get(key) for key in ("name", "automationId", "controlType", "processId", "runtimeId")), f"Review row {index} changed UIA cell identity between input and verification.")
        validate_ai_cell_action(action, index, pid)
    require(review.get("applyButton", {}).get("isEnabled") is True and review.get("applyButton", {}).get("processId") == pid, "UIA Apply control is not enabled and PID-bound after all 25 checks.")
    validate_uia_snapshot(root, route, str(review.get("tableSnapshotRelativePath", "")), pid, 25)
    screenshots = validate_screenshots(root, route, {"ai-class-prompt-created", "ai-batch-review"}, pid)

    apply_record = route.get("aiApply", {})
    require(apply_record.get("appliedCount") == 25 and "Speaking evaluation saved." in str(apply_record.get("saveConfirmation", "")), "AI batch was not saved as one 25-comment batch.")
    require(apply_record.get("saveButton", {}).get("isEnabled") is True and apply_record.get("saveButton", {}).get("processId") == pid, "Save Changes action was not UIA-visible and PID-bound.")
    reopened = route.get("aiReopen", {})
    require(reopened.get("persistedVisibleCommentsVerified") == 25 and reopened.get("preservedBlankRowsVerified") == 75, "Reopened Speaking Evaluation did not verify 25 persisted comments and 75 unchanged rows.")
    descendants_path = str(reopened.get("mainDescendantSnapshotRelativePath", ""))
    descendants_refs = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == descendants_path]
    require(len(descendants_refs) == 1, "Reopened Next main UIA descendant snapshot is missing or duplicated.")
    descendants_snapshot = read_json(verify_artifact(root, descendants_path, str(descendants_refs[0].get("sha256", ""))))
    validate_reopened_matrix_descendants(descendants_snapshot, pid)
    validate_uia_snapshot(root, route, str(reopened.get("matrixSnapshotRelativePath", "")), pid, 100)
    lifecycle = validate_lifecycle_events(route, "speaking_evaluation", 2, 1)
    route_targets = [item.get("targetPageId") for item in route.get("routeEvents", [])]
    require(route_targets.count("speaking_evaluation") >= 2 and route_targets.count("my_workspace") >= 2, "AI route does not prove leave and re-entry page-route events.")
    require(review.get("responsePath") == "output/deterministic-ai-response.txt", "AI response was not retained as the deterministic local evidence artifact.")
    response_action = next((item for item in route.get("actions", []) if item.get("actionKind") == "enter-deterministic-response-locally"), None)
    require(response_action is not None and response_action.get("providerUsed") is False and response_action.get("clipboardUsed") is False, "AI response evidence does not prove local UIA entry without provider or clipboard use.")
    action_kinds = [str(item.get("actionKind", "")) for item in route.get("actions", [])]
    require(action_kinds.index("create-class-prompt") < action_kinds.index("enter-deterministic-response-locally") < action_kinds.index("parse-ai-response"), "AI prompt creation, local response entry, and parsing were not observed in the required order.")
    speaking_navigation_indexes = [index for index, kind in enumerate(action_kinds) if kind == "navigate-speaking_evaluation"]
    require(len(speaking_navigation_indexes) >= 2 and speaking_navigation_indexes[1] < action_kinds.index("reselect-ai-fixture-class-after-save"), "AI persisted-output verification did not reselect the exact fixture class after route re-entry.")
    implementation = route.get("aiImplementation", {})
    require(implementation.get("routePageKey") == "speaking_evaluation" and implementation.get("routeMode", "").startswith("current-next-speaking-evaluation"), "Observed AI route is not the current Next Speaking Evaluation page.")
    require(implementation.get("applicationName") == "ClassMngrNext.exe" and implementation.get("dialogRootAutomationId") == "QApplication.speakingEvaluationAiBatchDialog", "Visible modal root does not prove the Next AI dialog object identity.")
    require(implementation.get("dialogRootProcessId") == pid and implementation.get("dialogRootWindowHandle", 0) > 0 and implementation.get("visibleNextDialogCount") == 1 and implementation.get("visibleLegacyDialogCount") == 0, "Visible app-owned dialog inventory does not uniquely prove Next dialog use.")
    require(implementation.get("implementationProof", {}).get("legacyDialogInvoked") is False, "Implementation proof does not exclude invocation of the Legacy dialog.")
    require("lifecycle-speaking-evaluation" not in implementation.get("lifecyclePageKeys", []), "Forbidden Legacy lifecycle-speaking-evaluation key was observed.")
    dialog_snapshot_path = str(implementation.get("dialogRootSnapshotRelativePath", ""))
    dialog_snapshot_ref = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == dialog_snapshot_path]
    require(len(dialog_snapshot_ref) == 1, "Next AI dialog root UIA snapshot is missing.")
    dialog_snapshot = read_json(verify_artifact(root, dialog_snapshot_path, str(dialog_snapshot_ref[0].get("sha256", ""))))
    dialog_control = dialog_snapshot.get("control", {})
    require(dialog_control.get("automationId") == "QApplication.speakingEvaluationAiBatchDialog" and dialog_control.get("processId") == pid and dialog_control.get("ownerWindowProcessId") == pid, "Next AI dialog root snapshot does not identify the measured process and exact object name.")
    descendants_path = str(implementation.get("dialogDescendantSnapshotRelativePath", ""))
    descendants_ref = [item for item in route.get("uiaSnapshots", []) if item.get("relativePath") == descendants_path]
    require(len(descendants_ref) == 1, "Next AI modal descendant UIA snapshot is missing.")
    descendants_snapshot = read_json(verify_artifact(root, descendants_path, str(descendants_ref[0].get("sha256", ""))))
    require(descendants_snapshot.get("root", {}).get("automationId") == "QApplication.speakingEvaluationAiBatchDialog" and descendants_snapshot.get("root", {}).get("processId") == pid, "Next AI descendant snapshot is detached from the exact measured dialog root.")
    require(all(item.get("processId") == pid for item in descendants_snapshot.get("descendants", [])), "Next AI modal descendant snapshot contains a foreign process element.")
    require(all(item.get("processId") == pid for item in implementation.get("visibleAppOwnedTopLevelWindows", [])), "Visible application window inventory contains a foreign process window.")
    require(not any(item.get("automationId") == "QApplication.speakingEvalAiBatchDialog" for item in implementation.get("visibleAppOwnedTopLevelWindows", [])), "A visible Legacy speakingEvalAiBatchDialog was instantiated.")
    action_count = validate_actions(root, route, pid)
    return {"workflow": "speaking-evaluation-ai-batch", "candidateRows": 100, "selectedRows": 25, "parsedValidRows": 25, "appliedRows": 25, "preservedRows": 75, "responseBlocks": blocks, "speakingEvaluationLifecycle": lifecycle, "screenshots": screenshots, "uiaActionCount": action_count}


def validate_evidence(evidence_dir: Path) -> dict[str, Any]:
    root = evidence_dir.resolve(strict=True)
    manifest_path = root / "manifest.json"
    manifest = read_json(manifest_path)
    require(manifest.get("schema") == SCHEMA, "P9-14 manifest schema is invalid.")
    require(manifest.get("status") in {"evidence-collected", "passed"}, "P9-14 run did not collect complete evidence.")
    require(manifest.get("failure") is None, "P9-14 run manifest contains a failure.")
    mode_to_task = {"staff-directory": "P9_14_STAFF_DIRECTORY_PACKAGED_LIFECYCLE", "ai-batch-review": "P9_14_AI_BATCH_REVIEW_PACKAGED_LIFECYCLE"}
    mode = str(manifest.get("mode", ""))
    require(mode in mode_to_task and manifest.get("taskId") == mode_to_task[mode], "P9-14 route mode and task ID do not match.")
    require(manifest.get("runId") == root.name, "Evidence directory name differs from the immutable run ID.")

    route_reference = manifest.get("routeManifest", {})
    route_path = verify_artifact(root, str(route_reference.get("path", "")), str(route_reference.get("sha256", "")))
    route = read_json(route_path)
    require(route == manifest.get("route"), "Separate route manifest content differs from the main run manifest.")
    require(route.get("workflow") == mode and route.get("actions") and route.get("completedAtUtc"), "Separate route manifest is incomplete.")

    preparation, before, after_pack = validate_fixture(root, manifest)
    settings_summary = validate_settings(root, manifest)
    package_summary = validate_package_provenance(root, manifest)
    process_summary = validate_process(root, manifest, package_summary)
    if mode == "staff-directory":
        route_summary = validate_staff_route(root, manifest, int(process_summary["processId"]))
        minimum_checkpoints = 5
    else:
        route_summary = validate_ai_route(root, manifest, preparation, int(process_summary["processId"]))
        minimum_checkpoints = 4
    memory_summary = validate_memory(root, manifest, int(process_summary["processId"]), minimum_checkpoints)

    cleanup = manifest.get("cleanup", {})
    trace_path = cleanup.get("shutdownTracePath")
    if trace_path:
        trace = contained_path(root, str(trace_path))
        require(trace.stat().st_size > 0, "Application shutdown trace is present but empty.")
    else:
        raise ValidationError("Natural exit did not retain an application shutdown trace.")
    result = {
        "schema": "classmngr-phase9-p9-14-validator-result-v1",
        "status": "passed",
        "taskId": manifest["taskId"],
        "runId": manifest["runId"],
        "mode": mode,
        "packageProvenance": package_summary,
        "fixture": after_pack["summary"],
        "settings": settings_summary,
        "process": process_summary,
        "route": route_summary,
        "memory": memory_summary,
        "shutdownTrace": {"path": str(trace_path), "bytes": trace.stat().st_size},
        "evidenceDirectory": str(root),
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = validate_evidence(args.evidence_dir)
        print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
        return 0
    except Exception as error:
        print(json.dumps({"schema": "classmngr-phase9-p9-14-validator-result-v1", "status": "failed", "evidenceDirectory": str(args.evidence_dir), "error": str(error)}, ensure_ascii=False, separators=(",", ":")))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
