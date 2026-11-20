#!/usr/bin/env python3
"""Fail-closed validator for the P9-15 packaged Schedule locale/theme route."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA = "classmngr-phase9-p9-15-packaged-schedule-locale-theme-v1"
PREPARATION_SCHEMA = "classmngr-phase9-p9-15-schedule-workspace-v1"
TASK_ID = "P9_15_PACKAGED_NEXT_SCHEDULE_LOCALE_THEME"
STRICT_WS_LIMIT = 262_144_000
OPERATION_WS_LIMIT = 536_870_912
SCHEDULE_STATES = [
    ("initial", "English", "Light"),
    ("ko_light", "Korean", "Light"),
    ("ko_dark", "Korean", "Dark"),
    ("en_dark", "English", "Dark"),
    ("en_light", "English", "Light"),
    ("reentry_1", "English", "Light"),
    ("reentry_2", "English", "Light"),
]
P913_ALLOWED = {
    "scripts/phase9/prepare_class_transfer_workspace.py",
    "scripts/phase9/prepare_speaking_evaluation_workspace.py",
    "scripts/phase9/run_packaged_next_class_transfer.ps1",
    "scripts/phase9/run_packaged_next_pdf_viewer_lifecycle.ps1",
    "scripts/phase9/run_packaged_next_speaking_evaluation_lifecycle.ps1",
    "scripts/phase9/test_prepare_class_transfer_workspace.py",
    "scripts/phase9/test_validate_packaged_next_class_transfer.py",
    "scripts/phase9/test_validate_packaged_next_pdf_viewer_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_speaking_evaluation_lifecycle.py",
    "scripts/phase9/validate_packaged_next_class_transfer.py",
    "scripts/phase9/validate_packaged_next_pdf_viewer_lifecycle.py",
    "scripts/phase9/validate_packaged_next_speaking_evaluation_lifecycle.py",
}
P914_SOURCES = {
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/prepare_p9_14_staff_ai_workspaces.py",
    "scripts/phase9/run_packaged_next_p9_14_staff_directory_lifecycle.ps1",
    "scripts/phase9/run_packaged_next_p9_14_ai_batch_review_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_p9_14_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_p9_14_lifecycle.py",
}
P915_SOURCES = {
    "scripts/phase9/prepare_p9_15_schedule_workspace.py",
    "scripts/phase9/run_packaged_next_p9_15_schedule_locale_theme.ps1",
    "scripts/phase9/validate_packaged_next_p9_15_schedule_locale_theme.py",
    "scripts/phase9/test_validate_packaged_next_p9_15_schedule_locale_theme.py",
}
ENGLISH_HEADERS = ["Time", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
KOREAN_HEADERS = ["\uc2dc\uac04", "\uc6d4\uc694\uc77c", "\ud654\uc694\uc77c", "\uc218\uc694\uc77c", "\ubaa9\uc694\uc77c", "\uae08\uc694\uc77c", "\ud1a0\uc694\uc77c", "\uc77c\uc694\uc77c"]
ENGLISH_TITLE = "Weekly Class Schedule"
KOREAN_TITLE = "\uc8fc\uac04 \uc218\uc5c5 \uc77c\uc815"
KOREAN_PREFERENCES_TITLE = "\ud658\uacbd \uc124\uc815"
KOREAN_ESSAY_LABEL = "\uc5d0\uc138\uc774"


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
    require(not candidate.is_absolute() and ".." not in candidate.parts, f"Unsafe relative evidence path: {relative!r}")
    target = (root / Path(*candidate.parts)).resolve(strict=True)
    resolved_root = root.resolve(strict=True)
    require(target == resolved_root or resolved_root in target.parents, f"Evidence artifact escaped its run directory: {relative}")
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


def normalized_lines(value: str) -> list[str]:
    return [" ".join(line.split()) for line in value.replace("\r", "").split("\n") if line.strip()]


def validate_cell_projection(preparation_schedule: dict[str, Any], snapshot: dict[str, Any], locale: str) -> dict[str, Any]:
    table = snapshot.get("table", {})
    grid = table.get("gridPattern", {})
    expected_grid = preparation_schedule.get("uiGrid", {})
    rows = int(expected_grid.get("rowCount", -1))
    columns = int(expected_grid.get("columnCount", -1))
    require(grid.get("available") is True and grid.get("rowCount") == rows and grid.get("columnCount") == columns,
            f"Schedule UIA GridPattern dimensions differ from the fixture: expected {rows}x{columns}.")
    main = snapshot.get("mainWindow", {})
    table_control = table.get("control", {})
    table_id = str(table_control.get("automationId", ""))
    require(table_control.get("processId") == snapshot.get("processId") == main.get("processId")
            and table_control.get("ownerWindowHandle") == main.get("windowHandle")
            and table_control.get("ownerWindowProcessId") == snapshot.get("processId"),
            "Schedule table control is not bound to the measured main PID/HWND.")
    require((table_id == "nextScheduleTable" or table_id.endswith(".nextScheduleTable"))
            and table_control.get("controlType") in {"ControlType.Table", "ControlType.DataGrid"},
            "Schedule table UIA control identity does not match nextScheduleTable Table/DataGrid.")
    cells = snapshot.get("cells")
    require(isinstance(cells, list) and len(cells) == rows * columns, "Schedule UIA evidence does not contain every table cell.")
    by_coordinate: dict[tuple[int, int], dict[str, Any]] = {}
    for cell in cells:
        row, column = int(cell.get("row", -1)), int(cell.get("column", -1))
        key = (row, column)
        require(0 <= row < rows and 0 <= column < columns and key not in by_coordinate,
                f"Schedule UIA cell coordinate is invalid or duplicated: {key}.")
        control = cell.get("control", {})
        require(control.get("processId") == snapshot.get("processId") == main.get("processId"),
                f"Schedule UIA cell {key} is not bound to the measured process.")
        require(control.get("ownerWindowHandle") == main.get("windowHandle")
                and control.get("ownerWindowProcessId") == snapshot.get("processId"),
                f"Schedule UIA cell {key} is not bound to the measured main HWND.")
        by_coordinate[key] = cell
    headers = snapshot.get("headers")
    expected_headers = ENGLISH_HEADERS if locale == "English" else KOREAN_HEADERS
    require(isinstance(headers, list) and len(headers) == columns, "Schedule UIA evidence is missing table column headers.")
    actual_headers = []
    for index, header in enumerate(headers):
        require(int(header.get("column", -1)) == index, "Schedule UIA column headers are not in grid order.")
        require(int(header.get("processId", 0)) == int(snapshot.get("processId", 0)), "Schedule UIA header is not bound to the measured PID.")
        require(int(header.get("ownerWindowHandle", 0)) == int(main.get("windowHandle", 0))
                and int(header.get("ownerWindowProcessId", 0)) == int(snapshot.get("processId", 0)),
                "Schedule UIA header is not bound to the measured main HWND/PID.")
        actual_headers.append(str(header.get("text", "")))
    require(actual_headers == expected_headers, f"Schedule UIA headers do not match the selected {locale} locale: {actual_headers!r}.")
    projection = preparation_schedule.get("fixtureProjection", {})
    expected_pairs: dict[tuple[int, int], list[str]] = {}
    for coordinate_text, entries in projection.items():
        row_text, column_text = coordinate_text.split(",", 1)
        key = (int(row_text), int(column_text))
        pairs: list[str] = []
        for entry in entries:
            pairs.extend([str(entry["teacherRoom"]), str(entry["classLine"])])
        expected_pairs[key] = pairs
    matched = 0
    for row in range(rows):
        time_cell = by_coordinate[(row, 0)]
        require(normalized_lines(str(time_cell.get("text", ""))), f"Schedule time label cell is blank at row {row}.")
        for column in range(1, columns):
            key = (row, column)
            actual = normalized_lines(str(by_coordinate[key].get("text", "")))
            expected = expected_pairs.get(key, [])
            if expected:
                require(sorted(actual) == sorted(expected), f"Schedule cell {key} differs from fixture-derived teacher/room and grade/level entries.")
                matched += len(expected) // 2
            else:
                default_states = preparation_schedule.get("defaultSlotStates", {})
                require(f"{row},{column}" in default_states, f"Schedule cell {key} has neither fixture entries nor a fixture-derived default slot state.")
                expected_default = []
                if default_states[f"{row},{column}"] == "Essay":
                    expected_default = ["Essay" if locale == "English" else KOREAN_ESSAY_LABEL]
                else:
                    require(default_states[f"{row},{column}"] == "Empty", f"Schedule cell {key} has an unsupported fixture default slot state.")
                require(actual == expected_default, f"Schedule cell {key} differs from its fixture-derived default {default_states[f'{row},{column}']} state.")
    require(matched == int(preparation_schedule.get("scheduleEntryCount", -1)) == 768,
            "Rendered Schedule UIA cells do not account for the 768 fixture entries.")
    title = str(snapshot.get("schedulePage", {}).get("accessibleName", ""))
    page = snapshot.get("schedulePage", {})
    require(page.get("processId") == snapshot.get("processId") and page.get("ownerWindowHandle") == main.get("windowHandle"),
            "Schedule page title control is not bound to the measured main PID/HWND.")
    required_title = ENGLISH_TITLE if locale == "English" else KOREAN_TITLE
    require(title == required_title, f"Schedule accessible page title does not show the selected {locale} translation.")
    return {"rows": rows, "columns": columns, "headerCount": len(actual_headers), "uiacellCount": len(cells),
            "fixtureEntriesMatchedInRenderedCells": matched, "headers": actual_headers,
            "pageTitle": title, "allCellsBoundToMeasuredPidAndHwnd": True}


def validate_preference_state(state: dict[str, Any], expected_locale: str, expected_theme: str, pid: int, hwnd: int) -> None:
    require(state.get("locale") == expected_locale and state.get("theme") == expected_theme, "Captured Schedule state differs from the requested locale/theme sequence.")
    preferences = state.get("preferences", {})
    dialog_hwnd = int(preferences.get("dialogWindowHandle", 0))
    require(dialog_hwnd > 0 and dialog_hwnd != hwnd and preferences.get("dialogProcessId") == pid,
            "Preferences dialog runtime HWND/PID identity is missing or inconsistent.")
    dialog_name = str(preferences.get("dialogName", ""))
    dialog_id = str(preferences.get("dialogAutomationId", ""))
    dialog_match = str(preferences.get("dialogIdentityMatchedBy", ""))
    require(preferences.get("dialogControlType") == "ControlType.Window"
            and isinstance(preferences.get("dialogClassName"), str),
            "Preferences root control type/native class identity is missing or inconsistent.")
    if dialog_id == "nextPreferencesDialog" or dialog_id.endswith(".nextPreferencesDialog"):
        require("AutomationId" in dialog_match, "Preferences dialog automation ID was not recorded as the identity match.")
    else:
        require(dialog_name in {"Preferences", KOREAN_PREFERENCES_TITLE}
                and "localized Preferences window title" in dialog_match and "measured PID/HWND" in dialog_match,
                "Preferences root was not identified by its measured localized title and PID/HWND.")
    for key, expected in (("language", expected_locale), ("theme", expected_theme)):
        record = preferences.get(key, {})
        control = record.get("control", {})
        require(record.get("selectedItem") == expected, f"Preferences {key} selection does not confirm {expected}.")
        require(control.get("processId") == pid and control.get("ownerWindowHandle") == dialog_hwnd and control.get("ownerWindowProcessId") == pid,
                f"Preferences {key} control identity is not bound to its measured dialog HWND/PID.")


def validate_generic_lifecycle(event: dict[str, Any], expected_pid: int) -> dict[str, Any]:
    record = event.get("record", event)
    page = record.get("page", {})
    page_key = record.get("page_key", record.get("pageKey"))
    sample_available = record.get("sample_available", record.get("sampleAvailable"))
    instance_available = record.get("page_instance_available", record.get("pageInstanceAvailable"))
    require(record.get("event") == "page-lifecycle" and page_key == "schedule" and record.get("state") == "activated",
            "Generic lifecycle sample is not an activated Schedule page event.")
    require(sample_available is True and instance_available is True,
            "Schedule lifecycle event lacks generic widget/model sample availability.")
    for key in ("widget_count", "item_model_count", "item_model_root_rows", "table_widget_count", "table_item_count",
                "inspected_table_cells", "view_index_widget_count", "editor_control_widget_count"):
        require(isinstance(page.get(key), int) and 0 <= page[key] <= 100_000, f"Generic lifecycle count is missing or outside its bounded range: {key}.")
    for key in ("widget_scan_truncated", "item_models_truncated", "table_scan_truncated", "view_index_widget_scan_truncated"):
        require(isinstance(page.get(key), bool), f"Generic lifecycle scan-truncation field is missing: {key}.")
        require(page[key] is False, f"Generic lifecycle diagnostic scan was truncated: {key}.")
    models = page.get("item_models")
    require(isinstance(models, list) and len(models) <= 256, "Generic lifecycle item-model details exceed their bounded diagnostic limit.")
    for model in models:
        require(isinstance(model.get("root_rows"), int) and 0 <= model["root_rows"] <= 100_000,
                "Generic lifecycle model root-row count is missing or outside its bound.")
    require(not any(re.search(r"schedule.*(?:entry|class).*count", str(key), re.IGNORECASE)
                    for key in page), "Generic lifecycle metrics must not be represented as Schedule entry/class metrics.")
    return {"widgetCount": page["widget_count"], "itemModelCount": page["item_model_count"],
            "itemModelRootRows": page["item_model_root_rows"], "detailedModels": len(models),
            "scanTruncation": {key: page[key] for key in ("widget_scan_truncated", "item_models_truncated", "table_scan_truncated", "view_index_widget_scan_truncated")},
            "scheduleSpecificEntryMetricClaimed": False, "processId": expected_pid}


def validate_workspace_rows(preparation: dict[str, Any], before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    require(preparation.get("kind") == "schedule-locale-theme" and preparation.get("status") == "passed", "P9-15 preparation receipt is not a passed Schedule fixture.")
    schedule = preparation.get("schedule", {})
    require(schedule.get("classCount") == 96 and schedule.get("classInfoCount") == 96 and schedule.get("scheduleEntryCount") == 768,
            "Seeded large_startup fixture is not exactly 96 classes and 768 schedule rows.")
    require(schedule.get("showEnglishNames") is True and schedule.get("showWeekends") is True
            and schedule.get("uiGrid") == {"headers": ENGLISH_HEADERS, "rowCount": 7, "columnCount": 8, "sourceRowOffset": 15},
            "Seeded Schedule display settings do not derive the expected teacher labels and seven day columns.")
    require(isinstance(schedule.get("defaultSlotStates"), dict), "Seeded Schedule fixture is missing the expected default-slot projection.")
    for snapshot, phase in ((before, "before"), (after, "after")):
        require(snapshot.get("schema") == PREPARATION_SCHEMA and snapshot.get("status") == "passed" and snapshot.get("phase") == phase,
                f"Workspace {phase} snapshot identity/status is invalid.")
        require(snapshot.get("integrityCheck") == "ok" and snapshot.get("foreignKeyErrors") == [], f"Workspace {phase} SQLite integrity failed.")
        require(snapshot.get("workspaceTables") == preparation.get("baselineTables"), f"Workspace {phase} rows differ from the seeded baseline.")
        require(snapshot.get("workspaceTablesSha256") == preparation.get("baselineTablesSha256"), f"Workspace {phase} row fingerprint differs from the seeded baseline.")
        require(snapshot.get("schedule", {}).get("scheduleRows") == schedule.get("scheduleRows"), f"Workspace {phase} schedule rows differ from the pinned seed fixture.")
    require(before.get("workspaceTables") == after.get("workspaceTables"), "Workspace rows changed during the Schedule route.")
    return {"classCount": 96, "scheduleEntries": 768, "workspaceTableCount": preparation.get("schedule", {}).get("workspaceTableCount", len(preparation.get("baselineTables", {}))),
            "allUserTableRowsUnchanged": True, "sqliteIntegrity": "ok"}


def validate_package_provenance(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    proof = manifest.get("packageProvenance", {})
    require(proof.get("reusedPackage") is True and proof.get("freshExactTreeRebuild") is False,
            "P9-15 package reuse was not backed by the accepted P9-14 equivalence route.")
    prior_manifest_path = verify_absolute_file(str(proof.get("priorRunManifestPath", "")), str(proof.get("priorRunManifestSha256", "")), "Accepted P9-14 manifest")
    prior = read_json(prior_manifest_path)
    require(prior.get("taskId") == "P9_14_STAFF_DIRECTORY_PACKAGED_LIFECYCLE" and prior.get("status") == "passed",
            "Package ancestry is not a passed P9-14 packaged Windows route.")
    prior_proof = prior.get("packageProvenance", {})
    require(prior_proof.get("productSourceSha256") == proof.get("productSourceSha256"), "P9-14 and P9-15 product-source fingerprints differ.")
    prior_run_root = prior_manifest_path.parent
    prior_source_path = verify_artifact(prior_run_root, str(prior_proof.get("currentSourceInventoryPath", "")), str(prior_proof.get("currentSourceInventorySha256", "")))
    old_source = read_json(prior_source_path)
    source_path = verify_artifact(root, str(proof.get("sourceInventoryPath", "")), str(proof.get("sourceInventoryFileSha256", "")))
    current_source = read_json(source_path)
    files = current_source.get("files", [])
    require(inventory_fingerprint(files) == current_source.get("sha256") == proof.get("currentSourceSha256"), "Current source inventory fingerprint is inconsistent.")
    old_map = {str(item["path"]): item for item in old_source.get("files", [])}
    current_map = {str(item["path"]): item for item in files}
    require(set(current_map) - set(old_map) == P915_SOURCES, "P9-15 source additions are not exactly its four owned files.")
    for path, old in old_map.items():
        now = current_map.get(path)
        require(now is not None and old.get("sizeBytes") == now.get("sizeBytes") and old.get("sha256") == now.get("sha256"), f"A P9-14 captured source changed during P9-15: {path}")

    p913_path = verify_absolute_file(str(proof.get("priorP913ManifestPath", "")), str(proof.get("priorP913ManifestSha256", "")), "P9-13 package ancestry manifest")
    p913 = read_json(p913_path)
    require(p913.get("taskId") == "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE" and p913.get("status") == "passed", "P9-13 source-derivation ancestry is not a passed run.")
    p913_root = p913_path.parent
    derived_path = verify_artifact(p913_root, str(p913.get("source", {}).get("derivedReceiptPath", "")), str(p913.get("source", {}).get("derivedReceiptSha256", "")))
    derived = read_json(derived_path)
    ignored = set(derived.get("source", {}).get("allowedPaths", [])) | P914_SOURCES | P915_SOURCES
    product_rows = [row for row in files if str(row["path"]) not in ignored]
    product_hash = inventory_fingerprint(product_rows)
    require(product_hash == proof.get("productSourceSha256") == p913.get("source", {}).get("productSourceSha256"), "Current product sources do not match the accepted Windows Release package.")

    package_inventory_path = verify_artifact(root, str(proof.get("packageInventoryPath", "")), str(proof.get("packageInventoryFileSha256", "")))
    package_inventory = read_json(package_inventory_path)
    package_root = Path(str(proof.get("packageRoot", ""))).resolve(strict=True)
    package_rows = package_inventory.get("files", [])
    require(inventory_fingerprint(package_rows) == package_inventory.get("sha256") == proof.get("packageSha256"), "Package inventory fingerprint differs from package proof.")
    actual_package_rows = []
    for row in package_rows:
        relative = PurePosixPath(str(row["path"]))
        path = (package_root / Path(*relative.parts)).resolve(strict=True)
        require(package_root == path.parent or package_root in path.parents, f"Package file escaped package root: {relative}")
        require(path.is_file() and path.stat().st_size == int(row["sizeBytes"]) and sha256_file(path) == str(row["sha256"]), f"Packaged file differs from accepted inventory: {relative}")
        actual_package_rows.append({"path": str(row["path"]), "sizeBytes": int(row["sizeBytes"]), "sha256": str(row["sha256"])})
    require(len(actual_package_rows) == int(proof.get("packageFileCount", -1)), "Package file count differs from accepted inventory.")
    app_path = verify_absolute_file(str(proof.get("applicationPath", "")), str(proof.get("applicationSha256", "")), "Packaged Next executable")
    require(app_path == package_root / "ClassMngrNext.exe", "Measured executable is outside the proven package root.")
    with app_path.open("rb") as stream:
        stream.seek(0x3C)
        pe_offset = struct.unpack("<I", stream.read(4))[0]
        stream.seek(pe_offset + 4)
        machine = struct.unpack("<H", stream.read(2))[0]
    require(machine == 0x8664 and proof.get("architecture") == "x64", "Packaged application is not Windows x64.")

    release_path = verify_absolute_file(str(proof.get("acceptedReleaseReceiptPath", "")), str(proof.get("acceptedReleaseReceiptSha256", "")), "Accepted Release build receipt")
    release = read_json(release_path)
    require(release.get("status") == "succeeded" and release.get("configuration") == "Release" and release.get("platform") == "x64"
            and release.get("build", {}).get("exitCode") == 0, "Accepted build receipt does not prove a successful local Windows x64 Release build.")
    require(release.get("releaseExecutableSha256") == sha256_file(app_path), "Packaged executable does not match the accepted Release receipt.")
    cache_path = verify_absolute_file(str(proof.get("configuredCachePath", "")), str(proof.get("configuredCacheSha256", "")), "Configured x64 Release CMake cache")
    require(sha256_file(cache_path) == release.get("cacheSha256After"), "Configured CMake cache differs from the accepted Release receipt.")
    resource_inventory_path = verify_artifact(root, str(proof.get("resourceInventoryPath", "")), str(proof.get("resourceInventoryFileSha256", "")))
    resource = read_json(resource_inventory_path)
    require(resource.get("sha256") == proof.get("resourceInventorySha256") == release.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256"),
            "Staged package resource inventory differs from the accepted Release receipt.")
    return {"packageRoot": str(package_root), "packageSha256": proof["packageSha256"], "packageFileCount": len(package_rows),
            "applicationPath": str(app_path), "applicationSha256": sha256_file(app_path), "architecture": "x64",
            "configuration": "Release", "productSourceSha256": product_hash, "freshExactTreeRebuild": False,
            "reusedFromTaskId": prior.get("taskId")}


def validate_settings(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    settings = manifest.get("settings", {})
    inventory_path = verify_artifact(root, settings.get("beforeLaunchInventoryPath", ""), settings.get("beforeLaunchInventorySha256", ""))
    inventory = read_json(inventory_path)
    require(inventory.get("sha256") == settings.get("beforeLaunchInventoryContentSha256") and inventory.get("fileCount") == 1,
            "Before-launch settings inventory does not identify one isolated profile.")
    profile_path = Path(str(settings.get("root", ""))) / "PaperCloud" / "ClassMngr.ini"
    initial = parse_ini(profile_path)
    require(initial.get("options", {}).get("language") == "1" and initial.get("options", {}).get("theme") == "1",
            "Isolated preferences did not start at English/Light.")
    cleanup = manifest.get("cleanup", {})
    after_ref = cleanup.get("settingsInventoryAfterExit", {})
    after_path = verify_artifact(root, after_ref.get("relativePath", ""), after_ref.get("inventoryFileSha256", ""))
    after = read_json(after_path)
    require(after.get("sha256") == after_ref.get("sha256") and after.get("fileCount") == 1,
            "After-exit isolated settings inventory is inconsistent.")
    require(len(after.get("files", [])) == 1 and after["files"][0].get("path") == "PaperCloud/ClassMngr.ini"
            and after["files"][0].get("sha256") == sha256_file(profile_path), "After-exit inventory does not fingerprint the final isolated preferences file.")
    final = parse_ini(profile_path)
    require(final.get("options", {}).get("language") == "1" and final.get("options", {}).get("theme") == "1",
            "Final isolated preferences did not return to English/Light.")
    require(settings.get("globalDefaultChanged") is False and settings.get("settingsRootFromEnvironment") is True,
            "Run does not prove preferences were isolated from global defaults.")
    return {"isolatedProfileFileCount": 1, "initialState": "English/Light", "finalState": "English/Light", "globalDefaultChanged": False}


def parse_ini(path: Path) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    active = ""
    for raw in path.read_text(encoding="utf-8").splitlines():
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


def validate_route(root: Path, manifest: dict[str, Any], preparation: dict[str, Any], pid: int, hwnd: int) -> dict[str, Any]:
    route = manifest.get("route", {})
    states = route.get("scheduleStates", [])
    require(len(states) == len(SCHEDULE_STATES), "Schedule locale/theme route does not contain all seven required state captures.")
    observed = []
    for state_record, (name, locale, theme) in zip(states, SCHEDULE_STATES, strict=True):
        require(state_record.get("name") == name, f"Schedule state observation is out of order; expected {name}.")
        validate_preference_state(state_record, locale, theme, pid, hwnd)
        ui_ref = state_record.get("uiaSnapshot", {})
        ui_path = verify_artifact(root, ui_ref.get("relativePath", ""), ui_ref.get("sha256", ""))
        snapshot = read_json(ui_path)
        require(snapshot.get("processId") == pid and snapshot.get("mainWindow", {}).get("windowHandle") == hwnd,
                f"Schedule UIA state {name} is not bound to the live app PID/HWND.")
        proof = validate_cell_projection(preparation["schedule"], snapshot, locale)
        lifecycle = validate_generic_lifecycle(snapshot.get("genericLifecycle", {}), pid)
        screenshot = state_record.get("screenshot", {})
        shot_path = verify_artifact(root, screenshot.get("relativePath", ""), screenshot.get("sha256", ""))
        require(screenshot.get("processId") == pid == screenshot.get("windowProcessId") and screenshot.get("windowHandle") == hwnd,
                f"Schedule screenshot {name} was not captured from the measured HWND/PID.")
        require(screenshot.get("width", 0) > 0 and screenshot.get("height", 0) > 0 and shot_path.stat().st_size > 1024,
                f"Schedule screenshot {name} is empty or has invalid dimensions.")
        visual = state_record.get("visualMetrics", {})
        require(0 <= float(visual.get("meanLuminance", -1)) <= 255 and 0 <= float(visual.get("darkPixelRatio", -1)) <= 1,
                f"Schedule screenshot {name} is missing sampled visual metrics.")
        observed.append({"name": name, "locale": locale, "theme": theme, "uia": proof, "genericLifecycle": lifecycle,
                         "screenshotPath": screenshot["relativePath"], "visualMetrics": visual})
    theme_metrics = {item["theme"]: item["visualMetrics"] for item in observed if item["name"] in ("en_dark", "en_light")}
    require(set(theme_metrics) == {"Light", "Dark"}, "English Light/Dark screenshots are not both available for visual theme comparison.")
    light, dark = float(theme_metrics["Light"]["meanLuminance"]), float(theme_metrics["Dark"]["meanLuminance"])
    require(light - dark >= 8.0, f"PID/HWND-bound English Light/Dark screenshots do not demonstrate the expected palette change: light={light:.1f}, dark={dark:.1f}.")

    events = [item for item in route.get("lifecycleEvents", []) if item.get("event") == "page-lifecycle" and item.get("pageKey") == "schedule"]
    activated = [item for item in events if item.get("state") == "activated"]
    suspended = [item for item in events if item.get("state") == "suspended"]
    require(len(activated) >= 3 and len(suspended) >= 2, "Schedule lifecycle evidence lacks initial activation or two clean leave/re-entry cycles.")
    for event in activated:
        generic = dict(event)
        generic["event"] = "page-lifecycle"
        generic["page_key"] = str(event.get("pageKey", ""))
        generic["sample_available"] = event.get("sampleAvailable")
        generic["page_instance_available"] = event.get("pageInstanceAvailable")
        validate_generic_lifecycle(generic, pid)
    entries = [item for item in route.get("routeEvents", []) if item.get("targetPageId") == "schedule"]
    workspaces = [item for item in route.get("routeEvents", []) if item.get("targetPageId") == "my_workspace"]
    require(len(entries) >= 3 and len(workspaces) >= 2, "Page-route events do not prove two clean Schedule leave/re-entry refreshes.")
    require(route.get("refreshReentryCount") == 2 and route.get("refreshMechanism") == "Schedule page activation reload after My Workspace leave/re-entry",
            "Schedule refresh count or page lifecycle mechanism is not explicitly recorded.")
    return {"stateCaptureCount": len(states), "scheduleActivationCount": len(activated), "scheduleSuspensionCount": len(suspended),
            "scheduleRouteCount": len(entries), "workspaceRouteCount": len(workspaces), "refreshReentryCount": 2,
            "englishLightMeanLuminance": light, "englishDarkMeanLuminance": dark, "visualThemeEffectDetected": True,
            "scheduleSpecificRenderedEntriesMatched": 768, "genericLifecycleUsedAsScheduleEntryCount": False}


def validate_actions(manifest: dict[str, Any], pid: int, hwnd: int) -> dict[str, Any]:
    actions = manifest.get("route", {}).get("actions", [])
    require(len(actions) >= 12, "P9-15 route lacks the expected navigation and Preferences UIA action evidence.")
    allowed = {"UIA-targeted window click", "InvokePattern.Invoke", "ValuePattern.SetValue", "SelectionItemPattern.Select",
               "ExpandCollapsePattern.Expand", "WindowPattern.Close", "UIA-derived ComboBox BoundingRectangle bounded WM click"}
    for action in actions:
        require(action.get("method") in allowed, f"P9-15 action used an unsupported or non-UIA method: {action.get('actionKind')}.")
        target = action.get("target", {})
        if target:
            require(int(target.get("processId", 0)) == pid and int(target.get("ownerWindowProcessId", 0)) == pid,
                    f"P9-15 action target does not belong to measured process: {action.get('actionKind')}.")
            require(int(target.get("ownerWindowHandle", 0)) > 0,
                    f"P9-15 action target has no owning HWND: {action.get('actionKind')}.")
    select_language = [item for item in actions if item.get("actionKind", "").startswith("set-language-") and item.get("selectedValue")]
    select_theme = [item for item in actions if item.get("actionKind", "").startswith("set-theme-") and item.get("selectedValue")]
    require(len(select_language) == 2 and len(select_theme) == 2, "P9-15 route did not perform both language and both theme changes through UIA.")
    require(all(item.get("result") == "succeeded" for item in select_language + select_theme),
            "A requested UIA language/theme action lacks a successful selected-value confirmation.")
    require({item.get("selectedValue") for item in select_language} == {"English", "Korean"}
            and {item.get("selectedValue") for item in select_theme} == {"Light", "Dark"},
            "Successful UIA actions do not cover both supported language and theme choices.")
    require(any(item.get("actionKind") == "open-edit-preferences" for item in actions)
            and any(item.get("actionKind") == "close-preferences-dialog" for item in actions),
            "P9-15 route did not open Edit > Preferences and close its identified button box through UIA.")
    dialog_handles = {int(item.get("preferences", {}).get("dialogWindowHandle", 0)) for item in manifest.get("route", {}).get("scheduleStates", [])}
    dialog_handles.discard(0)
    for action in actions:
        target = action.get("target", {})
        if not target:
            continue
        handle = int(target.get("ownerWindowHandle", 0))
        kind = str(action.get("actionKind", ""))
        if kind.startswith("set-language-") or kind.startswith("set-theme-") or kind in {"close-preferences-dialog", "open-preferences-appearance-tab"}:
            require(handle in dialog_handles, f"Preferences UIA action is not bound to a captured Preferences dialog HWND: {kind}.")
        elif kind.startswith("navigate-") or kind in {"open-edit-menu", "open-edit-preferences"}:
            require(handle == hwnd, f"Main route/menu UIA action is not bound to the main HWND: {kind}.")
    return {"actionCount": len(actions), "languageChanges": len(select_language), "themeChanges": len(select_theme),
            "methods": sorted({str(item["method"]) for item in actions}), "allTargetsPidHwndBound": True}


def validate_process(root: Path, manifest: dict[str, Any], package: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process", {})
    pid, hwnd = int(process.get("processId", 0)), int(process.get("mainWindowHandle", 0))
    require(pid > 0 and hwnd > 0 and int(process.get("windowProcessId", 0)) == pid and int(process.get("hwndProcessId", 0)) == pid,
            "Measured packaged process PID/HWND identity is missing or inconsistent.")
    require(process.get("architecture") == "x64" and str(Path(process.get("processImagePath", "")).resolve()) == package["applicationPath"]
            and process.get("processImageSha256") == package["applicationSha256"], "Live process image does not match the verified x64 packaged executable.")
    require(str(Path(process.get("workspacePath", "")).resolve()) == str(Path(manifest.get("fixture", {}).get("workspacePath", "")).resolve()),
            "Live process workspace argument differs from the isolated Schedule fixture.")
    exit_record = manifest.get("processExit", {})
    require(exit_record.get("normalExit") is True and exit_record.get("forcedTermination") is False and exit_record.get("exitCode") == 0
            and exit_record.get("closeMethod") == "WindowPattern.Close", "Packaged process did not exit naturally with code 0.")
    require(process.get("forcedTermination") is False and process.get("closeMethod") == "WindowPattern.Close", "Process record claims forced termination or an unverified close path.")
    for key in ("stdoutPath", "stderrPath"):
        require(contained_path(root, str(process.get(key, ""))).is_file(), f"Process {key} log is missing.")
    return {"processId": pid, "mainWindowHandle": hwnd, "architecture": "x64", "exitCode": 0,
            "closeMethod": "WindowPattern.Close", "forcedTermination": False}


def validate_memory(root: Path, manifest: dict[str, Any], pid: int) -> dict[str, Any]:
    memory = manifest.get("memory", {})
    checkpoints = memory.get("stableCheckpoints", [])
    require(len(checkpoints) >= 7, "Stable Schedule checkpoints are missing from one or more relevant states.")
    for item in checkpoints:
        require(item.get("category") == "normal-use" and int(item.get("processId", 0)) == pid,
                "A stable checkpoint is not a normal-use sample from the measured process.")
        require(0 <= int(item.get("workingSetBytes", -1)) < STRICT_WS_LIMIT,
                f"Stable working set reached the strict exclusive cap at {item.get('point')}: {item.get('workingSetBytes')}.")
    metric_ref = memory.get("processMetrics", {})
    metric_path = verify_artifact(root, metric_ref.get("path", ""), metric_ref.get("sha256", ""))
    rows = [json.loads(line) for line in metric_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(rows and all(int(row.get("pid", 0)) == pid for row in rows), "Process sampler metrics are missing or contain another PID.")
    operation = [int(row["workingSetBytes"]) for row in rows if row.get("category") == "operation"]
    require(operation, "Operation peak working-set samples are missing.")
    peak = max(operation)
    return {"stableCheckpointCount": len(checkpoints), "stableMaximumWorkingSetBytes": max(int(item["workingSetBytes"]) for item in checkpoints),
            "strictStableLimitExclusiveBytes": STRICT_WS_LIMIT, "operationPeakWorkingSetBytes": peak,
            "operationPeakAdvisoryExclusiveBytes": OPERATION_WS_LIMIT, "operationPeakAdvisoryExceeded": peak >= OPERATION_WS_LIMIT,
            "operationPeakAdvisoryOnly": True, "returnToBaselineAllowance": "none defined"}


def validate_run(evidence_dir: Path) -> dict[str, Any]:
    root = evidence_dir.resolve(strict=True)
    manifest_path = root / "manifest.json"
    manifest = read_json(manifest_path)
    require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID, "Manifest schema/task ID is invalid.")
    require(manifest.get("status") in {"evidence-collected", "passed"} and manifest.get("failure") is None, "P9-15 run is incomplete or contains a failure.")
    package = validate_package_provenance(root, manifest)
    process = manifest.get("process", {})
    identity = validate_process(root, manifest, package)
    pid, hwnd = identity["processId"], identity["mainWindowHandle"]
    fixture = manifest.get("fixture", {})
    preparation_path = verify_artifact(root, fixture.get("preparationReceipt", {}).get("relativePath", ""), fixture.get("preparationReceipt", {}).get("sha256", ""))
    preparation = read_json(preparation_path)
    require(preparation.get("schema") == PREPARATION_SCHEMA and fixture.get("preparationReceipt", {}).get("workspaceSha256") == preparation.get("workspaceSha256"),
            "P9-15 workspace preparation receipt does not match the pinned fixture identity.")
    before_path = verify_artifact(root, fixture.get("beforeSnapshot", {}).get("relativePath", ""), fixture.get("beforeSnapshot", {}).get("sha256", ""))
    after_path = verify_artifact(root, fixture.get("afterSnapshot", {}).get("relativePath", ""), fixture.get("afterSnapshot", {}).get("sha256", ""))
    workspace = validate_workspace_rows(preparation, read_json(before_path), read_json(after_path))
    require(str(Path(fixture.get("workspacePath", "")).resolve()) == str(Path(preparation.get("workspace", "")).resolve()), "Measured workspace is not the identified prepared fixture.")
    for suffix in ("-wal", "-shm", "-journal"):
        require(not Path(str(fixture["workspacePath"]) + suffix).exists(), f"Workspace sidecar remains after natural exit: {suffix}")
    settings = validate_settings(root, manifest)
    route = validate_route(root, manifest, preparation, pid, hwnd)
    actions = validate_actions(manifest, pid, hwnd)
    memory = validate_memory(root, manifest, pid)
    require(manifest.get("cleanup", {}).get("processExited") is True and manifest["cleanup"].get("workspaceSidecarFilesAfterExit") == [],
            "Runtime cleanup does not prove natural exit and clean workspace sidecars.")
    return {"taskId": TASK_ID, "status": "passed", "evidenceDirectory": str(root), "package": package,
            "process": identity, "fixture": workspace, "settings": settings, "route": route,
            "uiaActions": actions, "memory": memory,
            "limitations": ["Lifecycle widget/model counts are generic and are not used as Schedule entry counts.",
                            "No return-to-baseline memory allowance is defined."]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = validate_run(args.evidence_dir)
    except Exception as error:
        print(f"P9-15 validation failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
