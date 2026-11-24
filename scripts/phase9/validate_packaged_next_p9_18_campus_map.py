#!/usr/bin/env python3
"""Validate P9-18 current-source packaged Windows campus-map lifecycle evidence."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import statistics
import struct
import sys
from pathlib import Path
from typing import Any, Callable


SCHEMA = "classmngr-phase9-p9-18-packaged-campus-map-v1"
TASK_ID = "P9_18_PACKAGED_NEXT_CAMPUS_MAP_OPEN_CLOSE"
BUILD_SCHEMA = "classmngr-phase9-p9-18-exact-release-build-v1"
FIXTURE_SCHEMA = "classmngr-phase9-p9-18-bundang-map-fixture-v1"
TARGETED_UIA_SCHEMA = "classmngr-phase9-p9-18-targeted-uia-v1"
WORKING_SET_TARGET = 268_435_456
WORKING_SET_CEILING = 471_859_200
OPERATION_PEAK_ADVISORY = 536_870_912
MAIN_WINDOW_AUTOMATION_ID = "QApplication.classMngrNextApplicationShell"
CAMPUS_DIRECTORY_STACK_AUTOMATION_ID = (
    "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget"
)
INFORMATION_PAGE_AUTOMATION_ID = CAMPUS_DIRECTORY_STACK_AUTOMATION_ID + ".nextCampusDirectoryPage_0"
MAP_PAGE_AUTOMATION_ID = CAMPUS_DIRECTORY_STACK_AUTOMATION_ID + ".nextCampusDirectoryPage_4"
MAP_IMAGE_CARD_AUTOMATION_ID = (
    MAP_PAGE_AUTOMATION_ID
    + ".campusDirectoryScrollArea.qt_scrollarea_viewport.campusDirectoryForm.campusMapImageCard"
)
MAP_IMAGE_AUTOMATION_ID = MAP_IMAGE_CARD_AUTOMATION_ID + ".campusMapImage"
TARGETED_UIA_CACHED_PROPERTIES = [
    "AutomationId",
    "Name",
    "ControlType",
    "ProcessId",
    "NativeWindowHandle",
    "ClassName",
    "IsEnabled",
    "IsOffscreen",
    "BoundingRectangle",
]
PRODUCT_ROOTS = [
    "CMakeLists.txt",
    "CMakePresets.json",
    "cmake",
    "docs/qt-rewrite/phase-4-resource-inventory.json",
    "licenses",
    "resources",
    "scripts/phase4",
    "src",
    "tests",
    "third_party",
]
HARNESS_PATHS = [
    "scripts/phase9/p9_14_packaged_lifecycle_common.ps1",
    "scripts/phase9/p9_18_build_current_source_release.ps1",
    "scripts/phase9/prepare_p9_18_bundang_fixture.py",
    "scripts/phase9/run_packaged_next_p9_18_campus_map.ps1",
    "scripts/phase9/validate_packaged_next_p9_18_campus_map.py",
    "scripts/phase9/test_prepare_p9_18_bundang_fixture.py",
    "scripts/phase9/test_validate_packaged_next_p9_18_campus_map.py",
]
EXPECTED_IMAGE_PATHS = [
    "assets/campuses/bundang/bundang_map.png",
    "assets/campuses/bundang/bundang_building.png",
]
FOCUSED_CTESTS = [
    "ClassMngrNextPageHostTests",
    "ClassMngrNextClassesNavigatorPageTests",
    "ClassMngrNextRosterEditorTests",
    "ClassMngrNextApplicationRosterSaveUseCaseTests",
    "ClassMngrNextPlatformApplicationServicesRosterSavePortTests",
]


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
        if not path.is_file():
            continue
        if any(part in {"__pycache__", ".pytest_cache", ".mypy_cache"} for part in path.parts):
            continue
        if path.suffix.lower() in {".pyc", ".pyo"}:
            continue
        resolved = path.resolve(strict=True)
        require(root == resolved.parent or root in resolved.parents, f"Inventory path escapes its root: {path}")
        files.append(
            {
                "path": resolved.relative_to(root).as_posix(),
                "sizeBytes": resolved.stat().st_size,
                "sha256": sha256_file(resolved),
            }
        )
    files.sort(key=lambda row: str(row["path"]).lower())
    return {
        "root": str(root),
        "sha256": canonical_fingerprint(files),
        "fileCount": len(files),
        "files": files,
    }


def canonical_inventory_rows(files: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return sorted(
        [
            {
                "path": str(row["path"]).replace("\\", "/"),
                "sizeBytes": int(row["sizeBytes"]),
                "sha256": str(row["sha256"]).lower(),
            }
            for row in files
        ],
        key=lambda row: row["path"].lower(),
    )


def source_inventory(repository: Path, roots: list[str]) -> dict[str, Any]:
    repository = repository.resolve(strict=True)
    files: list[dict[str, Any]] = []
    for relative in roots:
        target = repository / relative
        require(target.exists(), f"Source inventory root is missing: {relative}")
        paths = [target] if target.is_file() else [path for path in target.rglob("*") if path.is_file()]
        for path in paths:
            if any(part in {"__pycache__", ".pytest_cache", ".mypy_cache"} for part in path.parts):
                continue
            if path.suffix.lower() in {".pyc", ".pyo"}:
                continue
            resolved = path.resolve(strict=True)
            require(repository == resolved.parent or repository in resolved.parents, f"Source file escapes the repository: {path}")
            files.append(
                {
                    "path": resolved.relative_to(repository).as_posix(),
                    "sizeBytes": resolved.stat().st_size,
                    "sha256": sha256_file(resolved),
                }
            )
    files.sort(key=lambda row: str(row["path"]).lower())
    return {
        "repositoryRoot": str(repository),
        "includedRoots": roots,
        "sha256": canonical_fingerprint(files),
        "fileCount": len(files),
        "files": files,
    }


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"Cannot read JSON evidence {path}: {error}") from error
    require(isinstance(value, dict), f"JSON evidence is not an object: {path}")
    return value


def parse_utc(value: Any, label: str) -> dt.datetime:
    require(isinstance(value, str) and value, f"{label} timestamp is missing.")
    try:
        parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValidationError(f"{label} timestamp is invalid: {value}") from error
    require(parsed.tzinfo is not None, f"{label} timestamp has no UTC offset.")
    return parsed.astimezone(dt.timezone.utc)


def resolve_evidence_path(evidence_root: Path, relative: str) -> Path:
    path = (evidence_root / relative).resolve(strict=True)
    require(evidence_root.resolve(strict=True) == path.parent or evidence_root.resolve(strict=True) in path.parents,
            f"Evidence path escapes its run root: {relative}")
    return path


def resolve_repo_path(repository: Path, value: str) -> Path:
    raw = Path(value)
    path = raw if raw.is_absolute() else repository / raw
    return path.resolve(strict=True)


def verify_file_record(
    evidence_root: Path,
    relative: str,
    expected_sha: str,
    label: str,
    expected_size: int | None = None,
) -> Path:
    path = resolve_evidence_path(evidence_root, relative)
    actual_sha = sha256_file(path)
    require(actual_sha == expected_sha, f"{label} SHA-256 differs from its manifest record.")
    if expected_size is not None:
        require(path.stat().st_size == expected_size, f"{label} size differs from its manifest record.")
    return path


def read_png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR",
            f"Screenshot or map asset is not a valid PNG: {path}")
    width, height = struct.unpack(">II", header[16:24])
    require(width > 0 and height > 0, f"PNG dimensions are invalid: {path}")
    return width, height


def validate_fixture_catalog_record(
    catalog_record: dict[str, Any], resource_id: str, expected_relative: str, image_sha256: str
) -> None:
    provenance = catalog_record.get("provenance") or {}
    require(catalog_record.get("location") == "resource_file"
            and catalog_record.get("resource_kind") == "campus_map_image"
            and catalog_record.get("expected_type") == "png"
            and catalog_record.get("logical_id") == resource_id
            and catalog_record.get("package_relative_path") == expected_relative
            and (provenance.get("source") or {}).get("sha256") == image_sha256
            and (provenance.get("baked") or {}).get("sha256") == image_sha256,
            f"Resource catalog does not bind the current Bundang image bytes: {expected_relative}")


def validate_packaged_map_resource_load(
    record: dict[str, Any], fixture_image: dict[str, Any], activation_id: str
) -> None:
    resource_id = str(fixture_image.get("resourceId", ""))
    require(record.get("resource_id") == resource_id,
            f"Runtime resource ID differs from its Bundang fixture in {activation_id}.")
    require(record.get("event") == "resource-load" and record.get("outcome") == "succeeded"
            and (record.get("request_scope") or {}).get("kind") == "page"
            and (record.get("request_scope") or {}).get("owner") == "campus_map",
            f"Image loader event is not a successful campus_map page-scoped decode in {activation_id}.")
    catalog = record.get("catalog_resource") or {}
    require(catalog.get("location") == "package_file"
            and catalog.get("kind") == "campus_map_image"
            and catalog.get("type") == "png",
            f"Runtime resource loader event has unexpected packaged catalog type/location in {activation_id}.")
    image = record.get("image") or {}
    payload = record.get("payload_accounting") or {}
    require(int(payload.get("decoded_payload_bytes", 0)) > 0
            and int(payload.get("source_catalog_bytes", -1)) == int(fixture_image.get("sizeBytes", -2))
            and isinstance(image.get("decoded_dimensions"), dict),
            f"Runtime resource loader event does not bind decoded payload and source bytes to the fixture in {activation_id}.")
    expected_dimensions = (int(fixture_image.get("widthPixels", -1)), int(fixture_image.get("heightPixels", -1)))
    source_dimensions = image.get("source_dimensions") or {}
    decoded_dimensions = image.get("decoded_dimensions") or {}
    require((int(source_dimensions.get("width_pixels", -1)), int(source_dimensions.get("height_pixels", -1))) == expected_dimensions
            and (int(decoded_dimensions.get("width_pixels", -1)), int(decoded_dimensions.get("height_pixels", -1))) == expected_dimensions,
            f"Runtime resource loader source/decoded dimensions differ from the pinned Bundang fixture in {activation_id}.")


def expected_targeted_uia_contract(expected_title: str) -> dict[str, Any]:
    require(expected_title in ("Maps", "Office Information"),
            f"Unexpected P9-18 targeted UIA page title: {expected_title}.")
    page_id = MAP_PAGE_AUTOMATION_ID if expected_title == "Maps" else INFORMATION_PAGE_AUTOMATION_ID
    page_title = {
        "automationId": page_id + ".campusDirectoryTitle",
        "controlType": "ControlType.Text",
        "name": expected_title,
    }
    map_image_card = {
        "automationId": MAP_IMAGE_CARD_AUTOMATION_ID,
        "controlType": "ControlType.Custom",
    }
    map_image = {
        "automationId": MAP_IMAGE_AUTOMATION_ID,
        "controlType": "ControlType.Image",
    }
    return {
        "pageObjectAutomationId": page_id,
        "queryTargets": {
            "pageTitle": page_title,
            "mapImageCard": map_image_card,
            "mapImage": map_image,
        },
        "controls": {
            "pageTitle": page_title,
            "mapImageCard": {**map_image_card, "name": ""},
            "mapImage": {**map_image, "name": ""},
        },
    }


def targeted_uia_artifact_name_for_window(window_id: str) -> str:
    confirmation_ids = {
        "warmup-open": "warmup-map-open",
        "warmup-return": "warmup-map-suspended",
    }
    return "state-" + confirmation_ids.get(window_id, window_id)


def validate_targeted_uia_artifact(
    artifact: dict[str, Any], expected_title: str, process_id: int, main_window_handle: int
) -> dict[str, int]:
    contract = expected_targeted_uia_contract(expected_title)
    require(artifact.get("schema") == TARGETED_UIA_SCHEMA and artifact.get("taskId") == TASK_ID,
            "Target-control UIA artifact schema or task identity differs.")
    require(artifact.get("expectedPageTitle") == expected_title,
            "Target-control UIA artifact page title differs from the route claim.")
    require(type(artifact.get("processId")) is int and artifact.get("processId") == process_id
            and type(artifact.get("mainWindowHandle")) is int and artifact.get("mainWindowHandle") == main_window_handle
            and type(artifact.get("windowProcessId")) is int and artifact.get("windowProcessId") == process_id
            and type(artifact.get("rootNativeWindowHandle")) is int
            and artifact.get("rootNativeWindowHandle") == main_window_handle,
            "Target-control UIA artifact is not bound to the measured PID and main HWND.")
    root = artifact.get("root") or {}
    require(type(root.get("processId")) is int and root.get("processId") == process_id
            and root.get("automationId") == MAIN_WINDOW_AUTOMATION_ID
            and root.get("controlType") == "ControlType.Window"
            and type(root.get("ownerWindowHandle")) is int and root.get("ownerWindowHandle") == main_window_handle
            and type(root.get("ownerWindowProcessId")) is int and root.get("ownerWindowProcessId") == process_id
            and root.get("isOffscreen") is False,
            "Target-control UIA artifact main-window root identity differs from the measured PID/HWND.")
    query = artifact.get("query") or {}
    require(query.get("method") == "AutomationElement.FindAll with property-constrained AndCondition and CacheRequest.Activate"
            and query.get("treeScope") == "Descendants" and query.get("cacheRequestUsed") is True
            and query.get("cacheTreeScope") == "Element"
            and query.get("cachedProperties") == TARGETED_UIA_CACHED_PROPERTIES
            and query.get("targetPageObjectAutomationId") == contract["pageObjectAutomationId"]
            and query.get("targets") == contract["queryTargets"],
            "Target-control UIA query record does not prove the exact bounded cached property queries.")
    controls = artifact.get("controls") or {}
    require(set(controls) == {"pageTitle", "mapImageCard", "mapImage"},
            "Target-control UIA artifact has missing or extra control groups.")
    counts: dict[str, int] = {}
    for key, expected in contract["controls"].items():
        group = controls.get(key) or {}
        require(group.get("automationId") == expected["automationId"]
                and group.get("controlType") == expected["controlType"]
                and group.get("name") == expected["name"],
                f"Target-control UIA {key} AutomationId, type, or name differs from its exact contract.")
        count = group.get("count")
        elements = group.get("elements")
        require(type(count) is int and isinstance(elements, list) and count == len(elements),
                f"Target-control UIA {key} count is malformed or differs from its element records.")
        counts[key] = count
        for element in elements:
            require(isinstance(element, dict),
                    f"Target-control UIA {key} element is malformed.")
            require(element.get("automationId") == expected["automationId"]
                    and element.get("controlType") == expected["controlType"]
                    and element.get("name") == expected["name"],
                    f"Target-control UIA {key} element AutomationId, type, or name differs from its exact contract.")
            require(type(element.get("processId")) is int and element.get("processId") == process_id
                    and type(element.get("ownerWindowHandle")) is int and element.get("ownerWindowHandle") == main_window_handle
                    and type(element.get("ownerWindowProcessId")) is int and element.get("ownerWindowProcessId") == process_id
                    and type(element.get("nativeWindowHandle")) is int and element.get("nativeWindowHandle") >= 0
                    and type(element.get("isOffscreen")) is bool
                    and type(element.get("isEnabled")) is bool,
                    f"Target-control UIA {key} contains a malformed or foreign element.")
    require(counts["pageTitle"] == 1 and controls["pageTitle"]["elements"][0].get("isOffscreen") is False,
            "Target-control UIA artifact lacks one exact visible page title.")
    if expected_title == "Maps":
        for key in ("mapImageCard", "mapImage"):
            count = counts[key]
            require(count in (0, 2), f"Target-control UIA Maps {key} count must be zero or exactly two.")
            if count == 2:
                require(all(element.get("isOffscreen") is False for element in controls[key]["elements"]),
                        f"Target-control UIA Maps {key} controls are not all visible.")
    else:
        require(counts["mapImageCard"] == 0 and counts["mapImage"] == 0,
                "Target-control UIA Information state still exposes map cards or images.")
    return counts


def median(values: list[int | float]) -> float:
    return float(statistics.median(values))


def linear_fitted_change(values: list[float]) -> float:
    """Keep the Phase 9 P9-17 least-squares trend calculation unchanged."""
    if len(values) < 2:
        return 0.0
    x_values = list(range(len(values)))
    x_mean = statistics.mean(x_values)
    y_mean = statistics.mean(values)
    denominator = sum((value - x_mean) ** 2 for value in x_values)
    slope = sum((x - x_mean) * (y - y_mean) for x, y in zip(x_values, values)) / denominator
    return slope * (len(values) - 1)


def compute_cleanup_analysis(
    cold_baseline: list[int],
    first_return: list[int],
    plateau_baselines: list[list[int]],
    cycle_returns: list[list[int]],
) -> dict[str, Any]:
    """Apply the existing empirical cold and plateau Phase 9 cleanup formulas."""
    require(len(cold_baseline) == 10 and len(first_return) == 10, "Cold baseline and first return require ten samples each.")
    require(len(plateau_baselines) == 5 and all(len(row) == 10 for row in plateau_baselines),
            "Plateau cleanup requires five ten-sample no-action baseline windows.")
    require(len(cycle_returns) == 10 and all(len(row) == 10 for row in cycle_returns),
            "Plateau cleanup requires ten ten-sample measured return windows.")
    cold_range = max(cold_baseline) - min(cold_baseline)
    cold_median = median(cold_baseline)
    first_return_median = median(first_return)
    cold_allowance = cold_median + cold_range
    cold_passed = first_return_median <= cold_allowance

    plateau_medians = [median(row) for row in plateau_baselines]
    plateau_center = median(plateau_medians)
    plateau_envelope = max(abs(value - plateau_center) for value in plateau_medians)
    plateau_drift = linear_fitted_change(plateau_medians)
    plateau_stable = plateau_center > 0 and plateau_envelope < plateau_center
    plateau_drift_within_envelope = abs(plateau_drift) <= plateau_envelope
    plateau_upper_bound = plateau_center + plateau_envelope
    return_medians = [median(row) for row in cycle_returns]
    return_exceedances = [
        {"cycleNumber": index, "medianWorkingSetBytes": value, "upperBoundBytes": plateau_upper_bound}
        for index, value in enumerate(return_medians, start=1)
        if value > plateau_upper_bound
    ]
    cycle_trend_change = linear_fitted_change(return_medians)
    cycle_trend_within_envelope = cycle_trend_change <= plateau_envelope
    plateau_failures = []
    if not plateau_stable:
        plateau_failures.append("Post-priming Working Set baseline is unstable/inconclusive because its noise envelope is at least its median.")
    if not plateau_drift_within_envelope:
        plateau_failures.append("Post-priming Working Set baseline is unstable/inconclusive because it has directional drift beyond its envelope.")
    if return_exceedances:
        plateau_failures.append(f"Post-suspension returns exceed the stable post-priming baseline envelope: {return_exceedances}.")
    if not cycle_trend_within_envelope:
        plateau_failures.append(f"Measured post-suspension Working Set trend {cycle_trend_change} exceeds the baseline noise envelope {plateau_envelope}.")
    return {
        "passed": cold_passed and not plateau_failures,
        "coldBaselineToFirstReturn": {
            "passed": cold_passed,
            "baselineMedianWorkingSetBytes": cold_median,
            "baselineRangeBytes": cold_range,
            "firstReturnMedianWorkingSetBytes": first_return_median,
            "maximumAllowedReturnMedianBytes": cold_allowance,
            "initialUpliftBytes": first_return_median - cold_median,
        },
        "postPrimingPlateauAndCycleReturns": {
            "passed": not plateau_failures,
            "failureReasons": plateau_failures,
            "baselineWindowMediansWorkingSetBytes": plateau_medians,
            "baselineMedianWorkingSetBytes": plateau_center,
            "baselineNoiseEnvelopeBytes": plateau_envelope,
            "baselineFittedDriftBytes": plateau_drift,
            "maximumAllowedReturnMedianBytes": plateau_upper_bound,
            "cycleReturnMediansWorkingSetBytes": return_medians,
            "cycleReturnExceedances": return_exceedances,
            "cycleFittedChangeBytes": cycle_trend_change,
            "cycleTrendWithinBaselineEnvelope": cycle_trend_within_envelope,
        },
    }


def expected_window_ids() -> list[str]:
    result = ["cold-baseline", "warmup-open", "warmup-return"]
    result.extend(f"plateau-baseline-{index:02d}" for index in range(1, 6))
    for index in range(1, 11):
        tag = f"{index:02d}"
        result.extend((f"cycle-{tag}-map-open", f"cycle-{tag}-return"))
    return result


def validate_build_package_and_harness(
    manifest: dict[str, Any], evidence_root: Path, repository: Path
) -> dict[str, Any]:
    proof = manifest.get("buildProvenance") or {}
    require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID,
            "Manifest schema or P9-18 task identity differs.")
    receipt_path = resolve_repo_path(repository, str(proof.get("receiptPath", "")))
    receipt_sha = sha256_file(receipt_path)
    require(receipt_sha == proof.get("receiptSha256"), "Recorded exact-source build receipt hash differs.")
    receipt = load_json(receipt_path)
    require(receipt.get("schema") == BUILD_SCHEMA and receipt.get("taskId") == TASK_ID and receipt.get("status") == "succeeded",
            "Build receipt is not a successful P9-18 exact-source Release build.")
    require(receipt.get("configuration") == "Release" and receipt.get("platform") == "x64"
            and receipt.get("generator") == "Visual Studio 17 2022" and receipt.get("generatorPlatform") == "x64"
            and receipt.get("generatorToolset") == "v143" and receipt.get("windowsSdkVersion") == "10.0.26100.0",
            "Build receipt does not prove the required VS2022 2022/x64/v143 Release toolchain.")
    compiler = receipt.get("compiler") or {}
    require(compiler.get("fileVersion") == "19.44.35230.0" and compiler.get("toolsetVersion") == "14.44.35207",
            "Build receipt compiler identity differs from the pinned VS2022 v143 toolchain.")
    redist = receipt.get("vcRedist") or {}
    redist_path = Path(str(redist.get("path", ""))).resolve(strict=True)
    redist_root = (Path(str(receipt.get("generatorInstance", ""))) / "VC/Redist/MSVC").resolve(strict=True)
    redist_cache_value_path = Path(str(redist.get("cacheReferenceValuePath", ""))).resolve(strict=True)
    require(redist_path.name == "vc_redist.x64.exe"
            and redist_root in redist_path.parents
            and redist.get("signatureStatus") == "Valid"
            and "CN=Microsoft Corporation," in str(redist.get("signer", ""))
            and bool(redist.get("signerThumbprint"))
            and sha256_file(redist_path) == redist.get("sha256")
            and redist_path == (redist_root / "v143/vc_redist.x64.exe").resolve(strict=True)
            and redist_cache_value_path.is_file()
            and sha256_file(redist_cache_value_path) == redist.get("cacheReferenceValueSha256") == redist.get("sha256"),
            "Build receipt does not bind the signed VS2022 x64 redistributable used by the clean package configuration.")
    expected_redist_argument = f"-DCLASSMNGR_INSTALLER_VC_REDIST_FILE={redist_path}"
    cache_reference_path = Path(str(redist.get("cacheReferencePath", ""))).resolve(strict=True)
    cache_reference_value = cache_reference_path.read_text(encoding="utf-8-sig").splitlines()
    cache_redist_entries = [line.split("=", 1)[1] for line in cache_reference_value if line.startswith("CLASSMNGR_INSTALLER_VC_REDIST_FILE:") and "=" in line]
    require(expected_redist_argument in (receipt.get("configure") or {}).get("arguments", [])
            and cache_reference_path
            == (repository / "build/windows-x64-release/CMakeCache.txt").resolve(strict=True)
            and len(cache_redist_entries) == 1
            and Path(cache_redist_entries[0]).resolve(strict=True) == redist_cache_value_path
            and Path(str(receipt.get("preservedStandardReleasePath", ""))).resolve(strict=True)
            == (repository / "build/windows-x64-release").resolve(strict=True),
            "Clean CMake configure does not bind the verified redistributable from the preserved VS2022 reference cache.")
    provenance = manifest.get("buildProvenance") or {}
    require(provenance.get("vcRedistPath") == str(redist_path)
            and provenance.get("vcRedistSha256") == redist.get("sha256")
            and provenance.get("vcRedistSignatureStatus") == "Valid",
            "Route manifest does not bind the verified VS2022 x64 redistributable provenance.")
    build_root = (repository / "artifacts/phase9/p9-18-exact-release-builds").resolve(strict=True)
    build_dir = Path(str(receipt.get("buildDirectory", ""))).resolve(strict=True)
    require(build_root == build_dir.parent or build_root in build_dir.parents, "Exact Release build output is outside the P9-18 artifacts build root.")
    preserved = (repository / "build/windows-x64-release").resolve()
    require(build_dir != preserved, "Build receipt points into the preserved standard Release tree.")
    require(int((receipt.get("configure") or {}).get("exitCode", -1)) == 0
            and int((receipt.get("build") or {}).get("exitCode", -1)) == 0,
            "Fresh CMake configure or exact-source Release build failed.")
    build_log = Path(str((receipt.get("build") or {}).get("logPath", ""))).resolve(strict=True)
    require(sha256_file(build_log) == (receipt.get("build") or {}).get("logSha256"), "Exact Release build log hash differs.")
    release_exe = Path(str(receipt.get("releaseExecutablePath", ""))).resolve(strict=True)
    require(sha256_file(release_exe) == receipt.get("releaseExecutableSha256"), "Built Release executable hash differs from its receipt.")
    require(pe_machine(release_exe) == 0x8664, "Built Release executable is not Windows x64.")
    current_source = source_inventory(repository, PRODUCT_ROOTS)
    require(receipt.get("sourceIncludedRoots") == PRODUCT_ROOTS, "Build receipt product source roots differ from the P9-18 contract.")
    require(current_source.get("sha256") == receipt.get("sourceSha256")
            and int(current_source.get("fileCount", -1)) == int(receipt.get("sourceFileCount", -2)),
            "Current product/resource/test source differs from the exact-source Release build receipt.")
    source_inventory_path = Path(str(receipt.get("sourceInventoryPath", ""))).resolve(strict=True)
    require(sha256_file(source_inventory_path) == receipt.get("sourceInventoryFileSha256"), "Build source inventory file hash differs.")
    recorded_source = load_json(source_inventory_path)
    require(recorded_source.get("sha256") == current_source.get("sha256")
            and recorded_source.get("files") == current_source.get("files"),
            "Recorded exact-source inventory does not reproduce the current product source.")
    builder_path = Path(str(receipt.get("buildHelperPath", ""))).resolve(strict=True)
    require(sha256_file(builder_path) == receipt.get("buildHelperSha256"), "Exact-source build helper changed after the receipt.")
    harness_inventory_path = Path(str(receipt.get("harnessInventoryPath", ""))).resolve(strict=True)
    require(sha256_file(harness_inventory_path) == receipt.get("harnessInventoryFileSha256"), "Build harness inventory file hash differs.")
    harness = load_json(harness_inventory_path)
    current_harness = []
    for relative in HARNESS_PATHS:
        path = repository / relative
        require(path.is_file(), f"Required P9-18 harness dependency is missing: {relative}")
        current_harness.append({"path": relative, "sizeBytes": path.stat().st_size, "sha256": sha256_file(path)})
    require(canonical_fingerprint(current_harness) == receipt.get("harnessSourceSha256") == harness.get("sha256"),
            "P9-18 runner, fixture, validator, lifecycle helper, or focused test sources differ from the build receipt.")
    manifest_harness = manifest.get("harnessSources") or {}
    require(manifest_harness.get("sha256") == receipt.get("harnessSourceSha256")
            and manifest_harness.get("files") == harness.get("files"),
            "Route manifest does not bind the exact P9-18 harness sources and shared lifecycle helper.")
    package = manifest.get("package") or {}
    package_root = Path(str(package.get("packageRoot", ""))).resolve(strict=True)
    package_inventory_path = resolve_evidence_path(evidence_root, str(package.get("packageInventoryPath", "")))
    require(sha256_file(package_inventory_path) == package.get("packageInventoryFileSha256"), "Package inventory file hash differs.")
    package_inventory = load_json(package_inventory_path)
    actual_package = canonical_inventory(package_root)
    require(package_inventory.get("sha256") == package.get("packageSha256") == actual_package.get("sha256")
            and canonical_inventory_rows(package_inventory.get("files") or [])
            == canonical_inventory_rows(actual_package.get("files") or []),
            "Staged Release package differs from its complete package inventory.")
    application = (package_root / "ClassMngrNext.exe").resolve(strict=True)
    require(sha256_file(application) == package.get("applicationSha256") == receipt.get("releaseExecutableSha256"),
            "Live route package executable differs from the exact-source Release executable.")
    resource_root = Path(str(package.get("resourceRoot", ""))).resolve(strict=True)
    resource_inventory_path = resolve_evidence_path(evidence_root, str(package.get("resourceInventoryPath", "")))
    require(sha256_file(resource_inventory_path) == package.get("resourceInventoryFileSha256"), "Packaged resource inventory file hash differs.")
    resource_inventory = load_json(resource_inventory_path)
    actual_resource = canonical_inventory(resource_root)
    require(resource_inventory.get("sha256") == package.get("resourceSha256") == actual_resource.get("sha256")
            and canonical_inventory_rows(resource_inventory.get("files") or [])
            == canonical_inventory_rows(actual_resource.get("files") or []),
            "Packaged resources differ from their complete resource inventory.")
    focused_tests = receipt.get("focusedTests") or {}
    focused_test_build = receipt.get("focusedTestBuild") or {}
    require([*FOCUSED_CTESTS] == list(focused_tests.get("testNames") or FOCUSED_CTESTS), "Focused CTest evidence does not name the five assigned test targets.")
    for record, label in ((focused_test_build, "focused C++ target build"), (focused_tests, "focused CTest run")):
        if record.get("logPath") and record.get("logSha256"):
            log = Path(str(record["logPath"])).resolve(strict=True)
            require(sha256_file(log) == record["logSha256"], f"{label} log hash differs.")
    return {
        "sourceSha256": current_source["sha256"],
        "sourceFileCount": current_source["fileCount"],
        "releaseExecutableSha256": package["applicationSha256"],
        "packageSha256": package["packageSha256"],
        "packageFileCount": package["packageFileCount"],
        "resourceSha256": package["resourceSha256"],
        "harnessSourceSha256": receipt["harnessSourceSha256"],
        "generator": receipt["generator"],
        "platform": receipt["generatorPlatform"],
        "toolset": receipt["generatorToolset"],
        "compilerVersion": compiler["fileVersion"],
        "windowsSdkVersion": receipt["windowsSdkVersion"],
        "focusedCTestExitCode": focused_tests.get("exitCode"),
        "focusedCTestNames": focused_tests.get("testNames", []),
    }


def pe_machine(path: Path) -> int:
    with path.open("rb") as stream:
        stream.seek(0x3C)
        offset_data = stream.read(4)
        require(len(offset_data) == 4, "PE header pointer is missing.")
        offset = struct.unpack("<I", offset_data)[0]
        stream.seek(offset + 4)
        machine = stream.read(2)
    require(len(machine) == 2, "PE machine type is missing.")
    return struct.unpack("<H", machine)[0]


def validate_fixture(manifest: dict[str, Any], evidence_root: Path, repository: Path) -> dict[str, Any]:
    fixture = manifest.get("fixture") or {}
    path = verify_file_record(evidence_root, str(fixture.get("relativePath", "")), str(fixture.get("sha256", "")), "Bundang fixture receipt", int(fixture.get("sizeBytes", -1)))
    receipt = load_json(path)
    require(receipt.get("schema") == FIXTURE_SCHEMA and receipt.get("status") == "passed"
            and receipt.get("taskId") == TASK_ID and receipt.get("campusId") == "bundang",
            "Fixture receipt does not identify the passed read-only Bundang map fixture.")
    require(receipt.get("mapResourceIds") == [f"resource:{path}" for path in EXPECTED_IMAGE_PATHS],
            "Fixture receipt resource IDs differ from the two required Bundang map images.")
    campus_json = repository / "resources/assets/campuses/bundang.json"
    require(sha256_file(campus_json) == receipt.get("campusJsonSha256"), "Bundang campus source JSON changed after fixture preparation.")
    campus = load_json(campus_json)
    expected = [f":/{path}" for path in EXPECTED_IMAGE_PATHS]
    require(campus.get("id") == "bundang" and (campus.get("map") or {}).get("images") == expected,
            "Current Bundang record no longer identifies the exact map/building image pair.")
    images = receipt.get("images")
    require(isinstance(images, list) and len(images) == 2, "Fixture receipt must contain exactly two Bundang images.")
    package_root = Path(str((manifest.get("package") or {}).get("packageRoot", ""))).resolve(strict=True)
    for index, (image, expected_relative) in enumerate(zip(images, EXPECTED_IMAGE_PATHS)):
        require(image.get("role") == ("map" if index == 0 else "building")
                and image.get("resourceId") == f"resource:{expected_relative}"
                and image.get("sourcePath") == f"resources/{expected_relative}"
                and image.get("packagePath") == f"resources/{expected_relative}",
                f"Bundang fixture image {index} identity/order differs from the required path.")
        source = repository / str(image["sourcePath"])
        package = package_root / str(image["packagePath"])
        require(sha256_file(source) == image.get("sha256") == sha256_file(package),
                f"Bundang fixture image bytes differ in source or staged package: {expected_relative}")
        require(source.stat().st_size == int(image.get("sizeBytes", -1)), f"Bundang fixture image size differs: {expected_relative}")
        dimensions = read_png_dimensions(source)
        require(dimensions == (int(image.get("widthPixels", -1)), int(image.get("heightPixels", -1))),
                f"Bundang fixture dimensions differ: {expected_relative}")
        catalog_record = image.get("catalogRecord") or {}
        validate_fixture_catalog_record(
            catalog_record,
            str(image.get("resourceId", "")),
            expected_relative,
            str(image.get("sha256", "")),
        )
    require(fixture.get("mapResourceIds") == receipt.get("mapResourceIds"), "Route fixture resource IDs differ from its preparation receipt.")
    return {"campusId": "bundang", "mapResourceIds": receipt["mapResourceIds"], "imageHashes": [image["sha256"] for image in images], "imageDimensions": [[image["widthPixels"], image["heightPixels"]] for image in images]}


def validate_process_exit_and_app_data(manifest: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process") or {}
    exit_evidence = manifest.get("processExit") or {}
    require(int(process.get("processId", -1)) > 0 and process.get("architecture") == "x64", "Measured process identity/architecture is missing.")
    require(int(process.get("mainWindowHandle", 0)) > 0 and process.get("mainWindowAutomationId") == "QApplication.classMngrNextApplicationShell",
            "Measured process is not bound to the exact Next main-window HWND identity.")
    require(process.get("imageSha256") == (manifest.get("package") or {}).get("applicationSha256"), "Measured process image hash differs from its staged package.")
    require(exit_evidence.get("normalExit") is True and exit_evidence.get("forcedTermination") is False
            and exit_evidence.get("closeMethod") == "WindowPattern.Close" and int(exit_evidence.get("exitCode", -1)) == 0,
            "Packaged Next process did not exit normally with code 0 through UIA WindowPattern.Close.")
    require(process.get("finalState") == "exited" and process.get("forcedTermination") is False,
            "Measured process final state or termination provenance is invalid.")
    cleanup = manifest.get("cleanup") or {}
    require(cleanup.get("processExited") is True and cleanup.get("forcedTermination") is False,
            "P9-18 cleanup did not observe natural process exit.")
    require(cleanup.get("readOnlyInvariantPassed") is True and cleanup.get("settingsUnchanged") is True
            and cleanup.get("localAppDataUnchanged") is True and cleanup.get("appDataUnchanged") is True,
            "Isolated app-data/settings snapshots changed during the read-only Campus Maps route.")
    require(int(cleanup.get("isolatedTempFileCountAfterExit", -1)) == 0,
            "Isolated application temp directory is not empty after process exit.")
    require(cleanup.get("sqliteSidecarFilesAfterExit") == [], "SQLite sidecar files remain after process exit.")
    snapshots = manifest.get("appDataSnapshots") or {}
    require(snapshots.get("readOnly") is True and snapshots.get("databaseOpened") is False,
            "Route did not declare and preserve its read-only app-data/database invariant.")
    before = snapshots.get("beforeLaunch") or {}
    after = snapshots.get("afterExit") or {}
    for key in ("settings", "localAppData", "appData"):
        require((before.get(key) or {}).get("sha256") == (after.get(key) or {}).get("sha256"),
                f"Read-only app-data snapshot differs before and after for {key}.")
    require(manifest.get("failure") is None and manifest.get("status") == "completed",
            "Execution manifest contains a recorded failure or did not complete before validation.")
    return {
        "normalExit": True,
        "exitCode": 0,
        "processId": process["processId"],
        "mainWindowHandle": process["mainWindowHandle"],
        "settingsUnchanged": True,
        "localAppDataUnchanged": True,
        "appDataUnchanged": True,
        "tempFilesAfterExit": cleanup["isolatedTempFileCountAfterExit"],
        "sqliteSidecarsAfterExit": cleanup["sqliteSidecarFilesAfterExit"],
    }


def validate_navigation_and_resources(
    manifest: dict[str, Any], evidence_root: Path, fixture_info: dict[str, Any]
) -> dict[str, Any]:
    route = manifest.get("route") or {}
    process = manifest.get("process") or {}
    pid = int(process.get("processId", -1))
    hwnd = int(process.get("mainWindowHandle", -1))
    require(route.get("warmupCount") == 1 and route.get("measuredCycleCount") == 10
            and route.get("baselineWindowsPerState") == 5,
            "Route does not declare exactly one warm-up, five no-action baselines, and ten measured cycles.")
    require(len(route.get("cycles") or []) == 10, "Route does not contain exactly ten P9-18 measured cycles.")
    require(len(route.get("resourceActivations") or []) == 11, "Route does not contain one warm-up plus ten measured image activation records.")
    cycles = route["cycles"]
    for index, cycle in enumerate(cycles, start=1):
        require(cycle.get("schema") == "classmngr-phase9-p9-18-campus-map-cycle-v1"
                and int(cycle.get("cycleNumber", -1)) == index,
                f"Measured route cycle sequence/schema is invalid at cycle {index}.")
        require(int(cycle.get("mapPageWidgetCountReleased", 0)) >= 6
                and int(cycle.get("mapPageWidgetCountBeforeSuspend", 0)) > int(cycle.get("mapPageWidgetCountAfterSuspend", 0)),
                f"Cycle {index} does not prove both Bundang map image cards were released on suspension.")
        require(cycle.get("openMemoryWindowId") == f"cycle-{index:02d}-map-open"
                and cycle.get("returnMemoryWindowId") == f"cycle-{index:02d}-return",
                f"Cycle {index} memory windows are not linked to its open/suspend chronology.")
        validate_lifecycle_sequence(
            cycle.get("openLifecycleEvents") or [],
            [("campus_information", "suspended"), ("campus_map", "activated")],
            f"cycle {index} map activation",
        )
        validate_lifecycle_sequence(
            cycle.get("suspendLifecycleEvents") or [],
            [("campus_map", "suspended"), ("campus_information", "activated")],
            f"cycle {index} map suspension",
        )
        validate_lifecycle_pair(cycle.get("openLifecycleEvents") or [], "campus_map", "activated", "campus_information", "suspended", f"cycle {index} map activation")
        validate_lifecycle_pair(cycle.get("suspendLifecycleEvents") or [], "campus_map", "suspended", "campus_information", "activated", f"cycle {index} map suspension")
    warmup = route.get("warmup") or {}
    require(int(warmup.get("mapPageWidgetCountReleased", 0)) >= 6
            and int(warmup.get("mapPageWidgetCountBeforeSuspend", 0)) > int(warmup.get("mapPageWidgetCountAfterSuspend", 0)),
            "Warm-up does not prove both Bundang map image cards were released on suspension.")
    validate_lifecycle_sequence(
        warmup.get("openLifecycleEvents") or [],
        [("campus_map", "created"), ("campus_information", "suspended"), ("campus_map", "activated")],
        "first map warm-up activation",
    )
    validate_lifecycle_sequence(
        warmup.get("suspendLifecycleEvents") or [],
        [("campus_map", "suspended"), ("campus_information", "activated")],
        "warm-up map suspension",
    )
    validate_lifecycle_pair(warmup.get("openLifecycleEvents") or [], "campus_map", "activated", "campus_information", "suspended", "warm-up map activation")
    validate_lifecycle_pair(warmup.get("suspendLifecycleEvents") or [], "campus_map", "suspended", "campus_information", "activated", "warm-up map suspension")
    baseline_ids = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    require([window.get("windowId") for window in (manifest.get("memory") or {}).get("windows", []) if str(window.get("windowId", "")).startswith("plateau-baseline-")] == baseline_ids,
            "Five no-action return-baseline windows are absent or out of order.")
    route_actions = route.get("actions") or []
    navigations = [action for action in route_actions if str(action.get("actionKind", "")).startswith("navigate-")]
    expected_targets = ["campus_information"]
    for _ in range(11):
        expected_targets.extend(("campus_map", "campus_information"))
    require([str(action.get("targetPageId")) for action in navigations] == expected_targets,
            "UIA-bound page navigation actions do not follow the initial-state, warm-up, and ten-cycle protocol.")
    for action in navigations:
        target = action.get("target") or {}
        pointer = action.get("pointerAction") or {}
        route_event = action.get("pageRouteEvent") or {}
        require(action.get("method") == "UIA-targeted window click" and action.get("verifiedByFollowingPageRoute") is True
                and route_event.get("targetPageId") == action.get("targetPageId")
                and int(target.get("processId", -1)) == pid and int(target.get("ownerWindowProcessId", -1)) == pid
                and int(target.get("ownerWindowHandle", -1)) == hwnd
                and int(pointer.get("targetProcessId", -1)) == pid and int(pointer.get("windowHandle", -1)) == hwnd
                and int(pointer.get("windowProcessId", -1)) == pid,
                f"UIA route action lacks exact measured PID/HWND provenance: {action.get('actionKind')}.")
        require(int(route_event.get("processId", -1)) == pid, f"Page-route event is not bound to the measured PID: {action.get('actionKind')}.")
    activations = route.get("resourceActivations") or []
    expected_activation_ids = ["warmup", *[f"cycle-{index:02d}" for index in range(1, 11)]]
    require([activation.get("activationId") for activation in activations] == expected_activation_ids,
            "Resource activation records do not cover the single warm-up and all ten measured cycles.")
    fixture_images = {
        str(image.get("resourceId", "")): image
        for image in (manifest.get("fixture") or {}).get("images", [])
    }
    all_loaded_ids: list[str] = []
    for activation in activations:
        route_event = activation.get("routeEvent") or {}
        require(route_event.get("targetPageId") == "campus_map" and int(route_event.get("processId", -1)) == pid,
                f"Bundang resource activation is not bound to a campus_map UIA route: {activation.get('activationId')}.")
        expected_lifecycle = (
            [("campus_map", "created"), ("campus_information", "suspended"), ("campus_map", "activated")]
            if activation.get("activationId") == "warmup"
            else [("campus_information", "suspended"), ("campus_map", "activated")]
        )
        validate_lifecycle_sequence(
            activation.get("lifecycleEvents") or [],
            expected_lifecycle,
            f"resource activation {activation.get('activationId')}",
        )
        validate_lifecycle_pair(activation.get("lifecycleEvents") or [], "campus_map", "activated", "campus_information", "suspended", f"resource activation {activation.get('activationId')}")
        loads = activation.get("loads") or []
        require(len(loads) == 2, f"Activation {activation.get('activationId')} lacks two resource-loader image records.")
        loaded_ids: list[str] = []
        for event in loads:
            record = event.get("record") or {}
            resource_id = str(record.get("resource_id", ""))
            loaded_ids.append(resource_id)
            require(resource_id in fixture_info["mapResourceIds"], f"Unexpected map resource loaded in {activation.get('activationId')}: {resource_id}.")
            fixture_image = fixture_images.get(resource_id)
            require(fixture_image is not None,
                    f"Pinned Bundang fixture image details are missing for {resource_id}.")
            validate_packaged_map_resource_load(record, fixture_image, str(activation.get("activationId")))
        require(sorted(loaded_ids) == sorted(fixture_info["mapResourceIds"]),
                f"Activation {activation.get('activationId')} did not load precisely the map and building images.")
        all_loaded_ids.extend(loaded_ids)
    screenshots = {str(record.get("relativePath")): record for record in route.get("screenshots", [])}
    targeted_uia_records = route.get("targetedUiaArtifacts") or []
    targeted_uia_artifacts = {str(record.get("relativePath")): record for record in targeted_uia_records}
    windows = (manifest.get("memory") or {}).get("windows") or []
    require(isinstance(targeted_uia_records, list) and len(targeted_uia_records) == len(windows)
            and len(targeted_uia_artifacts) == len(targeted_uia_records),
            "Route does not contain one uniquely named targeted UIA artifact per memory window.")
    for window in windows:
        window_id = str(window.get("windowId", ""))
        confirmation = window.get("stateConfirmation") or {}
        screenshot = confirmation.get("screenshot") or {}
        ui_snapshot = confirmation.get("targetedUiaArtifact") or {}
        require(confirmation.get("state") == window.get("matchedState")
                and int(confirmation.get("processId", -1)) == pid
                and int(confirmation.get("mainWindowHandle", -1)) == hwnd,
                f"Memory window is not tied to its confirmed UI state/process: {window.get('windowId')}.")
        if window_id == "cold-baseline" or window_id.startswith("plateau-baseline-"):
            expected_lifecycle = [("campus_information", "activated")]
        elif window_id == "warmup-open":
            expected_lifecycle = [
                ("campus_map", "created"),
                ("campus_information", "suspended"),
                ("campus_map", "activated"),
            ]
        elif window_id.endswith("-map-open"):
            expected_lifecycle = [("campus_information", "suspended"), ("campus_map", "activated")]
        elif window_id == "warmup-return" or window_id.endswith("-return"):
            expected_lifecycle = [("campus_map", "suspended"), ("campus_information", "activated")]
        else:
            raise ValidationError(f"Settled memory window has no pinned lifecycle contract: {window_id}.")
        validate_lifecycle_sequence(
            confirmation.get("lifecycleEvents") or [],
            expected_lifecycle,
            f"settled memory window {window_id}",
        )
        screenshot_record = screenshots.get(str(screenshot.get("relativePath", "")))
        require(screenshot_record is not None and screenshot_record.get("sha256") == screenshot.get("sha256")
                and int(screenshot_record.get("processId", -1)) == pid and int(screenshot_record.get("windowHandle", -1)) == hwnd
                and int(screenshot_record.get("windowProcessId", -1)) == pid,
                f"Memory window lacks a PID/HWND-bound screenshot: {window.get('windowId')}.")
        screenshot_path = verify_file_record(evidence_root, str(screenshot_record["relativePath"]), str(screenshot_record["sha256"]), f"Screenshot {window.get('windowId')}", int(screenshot_record.get("sizeBytes", -1)))
        width, height = read_png_dimensions(screenshot_path)
        require(width == int(screenshot_record.get("width", -1)) and height == int(screenshot_record.get("height", -1)),
                f"Screenshot dimensions differ from the PID/HWND capture record: {window.get('windowId')}.")
        ui_record = targeted_uia_artifacts.get(str(ui_snapshot.get("relativePath", "")))
        require(ui_record is not None and ui_record.get("sha256") == ui_snapshot.get("sha256")
                and int(ui_record.get("processId", -1)) == pid and int(ui_record.get("windowHandle", -1)) == hwnd,
                f"Memory window lacks a PID/HWND-bound target-control UIA artifact: {window.get('windowId')}.")
        ui_path = verify_file_record(evidence_root, str(ui_record["relativePath"]), str(ui_record["sha256"]), f"Target-control UIA artifact {window.get('windowId')}", int(ui_record.get("sizeBytes", -1)))
        expected_title = "Maps" if window.get("matchedState") == "campus-map" else "Office Information"
        require(confirmation.get("pageTitle") == expected_title, f"UIA confirmed the wrong campus page title in {window.get('windowId')}.")
        artifact = load_json(ui_path)
        expected_artifact_name = targeted_uia_artifact_name_for_window(window_id)
        require(artifact.get("name") == expected_artifact_name,
                f"Target-control UIA artifact name differs from its memory window: {window.get('windowId')}.")
        targeted_counts = validate_targeted_uia_artifact(artifact, expected_title, pid, hwnd)
        require(confirmation.get("uiaMapImageControlCount") == targeted_counts["mapImage"]
                and confirmation.get("uiaMapImageCardCount") == targeted_counts["mapImageCard"],
                f"Route UIA control counts differ from the parsed target-control artifact in {window.get('windowId')}.")
    require(len(all_loaded_ids) == 22, "Expected exactly two successful image decodes per warm-up and measured activation.")
    return {"uiaNavigationCount": len(navigations), "measuredCycles": len(cycles), "resourceActivationCount": len(activations), "imageLoadCount": len(all_loaded_ids), "imageLoadResourceIds": sorted(set(all_loaded_ids)), "mapWidgetReleaseMinimum": min([int(warmup["mapPageWidgetCountReleased"]), *[int(cycle["mapPageWidgetCountReleased"]) for cycle in cycles]])}


def validate_lifecycle_pair(
    events: list[dict[str, Any]],
    first_page: str,
    first_state: str,
    second_page: str,
    second_state: str,
    label: str,
) -> None:
    found = {(str(event.get("pageKey")), str(event.get("state"))) for event in events}
    require((first_page, first_state) in found and (second_page, second_state) in found,
            f"Lifecycle confirmation is incomplete for {label}.")
    require(all(bool(event.get("pageInstanceAvailable")) for event in events if (event.get("pageKey"), event.get("state")) in {(first_page, first_state), (second_page, second_state)}),
            f"Lifecycle page instance was unavailable for {label}.")


def validate_lifecycle_sequence(
    events: list[dict[str, Any]] | dict[str, Any],
    expected: list[tuple[str, str]],
    label: str,
) -> None:
    normalized_events = [events] if isinstance(events, dict) else events
    require(isinstance(normalized_events, list) and all(isinstance(event, dict) for event in normalized_events),
            f"Lifecycle event evidence has malformed shape for {label}.")
    actual = [(str(event.get("pageKey")), str(event.get("state"))) for event in normalized_events]
    require(actual == expected, f"Lifecycle sequence differs for {label}: {actual}.")
    arrivals = [int(event.get("arrivalIndex", -1)) for event in normalized_events]
    require(all(value >= 0 for value in arrivals) and arrivals == sorted(arrivals)
            and len(set(arrivals)) == len(arrivals),
            f"Lifecycle event arrival order differs for {label}.")
    require(all(
        type(event.get("pageInstanceAvailable")) is bool
        and event.get("pageInstanceAvailable") is (state != "created")
        and type(event.get("sampleAvailable")) is bool
        and event.get("sampleAvailable") is True
        for event, (_, state) in zip(normalized_events, expected)
    ), f"Lifecycle page-instance state or process sample was unavailable or untyped for {label}.")


def validate_memory(manifest: dict[str, Any]) -> dict[str, Any]:
    memory = manifest.get("memory") or {}
    route = manifest.get("route") or {}
    windows = memory.get("windows")
    require(isinstance(windows, list) and [window.get("windowId") for window in windows] == expected_window_ids(),
            "Memory windows are missing, duplicated, or out of the required warm-up/baseline/ten-cycle order.")
    require(memory.get("workingSetTargetBytes") == WORKING_SET_TARGET and memory.get("workingSetTargetExclusive") is True
            and memory.get("workingSetCeilingBytes") == WORKING_SET_CEILING and memory.get("workingSetCeilingExclusive") is False,
            "Memory target or inclusive 450 MiB ceiling differs from the pinned Phase 9 policy.")
    require(memory.get("operationPeakAdvisoryLimitBytes") == OPERATION_PEAK_ADVISORY and memory.get("operationPeakAdvisoryOnly") is True,
            "Separate operation peak advisory threshold differs from 512 MiB.")
    process = manifest.get("process") or {}
    pid = int(process.get("processId", -1))
    hwnd = int(process.get("mainWindowHandle", -1))
    by_id: dict[str, dict[str, Any]] = {}
    target_misses: list[str] = []
    ceiling_exceedances: list[dict[str, Any]] = []
    all_values: list[int] = []
    for window in windows:
        name = str(window.get("windowId", ""))
        by_id[name] = window
        require(int(window.get("processId", -1)) == pid and int(window.get("mainWindowHandle", -1)) == hwnd,
                f"Memory window {name} is bound to a different process or main HWND.")
        require(window.get("stateConfirmedBeforeSampling") is True and int(window.get("settleDelaySeconds", -1)) == 2
                and int(window.get("sampleIntervalMilliseconds", -1)) == 100,
                f"Memory window {name} lacks confirmed state, two-second settle, or 100 ms cadence.")
        confirmation = window.get("stateConfirmation") or {}
        require(parse_utc(window.get("samplingStartedAtUtc"), f"{name} sampling start") - parse_utc(confirmation.get("confirmedAtUtc"), f"{name} UIA confirmation") >= dt.timedelta(seconds=1.9),
                f"Memory window {name} started before its confirmed state settled for two seconds.")
        samples = window.get("samples")
        require(isinstance(samples, list) and len(samples) == 10 and int(window.get("sampleCount", -1)) == 10
                and window.get("samplingCompleted") is True and int(window.get("samplingErrorCount", -1)) == 0
                and window.get("samplingErrors") == [], f"Memory window {name} does not contain ten complete samples.")
        require([int(sample.get("sampleIndex", -1)) for sample in samples] == list(range(10)),
                f"Memory window {name} sample indexes are missing or reordered.")
        elapsed = [int(sample.get("elapsedMilliseconds", -1)) for sample in samples]
        require(elapsed == sorted(elapsed) and len(set(elapsed)) == 10 and all(value >= 0 for value in elapsed)
                and 750 <= elapsed[-1] - elapsed[0] <= 2_000
                and all(35 <= current - previous <= 350 for previous, current in zip(elapsed, elapsed[1:])),
                f"Memory window {name} samples do not span the bounded 100 ms schedule.")
        time_values = [parse_utc(sample.get("capturedAtUtc"), f"{name} sample timestamp") for sample in samples]
        require(time_values == sorted(time_values) and len(set(time_values)) == 10,
                f"Memory window {name} sample timestamps are missing or not strictly ordered.")
        sample_working_sets: list[int] = []
        actual_target_misses: list[int] = []
        actual_ceiling_misses: list[int] = []
        for index, sample in enumerate(samples):
            require(int(sample.get("processId", -1)) == pid and sample.get("processState") == "running"
                    and sample.get("metricErrors") == [], f"Memory window {name} sample {index} is not a valid running-process sample.")
            for metric in ("workingSetBytes", "privateBytes", "peakWorkingSetBytes", "handles", "threadCount"):
                require(type(sample.get(metric)) is int and int(sample[metric]) >= 0, f"Memory window {name} lacks nonnegative {metric} evidence.")
            value = int(sample["workingSetBytes"])
            sample_working_sets.append(value)
            all_values.append(value)
            if value >= WORKING_SET_TARGET:
                actual_target_misses.append(index)
            if value > WORKING_SET_CEILING:
                actual_ceiling_misses.append(index)
                ceiling_exceedances.append({"windowId": name, "sampleIndex": index, "workingSetBytes": value})
        require(window.get("workingSetTargetExceededSampleIndexes") == actual_target_misses
                and window.get("workingSetTargetMet") is (not actual_target_misses),
                f"Memory window {name} target-miss evidence differs from its samples.")
        require(window.get("workingSetCeilingExceededSampleIndexes") == actual_ceiling_misses
                and window.get("workingSetCeilingPassed") is (not actual_ceiling_misses),
                f"Memory window {name} ceiling evidence differs from its samples.")
        if actual_target_misses:
            target_misses.append(name)
    require(route.get("warmupCount") == 1 and route.get("baselineWindowsPerState") == 5 and route.get("measuredCycleCount") == 10,
            "Memory route summary differs from the fixed P9-18 sequence.")
    expected_baselines = [f"plateau-baseline-{index:02d}" for index in range(1, 6)]
    require(memory.get("coldBaselineWindowId") == "cold-baseline" and memory.get("coldReturnWindowId") == "warmup-return"
            and memory.get("plateauBaselineWindowIds") == expected_baselines,
            "Memory evidence does not identify its matched cold/return and five plateau baseline windows.")
    cycle_returns = [f"cycle-{index:02d}-return" for index in range(1, 11)]
    analysis = compute_cleanup_analysis(
        [int(sample["workingSetBytes"]) for sample in by_id["cold-baseline"]["samples"]],
        [int(sample["workingSetBytes"]) for sample in by_id["warmup-return"]["samples"]],
        [[int(sample["workingSetBytes"]) for sample in by_id[name]["samples"]] for name in expected_baselines],
        [[int(sample["workingSetBytes"]) for sample in by_id[name]["samples"]] for name in cycle_returns],
    )
    require(not ceiling_exceedances, f"Settled Working Set exceeded the inclusive 450 MiB ceiling: {ceiling_exceedances}.")
    memory["analysis"] = analysis
    cleanup_failures: list[str] = []
    cold_cleanup = analysis["coldBaselineToFirstReturn"]
    if not cold_cleanup["passed"]:
        cleanup_failures.append(
            "Cold cleanup gate failed: first-return median "
            f"{cold_cleanup['firstReturnMedianWorkingSetBytes']} B exceeds its matched-baseline maximum "
            f"{cold_cleanup['maximumAllowedReturnMedianBytes']} B by "
            f"{cold_cleanup['firstReturnMedianWorkingSetBytes'] - cold_cleanup['maximumAllowedReturnMedianBytes']} B."
        )
    plateau_cleanup = analysis["postPrimingPlateauAndCycleReturns"]
    if not plateau_cleanup["passed"]:
        cleanup_failures.extend(plateau_cleanup["failureReasons"])
    operation_peak = memory.get("operationPeakWorkingSetBytes")
    require(type(operation_peak) is int and operation_peak >= 0, "Separate operation peak Working Set measurement is missing.")
    return {
        "passed": bool(analysis["passed"] and not ceiling_exceedances),
        "failureReasons": cleanup_failures,
        "windowCount": len(windows),
        "samplesPerWindow": {name: len(window["samples"]) for name, window in by_id.items()},
        "strictTargetBytes": WORKING_SET_TARGET,
        "inclusiveCeilingBytes": WORKING_SET_CEILING,
        "maximumSettledWorkingSetBytes": max(all_values),
        "windowsWithTargetMisses": target_misses,
        "settledCeilingExceedances": ceiling_exceedances,
        "operationPeakWorkingSetBytes": operation_peak,
        "operationPeakAdvisoryLimitBytes": OPERATION_PEAK_ADVISORY,
        "operationPeakAdvisoryExceeded": operation_peak >= OPERATION_PEAK_ADVISORY,
        "cleanupGates": analysis,
        "privateBytesDiagnosticOnly": True,
    }


def add_gate(report: dict[str, Any], name: str, function: Callable[[], dict[str, Any]]) -> None:
    try:
        details = function()
        require(isinstance(details, dict), f"Gate {name} returned malformed details.")
        if "passed" in details and type(details["passed"]) is not bool:
            report["gates"][name] = {
                "passed": False,
                "failureReasons": [f"Gate {name} returned a non-boolean passed field."],
                "details": details,
            }
        elif details.get("passed") is False:
            reasons = details.get("failureReasons")
            if not isinstance(reasons, list) or not reasons or not all(isinstance(reason, str) for reason in reasons):
                reasons = [f"Gate {name} reported passed=false."]
            report["gates"][name] = {"passed": False, "failureReasons": reasons, "details": details}
        else:
            report["gates"][name] = {"passed": True, "details": details}
    except (ValidationError, OSError, ValueError, TypeError, KeyError, IndexError) as error:
        report["gates"][name] = {"passed": False, "failureReasons": [str(error)]}


def validate(manifest_path: Path, repository: Path) -> dict[str, Any]:
    manifest_path = manifest_path.resolve(strict=True)
    evidence_root = manifest_path.parent
    repository = repository.resolve(strict=True)
    manifest = load_json(manifest_path)
    report: dict[str, Any] = {
        "schema": "classmngr-phase9-p9-18-validation-report-v1",
        "taskId": TASK_ID,
        "runId": manifest.get("runId"),
        "manifestPath": str(manifest_path),
        "manifestSha256": sha256_file(manifest_path),
        "passed": False,
        "gates": {},
        "validationStartedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z"),
    }
    fixture_info: dict[str, Any] = {}
    try:
        fixture_info = validate_fixture(manifest, evidence_root, repository)
        report["gates"]["bundangFixture"] = {"passed": True, "details": fixture_info}
    except (ValidationError, OSError, ValueError, TypeError, KeyError, IndexError) as error:
        report["gates"]["bundangFixture"] = {"passed": False, "failureReasons": [str(error)]}

    add_gate(report, "exactSourceBuildAndPackage", lambda: validate_build_package_and_harness(manifest, evidence_root, repository))
    if fixture_info:
        add_gate(report, "uiaLifecycleImagesAndScreenshots", lambda: validate_navigation_and_resources(manifest, evidence_root, fixture_info))
    else:
        report["gates"]["uiaLifecycleImagesAndScreenshots"] = {"passed": False, "failureReasons": ["Bundang fixture could not be validated."]}
    add_gate(report, "normalExitAndReadOnlyAppData", lambda: validate_process_exit_and_app_data(manifest))
    add_gate(report, "settledMemoryAndCleanupGates", lambda: validate_memory(manifest))
    report["passed"] = all(bool(gate.get("passed")) for gate in report["gates"].values())
    report["status"] = "passed" if report["passed"] else "failed"
    report["validationCompletedAtUtc"] = dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
    report["summary"] = {
        "maximumSettledWorkingSetBytes": report.get("gates", {}).get("settledMemoryAndCleanupGates", {}).get("details", {}).get("maximumSettledWorkingSetBytes"),
        "windowsWithTargetMisses": report.get("gates", {}).get("settledMemoryAndCleanupGates", {}).get("details", {}).get("windowsWithTargetMisses", []),
        "operationPeakWorkingSetBytes": report.get("gates", {}).get("settledMemoryAndCleanupGates", {}).get("details", {}).get("operationPeakWorkingSetBytes"),
        "coldCleanup": report.get("gates", {}).get("settledMemoryAndCleanupGates", {}).get("details", {}).get("cleanupGates", {}).get("coldBaselineToFirstReturn"),
        "plateauCleanup": report.get("gates", {}).get("settledMemoryAndCleanupGates", {}).get("details", {}).get("cleanupGates", {}).get("postPrimingPlateauAndCycleReturns"),
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--report-output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = validate(args.manifest, args.repository)
    except (OSError, ValidationError, ValueError, TypeError, KeyError, IndexError) as error:
        report = {
            "schema": "classmngr-phase9-p9-18-validation-report-v1",
            "taskId": TASK_ID,
            "status": "failed",
            "passed": False,
            "gates": {"manifest": {"passed": False, "failureReasons": [str(error)]}},
        }
    args.report_output.parent.mkdir(parents=True, exist_ok=True)
    args.report_output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"status": report.get("status"), "passed": report.get("passed"), "report": str(args.report_output.resolve()), "gates": report.get("gates")}, ensure_ascii=False))
    return 0 if report.get("passed") is True else 1


if __name__ == "__main__":
    raise SystemExit(main())
