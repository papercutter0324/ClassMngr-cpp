#!/usr/bin/env python3
"""Validate source-bound Windows packaged-app idle-memory evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import ntpath
import re
import statistics
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any


TASK_ID = "P9_06_PACKAGED_STARTUP_IDLE_GATE"
MANIFEST_SCHEMA = "classmngr-phase9-packaged-idle-memory-v3"
SUMMARY_SCHEMA = "classmngr-phase9-packaged-idle-memory-summary-v2"
P9_07_TASK_ID = "P9_07_REPEATED_NORMAL_NAVIGATION_AND_IDLE_GATE"
P9_07_MANIFEST_SCHEMA = "classmngr-phase9-packaged-navigation-idle-memory-v3"
P9_07_SUMMARY_SCHEMA = "classmngr-phase9-packaged-navigation-idle-memory-summary-v3"
EXPECTED_SCENARIOS = ("empty-workspace", "representative-96-class-workspace")
EXPECTED_FIXTURES = {
    "empty-workspace": ("schema-only-v6-empty", 0),
    "representative-96-class-workspace": ("synthetic-large-startup-96-classes", 96),
}
EXPECTED_FIXTURE_GENERATORS = {
    "empty-workspace": "empty-workspace-fixture-generator",
    "representative-96-class-workspace": "representative-workspace-fixture-generator",
}
REQUIRED_PACKAGE_BUILD_TARGETS = (
    "ClassMngrNextResourcePackage",
    "ClassMngrPhase7NextBlankFixtureTests",
    "ClassMngrStartupPerformanceTests",
)
STARTUP_BOUNDARY_METRICS_SCHEMA = "classmngr-phase9-startup-boundary-metrics-v1"
STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS = 2_000
EXPECTED_STARTUP_BOUNDARIES = (
    {"eventId": "theme-loaded", "kind": "stage", "value": "theme-loaded"},
    {
        "eventId": "inter-font-resource-loaded",
        "kind": "resource",
        "value": "resource:assets/fonts/Inter-Regular.ttf",
    },
    {
        "eventId": "pretendard-font-resource-loaded",
        "kind": "resource",
        "value": "resource:assets/fonts/PretendardVariable.ttf",
    },
    {
        "eventId": "required-fonts-loaded",
        "kind": "stage",
        "value": "required-fonts-loaded",
    },
    {"eventId": "shell-created", "kind": "stage", "value": "shell-created"},
)
EXPECTED_INTER_FONT_RESOURCE_IDS = (
    "resource:assets/fonts/Inter-Regular.ttf",
    "resource:assets/fonts/Inter-Medium.ttf",
    "resource:assets/fonts/Inter-SemiBold.ttf",
    "resource:assets/fonts/Inter-Bold.ttf",
    "resource:assets/fonts/Inter-BoldItalic.ttf",
    "resource:assets/fonts/Inter-Black.ttf",
)
EXPECTED_INTER_FONT_CATALOG_ENTRIES = tuple(
    {
        "resourceId": resource_id,
        "sourcePath": resource_id.replace("resource:assets/", "resources/assets/"),
        "packageRelativePath": resource_id.replace("resource:", ""),
    }
    for resource_id in EXPECTED_INTER_FONT_RESOURCE_IDS
)
INTER_FONT_LICENSE_SOURCE_PATH = "licenses/fonts/inter/LICENSE.txt"
INTER_FONT_LICENSE_PACKAGE_PATH = "licenses/fonts/inter/LICENSE.txt"
REQUIRED_SOURCE_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "docs/qt-rewrite/phase-4-resource-inventory.json",
    "licenses/qt/LGPL-3.0-only.txt",
    "licenses/fonts/inter/LICENSE.txt",
    "scripts/phase4/bake_resources.py",
    "scripts/phase4/generate_resource_catalog.py",
    "scripts/phase4/generate_resource_inventory.py",
    "scripts/phase9/run_packaged_idle_memory_gate.ps1",
    "scripts/phase9/test_validate_packaged_idle_memory_evidence.py",
    "scripts/phase9/validate_packaged_idle_memory_evidence.py",
    "src/next/bootstrap/application_bootstrap.cpp",
    "src/next/ui/application_shell.cpp",
    "third_party/sqlite/3.54.1/sqlite3.c",
    "third_party/sqlite/3.54.1/sqlite3.h",
}
THRESHOLD_BYTES = 262_144_000
IDLE_SECONDS = 300
SAMPLE_COUNT = IDLE_SECONDS + 1
NAVIGATION_METRICS_SCHEMA = "classmngr-phase9-navigation-metrics-v3"
NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA = "classmngr-phase9-navigation-plateau-diagnostic-v1"
STARTUP_READY_METRICS_SCHEMA = "classmngr-phase9-startup-ready-metrics-v1"
NAVIGATION_SELECTION_METHODS = (
    "uia-verified-pointer-click",
    "uia-verified-selection-invoke",
    "uia-verified-window-message-click",
)
NAVIGATION_CYCLES = 10
NAVIGATION_PLATEAU_CYCLES = 20
SUPPORTED_NAVIGATION_PLATEAU_CYCLE_COUNTS = (20, 30)
NAVIGATION_ROUTE = (
    ("my_workspace", "schedule"),
    ("schedule", "calendar"),
    ("calendar", "my_classes"),
    ("my_classes", "my_workspace"),
)
EXPECTED_NAVIGATION_TRANSITIONS = NAVIGATION_CYCLES * len(NAVIGATION_ROUTE)
SAMPLE_INTERVAL_MIN_SECONDS = 0.75
SAMPLE_INTERVAL_MAX_SECONDS = 1.5
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
REVISION_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
STARTUP_READY_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=startup-ready outcome=completed "
    r"diagnostic=none diagnostic_stage=none checkpoint=(\{.*\})$"
)
WORKSPACE_OPENED_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=workspace-opened "
    r"outcome=(?P<outcome>[a-z-]+) diagnostic=(?P<diagnostic>[a-z-]+) "
    r"diagnostic_stage=(?P<diagnosticStage>[a-z-]+) checkpoint=(?P<checkpoint>\{.*\})$"
)
STARTUP_BOUNDARY_STAGE_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=(?P<stage>[a-z-]+) "
    r"outcome=(?P<outcome>[a-z-]+) diagnostic=(?P<diagnostic>[a-z-]+) "
    r"diagnostic_stage=(?P<diagnosticStage>[a-z-]+) checkpoint=(?P<checkpoint>\{.*\})$"
)
RESOURCE_CHECKPOINT_RE = re.compile(r"^ClassMngrNext resource checkpoint=(\{.*\})$")
DOCUMENT_CATALOG_RE = re.compile(
    r"^ClassMngrNext document-catalog event=projected outcome=(?P<outcome>[a-z-]+) "
    r"document_count=(?P<documentCount>\d+) loaded_pdf_body_count=(?P<bodyCount>\d+)$"
)
NAVIGATION_ROUTE_RE = re.compile(
    r"^ClassMngrNext navigation event=page-route outcome=(?P<outcome>[a-z-]+) "
    r"cause=(?P<cause>[a-z-]+) from=(?P<from>[a-z0-9_-]+) to=(?P<to>[a-z0-9_-]+) "
    r"catalog_document_count=(?P<documentCount>-?\d+) loaded_pdf_body_count=(?P<bodyCount>\d+)$"
)
EXPECTED_NAVIGATION_CONTROL_AUTOMATION_IDS = (
    "",
    "classMngrNextNavigationTree",
    "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
)


class EvidenceValidationError(ValueError):
    """Raised when evidence is incomplete or cannot be trusted."""


def _parse_timestamp(value: Any, label: str) -> datetime:
    if not isinstance(value, str):
        raise EvidenceValidationError(f"{label} must be a UTC timestamp string")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise EvidenceValidationError(f"{label} is not an ISO timestamp") from error
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise EvidenceValidationError(f"{label} must use UTC")
    return parsed.astimezone(timezone.utc)


def _require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceValidationError(f"{label} must be an object")
    return value


def _require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise EvidenceValidationError(f"{label} must be a lowercase SHA-256 digest")
    return value


def _require_positive_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise EvidenceValidationError(f"{label} must be a positive integer")
    return value


def _require_nonnegative_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise EvidenceValidationError(f"{label} must be a nonnegative integer")
    return value


def _same_windows_path(left: Any, right: Any) -> bool:
    if not isinstance(left, str) or not isinstance(right, str):
        return False
    return ntpath.normcase(ntpath.normpath(left)) == ntpath.normcase(
        ntpath.normpath(right)
    )


def _read_json(path: Path, label: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error


def _read_jsonl(path: Path, label: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    raise EvidenceValidationError(
                        f"{label} has an empty line at {line_number}"
                    )
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{label} line {line_number} is invalid JSON"
                    ) from error
                rows.append(_require_object(value, f"{label} line {line_number}"))
    except OSError as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error
    return rows


def _startup_boundary_event_id(line: str) -> str | None:
    for boundary in EXPECTED_STARTUP_BOUNDARIES:
        if boundary["kind"] == "stage" and (
            f"ClassMngrNext startup stage={boundary['value']} " in line
        ):
            return str(boundary["eventId"])
    for boundary in EXPECTED_STARTUP_BOUNDARIES:
        if boundary["kind"] != "resource" or boundary["value"] not in line:
            continue
        match = RESOURCE_CHECKPOINT_RE.fullmatch(line)
        if match is None:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} record is malformed"
            )
        try:
            checkpoint = json.loads(match.group(1))
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} JSON is malformed"
            ) from error
        if not isinstance(checkpoint, dict) or checkpoint.get("resource_id") != boundary["value"]:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} identity is malformed"
            )
        return str(boundary["eventId"])
    return None


def _inter_font_resource_checkpoint(line: str) -> dict[str, Any] | None:
    if "resource:assets/fonts/Inter" not in line:
        return None
    match = RESOURCE_CHECKPOINT_RE.fullmatch(line)
    if match is None:
        raise EvidenceValidationError("Inter font resource startup record is malformed")
    try:
        checkpoint = json.loads(match.group(1))
    except json.JSONDecodeError as error:
        raise EvidenceValidationError("Inter font resource checkpoint JSON is malformed") from error
    if not isinstance(checkpoint, dict):
        raise EvidenceValidationError("Inter font resource checkpoint must be an object")
    return checkpoint


def _validate_msvc_release_flags(value: Any, label: str) -> dict[str, Any]:
    flags = _require_object(value, label)
    raw = flags.get("raw")
    if not isinstance(raw, str):
        raise EvidenceValidationError(f"{label}.raw must be a string")
    optimization_options = [
        match.group(0).strip()
        for match in re.finditer(r"(?i)(?:^|\s)[/-](?:O1|O2|Ox)(?=\s|$)", raw)
    ]
    if not optimization_options or re.search(r"(?i)(?:^|\s)[/-]Od(?=\s|$)", raw):
        raise EvidenceValidationError(
            f"{label} must include /O1, /O2, or /Ox and must not disable optimization"
        )
    if not re.search(r"(?i)(?:^|\s)[/-]D\s*NDEBUG(?=\s|$)", raw):
        raise EvidenceValidationError(f"{label} must define NDEBUG with /DNDEBUG")
    recorded_options = flags.get("optimizationOptions")
    if recorded_options != optimization_options or flags.get("definesNdebug") is not True:
        raise EvidenceValidationError(f"{label} verified flag summary differs from raw flags")
    return {"raw": raw, "optimizationOptions": optimization_options}


def _canonical_package_sha(files: list[dict[str, Any]]) -> str:
    entries = []
    for index, raw_entry in enumerate(files):
        entry = _require_object(raw_entry, f"package file inventory entry {index}")
        path = entry.get("path")
        digest = entry.get("sha256")
        if not isinstance(path, str) or not path or "\n" in path:
            raise EvidenceValidationError("package file path is invalid")
        normalized_path = path.replace("\\", "/")
        if normalized_path.startswith("/") or any(
            part in ("", ".", "..") for part in normalized_path.split("/")
        ):
            raise EvidenceValidationError("package file path must be relative and normalized")
        _require_sha(digest, "package file sha256")
        size = _require_nonnegative_int(entry.get("sizeBytes"), "package file sizeBytes")
        entries.append(f"{normalized_path}\t{size}\t{digest}")
    return hashlib.sha256(("\n".join(entries) + "\n").encode("utf-8")).hexdigest()


def _canonical_source_sha(files: list[dict[str, Any]], included_roots: list[str]) -> str:
    source_paths: list[str] = []
    for index, raw_entry in enumerate(files):
        entry = _require_object(raw_entry, f"source file inventory entry {index}")
        path = entry.get("path")
        if not isinstance(path, str):
            raise EvidenceValidationError("source file path is invalid")
        normalized_path = path.replace("\\", "/")
        if not any(
            normalized_path == root
            or normalized_path.startswith(root.rstrip("/") + "/")
            for root in included_roots
        ):
            raise EvidenceValidationError("source file inventory contains a path outside includedRoots")
        source_paths.append(normalized_path)
    if len(source_paths) != len(set(source_paths)):
        raise EvidenceValidationError("source file inventory contains duplicate paths")
    return _canonical_package_sha(files)


def _metric_summary(values: list[int]) -> dict[str, int | float]:
    return {"min": min(values), "median": statistics.median(values), "max": max(values)}


def _validate_metric_process_identity(
    row: dict[str, Any],
    scenario_id: str,
    sample_label: str,
    pid: int,
    created_at_text: str,
    executable_path: str,
    application_sha: str,
) -> dict[str, int]:
    if row.get("scenarioId") != scenario_id:
        raise EvidenceValidationError(f"{scenario_id} {sample_label} scenario identity differs")
    if row.get("pid") != pid or row.get("processCreatedAtUtc") != created_at_text:
        raise EvidenceValidationError(f"{scenario_id} {sample_label} process identity differs")
    if not _same_windows_path(row.get("canonicalExecutablePath"), executable_path):
        raise EvidenceValidationError(f"{scenario_id} {sample_label} image path differs from package")
    if row.get("architecture") != "x64" or _require_sha(
        row.get("sha256"), f"{scenario_id}.{sample_label}.sha256"
    ) != application_sha:
        raise EvidenceValidationError(f"{scenario_id} {sample_label} executable identity differs")
    values = {
        name: _require_positive_int(row.get(name), f"{scenario_id}.{sample_label}.{name}")
        for name in (
            "currentWorkingSetBytes",
            "privateBytes",
            "peakWorkingSetBytes",
            "handles",
            "threads",
        )
    }
    if values["peakWorkingSetBytes"] < values["currentWorkingSetBytes"]:
        raise EvidenceValidationError(
            f"{scenario_id} {sample_label} peak working set is below current working set"
        )
    return values


def validate_navigation_plateau_documents(
    manifest_value: Any,
    metric_rows: list[dict[str, Any]],
    startup_events: list[dict[str, Any]],
    startup_ready_rows: list[dict[str, Any]],
    navigation_rows: list[dict[str, Any]],
    package_file_rows: list[dict[str, Any]],
    source_file_rows: list[dict[str, Any]],
) -> dict[str, Any]:
    """Validate a representative-workspace navigation diagnostic without evaluating the gate."""
    manifest = _require_object(manifest_value, "manifest")
    if manifest.get("taskId") != P9_07_TASK_ID or manifest.get("schema") != P9_07_MANIFEST_SCHEMA:
        raise EvidenceValidationError("navigation plateau diagnostic manifest identity is invalid")
    if manifest.get("status") != "completed":
        raise EvidenceValidationError("navigation plateau diagnostic run did not complete")
    navigation_manifest = _require_object(manifest.get("navigation"), "navigation")
    plateau_cycles = navigation_manifest.get("cycles")
    if (
        isinstance(plateau_cycles, bool)
        or not isinstance(plateau_cycles, int)
        or plateau_cycles not in SUPPORTED_NAVIGATION_PLATEAU_CYCLE_COUNTS
    ):
        raise EvidenceValidationError("navigation plateau diagnostic cycle count is unsupported")
    if (
        navigation_manifest.get("schema") != NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA
        or navigation_manifest.get("executionMode") != "diagnostic-navigation-plateau"
        or navigation_manifest.get("diagnosticOnly") is not True
        or navigation_manifest.get("cycles") != plateau_cycles
        or navigation_manifest.get("transitionCountPerScenario")
        != plateau_cycles * len(NAVIGATION_ROUTE)
        or navigation_manifest.get("catalogBodyCountRequired") != 0
    ):
        raise EvidenceValidationError("navigation plateau diagnostic contract is invalid")
    candidate_source = _require_object(manifest.get("candidateSource"), "candidateSource")
    source_sha = _require_sha(candidate_source.get("sha256"), "candidateSource.sha256")
    if (
        candidate_source.get("stableDuringRun") is not True
        or candidate_source.get("sha256AtRunEnd") != source_sha
    ):
        raise EvidenceValidationError("candidate source identity changed during diagnostic")
    included_roots = candidate_source.get("includedRoots")
    if not isinstance(included_roots, list) or _canonical_source_sha(source_file_rows, included_roots) != source_sha:
        raise EvidenceValidationError("source inventory does not reproduce the candidate source fingerprint")
    configuration = _require_object(manifest.get("configuration"), "configuration")
    cache_sha = _require_sha(configuration.get("cmakeCacheSha256"), "configuration.cmakeCacheSha256")
    if (
        configuration.get("cmakeCacheStableDuringRun") is not True
        or configuration.get("cmakeCacheSha256AtRunEnd") != cache_sha
        or configuration.get("buildType") != "Release"
        or configuration.get("architecture") != "x64"
    ):
        raise EvidenceValidationError("Release configuration identity is incomplete or changed")
    compiler = _require_object(configuration.get("releaseCompiler"), "configuration.releaseCompiler")
    if (
        compiler.get("compiler") != "MSVC"
        or compiler.get("configuration") != "Release"
        or compiler.get("platform") != "x64"
        or compiler.get("projectOptimizationSetting") not in ("MaxSpeed", "Full")
    ):
        raise EvidenceValidationError("diagnostic Release compiler configuration is not optimized MSVC x64")
    _validate_msvc_release_flags(compiler.get("cFlagsRelease"), "configuration.releaseCompiler.cFlagsRelease")
    _validate_msvc_release_flags(compiler.get("cxxFlagsRelease"), "configuration.releaseCompiler.cxxFlagsRelease")
    package = _require_object(manifest.get("package"), "package")
    package_sha = _require_sha(package.get("sha256"), "package.sha256")
    if _canonical_package_sha(package_file_rows) != package_sha or package.get("fileCount") != len(package_file_rows):
        raise EvidenceValidationError("package inventory does not reproduce the packaged file fingerprint")
    font_license = _require_object(package.get("interFontLicense"), "package.interFontLicense")
    if (
        font_license.get("sourcePath") != INTER_FONT_LICENSE_SOURCE_PATH
        or font_license.get("packagePath") != INTER_FONT_LICENSE_PACKAGE_PATH
        or _require_positive_int(font_license.get("sizeBytes"), "package.interFontLicense.sizeBytes") <= 0
        or len(_require_sha(font_license.get("sha256"), "package.interFontLicense.sha256")) != 64
    ):
        raise EvidenceValidationError("Inter OFL license identity is missing or differs from the source")
    package_entry_by_path = {
        str(entry.get("path")): entry for entry in package_file_rows if isinstance(entry, dict)
    }
    license_entry = package_entry_by_path.get(INTER_FONT_LICENSE_PACKAGE_PATH)
    if (
        not isinstance(license_entry, dict)
        or license_entry.get("sizeBytes") != font_license.get("sizeBytes")
        or license_entry.get("sha256") != font_license.get("sha256")
    ):
        raise EvidenceValidationError("Inter OFL license is not bound to the exact package file inventory")
    catalog_entries = package.get("interFontCatalogEntries")
    if not isinstance(catalog_entries, list) or len(catalog_entries) != len(EXPECTED_INTER_FONT_CATALOG_ENTRIES):
        raise EvidenceValidationError("diagnostic package does not contain exactly six Inter catalog entries")
    for index, (entry_value, expected_entry) in enumerate(zip(catalog_entries, EXPECTED_INTER_FONT_CATALOG_ENTRIES, strict=True)):
        entry = _require_object(entry_value, f"package.interFontCatalogEntries[{index}]")
        if any(entry.get(key) != value for key, value in expected_entry.items()):
            raise EvidenceValidationError("diagnostic package Inter catalog identity differs from the expected font set")
        _require_positive_int(entry.get("sizeBytes"), f"package.interFontCatalogEntries[{index}].sizeBytes")
        _require_sha(entry.get("sha256"), f"package.interFontCatalogEntries[{index}].sha256")
        package_font_entry = package_entry_by_path.get("resources/" + str(entry.get("packageRelativePath")))
        if (
            not isinstance(package_font_entry, dict)
            or package_font_entry.get("sizeBytes") != entry.get("sizeBytes")
            or package_font_entry.get("sha256") != entry.get("sha256")
        ):
            raise EvidenceValidationError("catalogued Inter face differs from the exact package inventory")
    application = _require_object(manifest.get("application"), "application")
    app_sha = _require_sha(application.get("sha256"), "application.sha256")
    if application.get("candidateSourceSha256") != source_sha or package.get("candidateSourceSha256") != source_sha:
        raise EvidenceValidationError("diagnostic package/application is not bound to the candidate source")

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list) or len(scenarios) != 1:
        raise EvidenceValidationError("navigation plateau diagnostic must contain one representative scenario")
    scenario = _require_object(scenarios[0], "scenario")
    scenario_id = "representative-96-class-workspace"
    if (
        scenario.get("id") != scenario_id
        or scenario.get("outcome") != "diagnostic-completed"
        or scenario.get("budgetScope") != "diagnostic-only"
        or scenario.get("navigationCycles") != plateau_cycles
        or scenario.get("navigationTransitionCount") != plateau_cycles * len(NAVIGATION_ROUTE)
        or scenario.get("returnToNormalMilestoneCount") != plateau_cycles
        or scenario.get("normalExit") is not True
        or scenario.get("idleSeconds") != 0
    ):
        raise EvidenceValidationError("representative diagnostic scenario is incomplete or misclassified")
    fixture = _require_object(scenario.get("fixture"), "scenario.fixture")
    fixture_path = fixture.get("pathPassedToApp")
    if (
        fixture.get("classCount") != 96
        or fixture.get("profilePathArgumentSupplied") is not True
        or not isinstance(fixture_path, str)
        or not ntpath.isabs(fixture_path)
    ):
        raise EvidenceValidationError("representative diagnostic fixture identity is invalid")
    font_events = scenario.get("interFontResourceEvents")
    if (
        not isinstance(font_events, list)
        or [event.get("resourceId") for event in font_events if isinstance(event, dict)]
        != list(EXPECTED_INTER_FONT_RESOURCE_IDS)
        or any(event.get("outcome") != "succeeded" for event in font_events if isinstance(event, dict))
    ):
        raise EvidenceValidationError("diagnostic startup did not successfully load the exact six Inter faces")
    process = _require_object(scenario.get("process"), "scenario.process")
    pid = _require_positive_int(process.get("pid"), "scenario.process.pid")
    created_at_text = process.get("createdAtUtc")
    _parse_timestamp(created_at_text, "scenario.process.createdAtUtc")
    executable_path = process.get("canonicalExecutablePath")
    if not isinstance(executable_path, str) or not ntpath.isabs(executable_path):
        raise EvidenceValidationError("scenario.process.canonicalExecutablePath must be absolute")
    if process.get("architecture") != "x64" or _require_sha(
        process.get("sha256"), "scenario.process.sha256"
    ) != app_sha:
        raise EvidenceValidationError("diagnostic process does not match the packaged x64 executable")
    if application.get("architecture") != "x64":
        raise EvidenceValidationError("diagnostic package application is not x64")
    app_relative = package.get("applicationRelativePath")
    app_entry = package_entry_by_path.get(str(app_relative))
    if not isinstance(app_entry, dict) or app_entry.get("sha256") != app_sha:
        raise EvidenceValidationError("packaged executable hash differs from the package file inventory")

    expected_count = plateau_cycles * len(NAVIGATION_ROUTE)
    if len(navigation_rows) != expected_count:
        raise EvidenceValidationError(
            f"navigation plateau trace does not contain exactly {expected_count} transitions"
        )
    expected_item_names = {
        "schedule": "Schedule",
        "calendar": "Calendar",
        "my_classes": "My Classes",
        "my_workspace": "My Workspace",
    }
    route_events = [
        event for event in startup_events
        if event.get("scenarioId") == scenario_id
        and isinstance(event.get("line"), str)
        and event["line"].startswith("ClassMngrNext navigation event=page-route ")
        and " cause=selection " in event["line"]
    ]
    if len(route_events) != expected_count:
        raise EvidenceValidationError(
            f"startup log does not contain exactly {expected_count} normal navigation transitions"
        )
    catalog_events = [
        event for event in startup_events
        if event.get("scenarioId") == scenario_id
        and isinstance(event.get("line"), str)
        and event["line"].startswith("ClassMngrNext document-catalog event=projected ")
    ]
    catalog_line = catalog_events[0].get("line", "") if len(catalog_events) == 1 else ""
    catalog_match = DOCUMENT_CATALOG_RE.fullmatch(str(catalog_line))
    if (
        catalog_match is None
        or catalog_match.group("outcome") != "completed"
        or int(catalog_match.group("documentCount")) <= 0
        or int(catalog_match.group("bodyCount")) != 0
    ):
        raise EvidenceValidationError("representative document catalog did not report zero loaded bodies")

    route_values: list[tuple[int, int, int, int, int]] = []
    return_snapshots: list[dict[str, int]] = []
    for route_index, (row_value, event) in enumerate(zip(navigation_rows, route_events, strict=True)):
        row = _require_object(row_value, f"navigation row {route_index}")
        cycle = route_index // len(NAVIGATION_ROUTE) + 1
        step = route_index % len(NAVIGATION_ROUTE) + 1
        from_page, to_page = NAVIGATION_ROUTE[route_index % len(NAVIGATION_ROUTE)]
        if (
            row.get("scenarioId") != scenario_id
            or row.get("phase") != "navigation"
            or row.get("routeIndex") != route_index
            or row.get("cycle") != cycle
            or row.get("routeStep") != step
            or row.get("fromPageId") != from_page
            or row.get("toPageId") != to_page
            or row.get("targetPageId") != to_page
            or row.get("selectionVerified") is not True
            or row.get("selectionMethod") not in NAVIGATION_SELECTION_METHODS
            or row.get("uiaItemName") != expected_item_names[to_page]
            or row.get("uiaWindowProcessId") != pid
        ):
            raise EvidenceValidationError(f"navigation plateau route is out of order or unverified at transition {route_index}")
        _require_positive_int(row.get("uiaWindowHandle"), f"navigation row {route_index}.uiaWindowHandle")
        values = _validate_metric_process_identity(
            row, scenario_id, f"navigation[{route_index}]", pid, created_at_text,
            executable_path, app_sha,
        )
        route_match = NAVIGATION_ROUTE_RE.fullmatch(str(row.get("rawLine", "")))
        if (
            route_match is None
            or route_match.group("outcome") != "completed"
            or route_match.group("cause") != "selection"
            or route_match.group("from") != from_page
            or route_match.group("to") != to_page
            or int(route_match.group("documentCount")) <= 0
            or int(route_match.group("bodyCount")) != 0
        ):
            raise EvidenceValidationError(f"navigation plateau app route evidence is invalid at transition {route_index}")
        event_index = _require_nonnegative_int(row.get("eventLogIndex"), f"navigation[{route_index}].eventLogIndex")
        if event_index >= len(startup_events):
            raise EvidenceValidationError(f"navigation transition {route_index} refers outside the startup log")
        actual_event = startup_events[event_index]
        if (
            actual_event.get("line") != row.get("rawLine")
            or actual_event.get("observedAtUtc") != row.get("eventObservedAtUtc")
            or actual_event.get("scenarioId") != scenario_id
            or actual_event.get("line") != event.get("line")
        ):
            raise EvidenceValidationError(f"navigation transition {route_index} is not correlated to the startup log")
        event_time = _parse_timestamp(row.get("eventObservedAtUtc"), f"navigation[{route_index}].eventObservedAtUtc")
        sample_time = _parse_timestamp(row.get("timestampUtc"), f"navigation[{route_index}].timestampUtc")
        delay = (sample_time - event_time).total_seconds() * 1000
        if delay < 0 or delay > STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS:
            raise EvidenceValidationError(f"navigation transition {route_index} process snapshot is stale")
        route_values.append((values["currentWorkingSetBytes"], values["privateBytes"], values["handles"], values["threads"], values["peakWorkingSetBytes"]))
        if to_page == "my_workspace":
            if row.get("milestone") != "return-to-normal":
                raise EvidenceValidationError(f"navigation return milestone is missing at cycle {cycle}")
            return_snapshots.append({
                "cycle": cycle,
                "timestampUtc": row["timestampUtc"],
                "workingSetBytes": values["currentWorkingSetBytes"],
                "privateBytes": values["privateBytes"],
                "peakWorkingSetBytes": values["peakWorkingSetBytes"],
                "handles": values["handles"],
                "threads": values["threads"],
            })
        elif row.get("milestone") is not None:
            raise EvidenceValidationError(f"unexpected return milestone at transition {route_index}")
    if len(return_snapshots) != plateau_cycles:
        raise EvidenceValidationError(
            f"navigation plateau trace does not contain {plateau_cycles} return snapshots"
        )

    milestone_rows = scenario.get("returnToNormalMilestones")
    if not isinstance(milestone_rows, list) or len(milestone_rows) != plateau_cycles:
        raise EvidenceValidationError(
            f"manifest does not contain exactly {plateau_cycles} return-to-normal milestones"
        )
    for cycle, (milestone, snapshot, row) in enumerate(
        zip(milestone_rows, return_snapshots, [r for r in navigation_rows if r.get("toPageId") == "my_workspace"], strict=True),
        start=1,
    ):
        milestone = _require_object(milestone, f"return milestone {cycle}")
        if (
            milestone.get("cycle") != cycle
            or milestone.get("pageId") != "my_workspace"
            or milestone.get("eventLogIndex") != row.get("eventLogIndex")
            or milestone.get("timestampUtc") != snapshot["timestampUtc"]
            or milestone.get("currentWorkingSetBytes") != snapshot["workingSetBytes"]
        ):
            raise EvidenceValidationError(f"manifest return milestone differs from transition evidence at cycle {cycle}")

    if len(startup_ready_rows) != 1:
        raise EvidenceValidationError("diagnostic requires exactly one external startup-ready identity sample")
    startup_ready = _require_object(startup_ready_rows[0], "startup-ready sample")
    startup_ready_values = _validate_metric_process_identity(
        startup_ready, scenario_id, "startup-ready", pid, created_at_text, executable_path, app_sha
    )
    if startup_ready.get("phase") != "startup-ready" or startup_ready.get("sampleIndex") != 0:
        raise EvidenceValidationError("diagnostic startup-ready sample has an invalid checkpoint identity")

    if len(metric_rows) != 1 or metric_rows[0].get("phase") != "diagnostic-post-suspension":
        raise EvidenceValidationError("diagnostic requires exactly one delayed post-suspension process sample and no idle trace")
    post_suspension = _require_object(metric_rows[0], "post-suspension sample")
    post_values = _validate_metric_process_identity(
        post_suspension, scenario_id, "post-suspension", pid, created_at_text, executable_path, app_sha
    )
    post_time = _parse_timestamp(post_suspension.get("timestampUtc"), "post-suspension.timestampUtc")
    last_return_time = _parse_timestamp(return_snapshots[-1]["timestampUtc"], "last return timestamp")
    if (post_time - last_return_time).total_seconds() < 1.5:
        raise EvidenceValidationError("post-suspension sample was not delayed after the final return")
    diagnostic = _require_object(scenario.get("diagnosticNavigation"), "scenario.diagnosticNavigation")
    delayed_sample = _require_object(diagnostic.get("delayedPostSuspensionSample"), "delayed post-suspension sample")
    if delayed_sample != post_suspension:
        raise EvidenceValidationError("manifest post-suspension sample differs from the metrics trace")

    lifecycle_rows: list[dict[str, Any]] = []
    for event in startup_events:
        line = event.get("line")
        if event.get("scenarioId") != scenario_id or not isinstance(line, str) or not line.startswith("ClassMngrNext page lifecycle checkpoint="):
            continue
        try:
            payload = json.loads(line.split("checkpoint=", 1)[1])
        except (IndexError, json.JSONDecodeError) as error:
            raise EvidenceValidationError("calendar lifecycle diagnostic is malformed") from error
        if not isinstance(payload, dict) or payload.get("page_key") != "calendar":
            continue
        lifecycle_rows.append(payload)
    calendar_content_ready = [
        row for row in lifecycle_rows
        if row.get("state") == "activated"
        and isinstance(row.get("page"), dict)
        and isinstance(row["page"].get("calendar_runtime_metrics"), dict)
        and row["page"]["calendar_runtime_metrics"].get("observation") == "content-ready"
    ]
    calendar_suspended = [
        row for row in lifecycle_rows
        if row.get("state") == "suspended"
        and isinstance(row.get("page"), dict)
        and isinstance(row["page"].get("calendar_runtime_metrics"), dict)
        and row["page"]["calendar_runtime_metrics"].get("observation") == "post-suspension"
    ]
    if len(calendar_content_ready) != plateau_cycles or len(calendar_suspended) != plateau_cycles:
        raise EvidenceValidationError("Calendar lifecycle trace is missing a content-ready or post-suspension cycle record")
    for cycle, row in enumerate(calendar_suspended, start=1):
        runtime = _require_object(row["page"].get("calendar_runtime_metrics"), f"Calendar suspension {cycle} metrics")
        for field in ("calendar_view_count", "calendar_root_object_count", "live_heavy_session_count", "cache_loaded_range_count", "cache_retained_range_count"):
            if runtime.get(field) != 0:
                raise EvidenceValidationError(f"Calendar suspension {cycle} retained {field}")

    return {
        "schema": NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA,
        "taskId": P9_07_TASK_ID,
        "runId": manifest.get("runId"),
        "diagnosticOnly": True,
        "status": "valid-diagnostic",
        "validation": "not-evaluated",
        "budgetResult": "not-evaluated",
        "gateAcceptanceClaimed": False,
        "cycles": plateau_cycles,
        "transitions": len(navigation_rows),
        "candidateSourceSha256": source_sha,
        "cmakeCacheSha256": cache_sha,
        "packageSha256": package_sha,
        "startupReadyMetrics": {
            "workingSetBytes": startup_ready_values["currentWorkingSetBytes"],
            "privateBytes": startup_ready_values["privateBytes"],
            "handles": startup_ready_values["handles"],
            "threads": startup_ready_values["threads"],
        },
        "returnSnapshots": return_snapshots,
        "postSuspensionSnapshot": {
            "timestampUtc": post_suspension["timestampUtc"],
            "workingSetBytes": post_values["currentWorkingSetBytes"],
            "privateBytes": post_values["privateBytes"],
            "peakWorkingSetBytes": post_values["peakWorkingSetBytes"],
            "handles": post_values["handles"],
            "threads": post_values["threads"],
        },
        "calendarLifecycle": {
            "contentReadyCount": len(calendar_content_ready),
            "postSuspensionCount": len(calendar_suspended),
            "zeroLiveViewRootAndSessionCount": len(calendar_suspended),
        },
        "scenario": scenario,
    }


def _validate_navigation_scenario(
    scenario: dict[str, Any],
    startup_events: list[dict[str, Any]],
    startup_ready_metrics: list[dict[str, Any]],
    navigation_metrics: list[dict[str, Any]],
    pid: int,
    created_at_text: str,
    executable_path: str,
    application_sha: str,
    diagnostic_at: datetime,
    ready_log_index: int,
    workspace_log_index: int,
) -> dict[str, Any]:
    scenario_id = str(scenario["id"])
    catalog_logs = [
        (index, event)
        for index, event in enumerate(startup_events)
        if event.get("scenarioId") == scenario_id
        and isinstance(event.get("line"), str)
        and event["line"].startswith("ClassMngrNext document-catalog ")
    ]
    if len(catalog_logs) != 1:
        raise EvidenceValidationError(
            f"{scenario_id} must report exactly one startup document catalog projection"
        )
    catalog_log_index, catalog_event = catalog_logs[0]
    catalog_match = DOCUMENT_CATALOG_RE.fullmatch(str(catalog_event["line"]))
    if catalog_match is None:
        raise EvidenceValidationError(f"{scenario_id} startup document catalog record is malformed")
    document_count = int(catalog_match.group("documentCount"))
    shell_logs = [
        index
        for index, event in enumerate(startup_events)
        if event.get("scenarioId") == scenario_id
        and isinstance(event.get("line"), str)
        and "ClassMngrNext startup stage=shell-created " in event["line"]
    ]
    if (
        catalog_match.group("outcome") != "completed"
        or document_count <= 0
        or int(catalog_match.group("bodyCount")) != 0
        or len(shell_logs) != 1
        or catalog_log_index <= workspace_log_index
        or catalog_log_index >= shell_logs[0]
        or catalog_log_index >= ready_log_index
    ):
        raise EvidenceValidationError(
            f"{scenario_id} startup document catalog did not report metadata only with zero loaded PDF bodies between workspace-opened and shell-created"
        )
    catalog_at = _parse_timestamp(
        catalog_event.get("observedAtUtc"), f"{scenario_id}.documentCatalog.observedAtUtc"
    )
    if catalog_at > diagnostic_at:
        raise EvidenceValidationError(f"{scenario_id} startup document catalog followed startup-ready")

    document_body_requests: list[str] = []
    for event in startup_events:
        if event.get("scenarioId") != scenario_id or not isinstance(event.get("line"), str):
            continue
        resource_match = RESOURCE_CHECKPOINT_RE.fullmatch(event["line"])
        if resource_match is None:
            continue
        try:
            checkpoint = json.loads(resource_match.group(1))
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(
                f"{scenario_id} resource diagnostic is malformed during navigation evidence"
            ) from error
        resource_id = checkpoint.get("resource_id")
        catalog_resource = checkpoint.get("catalog_resource")
        resource_kind = catalog_resource.get("kind") if isinstance(catalog_resource, dict) else None
        if (
            isinstance(resource_id, str) and resource_id.startswith("document.body:")
        ) or resource_kind == "document_body":
            document_body_requests.append(str(resource_id))
    if document_body_requests:
        raise EvidenceValidationError(
            f"{scenario_id} navigation route loaded or requested a PDF body resource"
        )

    startup_rows = [
        row for row in startup_ready_metrics if row.get("scenarioId") == scenario_id
    ]
    if len(startup_rows) != 1:
        raise EvidenceValidationError(
            f"{scenario_id} must have exactly one external startup-ready process sample"
        )
    startup_row = startup_rows[0]
    if startup_row.get("sampleIndex") != 0 or startup_row.get("phase") != "startup-ready":
        raise EvidenceValidationError(f"{scenario_id} external startup-ready sample identity is invalid")
    startup_metrics = _validate_metric_process_identity(
        startup_row, scenario_id, "startup-ready external sample", pid,
        created_at_text, executable_path, application_sha
    )
    startup_sample_at = _parse_timestamp(
        startup_row.get("timestampUtc"), f"{scenario_id}.startupReadyExternal.timestampUtc"
    )
    if startup_sample_at < diagnostic_at or (startup_sample_at - diagnostic_at).total_seconds() > 5:
        raise EvidenceValidationError(
            f"{scenario_id} external startup-ready sample is not adjacent to startup-ready"
        )

    expected_route = NAVIGATION_ROUTE * NAVIGATION_CYCLES
    route_logs = [
        (index, event)
        for index, event in enumerate(startup_events)
        if event.get("scenarioId") == scenario_id
        and isinstance(event.get("line"), str)
        and event["line"].startswith("ClassMngrNext navigation event=page-route ")
        and index > ready_log_index
    ]
    if len(route_logs) != EXPECTED_NAVIGATION_TRANSITIONS:
        raise EvidenceValidationError(
            f"{scenario_id} must complete exactly {EXPECTED_NAVIGATION_TRANSITIONS} normal navigation transitions"
        )
    route_rows = [row for row in navigation_metrics if row.get("scenarioId") == scenario_id]
    if len(route_rows) != EXPECTED_NAVIGATION_TRANSITIONS:
        raise EvidenceValidationError(
            f"{scenario_id} navigation process samples are missing or duplicated"
        )

    route_summaries: list[dict[str, Any]] = []
    route_working_sets: list[int] = []
    route_violations: list[dict[str, Any]] = []
    prior_event_at: datetime | None = None
    prior_sample_at: datetime | None = None
    normal_milestones: list[dict[str, Any]] = []
    for route_index, ((log_index, event), row, (from_page, to_page)) in enumerate(
        zip(route_logs, route_rows, expected_route, strict=True)
    ):
        cycle = route_index // len(NAVIGATION_ROUTE) + 1
        step = route_index % len(NAVIGATION_ROUTE) + 1
        match = NAVIGATION_ROUTE_RE.fullmatch(str(event["line"]))
        if match is None:
            raise EvidenceValidationError(f"{scenario_id} navigation route event is malformed")
        if (
            match.group("outcome") != "completed"
            or match.group("cause") != "selection"
            or match.group("from") != from_page
            or match.group("to") != to_page
        ):
            raise EvidenceValidationError(
                f"{scenario_id} navigation route event failed or is out of order at transition {route_index}"
            )
        if int(match.group("documentCount")) != document_count or int(match.group("bodyCount")) != 0:
            raise EvidenceValidationError(
                f"{scenario_id} document catalog count changed or a PDF body was loaded during navigation"
            )
        event_at_text = event.get("observedAtUtc")
        event_at = _parse_timestamp(event_at_text, f"{scenario_id}.navigation.eventObservedAtUtc")
        if event_at <= diagnostic_at or event_at < startup_sample_at or log_index <= ready_log_index:
            raise EvidenceValidationError(f"{scenario_id} navigation event did not follow startup-ready")
        if prior_event_at is not None and event_at <= prior_event_at:
            raise EvidenceValidationError(f"{scenario_id} navigation route events are duplicated or out of order")
        prior_event_at = event_at
        if (
            row.get("sampleIndex") != route_index
            or row.get("phase") != "navigation"
            or row.get("routeIndex") != route_index
            or row.get("cycle") != cycle
            or row.get("routeStep") != step
            or row.get("fromPageId") != from_page
            or row.get("toPageId") != to_page
            or row.get("eventLogIndex") != log_index
            or row.get("eventObservedAtUtc") != event_at_text
            or row.get("stream") != event.get("stream")
            or row.get("rawLine") != event.get("line")
        ):
            raise EvidenceValidationError(
                f"{scenario_id} navigation sample does not match its ordered app route event"
            )
        if (
            row.get("selectionMethod") not in NAVIGATION_SELECTION_METHODS
            or (
                row.get("selectionMethod") == "uia-verified-pointer-click"
                and row.get("uiaPatternName") != "PointerClick"
            )
            or (
                row.get("selectionMethod") == "uia-verified-selection-invoke"
                and row.get("uiaPatternName")
                not in ("InvokePattern", "SelectionItemPattern")
            )
            or (
                row.get("selectionMethod") == "uia-verified-window-message-click"
                and row.get("uiaPatternName") != "WindowMessageClick"
            )
            or row.get("selectionVerified") is not True
            or row.get("targetPageId") != to_page
            or row.get("uiaControlName") != "ClassMngr Next navigation"
            or row.get("uiaControlAutomationId") not in EXPECTED_NAVIGATION_CONTROL_AUTOMATION_IDS
            or row.get("uiaWindowTitle") != "ClassMngr Next"
            or isinstance(row.get("uiaWindowHandle"), bool)
            or not isinstance(row.get("uiaWindowHandle"), int)
            or row.get("uiaWindowHandle", 0) <= 0
            or row.get("uiaWindowProcessId") != pid
            or row.get("uiaItemName") != {
                "schedule": "Schedule",
                "calendar": "Calendar",
                "my_classes": "My Classes",
                "my_workspace": "My Workspace",
            }[to_page]
        ):
            raise EvidenceValidationError(
                f"{scenario_id} navigation route was not verified through the packaged sidebar selection"
            )
        if row.get("selectionMethod") == "uia-verified-window-message-click":
            activation = row.get("windowMessageActivation")
            rectangle = row.get("uiaItemRectangle")
            expected_item_name = {
                "schedule": "Schedule",
                "calendar": "Calendar",
                "my_classes": "My Classes",
                "my_workspace": "My Workspace",
            }[to_page]
            sequence = [
                "WaitForInputIdle",
                "ScreenToClient",
                "GetClientRect",
                "WM_MOUSEMOVE",
                "WM_LBUTTONDOWN",
                "WM_LBUTTONUP",
            ]
            if (
                not isinstance(activation, dict)
                or activation.get("sequence") != sequence
                or activation.get("windowHandle") != row.get("uiaWindowHandle")
                or activation.get("expectedProcessId") != pid
                or activation.get("windowProcessId") != pid
                or activation.get("waitForInputIdleSucceeded") is not True
                or activation.get("mouseMoveSent") is not True
                or activation.get("leftButtonDownSent") is not True
                or activation.get("leftButtonUpSent") is not True
                or type(activation.get("messagesSent")) is not int
                or activation.get("messagesSent") != 3
                or row.get("uiaItemName") != expected_item_name
                or not isinstance(rectangle, dict)
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} window-message navigation activation identity or event sequence is invalid"
                )
            coordinate_fields = ("screenX", "screenY", "clientX", "clientY")
            if any(type(activation.get(field)) is not int for field in coordinate_fields):
                raise EvidenceValidationError(
                    f"{scenario_id} window-message navigation activation coordinates are invalid"
                )
            try:
                left = float(rectangle["x"])
                top = float(rectangle["y"])
                width = float(rectangle["width"])
                height = float(rectangle["height"])
            except (KeyError, TypeError, ValueError) as exc:
                raise EvidenceValidationError(
                    f"{scenario_id} UIA row bounds are malformed for window-message navigation"
                ) from exc
            if (
                width <= 0
                or height <= 0
                or not left <= activation["screenX"] < left + width
                or not top <= activation["screenY"] < top + height
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} window-message click point did not fall inside the exact UIA row"
                )
        values = _validate_metric_process_identity(
            row, scenario_id, f"navigation transition {route_index}", pid,
            created_at_text, executable_path, application_sha
        )
        sample_at = _parse_timestamp(
            row.get("timestampUtc"), f"{scenario_id}.navigation.timestampUtc"
        )
        delay = (sample_at - event_at).total_seconds() * 1000
        recorded_delay = row.get("sampleDelayMilliseconds")
        if (
            delay < 0
            or delay > STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS
            or isinstance(recorded_delay, bool)
            or not isinstance(recorded_delay, (int, float))
            or abs(recorded_delay - delay) > 100
        ):
            raise EvidenceValidationError(
                f"{scenario_id} navigation transition sample is stale or malformed"
            )
        if prior_sample_at is not None and sample_at <= prior_sample_at:
            raise EvidenceValidationError(f"{scenario_id} navigation process samples are out of order")
        prior_sample_at = sample_at
        route_working_sets.append(values["currentWorkingSetBytes"])
        route_summary = {
            "routeIndex": route_index,
            "cycle": cycle,
            "routeStep": step,
            "fromPageId": from_page,
            "toPageId": to_page,
            "eventLogIndex": log_index,
            "eventObservedAtUtc": event_at_text,
            "timestampUtc": row.get("timestampUtc"),
            "sampleDelayMilliseconds": recorded_delay,
            **values,
        }
        route_summaries.append(route_summary)
        if values["currentWorkingSetBytes"] >= THRESHOLD_BYTES:
            route_violations.append(
                {
                    "point": f"navigation-cycle-{cycle}-step-{step}-{to_page}",
                    "workingSetBytes": values["currentWorkingSetBytes"],
                }
            )
        if to_page == "my_workspace":
            normal_milestones.append(
                {
                    "cycle": cycle,
                    "pageId": to_page,
                    "eventLogIndex": log_index,
                    "timestampUtc": row.get("timestampUtc"),
                    "currentWorkingSetBytes": values["currentWorkingSetBytes"],
                }
            )

    recorded_milestones = scenario.get("returnToNormalMilestones")
    if not isinstance(recorded_milestones, list) or len(recorded_milestones) != NAVIGATION_CYCLES:
        raise EvidenceValidationError(f"{scenario_id} return-to-normal milestones are incomplete")
    for expected, recorded in zip(normal_milestones, recorded_milestones, strict=True):
        recorded = _require_object(recorded, f"{scenario_id}.returnToNormalMilestones")
        if any(recorded.get(key) != value for key, value in expected.items()):
            raise EvidenceValidationError(
                f"{scenario_id} return-to-normal milestone differs from the completed route"
            )

    return {
        "documentCount": document_count,
        "catalogEventLogIndex": catalog_log_index,
        "startupReadyExternalMetrics": startup_metrics,
        "startupReadyExternalTimestampUtc": startup_row.get("timestampUtc"),
        "navigationTransitions": route_summaries,
        "returnToNormalMilestones": normal_milestones,
        "returnToNormalWorkingSetBytes": [
            item["currentWorkingSetBytes"] for item in normal_milestones
        ],
        "cycleReturnWorkingSetDeltas": [
            normal_milestones[index]["currentWorkingSetBytes"]
            - normal_milestones[index - 1]["currentWorkingSetBytes"]
            for index in range(1, len(normal_milestones))
        ],
        "navigationViolations": route_violations,
    }


def validate_documents(
    manifest: dict[str, Any],
    metrics: list[dict[str, Any]],
    startup_events: list[dict[str, Any]],
    package_files: list[dict[str, Any]],
    source_files: list[dict[str, Any]],
    startup_boundary_metrics: list[dict[str, Any]] | None = None,
    build_log: bytes | None = None,
    startup_ready_metrics: list[dict[str, Any]] | None = None,
    navigation_metrics: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Validate identity and collection completeness, then evaluate the budget."""
    is_navigation_gate = manifest.get("taskId") == P9_07_TASK_ID
    expected_manifest_schema = P9_07_MANIFEST_SCHEMA if is_navigation_gate else MANIFEST_SCHEMA
    expected_task_id = P9_07_TASK_ID if is_navigation_gate else TASK_ID
    if manifest.get("schema") != expected_manifest_schema:
        raise EvidenceValidationError("unexpected manifest schema")
    if manifest.get("taskId") != expected_task_id:
        raise EvidenceValidationError("unexpected taskId")
    if is_navigation_gate and (
        startup_ready_metrics is None or navigation_metrics is None
    ):
        raise EvidenceValidationError("navigation or startup-ready process metric inventory is missing")
    if not is_navigation_gate and (startup_ready_metrics is not None or navigation_metrics is not None):
        raise EvidenceValidationError("P9-06 evidence unexpectedly contains P9-07 navigation metrics")
    if manifest.get("status") != "completed":
        raise EvidenceValidationError("run status is not completed")

    revision = manifest.get("revision")
    if not isinstance(revision, str) or not REVISION_RE.fullmatch(revision):
        raise EvidenceValidationError("revision must be a full Git object ID")
    started = _parse_timestamp(manifest.get("startedAtUtc"), "startedAtUtc")
    completed = _parse_timestamp(manifest.get("completedAtUtc"), "completedAtUtc")
    if completed < started:
        raise EvidenceValidationError("completedAtUtc precedes startedAtUtc")

    candidate_source = _require_object(manifest.get("candidateSource"), "candidateSource")
    source_sha = _require_sha(candidate_source.get("sha256"), "candidateSource.sha256")
    if candidate_source.get("revision") != revision:
        raise EvidenceValidationError("candidate source revision differs from run revision")
    included_roots = candidate_source.get("includedRoots")
    if included_roots != [
        "CMakeLists.txt",
        "CMakePresets.json",
        "cmake",
        "docs/qt-rewrite/phase-4-resource-inventory.json",
        "licenses",
        "resources",
        "scripts/phase4",
        "scripts/phase9",
        "src",
        "tests",
        "third_party",
    ]:
        raise EvidenceValidationError("candidate source includedRoots is missing")
    if candidate_source.get("inventoryFile") != "source-files.json":
        raise EvidenceValidationError("candidate source inventory file identity is unexpected")
    if (
        isinstance(candidate_source.get("fileCount"), bool)
        or not isinstance(candidate_source.get("fileCount"), int)
        or candidate_source.get("fileCount") != len(source_files)
        or not source_files
    ):
        raise EvidenceValidationError("candidate source file inventory is incomplete")
    if _canonical_source_sha(source_files, included_roots) != source_sha:
        raise EvidenceValidationError("candidate source file inventory does not match candidate source SHA")
    source_paths = {
        str(entry.get("path", "")).replace("\\", "/")
        for entry in source_files
        if isinstance(entry, dict)
    }
    missing_source_files = REQUIRED_SOURCE_FILES - source_paths
    if missing_source_files:
        raise EvidenceValidationError(
            "candidate source inventory omits required files: "
            + ", ".join(sorted(missing_source_files))
        )
    if candidate_source.get("sha256AtRunEnd") != source_sha or candidate_source.get("stableDuringRun") is not True:
        raise EvidenceValidationError("candidate source changed during the evidence run")

    configuration = _require_object(manifest.get("configuration"), "configuration")
    if (
        configuration.get("preset") != "windows-x64-release"
        or configuration.get("buildType") != "Release"
        or configuration.get("architecture") != "x64"
        or configuration.get("releasePackage") is not True
    ):
        raise EvidenceValidationError("configuration is not packaged Windows x64 Release")
    cache_path = configuration.get("cmakeCachePath")
    if not isinstance(cache_path, str) or not ntpath.isabs(cache_path):
        raise EvidenceValidationError("CMakeCache path must be absolute")
    if ntpath.basename(cache_path).casefold() != "cmakecache.txt":
        raise EvidenceValidationError("configuration path is not CMakeCache.txt")
    cache_sha = _require_sha(configuration.get("cmakeCacheSha256"), "configuration.cmakeCacheSha256")
    if (
        configuration.get("cmakeCacheSha256AtRunEnd") != cache_sha
        or configuration.get("cmakeCacheStableDuringRun") is not True
    ):
        raise EvidenceValidationError("CMakeCache.txt changed during the evidence run")
    generator = configuration.get("generator")
    if not isinstance(generator, str) or not generator.startswith("Visual Studio"):
        raise EvidenceValidationError("configuration generator is not MSVC Visual Studio")
    release_compiler = _require_object(configuration.get("releaseCompiler"), "configuration.releaseCompiler")
    if (
        release_compiler.get("compiler") != "MSVC"
        or release_compiler.get("generator") != generator
        or release_compiler.get("configuration") != "Release"
        or release_compiler.get("platform") != "x64"
    ):
        raise EvidenceValidationError("Release compiler configuration is not MSVC Release x64")
    c_flags = _validate_msvc_release_flags(
        release_compiler.get("cFlagsRelease"), "configuration.releaseCompiler.cFlagsRelease"
    )
    cxx_flags = _validate_msvc_release_flags(
        release_compiler.get("cxxFlagsRelease"), "configuration.releaseCompiler.cxxFlagsRelease"
    )
    project_path = release_compiler.get("projectPath")
    if (
        not isinstance(project_path, str)
        or not ntpath.isabs(project_path)
        or ntpath.basename(project_path).casefold() != "classmngrnext.vcxproj"
    ):
        raise EvidenceValidationError("generated ClassMngrNext project path is invalid")
    _require_sha(release_compiler.get("projectSha256"), "configuration.releaseCompiler.projectSha256")
    if release_compiler.get("projectOptimizationSetting") not in ("MaxSpeed", "MinSpace"):
        raise EvidenceValidationError("generated Release project does not enable optimization")

    package_build = _require_object(configuration.get("packageBuild"), "configuration.packageBuild")
    if package_build.get("targets") != list(REQUIRED_PACKAGE_BUILD_TARGETS):
        raise EvidenceValidationError("required package and fixture build targets are missing")
    cmake_path = package_build.get("cmakeExecutablePath")
    if (
        not isinstance(cmake_path, str)
        or not ntpath.isabs(cmake_path)
        or ntpath.basename(cmake_path).casefold() != "cmake.exe"
    ):
        raise EvidenceValidationError("package prerequisite CMake executable path is invalid")
    _require_sha(package_build.get("cmakeExecutableSha256"), "configuration.packageBuild.cmakeExecutableSha256")
    command = package_build.get("command")
    if (
        not isinstance(command, list)
        or len(command) != 9
        or not _same_windows_path(command[0], cmake_path)
        or command[1] != "--build"
        or command[3:6] != ["--config", "Release", "--target"]
        or command[6:] != list(REQUIRED_PACKAGE_BUILD_TARGETS)
        or not _same_windows_path(command[2], ntpath.dirname(cache_path))
    ):
        raise EvidenceValidationError("package prerequisite build command is incomplete")
    log_path = package_build.get("logPath")
    if not isinstance(log_path, str) or not ntpath.isabs(log_path) or ntpath.basename(log_path).casefold() != "cmake-build.log":
        raise EvidenceValidationError("package prerequisite build log path is invalid")
    log_sha = _require_sha(package_build.get("logSha256"), "configuration.packageBuild.logSha256")
    if build_log is not None and hashlib.sha256(build_log).hexdigest() != log_sha:
        raise EvidenceValidationError("package prerequisite build log hash does not match")
    if package_build.get("exitCode") != 0 or package_build.get("succeeded") is not True:
        raise EvidenceValidationError("package prerequisites did not build successfully")
    build_started = _parse_timestamp(package_build.get("startedAtUtc"), "packageBuild.startedAtUtc")
    build_completed = _parse_timestamp(package_build.get("completedAtUtc"), "packageBuild.completedAtUtc")
    if build_completed < build_started or build_started < started or build_completed > completed:
        raise EvidenceValidationError("package prerequisite build timestamps are invalid")
    qt_installation = _require_object(manifest.get("qtInstallation"), "qtInstallation")
    qt_prefix = qt_installation.get("prefix")
    qt6_directory = qt_installation.get("qt6Directory")
    if not isinstance(qt_prefix, str) or not ntpath.isabs(qt_prefix):
        raise EvidenceValidationError("Qt installation prefix must be absolute")
    if not isinstance(qt6_directory, str) or not ntpath.isabs(qt6_directory):
        raise EvidenceValidationError("Qt6_DIR must be absolute")
    if not _same_windows_path(
        qt6_directory, ntpath.join(qt_prefix, "lib", "cmake", "Qt6")
    ):
        raise EvidenceValidationError("Qt6_DIR and Qt installation prefix do not match")
    qt_version = qt_installation.get("version")
    if not isinstance(qt_version, str) or not re.fullmatch(r"\d+\.\d+\.\d+", qt_version):
        raise EvidenceValidationError("Qt version is missing or malformed")
    qt_installation_id = _require_sha(qt_installation.get("id"), "qtInstallation.id")
    expected_qt_installation_id = hashlib.sha256(
        f"{qt_prefix}\n{qt_version}\n{qt6_directory}\n".encode("utf-8")
    ).hexdigest()
    if qt_installation_id != expected_qt_installation_id:
        raise EvidenceValidationError("Qt installation identity does not match its prefix and version")
    qt_bin = ntpath.join(qt_prefix, "bin")
    core_dll_path = qt_installation.get("coreDllPath")
    if not _same_windows_path(core_dll_path, ntpath.join(qt_bin, "Qt6Core.dll")):
        raise EvidenceValidationError("Qt6Core.dll does not come from the recorded Qt installation")
    _require_sha(qt_installation.get("coreDllSha256"), "qtInstallation.coreDllSha256")
    windeployqt_path = qt_installation.get("windeployqtPath")
    if not _same_windows_path(windeployqt_path, ntpath.join(qt_bin, "windeployqt.exe")):
        raise EvidenceValidationError("windeployqt does not come from the recorded Qt installation")
    _require_sha(qt_installation.get("windeployqtSha256"), "qtInstallation.windeployqtSha256")
    if qt_installation.get("windeployqtVersion") != qt_version:
        raise EvidenceValidationError("windeployqt version differs from the recorded Qt installation")
    qt_fixture_generators = qt_installation.get("fixtureGenerators")
    if not isinstance(qt_fixture_generators, list) or len(qt_fixture_generators) != 2:
        raise EvidenceValidationError("Qt fixture generator runtime verification is incomplete")
    qt_generators_by_id: dict[str, dict[str, Any]] = {}
    for index, raw_generator in enumerate(qt_fixture_generators):
        generator = _require_object(raw_generator, f"qtInstallation.fixtureGenerators[{index}]")
        generator_id = generator.get("id")
        if not isinstance(generator_id, str) or generator_id in qt_generators_by_id:
            raise EvidenceValidationError("Qt fixture generator IDs are missing or duplicated")
        if generator.get("verifiedSameQtInstallation") is not True:
            raise EvidenceValidationError(f"{generator_id} runtime does not match Qt6_DIR")
        generator_path = generator.get("executablePath")
        if (
            not isinstance(generator_path, str)
            or not ntpath.isabs(generator_path)
            or ntpath.splitext(generator_path)[1].casefold() != ".exe"
        ):
            raise EvidenceValidationError(f"{generator_id} executable identity is invalid")
        if not _same_windows_path(generator.get("runtimeSearchPathPrefix"), qt_bin):
            raise EvidenceValidationError(f"{generator_id} runtime search path differs from Qt6_DIR")
        _require_sha(generator.get("executableSha256"), f"{generator_id}.executableSha256")
        colocated_dlls = generator.get("colocatedQtDlls")
        if not isinstance(colocated_dlls, list):
            raise EvidenceValidationError(f"{generator_id} colocated Qt DLL record is malformed")
        for dll_index, raw_dll in enumerate(colocated_dlls):
            dll = _require_object(raw_dll, f"{generator_id}.colocatedQtDlls[{dll_index}]")
            dll_name = dll.get("name")
            if not isinstance(dll_name, str) or ntpath.basename(dll_name) != dll_name or not dll_name.casefold().startswith("qt6"):
                raise EvidenceValidationError(f"{generator_id} has an invalid colocated Qt DLL name")
            if not isinstance(dll.get("colocatedPath"), str) or not ntpath.isabs(dll["colocatedPath"]):
                raise EvidenceValidationError(f"{generator_id} colocated Qt DLL path is invalid")
            if not _same_windows_path(dll.get("qtPrefixPath"), ntpath.join(qt_bin, dll_name)):
                raise EvidenceValidationError(f"{generator_id} Qt DLL source differs from Qt6_DIR")
            colocated_sha = _require_sha(dll.get("colocatedSha256"), f"{generator_id}.colocatedSha256")
            prefix_sha = _require_sha(dll.get("qtPrefixSha256"), f"{generator_id}.qtPrefixSha256")
            if colocated_sha != prefix_sha:
                raise EvidenceValidationError(f"{generator_id} colocated Qt DLL differs from Qt6_DIR")
        qt_generators_by_id[generator_id] = generator
    if set(qt_generators_by_id) != set(EXPECTED_FIXTURE_GENERATORS.values()):
        raise EvidenceValidationError("Qt fixture generator identity is unexpected")
    if manifest.get("thresholdBytes") != THRESHOLD_BYTES or manifest.get("comparison") != "strictly-less-than":
        raise EvidenceValidationError("manifest working-set threshold contract is incorrect")
    if manifest.get("idleSeconds") != IDLE_SECONDS or manifest.get("sampleIntervalSeconds") != 1:
        raise EvidenceValidationError("manifest idle sampling contract is incorrect")
    advisory = manifest.get("advisoryScenarios")
    if not isinstance(advisory, list) or not any(
        isinstance(item, dict)
        and item.get("id") == "fresh-initial-setup"
        and item.get("budgetScope") == "advisory"
        for item in advisory
    ):
        raise EvidenceValidationError("Fresh Initial Setup must remain advisory")
    operating_system = _require_object(manifest.get("os"), "os")
    if operating_system.get("platform") != "Windows" or operating_system.get(
        "architecture"
    ) != "x64":
        raise EvidenceValidationError("run did not execute on Windows x64")

    application = _require_object(manifest.get("application"), "application")
    executable_path = application.get("canonicalPath")
    if not isinstance(executable_path, str) or not ntpath.isabs(executable_path):
        raise EvidenceValidationError("application canonicalPath must be absolute")
    if ntpath.basename(executable_path).casefold() != "classmngrnext.exe":
        raise EvidenceValidationError("application path is not the staged ClassMngrNext.exe")
    if application.get("architecture") != "x64":
        raise EvidenceValidationError("staged application architecture is not x64")
    application_sha = _require_sha(application.get("sha256"), "application.sha256")
    if application.get("candidateSourceSha256") != source_sha:
        raise EvidenceValidationError("application is not bound to candidate source SHA")
    release_path = application.get("releaseExecutablePath")
    if not isinstance(release_path, str) or not ntpath.isabs(release_path):
        raise EvidenceValidationError("source Release executable path must be absolute")
    if ntpath.basename(release_path).casefold() != "classmngr.exe":
        raise EvidenceValidationError("source Release executable is not packaged ClassMngr.exe")
    if _require_sha(application.get("releaseExecutableSha256"), "application.releaseExecutableSha256") != application_sha:
        raise EvidenceValidationError("staged app image differs from source Release executable")

    package = _require_object(manifest.get("package"), "package")
    if package.get("id") != "classmngr-next-windows-x64-release-stage-v1":
        raise EvidenceValidationError("package identity is unexpected")
    if package.get("candidateSourceSha256") != source_sha:
        raise EvidenceValidationError("package is not bound to candidate source SHA")
    if package.get("applicationSha256") != application_sha:
        raise EvidenceValidationError("package application hash differs from process image")
    if package.get("sourceReleasePackage") is not True:
        raise EvidenceValidationError("package is not bound to the local Release package build")
    if package.get("inventoryFile") != "package-files.json":
        raise EvidenceValidationError("package inventory file identity is unexpected")
    if (
        isinstance(package.get("fileCount"), bool)
        or not isinstance(package.get("fileCount"), int)
        or package.get("fileCount") != len(package_files)
        or not package_files
    ):
        raise EvidenceValidationError("package file inventory is incomplete")
    package_sha = _require_sha(package.get("sha256"), "package.sha256")
    if _canonical_package_sha(package_files) != package_sha:
        raise EvidenceValidationError("package file inventory does not match package SHA")
    app_relative_path = package.get("applicationRelativePath")
    matching_exes = [
        entry
        for entry in package_files
        if isinstance(entry.get("path"), str)
        and entry["path"].replace("/", "\\").casefold()
        == str(app_relative_path).replace("/", "\\").casefold()
    ]
    if len(matching_exes) != 1 or matching_exes[0].get("sha256") != application_sha:
        raise EvidenceValidationError("package inventory does not contain the measured app image")
    file_paths = [entry.get("path") for entry in package_files]
    if len(file_paths) != len(set(file_paths)):
        raise EvidenceValidationError("package inventory contains duplicate paths")
    if manifest.get("requiredInterFontResourceIds") != list(EXPECTED_INTER_FONT_RESOURCE_IDS):
        raise EvidenceValidationError("manifest Inter font resource contract is missing or malformed")
    package_by_path = {
        str(entry["path"]).replace("\\", "/").casefold(): entry
        for entry in package_files
        if isinstance(entry, dict) and isinstance(entry.get("path"), str)
    }
    source_by_path = {
        str(entry["path"]).replace("\\", "/").casefold(): entry
        for entry in source_files
        if isinstance(entry, dict) and isinstance(entry.get("path"), str)
    }
    expected_inter_package_paths = {
        f"resources/{entry['packageRelativePath']}".casefold()
        for entry in EXPECTED_INTER_FONT_CATALOG_ENTRIES
    }
    actual_inter_package_paths = {
        path
        for path in package_by_path
        if re.fullmatch(r"resources/assets/fonts/inter[^/]*\.(?:ttf|ttc)", path)
    }
    if actual_inter_package_paths != expected_inter_package_paths:
        raise EvidenceValidationError(
            "package must contain exactly the six catalogued Inter TTF files and no TTC alias"
        )
    resource_catalog_path = package.get("resourceCatalogPath")
    if resource_catalog_path != "resources/resource-catalog.json":
        raise EvidenceValidationError("package resource catalog path is unexpected")
    resource_catalog_file = package_by_path.get(resource_catalog_path.casefold())
    if resource_catalog_file is None or _require_sha(
        package.get("resourceCatalogSha256"), "package.resourceCatalogSha256"
    ) != resource_catalog_file.get("sha256"):
        raise EvidenceValidationError("package resource catalog hash is missing or mismatched")
    catalog_entries = package.get("interFontCatalogEntries")
    if not isinstance(catalog_entries, list) or len(catalog_entries) != len(
        EXPECTED_INTER_FONT_CATALOG_ENTRIES
    ):
        raise EvidenceValidationError("package Inter font catalog entries are incomplete")
    for index, (expected, raw_entry) in enumerate(
        zip(EXPECTED_INTER_FONT_CATALOG_ENTRIES, catalog_entries, strict=True)
    ):
        entry = _require_object(raw_entry, f"package.interFontCatalogEntries[{index}]")
        if any(entry.get(name) != expected[name] for name in ("resourceId", "sourcePath", "packageRelativePath")):
            raise EvidenceValidationError("package Inter font catalog entries are missing, unexpected, or out of order")
        source_path = str(expected["sourcePath"]).casefold()
        package_path = f"resources/{expected['packageRelativePath']}".casefold()
        source_entry = source_by_path.get(source_path)
        packaged_entry = package_by_path.get(package_path)
        if source_entry is None or packaged_entry is None:
            raise EvidenceValidationError(f"catalogued Inter font is absent from source or package: {expected['resourceId']}")
        font_size = _require_positive_int(entry.get("sizeBytes"), f"Inter catalog entry {index}.sizeBytes")
        font_sha = _require_sha(entry.get("sha256"), f"Inter catalog entry {index}.sha256")
        if (
            source_entry.get("sizeBytes") != font_size
            or source_entry.get("sha256") != font_sha
            or packaged_entry.get("sizeBytes") != font_size
            or packaged_entry.get("sha256") != font_sha
        ):
            raise EvidenceValidationError(f"catalogued Inter source/package identity differs: {expected['resourceId']}")

    inter_license = _require_object(package.get("interFontLicense"), "package.interFontLicense")
    if (
        inter_license.get("sourcePath") != INTER_FONT_LICENSE_SOURCE_PATH
        or inter_license.get("packagePath") != INTER_FONT_LICENSE_PACKAGE_PATH
    ):
        raise EvidenceValidationError("package Inter OFL license path is missing or unexpected")
    source_license = source_by_path.get(INTER_FONT_LICENSE_SOURCE_PATH.casefold())
    packaged_license = package_by_path.get(INTER_FONT_LICENSE_PACKAGE_PATH.casefold())
    if source_license is None or packaged_license is None:
        raise EvidenceValidationError("Inter OFL license is absent from source or package inventory")
    inter_license_size = _require_positive_int(
        inter_license.get("sizeBytes"), "package.interFontLicense.sizeBytes"
    )
    inter_license_sha = _require_sha(
        inter_license.get("sha256"), "package.interFontLicense.sha256"
    )
    if (
        source_license.get("sizeBytes") != inter_license_size
        or source_license.get("sha256") != inter_license_sha
        or packaged_license.get("sizeBytes") != inter_license_size
        or packaged_license.get("sha256") != inter_license_sha
    ):
        raise EvidenceValidationError(
            "packaged Inter OFL license hash or size differs from its exact source identity"
        )

    if len(startup_events) < 1:
        raise EvidenceValidationError("startup log is empty")
    for event in startup_events:
        if not isinstance(event.get("stream"), str) or event.get("stream") not in (
            "stdout",
            "stderr",
        ):
            raise EvidenceValidationError("startup log stream is invalid")
        if not isinstance(event.get("line"), str):
            raise EvidenceValidationError("startup log line is missing")
        _parse_timestamp(event.get("observedAtUtc"), "startup log observedAtUtc")

    boundary_manifest = _require_object(
        manifest.get("startupBoundaryMetrics"), "startupBoundaryMetrics"
    )
    expected_boundary_ids = [
        boundary["eventId"] for boundary in EXPECTED_STARTUP_BOUNDARIES
    ]
    if (
        boundary_manifest.get("schema") != STARTUP_BOUNDARY_METRICS_SCHEMA
        or boundary_manifest.get("inventoryFile") != "startup-boundary-metrics.jsonl"
        or boundary_manifest.get("expectedEventIds") != expected_boundary_ids
        or boundary_manifest.get("samplesPerScenario") != len(EXPECTED_STARTUP_BOUNDARIES)
        or boundary_manifest.get("maxSampleDelayMilliseconds")
        != STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS
    ):
        raise EvidenceValidationError("startup boundary metric contract is missing or malformed")
    if startup_boundary_metrics is None:
        raise EvidenceValidationError("startup boundary metric inventory is missing")
    if is_navigation_gate:
        navigation_manifest = _require_object(
            manifest.get("navigation"), "navigation"
        )
        expected_route_manifest = [
            {"fromPageId": from_page, "toPageId": to_page}
            for from_page, to_page in NAVIGATION_ROUTE
        ]
        if (
            navigation_manifest.get("schema") != NAVIGATION_METRICS_SCHEMA
            or navigation_manifest.get("cycles") != NAVIGATION_CYCLES
            or navigation_manifest.get("transitionCountPerScenario")
            != EXPECTED_NAVIGATION_TRANSITIONS
            or navigation_manifest.get("route") != expected_route_manifest
            or navigation_manifest.get("selectionMethods")
            != list(NAVIGATION_SELECTION_METHODS)
            or navigation_manifest.get("metricsFile") != "navigation-metrics.jsonl"
            or navigation_manifest.get("startupReadyMetricsFile")
            != "startup-ready-metrics.jsonl"
            or navigation_manifest.get("returnToNormalPageId") != "my_workspace"
            or navigation_manifest.get("catalogBodyCountRequired") != 0
        ):
            raise EvidenceValidationError("navigation route, catalog, or process metric contract is malformed")
        startup_ready_manifest = _require_object(
            manifest.get("startupReadyMetrics"), "startupReadyMetrics"
        )
        if (
            startup_ready_manifest.get("schema") != STARTUP_READY_METRICS_SCHEMA
            or startup_ready_manifest.get("inventoryFile") != "startup-ready-metrics.jsonl"
            or startup_ready_manifest.get("samplesPerScenario") != 1
        ):
            raise EvidenceValidationError("external startup-ready process metric contract is malformed")
        if navigation_metrics is None or startup_ready_metrics is None:
            raise EvidenceValidationError("navigation process metric inventory is missing")
    elif navigation_metrics is not None or startup_ready_metrics is not None:
        raise EvidenceValidationError("P9-06 evidence unexpectedly includes P9-07 process metrics")
    expected_boundary_rows = [
        (scenario_id, event_id)
        for scenario_id in EXPECTED_SCENARIOS
        for event_id in expected_boundary_ids
    ]
    actual_boundary_rows = [
        (row.get("scenarioId"), row.get("eventId"))
        if isinstance(row, dict)
        else (None, None)
        for row in startup_boundary_metrics
    ]
    if actual_boundary_rows != expected_boundary_rows:
        raise EvidenceValidationError(
            "startup boundary metric rows are missing, duplicated, or out of order"
        )

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list) or tuple(
        scenario.get("id") if isinstance(scenario, dict) else None
        for scenario in scenarios
    ) != EXPECTED_SCENARIOS:
        raise EvidenceValidationError("manifest scenarios are missing, duplicated, or out of order")
    if is_navigation_gate:
        expected_startup_ready_rows = list(EXPECTED_SCENARIOS)
        actual_startup_ready_rows = [
            row.get("scenarioId") if isinstance(row, dict) else None
            for row in startup_ready_metrics or []
        ]
        if actual_startup_ready_rows != expected_startup_ready_rows:
            raise EvidenceValidationError(
                "external startup-ready process metric rows are missing, duplicated, or out of order"
            )
        expected_navigation_rows = [
            (scenario_id, route_index)
            for scenario_id in EXPECTED_SCENARIOS
            for route_index in range(EXPECTED_NAVIGATION_TRANSITIONS)
        ]
        actual_navigation_rows = [
            (row.get("scenarioId"), row.get("sampleIndex"))
            if isinstance(row, dict)
            else (None, None)
            for row in navigation_metrics or []
        ]
        if actual_navigation_rows != expected_navigation_rows:
            raise EvidenceValidationError(
                "navigation process metric rows are missing, duplicated, or out of order"
            )

    scenario_summaries: list[dict[str, Any]] = []
    violations: list[dict[str, Any]] = []
    consumed_rows: set[int] = set()
    previous_exit: datetime | None = None
    for scenario in scenarios:
        scenario = _require_object(scenario, "scenario")
        scenario_id = scenario["id"]
        if scenario.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} outcome is not completed")
        if scenario.get("budgetScope") != "normal-use-gate":
            raise EvidenceValidationError(f"{scenario_id} is not marked as a gated normal-use scenario")
        scenario_started = _parse_timestamp(scenario.get("startedAtUtc"), f"{scenario_id}.startedAtUtc")
        if (
            scenario.get("startupBoundarySampleCount") != len(EXPECTED_STARTUP_BOUNDARIES)
            or scenario.get("startupBoundaryEventIds") != expected_boundary_ids
        ):
            raise EvidenceValidationError(
                f"{scenario_id} manifest startup boundary checkpoints are incomplete"
            )

        fixture = _require_object(scenario.get("fixture"), f"{scenario_id}.fixture")
        fixture_id, class_count = EXPECTED_FIXTURES[scenario_id]
        if fixture.get("id") != fixture_id:
            raise EvidenceValidationError(f"{scenario_id} fixture identity is unexpected")
        _require_sha(fixture.get("sha256"), f"{scenario_id}.fixture.sha256")
        if fixture.get("classCount") != class_count or fixture.get("profilePathArgumentSupplied") is not True:
            raise EvidenceValidationError(f"{scenario_id} fixture shape is incorrect")
        fixture_argument = fixture.get("pathPassedToApp")
        if (
            not isinstance(fixture_argument, str)
            or not ntpath.isabs(fixture_argument)
            or ntpath.splitext(fixture_argument)[1].casefold() != ".tps"
        ):
            raise EvidenceValidationError(f"{scenario_id} exact .tps command-line path is missing")
        generator_id = EXPECTED_FIXTURE_GENERATORS[scenario_id]
        fixture_generator = qt_generators_by_id[generator_id]
        if not _same_windows_path(
            fixture.get("generatorExecutablePath"), fixture_generator.get("executablePath")
        ):
            raise EvidenceValidationError(f"{scenario_id} fixture generator path differs from Qt runtime evidence")
        if _require_sha(fixture.get("generatorExecutableSha256"), f"{scenario_id}.fixture.generatorExecutableSha256") != fixture_generator.get("executableSha256"):
            raise EvidenceValidationError(f"{scenario_id} fixture generator hash differs from Qt runtime evidence")
        if fixture.get("qtInstallationId") != qt_installation_id:
            raise EvidenceValidationError(f"{scenario_id} fixture generator used a different Qt installation")

        process = _require_object(scenario.get("process"), f"{scenario_id}.process")
        pid = _require_positive_int(process.get("pid"), f"{scenario_id}.process.pid")
        created_at_text = process.get("createdAtUtc")
        created_at = _parse_timestamp(created_at_text, f"{scenario_id}.process.createdAtUtc")
        process_path = process.get("canonicalExecutablePath")
        if not _same_windows_path(process_path, executable_path):
            raise EvidenceValidationError(f"{scenario_id} process image differs from staged package")
        if process.get("architecture") != "x64":
            raise EvidenceValidationError(f"{scenario_id} process architecture is not x64")
        if _require_sha(process.get("sha256"), f"{scenario_id}.process.sha256") != application_sha:
            raise EvidenceValidationError(f"{scenario_id} process image hash differs from package")
        readiness = _require_object(scenario.get("readiness"), f"{scenario_id}.readiness")
        if readiness.get("windowVisible") is not True or readiness.get("stablePolls") != 3:
            raise EvidenceValidationError(f"{scenario_id} did not prove a stable visible application window")
        window_handle = _require_positive_int(readiness.get("mainWindowHandle"), f"{scenario_id}.readiness.mainWindowHandle")
        window_title = readiness.get("mainWindowTitle")
        if not isinstance(window_title, str) or "ClassMngr Next" not in window_title:
            raise EvidenceValidationError(f"{scenario_id} has an unexpected application window title")
        if process.get("mainWindowHandle") != window_handle or process.get("mainWindowTitle") != window_title:
            raise EvidenceValidationError(f"{scenario_id} process and readiness window identity differ")

        diagnostic = _require_object(scenario.get("startupReadyDiagnostic"), f"{scenario_id}.startupReadyDiagnostic")
        diagnostic_at = _parse_timestamp(diagnostic.get("observedAtUtc"), f"{scenario_id}.startupReadyDiagnostic.observedAtUtc")
        if diagnostic_at < scenario_started or created_at > diagnostic_at:
            raise EvidenceValidationError(f"{scenario_id} startup-ready lifecycle timestamps are invalid")
        if diagnostic.get("stage") != "startup-ready" or diagnostic.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} did not capture a completed startup-ready diagnostic")
        if diagnostic.get("diagnostic") != "none" or diagnostic.get("diagnosticStage") != "none":
            raise EvidenceValidationError(f"{scenario_id} startup-ready diagnostic contains an error")
        checkpoint = _require_object(diagnostic.get("checkpoint"), f"{scenario_id}.startupReadyDiagnostic.checkpoint")
        if checkpoint.get("stage") != "startup-ready" or checkpoint.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} startup checkpoint identity is invalid")
        if checkpoint.get("sample_available") is not True or checkpoint.get("platform") != "windows":
            raise EvidenceValidationError(f"{scenario_id} startup checkpoint memory is unavailable")
        startup_memory = _require_object(checkpoint.get("metrics"), f"{scenario_id}.startup checkpoint metrics")
        startup_working_set = _require_positive_int(startup_memory.get("working_set_bytes"), f"{scenario_id}.startup working_set_bytes")
        raw_line = diagnostic.get("rawLine")
        if not isinstance(raw_line, str):
            raise EvidenceValidationError(f"{scenario_id} startup-ready raw line is missing")
        match = STARTUP_READY_RE.search(raw_line)
        if match is None:
            raise EvidenceValidationError(f"{scenario_id} raw line is not the app startup-ready diagnostic")
        try:
            parsed_checkpoint = json.loads(match.group(1))
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(f"{scenario_id} startup-ready checkpoint is malformed") from error
        if parsed_checkpoint != checkpoint:
            raise EvidenceValidationError(f"{scenario_id} parsed checkpoint differs from raw diagnostic")
        if not any(
            event.get("line") == raw_line
            and event.get("scenarioId") == scenario_id
            and event.get("stream") == diagnostic.get("stream")
            and event.get("observedAtUtc") == diagnostic.get("observedAtUtc")
            for event in startup_events
        ):
            raise EvidenceValidationError(f"{scenario_id} startup-ready diagnostic is absent from startup log")

        workspace_diagnostic = _require_object(
            scenario.get("workspaceOpenedDiagnostic"),
            f"{scenario_id}.workspaceOpenedDiagnostic",
        )
        workspace_at = _parse_timestamp(
            workspace_diagnostic.get("observedAtUtc"),
            f"{scenario_id}.workspaceOpenedDiagnostic.observedAtUtc",
        )
        workspace_line = workspace_diagnostic.get("rawLine")
        if not isinstance(workspace_line, str):
            raise EvidenceValidationError(f"{scenario_id} workspace-opened raw line is missing")
        workspace_match = WORKSPACE_OPENED_RE.search(workspace_line)
        if workspace_match is None:
            raise EvidenceValidationError(f"{scenario_id} workspace-opened record is malformed")
        workspace_log_matches = [
            (index, event)
            for index, event in enumerate(startup_events)
            if event.get("scenarioId") == scenario_id
            and isinstance(event.get("line"), str)
            and "ClassMngrNext startup stage=workspace-opened " in event["line"]
        ]
        if len(workspace_log_matches) != 1:
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one workspace-opened startup record"
            )
        workspace_log_index, workspace_log_event = workspace_log_matches[0]
        if (
            workspace_log_event.get("line") != workspace_line
            or workspace_log_event.get("stream") != workspace_diagnostic.get("stream")
            or workspace_log_event.get("observedAtUtc") != workspace_diagnostic.get("observedAtUtc")
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened diagnostic differs from captured startup log"
            )
        if (
            workspace_match.group("outcome") != "completed"
            or workspace_match.group("diagnostic") != "none"
            or workspace_match.group("diagnosticStage") != "none"
            or workspace_diagnostic.get("stage") != "workspace-opened"
            or workspace_diagnostic.get("outcome") != "completed"
            or workspace_diagnostic.get("diagnostic") != "none"
            or workspace_diagnostic.get("diagnosticStage") != "none"
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened record did not complete without a diagnostic"
            )
        try:
            workspace_checkpoint = _require_object(
                json.loads(workspace_match.group("checkpoint")),
                f"{scenario_id}.workspace-opened checkpoint",
            )
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint is malformed"
            ) from error
        if workspace_checkpoint != workspace_diagnostic.get("checkpoint"):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint differs from raw diagnostic"
            )
        if (
            workspace_checkpoint.get("stage") != "workspace-opened"
            or workspace_checkpoint.get("outcome") != "completed"
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint identity is invalid"
            )
        ready_log_matches = [
            index
            for index, event in enumerate(startup_events)
            if event.get("scenarioId") == scenario_id
            and event.get("line") == raw_line
            and event.get("stream") == diagnostic.get("stream")
            and event.get("observedAtUtc") == diagnostic.get("observedAtUtc")
        ]
        if len(ready_log_matches) != 1:
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one matching startup-ready startup record"
            )
        ready_log_index = ready_log_matches[0]
        if workspace_log_index >= ready_log_index or workspace_at >= diagnostic_at:
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened record did not precede startup-ready"
            )

        required_fonts_log_matches = [
            (index, event)
            for index, event in enumerate(startup_events)
            if event.get("scenarioId") == scenario_id
            and isinstance(event.get("line"), str)
            and "ClassMngrNext startup stage=required-fonts-loaded " in event["line"]
        ]
        if len(required_fonts_log_matches) != 1:
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one required-fonts-loaded startup record"
            )
        required_fonts_log_index, _ = required_fonts_log_matches[0]
        if required_fonts_log_index >= workspace_log_index or required_fonts_log_index >= ready_log_index:
            raise EvidenceValidationError(
                f"{scenario_id} required-fonts-loaded record did not precede workspace-opened and startup-ready"
            )

        navigation_detail: dict[str, Any] | None = None
        if is_navigation_gate:
            navigation_detail = _validate_navigation_scenario(
                scenario,
                startup_events,
                startup_ready_metrics or [],
                navigation_metrics or [],
                pid,
                created_at_text,
                executable_path,
                application_sha,
                diagnostic_at,
                ready_log_index,
                workspace_log_index,
            )

        inter_font_log_matches: list[tuple[int, dict[str, Any], dict[str, Any]]] = []
        for index, event in enumerate(startup_events):
            if event.get("scenarioId") != scenario_id or not isinstance(event.get("line"), str):
                continue
            checkpoint = _inter_font_resource_checkpoint(event["line"])
            if checkpoint is None:
                continue
            resource_id = checkpoint.get("resource_id")
            if resource_id not in EXPECTED_INTER_FONT_RESOURCE_IDS:
                raise EvidenceValidationError(
                    f"{scenario_id} has unexpected Inter font resource ID {resource_id!r}; TTC aliases are rejected"
                )
            request_scope = checkpoint.get("request_scope")
            if (
                checkpoint.get("event") != "resource-load"
                or checkpoint.get("outcome") != "succeeded"
                or not isinstance(request_scope, dict)
                or request_scope.get("kind") != "startup"
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} Inter font resource {resource_id} did not complete successfully during startup"
                )
            inter_font_log_matches.append((index, event, checkpoint))
        if [match[2].get("resource_id") for match in inter_font_log_matches] != list(
            EXPECTED_INTER_FONT_RESOURCE_IDS
        ):
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one successful startup event for each catalogued Inter TTF in order"
            )
        recorded_inter_events = scenario.get("interFontResourceEvents")
        if not isinstance(recorded_inter_events, list) or len(recorded_inter_events) != len(
            EXPECTED_INTER_FONT_RESOURCE_IDS
        ):
            raise EvidenceValidationError(f"{scenario_id} manifest Inter font event records are incomplete")
        prior_inter_at: datetime | None = None
        inter_font_summaries: list[dict[str, Any]] = []
        for expected_id, (index, event, checkpoint), raw_record in zip(
            EXPECTED_INTER_FONT_RESOURCE_IDS,
            inter_font_log_matches,
            recorded_inter_events,
            strict=True,
        ):
            record = _require_object(raw_record, f"{scenario_id}.interFontResourceEvents")
            event_at = _parse_timestamp(
                event.get("observedAtUtc"), f"{scenario_id}.{expected_id}.observedAtUtc"
            )
            if prior_inter_at is not None and event_at < prior_inter_at:
                raise EvidenceValidationError(f"{scenario_id} Inter TTF startup event timestamps are out of order")
            prior_inter_at = event_at
            if index >= required_fonts_log_index or index >= workspace_log_index or index >= ready_log_index or event_at >= diagnostic_at:
                raise EvidenceValidationError(
                    f"{scenario_id} Inter font resource {expected_id} occurred after startup readiness"
                )
            if (
                record.get("resourceId") != expected_id
                or record.get("outcome") != "succeeded"
                or record.get("requestScopeKind") != "startup"
                or record.get("startupLogIndex") != index
                or record.get("stream") != event.get("stream")
                or record.get("observedAtUtc") != event.get("observedAtUtc")
                or record.get("rawLine") != event.get("line")
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} Inter font manifest event differs from its startup log record"
                )
            inter_font_summaries.append(
                {
                    "resourceId": expected_id,
                    "startupLogIndex": index,
                    "outcome": "succeeded",
                    "observedAtUtc": event.get("observedAtUtc"),
                }
            )

        scenario_boundary_rows = [
            row for row in startup_boundary_metrics if row.get("scenarioId") == scenario_id
        ]
        scenario_boundary_logs: dict[str, tuple[int, dict[str, Any]]] = {}
        for boundary_log_index, event in enumerate(startup_events):
            if event.get("scenarioId") != scenario_id:
                continue
            event_line = event.get("line")
            if not isinstance(event_line, str):
                continue
            event_id = _startup_boundary_event_id(event_line)
            if event_id is None:
                continue
            if event_id in scenario_boundary_logs:
                raise EvidenceValidationError(
                    f"{scenario_id} has duplicate {event_id} startup boundary records"
                )
            scenario_boundary_logs[event_id] = (boundary_log_index, event)

        scenario_boundary_summaries: list[dict[str, Any]] = []
        prior_boundary_log_index = -1
        prior_boundary_at: datetime | None = None
        prior_boundary_sample_at: datetime | None = None
        for boundary_spec, boundary_row in zip(
            EXPECTED_STARTUP_BOUNDARIES, scenario_boundary_rows, strict=True
        ):
            event_id = str(boundary_spec["eventId"])
            log_match = scenario_boundary_logs.get(event_id)
            if log_match is None:
                raise EvidenceValidationError(
                    f"{scenario_id} is missing the {event_id} startup boundary log record"
                )
            boundary_log_index, boundary_event = log_match
            if boundary_log_index <= prior_boundary_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary log records are out of order"
                )
            if boundary_log_index >= ready_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup boundary occurred after startup-ready"
                )
            if event_id == "shell-created":
                if boundary_log_index <= workspace_log_index:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created startup record did not follow workspace-opened"
                    )
            elif boundary_log_index >= workspace_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup record occurred after workspace-opened"
                )
            prior_boundary_log_index = boundary_log_index

            event_line = str(boundary_event["line"])
            boundary_at_text = boundary_event.get("observedAtUtc")
            boundary_at = _parse_timestamp(
                boundary_at_text, f"{scenario_id}.{event_id}.startupLog.observedAtUtc"
            )
            if prior_boundary_at is not None and boundary_at < prior_boundary_at:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary event timestamps are out of order"
                )
            if event_id == "shell-created":
                if boundary_at < workspace_at:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created startup timestamp preceded workspace-opened"
                    )
            elif boundary_at > workspace_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup timestamp followed workspace-opened"
                )
            prior_boundary_at = boundary_at
            if boundary_spec["kind"] == "stage":
                stage_match = STARTUP_BOUNDARY_STAGE_RE.search(event_line)
                if (
                    stage_match is None
                    or stage_match.group("stage") != boundary_spec["value"]
                    or stage_match.group("outcome") != "completed"
                    or stage_match.group("diagnostic") != "none"
                    or stage_match.group("diagnosticStage") != "none"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage record is invalid"
                    )
                try:
                    event_checkpoint = json.loads(stage_match.group("checkpoint"))
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage checkpoint is malformed"
                    ) from error
                if (
                    not isinstance(event_checkpoint, dict)
                    or event_checkpoint.get("stage") != boundary_spec["value"]
                    or event_checkpoint.get("outcome") != "completed"
                    or event_checkpoint.get("sample_available") is not True
                    or event_checkpoint.get("platform") != "windows"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage checkpoint identity is invalid"
                    )
            else:
                resource_match = RESOURCE_CHECKPOINT_RE.fullmatch(event_line)
                if resource_match is None:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource checkpoint is malformed"
                    )
                try:
                    event_checkpoint = json.loads(resource_match.group(1))
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource checkpoint JSON is malformed"
                    ) from error
                request_scope = (
                    event_checkpoint.get("request_scope")
                    if isinstance(event_checkpoint, dict)
                    else None
                )
                if (
                    not isinstance(event_checkpoint, dict)
                    or event_checkpoint.get("event") != "resource-load"
                    or event_checkpoint.get("resource_id") != boundary_spec["value"]
                    or event_checkpoint.get("outcome") != "succeeded"
                    or not isinstance(request_scope, dict)
                    or request_scope.get("kind") != "startup"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource startup record is invalid"
                    )

            row_log_index = _require_nonnegative_int(
                boundary_row.get("startupLogIndex"), f"{scenario_id}.{event_id}.startupLogIndex"
            )
            if row_log_index != boundary_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} boundary snapshot references the wrong startup log record"
                )
            if (
                boundary_row.get("eventObservedAtUtc") != boundary_at_text
                or boundary_row.get("stream") != boundary_event.get("stream")
                or boundary_row.get("rawLine") != event_line
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} boundary snapshot differs from its startup log record"
                )
            boundary_sample_at = _parse_timestamp(
                boundary_row.get("timestampUtc"), f"{scenario_id}.{event_id}.timestampUtc"
            )
            delay_ms = (boundary_sample_at - boundary_at).total_seconds() * 1000
            recorded_delay = boundary_row.get("sampleDelayMilliseconds")
            recorded_delay_is_finite = False
            if isinstance(recorded_delay, (int, float)) and not isinstance(recorded_delay, bool):
                try:
                    recorded_delay_is_finite = math.isfinite(recorded_delay)
                except OverflowError:
                    recorded_delay_is_finite = False
            if (
                delay_ms < 0
                or delay_ms > STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS
                or isinstance(recorded_delay, bool)
                or not isinstance(recorded_delay, (int, float))
                or not recorded_delay_is_finite
                or recorded_delay < 0
                or abs(recorded_delay - delay_ms) > 1.0
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot was stale or its delay is malformed"
                )
            if prior_boundary_sample_at is not None and boundary_sample_at < prior_boundary_sample_at:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary snapshots are out of order"
                )
            prior_boundary_sample_at = boundary_sample_at
            if boundary_sample_at >= diagnostic_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot occurred after startup-ready"
                )
            if event_id == "shell-created":
                if boundary_sample_at < workspace_at:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created process snapshot preceded workspace-opened"
                    )
            elif boundary_sample_at > workspace_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot followed workspace-opened"
                )

            if (
                boundary_row.get("eventId") != event_id
                or boundary_row.get("phase") != "startup-boundary"
                or boundary_row.get("pid") != pid
                or boundary_row.get("processCreatedAtUtc") != created_at_text
                or not _same_windows_path(boundary_row.get("canonicalExecutablePath"), executable_path)
                or boundary_row.get("architecture") != "x64"
                or _require_sha(boundary_row.get("sha256"), f"{scenario_id}.{event_id}.sha256")
                != application_sha
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process identity differs from the packaged app"
                )
            boundary_metrics_values = {
                name: _require_positive_int(
                    boundary_row.get(name), f"{scenario_id}.{event_id}.{name}"
                )
                for name in (
                    "currentWorkingSetBytes",
                    "privateBytes",
                    "peakWorkingSetBytes",
                    "handles",
                    "threads",
                )
            }
            if boundary_metrics_values["peakWorkingSetBytes"] < boundary_metrics_values["currentWorkingSetBytes"]:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} peak working set is below current working set"
                )
            scenario_boundary_summaries.append(
                {
                    "eventId": event_id,
                    "startupLogIndex": boundary_log_index,
                    "eventObservedAtUtc": boundary_at_text,
                    "timestampUtc": boundary_row["timestampUtc"],
                    "sampleDelayMilliseconds": recorded_delay,
                    **boundary_metrics_values,
                }
            )

        if scenario.get("idleSeconds") != IDLE_SECONDS or scenario.get("sampleIntervalSeconds") != 1:
            raise EvidenceValidationError(f"{scenario_id} idle protocol is not a five-minute one-second trace")
        if scenario.get("userInteractionDuringIdle") is not False:
            raise EvidenceValidationError(f"{scenario_id} idle period included user interaction")
        samples = [(index, row) for index, row in enumerate(metrics) if row.get("scenarioId") == scenario_id]
        if len(samples) != SAMPLE_COUNT:
            raise EvidenceValidationError(f"{scenario_id} must contain exactly {SAMPLE_COUNT} metric samples")

        sample_times: list[datetime] = []
        sample_metrics: dict[str, list[int]] = {
            "currentWorkingSetBytes": [],
            "privateBytes": [],
            "peakWorkingSetBytes": [],
            "handles": [],
            "threads": [],
        }
        for expected_index, (row_index, sample) in enumerate(samples):
            if row_index in consumed_rows:
                raise EvidenceValidationError("a metric row was assigned to multiple scenarios")
            consumed_rows.add(row_index)
            sample_index = _require_nonnegative_int(sample.get("sampleIndex"), f"{scenario_id}.sampleIndex")
            if sample_index != expected_index:
                raise EvidenceValidationError(f"{scenario_id} sample indices are missing or out of order")
            expected_phase = (
                "post-navigation" if is_navigation_gate and expected_index == 0
                else "startup-ready" if expected_index == 0
                else "idle"
            )
            if sample.get("phase") != expected_phase:
                raise EvidenceValidationError(f"{scenario_id} sample phase is invalid")
            if sample.get("pid") != pid or sample.get("processCreatedAtUtc") != created_at_text:
                raise EvidenceValidationError(f"{scenario_id} sample process identity differs")
            if not _same_windows_path(sample.get("canonicalExecutablePath"), executable_path):
                raise EvidenceValidationError(f"{scenario_id} sample image path differs from package")
            if sample.get("architecture") != "x64" or _require_sha(sample.get("sha256"), "sample.sha256") != application_sha:
                raise EvidenceValidationError(f"{scenario_id} sample executable identity differs")
            sample_times.append(_parse_timestamp(sample.get("timestampUtc"), f"{scenario_id}.sample.timestampUtc"))
            for metric_name, values in sample_metrics.items():
                values.append(_require_positive_int(sample.get(metric_name), f"{scenario_id}.{metric_name}"))

        if is_navigation_gate:
            if navigation_detail is None:
                raise EvidenceValidationError(f"{scenario_id} navigation evidence was not evaluated")
            last_navigation_at = _parse_timestamp(
                navigation_detail["navigationTransitions"][-1].get("timestampUtc"),
                f"{scenario_id}.lastNavigationSample.timestampUtc",
            )
            if sample_times[0] < last_navigation_at:
                raise EvidenceValidationError(
                    f"{scenario_id} post-navigation checkpoint preceded the final route transition"
                )
            if sample_times[-1] - sample_times[0] < timedelta(seconds=IDLE_SECONDS):
                raise EvidenceValidationError(
                    f"{scenario_id} idle trace is shorter than five minutes after navigation"
                )
        else:
            if sample_times[0] < diagnostic_at or (sample_times[0] - diagnostic_at).total_seconds() > 5:
                raise EvidenceValidationError(f"{scenario_id} startup process sample is not adjacent to startup-ready")
        if not is_navigation_gate and sample_times[-1] - diagnostic_at < timedelta(seconds=IDLE_SECONDS):
            raise EvidenceValidationError(f"{scenario_id} idle trace is shorter than five minutes")
        for left, right in zip(sample_times, sample_times[1:]):
            interval = (right - left).total_seconds()
            if not SAMPLE_INTERVAL_MIN_SECONDS <= interval <= SAMPLE_INTERVAL_MAX_SECONDS:
                raise EvidenceValidationError(f"{scenario_id} sample interval {interval:.3f}s is outside the one-second cadence")
        first_external_sample_at = (
            _parse_timestamp(
                navigation_detail["startupReadyExternalTimestampUtc"],
                f"{scenario_id}.startupReadyExternal.timestampUtc",
            )
            if is_navigation_gate and navigation_detail is not None
            else sample_times[0]
        )
        if scenario_started > diagnostic_at or created_at > first_external_sample_at:
            raise EvidenceValidationError(f"{scenario_id} timestamp order is invalid")

        idle_completed = _parse_timestamp(scenario.get("idleCompletedAtUtc"), f"{scenario_id}.idleCompletedAtUtc")
        if idle_completed < sample_times[-1]:
            raise EvidenceValidationError(f"{scenario_id} idle completion timestamp precedes final sample")
        close_requested = _parse_timestamp(scenario.get("closeRequestedAtUtc"), f"{scenario_id}.closeRequestedAtUtc")
        exited_at = _parse_timestamp(scenario.get("exitedAtUtc"), f"{scenario_id}.exitedAtUtc")
        if close_requested < idle_completed or exited_at < close_requested:
            raise EvidenceValidationError(f"{scenario_id} process completion timestamps are invalid")
        if scenario.get("normalExit") is not True or scenario.get("exitCode") != 0:
            raise EvidenceValidationError(f"{scenario_id} did not exit normally with code 0")
        if exited_at > completed:
            raise EvidenceValidationError(f"{scenario_id} exit occurred after run completion")
        if previous_exit is not None and scenario_started < previous_exit:
            raise EvidenceValidationError("scenario processes were not isolated sequentially")
        previous_exit = exited_at

        idle_working_set = sample_metrics["currentWorkingSetBytes"][-1]
        post_navigation_working_set = sample_metrics["currentWorkingSetBytes"][0]
        startup_external_working_set = (
            navigation_detail["startupReadyExternalMetrics"]["currentWorkingSetBytes"]
            if is_navigation_gate and navigation_detail is not None
            else sample_metrics["currentWorkingSetBytes"][0]
        )
        scenario_violations = []
        if startup_working_set >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "startup-ready", "workingSetBytes": startup_working_set})
        if startup_external_working_set >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "startup-ready-external-sampler", "workingSetBytes": startup_external_working_set})
        if is_navigation_gate and navigation_detail is not None:
            scenario_violations.extend(navigation_detail["navigationViolations"])
            if post_navigation_working_set >= THRESHOLD_BYTES:
                scenario_violations.append(
                    {"point": "post-navigation", "workingSetBytes": post_navigation_working_set}
                )
        if idle_working_set >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "five-minute-idle", "workingSetBytes": idle_working_set})
        for violation in scenario_violations:
            violations.append({"scenarioId": scenario_id, **violation})

        scenario_summaries.append(
            {
                "id": scenario_id,
                "outcome": "completed",
                "budgetScope": "normal-use-gate",
                "fixtureId": fixture["id"],
                "fixtureSha256": fixture["sha256"],
                "fixturePathPassedToApp": fixture_argument,
                "workspaceOpenedAtUtc": workspace_diagnostic.get("observedAtUtc"),
                "workspaceOpenedOutcome": workspace_diagnostic.get("outcome"),
                "interFontResourceEvents": inter_font_summaries,
                "startupBoundaryMetrics": scenario_boundary_summaries,
                "sampleCount": SAMPLE_COUNT,
                "startupReadyDiagnosticAtUtc": diagnostic.get("observedAtUtc"),
                "startupReadyWorkingSetBytes": startup_working_set,
                "startupReadyExternalWorkingSetBytes": startup_external_working_set,
                "postNavigationWorkingSetBytes": post_navigation_working_set if is_navigation_gate else None,
                "documentCatalogDocumentCount": (
                    navigation_detail["documentCount"] if navigation_detail is not None else None
                ),
                "navigationTransitions": (
                    navigation_detail["navigationTransitions"] if navigation_detail is not None else []
                ),
                "returnToNormalMilestones": (
                    navigation_detail["returnToNormalMilestones"] if navigation_detail is not None else []
                ),
                "returnToNormalWorkingSetBytes": (
                    navigation_detail["returnToNormalWorkingSetBytes"] if navigation_detail is not None else []
                ),
                "cycleReturnWorkingSetDeltas": (
                    navigation_detail["cycleReturnWorkingSetDeltas"] if navigation_detail is not None else []
                ),
                "fiveMinuteIdleWorkingSetBytes": idle_working_set,
                "idleDurationSeconds": (
                    (sample_times[-1] - sample_times[0]).total_seconds()
                    if is_navigation_gate
                    else (sample_times[-1] - diagnostic_at).total_seconds()
                ),
                "currentWorkingSetBytes": _metric_summary(sample_metrics["currentWorkingSetBytes"]),
                "privateBytes": _metric_summary(sample_metrics["privateBytes"]),
                "peakWorkingSetBytes": _metric_summary(sample_metrics["peakWorkingSetBytes"]),
                "handles": _metric_summary(sample_metrics["handles"]),
                "threads": _metric_summary(sample_metrics["threads"]),
                "budgetViolations": scenario_violations,
            }
        )

    if len(consumed_rows) != len(metrics):
        raise EvidenceValidationError("metrics trace contains rows for an unknown scenario")
    budget_result = "failed" if violations else "passed"
    return {
        "schema": P9_07_SUMMARY_SCHEMA if is_navigation_gate else SUMMARY_SCHEMA,
        "taskId": P9_07_TASK_ID if is_navigation_gate else TASK_ID,
        "validation": "valid",
        "runStatus": "completed",
        "candidateSourceSha256": source_sha,
        "revision": revision,
        "configuration": configuration,
        "qtInstallation": qt_installation,
        "os": operating_system,
        "application": application,
        "package": {
            "id": package["id"],
            "sha256": package_sha,
            "fileCount": len(package_files),
        },
        "scenarioCount": len(scenario_summaries),
        "sampleCount": len(metrics),
        "thresholdBytes": THRESHOLD_BYTES,
        "comparison": "strictly-less-than",
        "startupBoundaryMetrics": {
            "schema": boundary_manifest["schema"],
            "inventoryFile": boundary_manifest["inventoryFile"],
            "sampleCount": len(startup_boundary_metrics),
            "samplesPerScenario": len(EXPECTED_STARTUP_BOUNDARIES),
        },
        "navigation": (
            {
                "schema": NAVIGATION_METRICS_SCHEMA,
                "metricsFile": "navigation-metrics.jsonl",
                "startupReadyMetricsFile": "startup-ready-metrics.jsonl",
                "cyclesPerScenario": NAVIGATION_CYCLES,
                "transitionsPerScenario": EXPECTED_NAVIGATION_TRANSITIONS,
                "sampleCount": len(navigation_metrics or []),
                "startupReadySampleCount": len(startup_ready_metrics or []),
                "selectionMethods": sorted(
                    {
                        str(row.get("selectionMethod"))
                        for row in navigation_metrics or []
                    }
                ),
                "documentBodyResourceEvents": 0,
            }
            if is_navigation_gate
            else None
        ),
        "budgetResult": budget_result,
        "gateResult": budget_result,
        "interpretation": (
            "Valid complete evidence; all gated working-set points are strictly below threshold."
            if budget_result == "passed"
            else "Valid complete evidence; one or more gated working-set points meet or exceed threshold."
        ),
        "scenarios": scenario_summaries,
        "budgetViolations": violations,
        "errors": [],
    }


def _load_manifest(path: Path) -> dict[str, Any]:
    return _require_object(_read_json(path, "manifest"), "manifest")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--startup-log", required=True, type=Path)
    parser.add_argument("--startup-boundary-metrics", required=True, type=Path)
    parser.add_argument("--startup-ready-metrics", type=Path)
    parser.add_argument("--navigation-metrics", type=Path)
    parser.add_argument("--navigation-plateau-diagnostic", action="store_true")
    parser.add_argument("--package-files", required=True, type=Path)
    parser.add_argument("--source-files", required=True, type=Path)
    parser.add_argument("--build-log", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    args = parser.parse_args(argv)

    manifest: dict[str, Any] | None = None
    try:
        manifest = _load_manifest(args.manifest)
        metrics = _read_jsonl(args.metrics, "metrics trace")
        startup_events = _read_jsonl(args.startup_log, "startup log")
        startup_boundary_metrics = _read_jsonl(
            args.startup_boundary_metrics, "startup boundary metric trace"
        )
        startup_ready_metrics = (
            _read_jsonl(args.startup_ready_metrics, "external startup-ready metric trace")
            if args.startup_ready_metrics is not None
            else None
        )
        navigation_metrics = (
            _read_jsonl(args.navigation_metrics, "navigation process metric trace")
            if args.navigation_metrics is not None
            else None
        )
        if args.navigation_plateau_diagnostic:
            if startup_ready_metrics is None or navigation_metrics is None:
                raise EvidenceValidationError(
                    "navigation plateau diagnostic requires startup-ready and navigation metric traces"
                )
            package_document = _require_object(
                _read_json(args.package_files, "package file inventory"), "package file inventory"
            )
            source_document = _require_object(
                _read_json(args.source_files, "source file inventory"), "source file inventory"
            )
            package_file_rows = package_document.get("files")
            source_file_rows = source_document.get("files")
            if not isinstance(package_file_rows, list) or not isinstance(source_file_rows, list):
                raise EvidenceValidationError("diagnostic package/source inventories must contain file arrays")
            summary = validate_navigation_plateau_documents(
                manifest,
                metrics,
                startup_events,
                startup_ready_metrics,
                navigation_metrics,
                package_file_rows,
                source_file_rows,
            )
            args.summary.parent.mkdir(parents=True, exist_ok=True)
            args.summary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(json.dumps({"validation": summary["status"], "budgetResult": summary["budgetResult"], "errors": []}))
            return 0
        boundary_manifest = _require_object(
            _require_object(manifest.get("startupBoundaryMetrics"), "startupBoundaryMetrics"),
            "startupBoundaryMetrics",
        )
        if args.startup_boundary_metrics.name != boundary_manifest.get("inventoryFile"):
            raise EvidenceValidationError("startup boundary metric file identity differs from manifest")
        if manifest.get("taskId") == P9_07_TASK_ID:
            navigation_manifest = _require_object(manifest.get("navigation"), "navigation")
            startup_ready_manifest = _require_object(
                manifest.get("startupReadyMetrics"), "startupReadyMetrics"
            )
            if (
                args.startup_ready_metrics is None
                or args.navigation_metrics is None
                or args.startup_ready_metrics.name != startup_ready_manifest.get("inventoryFile")
                or args.navigation_metrics.name != navigation_manifest.get("metricsFile")
            ):
                raise EvidenceValidationError("P9-07 process metric file identity differs from manifest")
        elif args.startup_ready_metrics is not None or args.navigation_metrics is not None:
            raise EvidenceValidationError("P9-06 validator invocation includes P9-07 metric files")
        package_files = _require_object(_read_json(args.package_files, "package file inventory"), "package file inventory").get("files")
        if not isinstance(package_files, list):
            raise EvidenceValidationError("package file inventory files must be an array")
        source_files = _require_object(_read_json(args.source_files, "source file inventory"), "source file inventory").get("files")
        if not isinstance(source_files, list):
            raise EvidenceValidationError("source file inventory files must be an array")
        try:
            build_log = args.build_log.read_bytes()
        except OSError as error:
            raise EvidenceValidationError(f"cannot read package prerequisite build log: {error}") from error
        package_build = _require_object(
            _require_object(manifest.get("configuration"), "configuration").get("packageBuild"),
            "configuration.packageBuild",
        )
        if not _same_windows_path(str(args.build_log.resolve()), package_build.get("logPath")):
            raise EvidenceValidationError("package prerequisite build log path differs from manifest")
        summary = validate_documents(
            manifest,
            metrics,
            startup_events,
            package_files,
            source_files,
            startup_boundary_metrics,
            build_log,
            startup_ready_metrics,
            navigation_metrics,
        )
        exit_code = 0 if summary["budgetResult"] == "passed" else 2
    except EvidenceValidationError as error:
        errors = [str(error)]
        if isinstance(manifest, dict) and isinstance(manifest.get("failure"), str):
            errors.append(manifest["failure"])
        if args.navigation_plateau_diagnostic:
            summary = {
                "schema": NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA,
                "taskId": P9_07_TASK_ID,
                "diagnosticOnly": True,
                "status": "invalid-diagnostic",
                "validation": "not-evaluated",
                "budgetResult": "not-evaluated",
                "gateAcceptanceClaimed": False,
                "errors": errors,
                "evidenceDirectory": str(args.summary.parent.resolve()),
            }
        else:
            summary = {
                "schema": (
                    P9_07_SUMMARY_SCHEMA
                    if isinstance(manifest, dict) and manifest.get("taskId") == P9_07_TASK_ID
                    else SUMMARY_SCHEMA
                ),
                "taskId": (
                    P9_07_TASK_ID
                    if isinstance(manifest, dict) and manifest.get("taskId") == P9_07_TASK_ID
                    else TASK_ID
                ),
                "validation": "invalid",
                "runStatus": manifest.get("status", "unknown") if isinstance(manifest, dict) else "unknown",
                "thresholdBytes": THRESHOLD_BYTES,
                "comparison": "strictly-less-than",
                "budgetResult": "not-evaluated",
                "gateResult": "not-evaluated",
                "interpretation": "Evidence is incomplete or invalid; memory budget was not evaluated.",
                "errors": errors,
                "budgetViolations": [],
            }
        exit_code = 1

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"validation": summary["validation"], "budgetResult": summary["budgetResult"], "errors": summary["errors"]}))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
